#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Memory transport implements the same API for explicitly selected original examples.
class MemoryReply : public QNetworkReply {
  public:
    MemoryReply(const QNetworkRequest& request, QByteArray bytes, int status, QObject* parent);
    void abort() override;
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override;

  protected:
    qint64 readData(char* data, qint64 maximum) override;

  private:
    QByteArray bytes_;
    qint64 position_ = 0;
};
class CatalogNetwork : public QNetworkAccessManager {
  public:
    using QNetworkAccessManager::QNetworkAccessManager;

  protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                                 QIODevice* outgoing = nullptr) override;
};
