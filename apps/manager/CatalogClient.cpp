#include "CatalogClient.h"
#include "CatalogNetwork.h"
#include "ipc/Protocol.h"
#include "scenes/ScenePackage.h"
#include "storage/LibraryStore.h"
#include <QBuffer>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QUuid>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

namespace {
using aetherwall::Json;
constexpr qint64 pageLimit = 512 * 1024, previewLimit = 2 * 1024 * 1024;
QString origin(const QUrl& url) {
    return url.scheme().toLower() + "://" + url.host().toLower() + ":" +
           QString::number(url.port(url.scheme() == "https" ? 443 : 80));
}
bool sha(const QString& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](QChar c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
QString text(const Json& value, std::size_t maximum = 512) {
    return QString::fromStdString(aetherwall::boundedText(value, maximum));
}
qint64 boundedSize(const Json& value, qint64 maximum) {
    if (!value.is_number_integer())
        throw std::invalid_argument("Expected bounded integer size");
    const auto size = value.get<std::int64_t>();
    if (size <= 0 || size > maximum)
        throw std::invalid_argument("Catalog size limit exceeded");
    return size;
}
QVariant variant(const Json& json) {
    const auto bytes = json.dump();
    return QJsonDocument::fromJson(QByteArray::fromStdString(bytes)).toVariant();
}
} // namespace
CatalogClient::CatalogClient(QObject* parent, QNetworkAccessManager* network,
                             QString cacheDirectory)
    : QObject(parent), network_(network ? network : new CatalogNetwork(this)),
      cache_(cacheDirectory.isEmpty()
                 ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/catalog"
                 : std::move(cacheDirectory)) {
    QDir().mkpath(cache_);
    deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        fail("Download deadline exceeded. Retry when the connection is available.");
    });
    connect(&validation_, &QFutureWatcher<QString>::finished, this, [this] {
        if (phase_ == "cancelling") {
            temporary_.reset();
            phase_ = "cancelled";
            emit changed();
            return;
        }
        const auto error = validation_.result();
        if (!error.isEmpty()) {
            fail(error);
            return;
        }
        const auto final = cache_ + "/" + downloadEntry_.value("sha256").toString() + ".awscene";
        if (QFile::exists(final) && !QFile::remove(final)) {
            fail("Cannot replace cached package.");
            return;
        }
        // close() keeps QTemporaryFile's native handle alive. Its own rename()
        // releases that handle before renaming, which Windows requires.
        if (!temporary_->rename(final)) {
            fail("Cannot commit verified package to cache: " + temporary_->errorString());
            return;
        }
        temporary_->setAutoRemove(false);
        temporary_.reset();
        readyPath_ = final;
        phase_ = "ready";
        emit changed();
        requestInstall();
    });
}
CatalogClient::~CatalogClient() {
    for (auto* reply : network_->findChildren<QNetworkReply*>()) {
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
    }
    validation_.waitForFinished();
}
QString CatalogClient::errorCode() const {
    if (error_.isEmpty())
        return {};
    if (phase_ == "ready")
        return "install";
    if (error_.contains("SHA-256") || error_.contains("exceeds catalog size"))
        return "integrity";
    if (error_.contains("Package rejected") || error_.contains("metadata disagrees"))
        return "package";
    for (const auto* term : {"cache", "Cache", "write", "flush", "staging", "budget"})
        if (error_.contains(QLatin1String(term)))
            return "storage";
    if (error_.startsWith("Invalid catalog response"))
        return "response";
    for (const auto* term : {"origin", "HTTPS", "URL", "Configure a catalog"})
        if (error_.contains(QLatin1String(term)))
            return "configuration";
    if (error_.contains("deadline"))
        return "timeout";
    return phase_ == "error" ? "download" : "catalog";
}
QString CatalogClient::configurationError(const QVariantMap& settings) {
    const auto values = settings.value("catalogAssetOrigins").toList();
    if (values.size() > 8)
        return "At most eight asset origins are supported.";
    for (const auto& value : values) {
        const QUrl url(value.toString());
        if (url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty() ||
            url.hasQuery() || url.hasFragment() || (!url.path().isEmpty() && url.path() != "/"))
            return "Asset origins require HTTPS scheme and host only.";
    }
    const QUrl url(settings.value("catalogBaseUrl").toString());
    const bool local =
        url.host() == "127.0.0.1" || url.host() == "localhost" || url.host() == "::1";
    if (!url.isEmpty() &&
        (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() ||
         url.hasFragment() || (!url.path().isEmpty() && url.path() != "/") ||
         (url.scheme() != "https" &&
          !(settings.value("allowLocalCatalog").toBool() && local && url.scheme() == "http"))))
        return "Use an HTTPS catalog origin, or explicitly enable loopback HTTP.";
    return {};
}
void CatalogClient::useConfigured() {
    const QVariantMap settings{{"catalogBaseUrl", configuration_.value("base")},
                               {"catalogAssetOrigins", configuration_.value("origins")},
                               {"allowLocalCatalog", configuration_.value("local")}};
    configuration_.clear();
    configure(settings);
}
void CatalogClient::configure(const QVariantMap& settings) {
    QVariantMap config{{"base", settings.value("catalogBaseUrl", "")},
                       {"origins", settings.value("catalogAssetOrigins", QVariantList())},
                       {"local", settings.value("allowLocalCatalog", false)}};
    if (configuration_ == config)
        return;
    configuration_ = config;
    origins_.clear();
    for (const auto& value : config.value("origins").toList()) {
        const QUrl url(value.toString());
        if (url.scheme() != "https" || url.host().isEmpty() || !url.userInfo().isEmpty() ||
            url.hasQuery() || url.hasFragment() || (!url.path().isEmpty() && url.path() != "/")) {
            setBase(QUrl());
            error_ = "Invalid asset origin. Use an HTTPS scheme and host only.";
            emit changed();
            return;
        }
        origins_.append(origin(url));
    }
    local_ = config.value("local").toBool();
    const QUrl url(config.value("base").toString());
    const bool loopback =
        url.host() == "127.0.0.1" || url.host() == "localhost" || url.host() == "::1";
    if (!url.isEmpty() &&
        (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() ||
         url.hasFragment() || (!url.path().isEmpty() && url.path() != "/") ||
         (url.scheme() != "https" && !(local_ && loopback && url.scheme() == "http")))) {
        setBase(QUrl());
        error_ = "Catalog requires an HTTPS origin. Loopback HTTP must be explicitly enabled.";
        emit changed();
        return;
    }
    setBase(url);
}
void CatalogClient::setBase(const QUrl& base) {
    cancel();
    ++generation_;
    if (pageReply_)
        pageReply_->abort();
    if (categoryReply_)
        categoryReply_->abort();
    const auto oldPreviews = previewReplies_;
    for (auto* reply : oldPreviews)
        reply->abort();
    loading_ = false;
    base_ = base;
    items_.clear();
    categories_.clear();
    serverDemo_ = false;
    pages_ = 1;
    total_ = 0;
    error_.clear();
    if (!base_.isEmpty()) {
        fetchCategories();
        browse();
    }
    emit changed();
}
void CatalogClient::useDemo() {
    origins_.clear();
    local_ = false;
    setBase(QUrl("mock://catalog"));
}
bool CatalogClient::allowedUrl(const QUrl& url) const {
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment())
        return false;
    if (base_.scheme() == "mock")
        return url.scheme() == "mock" && url.host() == "catalog" && url.port() == -1;
    const bool same = origin(url) == origin(base_);
    if (url.scheme() == "https")
        return same || origins_.contains(origin(url));
    const bool loopback =
        url.host() == "127.0.0.1" || url.host() == "localhost" || url.host() == "::1";
    return local_ && same && loopback && url.scheme() == "http";
}
QNetworkReply* CatalogClient::get(const QUrl& url) {
    if (!allowedUrl(url))
        throw std::invalid_argument("Catalog URL is outside the configured trusted origins");
    QNetworkRequest request(url);
    request.setTransferTimeout(10000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    request.setRawHeader("Accept-Encoding", "identity");
    request.setRawHeader("User-Agent", "AetherWall/0.4");
    auto* reply = network_->get(request);
    reply->setReadBufferSize(65536);
    return reply;
}
void CatalogClient::fetchCategories() {
    if (categoryReply_)
        categoryReply_->abort();
    auto* reply = get(base_.resolved(QUrl("/api/v1/categories")));
    categoryReply_ = reply;
    const auto base = base_;
    auto bytes = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, this, [reply, bytes] {
        while (reply->bytesAvailable()) {
            *bytes += reply->read(65536);
            if (bytes->size() > pageLimit) {
                reply->abort();
                break;
            }
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, base] {
        reply->deleteLater();
        if (base != base_ || reply->error() != QNetworkReply::NoError || bytes->size() > pageLimit)
            return;
        try {
            const auto value = aetherwall::parseJson(bytes->toStdString(), pageLimit);
            if (!value.at("items").is_array() || value.at("items").size() > 64)
                return;
            QVariantList categories;
            for (const auto& entry : value.at("items"))
                categories.append(QVariantMap{{"id", text(entry.at("id"), 32)}});
            categories_ = std::move(categories);
            emit changed();
        } catch (const std::exception&) { /* Browsing still works without category hints. */
        }
    });
}
void CatalogClient::browse(const QString& query, const QString& category, const QString& sort,
                           int page) {
    if (query.size() > 128 || category.size() > 32 ||
        (sort != "new" && sort != "popular" && sort != "trending") || page < 1 || page > 100000) {
        error_ = "Invalid catalog query.";
        emit changed();
        return;
    }
    query_ = query;
    category_ = category;
    sort_ = sort;
    page_ = page;
    const int generation = ++generation_;
    const auto oldPreviews = previewReplies_;
    for (auto* reply : oldPreviews)
        reply->abort();
    previewActive_ = previewNext_ = 0;
    if (pageReply_)
        pageReply_->abort();
    if (base_.isEmpty()) {
        error_ = "Configure a catalog address or open the bundled examples.";
        emit changed();
        return;
    }
    QUrl url = base_.resolved(
        QUrl(query.trimmed().isEmpty() ? "/api/v1/wallpapers" : "/api/v1/wallpapers/search"));
    QUrlQuery values;
    values.addQueryItem("q", query.trimmed());
    values.addQueryItem("category", category);
    values.addQueryItem("sort", sort);
    values.addQueryItem("page", QString::number(page));
    values.addQueryItem("limit", "24");
    url.setQuery(values);
    QNetworkReply* reply = nullptr;
    try {
        reply = get(url);
    } catch (const std::exception& error) {
        error_ = QString::fromUtf8(error.what());
        emit changed();
        return;
    }
    pageReply_ = reply;
    loading_ = true;
    error_.clear();
    auto bytes = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, this, [reply, bytes] {
        while (reply->bytesAvailable()) {
            *bytes += reply->read(65536);
            if (bytes->size() > pageLimit) {
                reply->abort();
                break;
            }
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, generation] {
        reply->deleteLater();
        if (generation != generation_)
            return;
        pageReply_ = nullptr;
        loading_ = false;
        if (reply->error() != QNetworkReply::NoError ||
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200 ||
            bytes->size() > pageLimit) {
            error_ = "Catalog unavailable or response rejected. Installed wallpapers keep working.";
            emit changed();
            return;
        }
        try {
            validatePage(*bytes);
            previews(generation);
        } catch (const std::exception& error) {
            error_ = "Invalid catalog response: " + QString::fromUtf8(error.what());
        }
        emit changed();
    });
    emit changed();
}
void CatalogClient::validatePage(const QByteArray& bytes) {
    const auto value = aetherwall::parseJson(bytes.toStdString(), pageLimit);
    if (value.at("schemaVersion") != 1 || !value.at("items").is_array() ||
        value.at("items").size() > 48 || !value.at("page").is_number_integer() ||
        value.at("page").get<int>() != page_ || !value.at("total").is_number_integer() ||
        !value.at("pages").is_number_integer())
        throw std::invalid_argument("Unsupported page envelope");
    const auto total = value.at("total").get<int>(), pages = value.at("pages").get<int>();
    if (total < 0 || total > 10000000 || pages < 1 || pages > 1000000)
        throw std::invalid_argument("Invalid page totals");
    QVariantList staged;
    QSet<QString> ids;
    for (const auto& item : value.at("items")) {
        const auto id = text(item.at("id"), 36);
        if (QUuid(id).isNull() || QUuid(id).toString(QUuid::WithoutBraces) != id ||
            ids.contains(id))
            throw std::invalid_argument("Invalid or duplicate scene UUID");
        ids.insert(id);
        text(item.at("title"), 128);
        text(item.at("author"), 128);
        text(item.at("description"), 2000);
        text(item.at("license"), 128);
        text(item.at("category"), 32);
        text(item.at("version"), 32);
        if (!sha(text(item.at("sha256"), 64)) || !sha(text(item.at("previewSha256"), 64)))
            throw std::invalid_argument("Invalid catalog hash");
        boundedSize(item.at("packageSize"),
                    static_cast<qint64>(aetherwall::SceneLimits::packageBytes));
        boundedSize(item.at("previewSize"), previewLimit);
        bool supported = false;
        if (!item.at("supportedSchemaVersions").is_array() ||
            item.at("supportedSchemaVersions").size() > 8)
            throw std::invalid_argument("Invalid scene schema list");
        for (const auto& version : item.at("supportedSchemaVersions"))
            if (version.is_number_integer() && version == 1)
                supported = true;
        if (!supported)
            throw std::invalid_argument("Scene requires an unsupported Core schema");
        for (const auto* key : {"previewUrl", "packageUrl", "licenseProofUrl"})
            if (!allowedUrl(QUrl(text(item.at(key), 2048))))
                throw std::invalid_argument("Untrusted asset origin");
        auto entry = variant(item).toMap();
        entry["previewLocal"] = QUrl();
        staged.append(entry);
    }
    items_ = std::move(staged);
    total_ = total;
    pages_ = pages;
    serverDemo_ = value.value("demo", false);
}
void CatalogClient::previews(int generation) {
    while (generation == generation_ && previewActive_ < 2 && previewNext_ < items_.size()) {
        const int index = previewNext_++;
        const auto entry = items_.at(index).toMap();
        const auto final = cache_ + "/" + entry.value("previewSha256").toString() + ".png";
        // Verify cached bytes too; cache corruption never bypasses hash/dimension checks.
        auto save = [this, index, generation, entry, final](const QByteArray& data) {
            if (generation != generation_ || index >= items_.size() ||
                data.size() != entry.value("previewSize").toLongLong() ||
                QString::fromLatin1(
                    QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) !=
                    entry.value("previewSha256").toString())
                return;
            QBuffer buffer;
            buffer.setData(data);
            buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer, "png");
            const auto size = reader.size();
            if (!size.isValid() || size.width() > 2048 || size.height() > 2048 ||
                reader.read().isNull())
                return;
            try {
                trimCache();
            } catch (const std::exception&) {
                return;
            }
            QSaveFile file(final);
            if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() ||
                !file.commit())
                return;
            auto changed = items_.at(index).toMap();
            changed["previewLocal"] = QUrl::fromLocalFile(final);
            items_[index] = changed;
            emit this->changed();
        };
        QFile cached(final);
        if (cached.size() == entry.value("previewSize").toLongLong() &&
            cached.open(QIODevice::ReadOnly)) {
            const auto bytes = cached.read(previewLimit + 1);
            if (QString::fromLatin1(
                    QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) ==
                entry.value("previewSha256").toString()) {
                save(bytes);
                continue;
            }
        }
        ++previewActive_;
        auto* reply = get(QUrl(entry.value("previewUrl").toString()));
        previewReplies_.insert(reply);
        auto bytes = std::make_shared<QByteArray>();
        connect(reply, &QIODevice::readyRead, this, [reply, bytes] {
            while (reply->bytesAvailable()) {
                *bytes += reply->read(65536);
                if (bytes->size() > previewLimit) {
                    reply->abort();
                    break;
                }
            }
        });
        connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, save, generation] {
            previewReplies_.remove(reply);
            reply->deleteLater();
            if (generation != generation_)
                return;
            --previewActive_;
            if (reply->error() == QNetworkReply::NoError &&
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200)
                save(*bytes);
            previews(generation);
        });
    }
}
void CatalogClient::trimCache() {
    constexpr qint64 budget = 256 * 1024 * 1024 - 34 * 1024 * 1024;
    auto files = QDir(cache_).entryInfoList(QDir::Files, QDir::Time | QDir::Reversed);
    qint64 size = 0;
    for (const auto& file : files)
        size += file.size();
    for (const auto& file : files) {
        if (size <= budget)
            break;
        if (file.absoluteFilePath() != readyPath_ &&
            (file.suffix() == "png" || file.suffix() == "awscene") &&
            QFile::remove(file.absoluteFilePath()))
            size -= file.size();
    }
    if (size > budget)
        throw std::runtime_error("Catalog cache budget exceeded");
}
void CatalogClient::download(const QString& id) {
    if (downloadReply_ || validation_.isRunning() || phase_ == "validating" ||
        phase_ == "cancelling" || phase_ == "installing")
        return;
    const auto found = std::find_if(items_.begin(), items_.end(), [&](const QVariant& value) {
        return value.toMap().value("id").toString() == id;
    });
    if (found == items_.end())
        return;
    beginDownload(found->toMap());
}
void CatalogClient::beginDownload(const QVariantMap& entry) {
    if (downloadReply_ || validation_.isRunning() || phase_ == "validating" ||
        phase_ == "cancelling" || phase_ == "installing")
        return;
    downloadEntry_ = entry;
    downloadId_ = entry.value("id").toString();
    readyPath_.clear();
    received_ = 0;
    expected_ = downloadEntry_.value("packageSize").toLongLong();
    error_.clear();
    phase_ = "downloading";
    try {
        trimCache();
        temporary_ = std::make_unique<QTemporaryFile>(cache_ + "/.download-XXXXXX.awscene");
        if (!temporary_->open())
            throw std::runtime_error("Cannot create download staging file");
        hash_ = std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);
        deadline_.start(120000);
        downloadTo(QUrl(downloadEntry_.value("packageUrl").toString()), 0);
    } catch (const std::exception& error) {
        fail(QString::fromUtf8(error.what()));
    }
    emit changed();
}
void CatalogClient::downloadTo(const QUrl& url, int redirects) {
    auto* reply = get(url);
    downloadReply_ = reply;
    connect(reply, &QIODevice::readyRead, this, [this, reply] {
        if (downloadReply_ != reply || phase_ != "downloading")
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200) {
            reply->read(65536);
            return;
        }
        if (!reply->rawHeader("Content-Encoding").isEmpty() &&
            reply->rawHeader("Content-Encoding") != "identity") {
            fail("Encoded package transfer rejected.");
            return;
        }
        while (reply->bytesAvailable()) {
            const auto bytes = reply->read(65536);
            if (received_ + bytes.size() > expected_) {
                fail("Package exceeds catalog size.");
                return;
            }
            if (temporary_->write(bytes) != bytes.size()) {
                fail("Cannot write downloaded package.");
                return;
            }
            received_ += bytes.size();
            hash_->addData(bytes);
        }
        emit changed();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, redirects] {
        reply->deleteLater();
        if (downloadReply_ != reply || phase_ != "downloading")
            return;
        downloadReply_ = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (status >= 300 && status < 400 && !redirect.isEmpty()) {
            try {
                if (redirects >= 3 || received_ != 0)
                    throw std::runtime_error("Package redirect limit exceeded");
                downloadTo(reply->url().resolved(redirect), redirects + 1);
            } catch (const std::exception& error) {
                fail(QString::fromUtf8(error.what()));
            }
            return;
        }
        deadline_.stop();
        if (reply->error() != QNetworkReply::NoError || status != 200) {
            fail("Download failed. Check the connection and retry.");
            return;
        }
        if (received_ != expected_ || QString::fromLatin1(hash_->result().toHex()) !=
                                          downloadEntry_.value("sha256").toString()) {
            fail("Package size or SHA-256 mismatch. Nothing was installed.");
            return;
        }
        if (!temporary_->flush()) {
            fail("Cannot flush downloaded package.");
            return;
        }
        temporary_->close();
        phase_ = "validating";
        emit changed();
        const auto path = temporary_->fileName();
        const auto entry = downloadEntry_;
        validation_.setFuture(QtConcurrent::run([path, entry]() -> QString {
            try {
                const auto package =
                    aetherwall::loadScenePackage(aetherwall::textPath(path.toStdString()));
                if (QString::fromStdString(package->scene.id).toLower() !=
                        entry.value("id").toString() ||
                    QString::fromStdString(package->scene.title) !=
                        entry.value("title").toString() ||
                    QString::fromStdString(package->scene.author) !=
                        entry.value("author").toString() ||
                    QString::fromStdString(package->scene.license) !=
                        entry.value("license").toString())
                    return "Package metadata disagrees with catalog.";
                return {};
            } catch (const std::exception& error) {
                return "Package rejected: " + QString::fromUtf8(error.what());
            }
        }));
    });
}
void CatalogClient::cancel() {
    deadline_.stop();
    if (validation_.isRunning() || phase_ == "validating") {
        phase_ = "cancelling";
        emit changed();
        return;
    }
    if (downloadReply_) {
        auto* reply = downloadReply_.data();
        downloadReply_ = nullptr;
        phase_ = "cancelled";
        reply->abort();
        temporary_.reset();
        hash_.reset();
        emit changed();
    }
}
void CatalogClient::fail(const QString& message) {
    deadline_.stop();
    error_ = message;
    phase_ = "error";
    if (downloadReply_) {
        auto* reply = downloadReply_.data();
        downloadReply_ = nullptr;
        reply->abort();
    }
    temporary_.reset();
    hash_.reset();
    emit changed();
}
void CatalogClient::retry() {
    if (!downloadId_.isEmpty() && !downloadEntry_.isEmpty())
        beginDownload(downloadEntry_);
    else
        browse(query_, category_, sort_, page_);
}
void CatalogClient::openLicense(const QString& id) {
    for (const auto& value : items_)
        if (value.toMap().value("id").toString() == id) {
            const QUrl url(value.toMap().value("licenseProofUrl").toString());
            if (url.scheme() == "https" && allowedUrl(url))
                QDesktopServices::openUrl(url);
        }
}
void CatalogClient::requestInstall() {
    if (readyPath_.isEmpty() || phase_ != "ready")
        return;
    const auto source =
        QUrl(downloadEntry_.value("packageUrl").toString())
            .adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveUserInfo)
            .toString();
    emit packageReady(readyPath_, downloadId_, downloadEntry_.value("sha256").toString(), expected_,
                      source);
}
void CatalogClient::installed() {
    phase_ = "installed";
    emit changed();
}
void CatalogClient::importFailed(const QString& message) {
    phase_ = "ready";
    error_ = message;
    emit changed();
}
