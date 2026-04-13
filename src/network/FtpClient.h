#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QThread>
#include <QMutex>

#include "models/FtpFileEntry.h"

class FtpClient : public QObject {
    Q_OBJECT
public:
    explicit FtpClient(const QString &host, uint16_t port = 21, const QString &password = {}, QObject *parent = nullptr);
    ~FtpClient();

    // All operations run on worker thread — results via signals
    void connectToServer();
    void disconnect();

    void listDirectory(const QString &path);
    void createDirectory(const QString &path);
    void deleteFile(const QString &path);
    void deleteDirectory(const QString &path);
    void rename(const QString &from, const QString &to);

    void uploadFile(const QString &localPath, const QString &remotePath);
    void downloadFile(const QString &remotePath, const QString &localPath);
    void uploadDirectory(const QString &localPath, const QString &remotePath);

signals:
    void connected();
    void disconnected();
    void directoryListed(const QString &path, const QList<FtpFileEntry> &entries);
    void transferProgress(qint64 bytesTransferred, qint64 totalBytes);
    void operationCompleted(const QString &operation);
    void operationFailed(const QString &operation, const QString &error);

private:
    // Internal methods that run on the worker thread
    void doConnect();
    void doListDirectory(const QString &path);
    void doCreateDirectory(const QString &path);
    void doDeleteFile(const QString &path);
    void doDeleteDirectory(const QString &path);
    void doRename(const QString &from, const QString &to);
    void doUploadFile(const QString &localPath, const QString &remotePath);
    void doDownloadFile(const QString &remotePath, const QString &localPath);
    void doUploadDirectory(const QString &localPath, const QString &remotePath);

    struct FtpResponse {
        int code = 0;
        QString message;
    };

    FtpResponse sendCommand(const QString &command);
    FtpResponse readResponse();
    bool enterPassiveMode(QString &host, uint16_t &port);
    QByteArray readDataConnection(QTcpSocket &dataSocket);
    QList<FtpFileEntry> parseDirectoryListing(const QString &path, const QString &listing);

    QString m_host;
    uint16_t m_port;
    QString m_password;

    QThread *m_workerThread = nullptr;
    QTcpSocket *m_controlSocket = nullptr;
    bool m_connected = false;
};
