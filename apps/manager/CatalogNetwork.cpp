#include "CatalogNetwork.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrlQuery>
#include <algorithm>
#include <cstring>

MemoryReply::MemoryReply(const QNetworkRequest& request, QByteArray bytes, int status,
                         QObject* parent)
    : QNetworkReply(parent), bytes_(std::move(bytes)) {
    setRequest(request);
    setUrl(request.url());
    setOperation(QNetworkAccessManager::GetOperation);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
    setHeader(QNetworkRequest::ContentLengthHeader, bytes_.size());
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    QTimer::singleShot(0, this, [this, status] {
        if (isFinished())
            return;
        if (status != 200) {
            setError(ContentNotFoundError, "Mock resource unavailable");
            emit errorOccurred(error());
        }
        emit downloadProgress(bytes_.size(), bytes_.size());
        emit readyRead();
        setFinished(true);
        emit finished();
    });
}
void MemoryReply::abort() {
    if (isFinished())
        return;
    setError(OperationCanceledError, "Cancelled");
    setFinished(true);
    emit errorOccurred(error());
    emit finished();
}
qint64 MemoryReply::bytesAvailable() const {
    return bytes_.size() - position_ + QNetworkReply::bytesAvailable();
}
qint64 MemoryReply::readData(char* data, qint64 maximum) {
    const auto count = std::min(maximum, static_cast<qint64>(bytes_.size()) - position_);
    if (count <= 0)
        return -1;
    std::memcpy(data, bytes_.constData() + position_, static_cast<std::size_t>(count));
    position_ += count;
    return count;
}
QNetworkReply* CatalogNetwork::createRequest(Operation operation, const QNetworkRequest& request,
                                             QIODevice* outgoing) {
    const auto url = request.url();
    if (url.scheme() != "mock")
        return QNetworkAccessManager::createRequest(operation, request, outgoing);
    auto reply = [&](QByteArray data, int status = 200) {
        return new MemoryReply(request, std::move(data), status, this);
    };
    if (url.host() != "catalog" || operation != GetOperation)
        return reply("{}", 404);
    QFile file(":/catalog/catalog.json");
    if (!file.open(QIODevice::ReadOnly))
        return reply("{}", 404);
    const auto catalog = QJsonDocument::fromJson(file.readAll()).object();
    const auto entries = catalog.value("items").toArray();
    if (url.path() == "/api/v1/health")
        return reply(R"({"status":"ok","apiVersion":1,"demo":true})");
    if (url.path() == "/api/v1/categories") {
        QMap<QString, int> counts;
        for (const auto& entry : entries)
            ++counts[entry.toObject().value("category").toString()];
        QJsonArray items;
        for (auto it = counts.begin(); it != counts.end(); ++it)
            items.append(QJsonObject{{"id", it.key()}, {"count", it.value()}});
        return reply(QJsonDocument(QJsonObject{{"items", items}}).toJson(QJsonDocument::Compact));
    }
    auto publicItem = [](QJsonObject item) {
        for (const auto& key : {"packageUrl", "previewUrl", "licenseProofUrl"})
            item[key] = "mock://catalog/" + item.value(key).toString();
        return item;
    };
    if (url.path() == "/api/v1/wallpapers" || url.path() == "/api/v1/wallpapers/search") {
        QUrlQuery query(url);
        const int page = std::max(1, query.queryItemValue("page").toInt());
        const int limit = query.hasQueryItem("limit")
                              ? std::clamp(query.queryItemValue("limit").toInt(), 1, 48)
                              : 24;
        const auto category = query.queryItemValue("category"), search = query.queryItemValue("q");
        QList<QJsonObject> filtered;
        for (const auto& entry : entries) {
            const auto item = entry.toObject();
            const auto text = item.value("title").toString() + " " +
                              item.value("author").toString() + " " +
                              item.value("description").toString();
            if ((!category.isEmpty() && item.value("category").toString() != category) ||
                !text.contains(search, Qt::CaseInsensitive))
                continue;
            filtered.append(item);
        }
        std::sort(filtered.begin(), filtered.end(), [](const auto& a, const auto& b) {
            return a.value("id").toString() < b.value("id").toString();
        });
        QJsonArray items;
        for (qsizetype i = (page - 1) * limit;
             i < std::min(static_cast<qsizetype>(page * limit), filtered.size()); ++i)
            items.append(publicItem(filtered.at(i)));
        return reply(
            QJsonDocument(
                QJsonObject{
                    {"schemaVersion", 1},
                    {"page", page},
                    {"limit", limit},
                    {"total", filtered.size()},
                    {"pages", std::max(1, static_cast<int>((filtered.size() + limit - 1) / limit))},
                    {"demo", true},
                    {"popularityAvailable", false},
                    {"items", items}})
                .toJson(QJsonDocument::Compact));
    }
    if (url.path().startsWith("/api/v1/wallpapers/")) {
        const auto id = url.path().section('/', -1);
        for (const auto& entry : entries)
            if (entry.toObject().value("id").toString() == id)
                return reply(
                    QJsonDocument(publicItem(entry.toObject())).toJson(QJsonDocument::Compact));
        return reply("{}", 404);
    }
    if (url.path().startsWith("/assets/") && !url.path().contains("..") &&
        !url.path().contains('\\')) {
        QFile asset(":/catalog" + url.path());
        if (asset.open(QIODevice::ReadOnly))
            return reply(asset.readAll());
    }
    return reply("{}", 404);
}
