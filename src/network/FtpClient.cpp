#include "network/FtpClient.h"
#include "app/Log.h"

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

static constexpr int ChunkSize = 65536;
static constexpr int SocketTimeout = 10000;
static constexpr int DataTimeout = 30000;

// === FtpClient (public proxy) ===

FtpClient::FtpClient(const QString &host, uint16_t port, const QString &password, QObject *parent)
    : QObject(parent)
{
    m_workerThread = new QThread(this);
    m_worker = new FtpWorker(host, port, password); // no parent — will be moved
    m_worker->moveToThread(m_workerThread);

    // Forward all signals from worker to this proxy
    connect(m_worker, &FtpWorker::connected, this, &FtpClient::connected);
    connect(m_worker, &FtpWorker::disconnected, this, &FtpClient::disconnected);
    connect(m_worker, &FtpWorker::directoryListed, this, &FtpClient::directoryListed);
    connect(m_worker, &FtpWorker::transferProgress, this, &FtpClient::transferProgress);
    connect(m_worker, &FtpWorker::operationCompleted, this, &FtpClient::operationCompleted);
    connect(m_worker, &FtpWorker::operationFailed, this, &FtpClient::operationFailed);

    // Clean up worker when thread finishes
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_workerThread->start();
}

FtpClient::~FtpClient()
{
    disconnect();
}

void FtpClient::connectToServer()
{
    QMetaObject::invokeMethod(m_worker, &FtpWorker::doConnect, Qt::QueuedConnection);
}

void FtpClient::disconnect()
{
    if (!m_workerThread)
        return;

    QMetaObject::invokeMethod(m_worker, &FtpWorker::doDisconnect, Qt::BlockingQueuedConnection);

    m_workerThread->quit();
    m_workerThread->wait();
    m_workerThread = nullptr; // owned by 'this', will be deleted with parent
}

void FtpClient::listDirectory(const QString &path)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, path]() { w->doListDirectory(path); }, Qt::QueuedConnection);
}

void FtpClient::createDirectory(const QString &path)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, path]() { w->doCreateDirectory(path); }, Qt::QueuedConnection);
}

void FtpClient::deleteFile(const QString &path)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, path]() { w->doDeleteFile(path); }, Qt::QueuedConnection);
}

void FtpClient::deleteDirectory(const QString &path)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, path]() { w->doDeleteDirectory(path); }, Qt::QueuedConnection);
}

void FtpClient::rename(const QString &from, const QString &to)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, from, to]() { w->doRename(from, to); }, Qt::QueuedConnection);
}

void FtpClient::uploadFile(const QString &localPath, const QString &remotePath)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, localPath, remotePath]() { w->doUploadFile(localPath, remotePath); }, Qt::QueuedConnection);
}

void FtpClient::downloadFile(const QString &remotePath, const QString &localPath)
{
    QMetaObject::invokeMethod(m_worker, [w = m_worker, remotePath, localPath]() { w->doDownloadFile(remotePath, localPath); }, Qt::QueuedConnection);
}

// === FtpWorker (runs on dedicated thread) ===

FtpWorker::FtpWorker(const QString &host, uint16_t port, const QString &password)
    : m_host(host), m_port(port), m_password(password)
{
}

FtpWorker::FtpResponse FtpWorker::readResponse()
{
    FtpResponse resp;
    static QRegularExpression re(R"(^(\d{3})\s)");

    while (true) {
        if (!m_controlSocket->canReadLine()) {
            if (!m_controlSocket->waitForReadyRead(SocketTimeout)) {
                resp.code = -1;
                resp.message = "Timeout reading response";
                return resp;
            }
        }

        while (m_controlSocket->canReadLine()) {
            QString line = QString::fromUtf8(m_controlSocket->readLine()).trimmed();
            qCDebug(logFtp) << "<" << line;

            auto match = re.match(line);
            if (match.hasMatch()) {
                resp.code = match.captured(1).toInt();
                resp.message = line.mid(4);
                return resp;
            }
        }
    }
}

FtpWorker::FtpResponse FtpWorker::sendCommand(const QString &command)
{
    qCDebug(logFtp) << ">" << command;
    m_controlSocket->write((command + "\r\n").toUtf8());
    m_controlSocket->flush();
    return readResponse();
}

bool FtpWorker::enterPassiveMode(QString &host, uint16_t &port)
{
    auto resp = sendCommand("PASV");
    if (resp.code != 227)
        return false;

    static QRegularExpression re(R"(\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\))");
    auto match = re.match(resp.message);
    if (!match.hasMatch())
        return false;

    // Use original host (device may report internal IP)
    host = m_host;
    port = static_cast<uint16_t>(match.captured(5).toInt() * 256 + match.captured(6).toInt());
    return true;
}

QList<FtpFileEntry> FtpWorker::parseDirectoryListing(const QString &path, const QString &listing)
{
    QList<FtpFileEntry> entries;
    const auto lines = listing.split('\n', Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        auto parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 9) continue;

        FtpFileEntry entry;
        entry.isDirectory = trimmed.startsWith('d');
        entry.size = parts[4].toULongLong();
        entry.name = QStringList(parts.mid(8)).join(' ');

        if (entry.name == "." || entry.name == "..") continue;

        entry.path = path;
        if (!entry.path.endsWith('/')) entry.path += '/';
        entry.path += entry.name;

        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const FtpFileEntry &a, const FtpFileEntry &b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
        return a.name.toLower() < b.name.toLower();
    });

    return entries;
}

void FtpWorker::doConnect()
{
    m_controlSocket = new QTcpSocket(this);
    m_controlSocket->connectToHost(m_host, m_port);

    if (!m_controlSocket->waitForConnected(SocketTimeout)) {
        emit operationFailed("connect", "Connection timed out");
        delete m_controlSocket;
        m_controlSocket = nullptr;
        return;
    }

    auto greeting = readResponse();
    if (greeting.code != 220) {
        emit operationFailed("connect", QStringLiteral("Unexpected greeting: %1").arg(greeting.code));
        delete m_controlSocket;
        m_controlSocket = nullptr;
        return;
    }

    QString user = m_password.isEmpty() ? "anonymous" : "";
    QString pass = m_password.isEmpty() ? "anonymous@" : m_password;

    auto userResp = sendCommand("USER " + user);
    if (userResp.code != 331 && userResp.code != 230) {
        emit operationFailed("connect", QStringLiteral("USER failed: %1").arg(userResp.message));
        return;
    }

    if (userResp.code == 331) {
        auto passResp = sendCommand("PASS " + pass);
        if (passResp.code != 230) {
            emit operationFailed("connect", QStringLiteral("PASS failed: %1").arg(passResp.message));
            return;
        }
    }

    sendCommand("TYPE I");
    m_connected = true;
    emit connected();
    qCInfo(logFtp) << "FTP connected to" << m_host;
}

void FtpWorker::doDisconnect()
{
    if (m_controlSocket) {
        if (m_connected)
            sendCommand("QUIT");
        m_controlSocket->disconnectFromHost();
        delete m_controlSocket;
        m_controlSocket = nullptr;
    }
    m_connected = false;
    emit disconnected();
}

void FtpWorker::doListDirectory(const QString &path)
{
    if (!m_connected) { emit operationFailed("listDirectory", "Not connected"); return; }

    auto cwdResp = sendCommand("CWD " + path);
    if (cwdResp.code != 250) {
        emit operationFailed("listDirectory", QStringLiteral("CWD failed: %1").arg(cwdResp.message));
        return;
    }

    QString dataHost; uint16_t dataPort;
    if (!enterPassiveMode(dataHost, dataPort)) {
        emit operationFailed("listDirectory", "PASV failed");
        return;
    }

    QTcpSocket dataSocket;
    dataSocket.connectToHost(dataHost, dataPort);
    if (!dataSocket.waitForConnected(SocketTimeout)) {
        emit operationFailed("listDirectory", "Data connection failed");
        return;
    }

    auto listResp = sendCommand("LIST");
    if (listResp.code != 150 && listResp.code != 125) {
        emit operationFailed("listDirectory", QStringLiteral("LIST failed: %1").arg(listResp.message));
        return;
    }

    QByteArray data;
    while (dataSocket.state() == QAbstractSocket::ConnectedState || dataSocket.bytesAvailable() > 0) {
        if (dataSocket.bytesAvailable() > 0)
            data.append(dataSocket.readAll());
        else
            dataSocket.waitForReadyRead(DataTimeout);
    }
    if (dataSocket.bytesAvailable() > 0) data.append(dataSocket.readAll());
    dataSocket.close();

    readResponse();

    auto entries = parseDirectoryListing(path, QString::fromUtf8(data));
    emit directoryListed(path, entries);
}

void FtpWorker::doCreateDirectory(const QString &path)
{
    if (!m_connected) { emit operationFailed("createDirectory", "Not connected"); return; }
    auto resp = sendCommand("MKD " + path);
    if (resp.code == 257) emit operationCompleted("createDirectory");
    else emit operationFailed("createDirectory", resp.message);
}

void FtpWorker::doDeleteFile(const QString &path)
{
    if (!m_connected) { emit operationFailed("deleteFile", "Not connected"); return; }
    auto resp = sendCommand("DELE " + path);
    if (resp.code == 250) emit operationCompleted("deleteFile");
    else emit operationFailed("deleteFile", resp.message);
}

void FtpWorker::doDeleteDirectory(const QString &path)
{
    if (!m_connected) { emit operationFailed("deleteDirectory", "Not connected"); return; }
    auto resp = sendCommand("RMD " + path);
    if (resp.code == 250) emit operationCompleted("deleteDirectory");
    else emit operationFailed("deleteDirectory", resp.message);
}

void FtpWorker::doRename(const QString &from, const QString &to)
{
    if (!m_connected) { emit operationFailed("rename", "Not connected"); return; }
    auto rnfr = sendCommand("RNFR " + from);
    if (rnfr.code != 350) { emit operationFailed("rename", rnfr.message); return; }
    auto rnto = sendCommand("RNTO " + to);
    if (rnto.code == 250) emit operationCompleted("rename");
    else emit operationFailed("rename", rnto.message);
}

void FtpWorker::doUploadFile(const QString &localPath, const QString &remotePath)
{
    if (!m_connected) { emit operationFailed("uploadFile", "Not connected"); return; }

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit operationFailed("uploadFile", "Cannot open local file: " + localPath);
        return;
    }

    qint64 totalSize = file.size();
    QString dataHost; uint16_t dataPort;
    if (!enterPassiveMode(dataHost, dataPort)) { emit operationFailed("uploadFile", "PASV failed"); return; }

    QTcpSocket dataSocket;
    dataSocket.connectToHost(dataHost, dataPort);
    if (!dataSocket.waitForConnected(SocketTimeout)) { emit operationFailed("uploadFile", "Data connection failed"); return; }

    auto storResp = sendCommand("STOR " + remotePath);
    if (storResp.code != 150 && storResp.code != 125) { emit operationFailed("uploadFile", storResp.message); return; }

    qint64 sent = 0;
    while (!file.atEnd()) {
        QByteArray chunk = file.read(ChunkSize);
        dataSocket.write(chunk);
        dataSocket.waitForBytesWritten(DataTimeout);
        sent += chunk.size();
        emit transferProgress(sent, totalSize);
    }

    dataSocket.close();
    file.close();
    readResponse();
    emit operationCompleted("uploadFile");
}

void FtpWorker::doDownloadFile(const QString &remotePath, const QString &localPath)
{
    if (!m_connected) { emit operationFailed("downloadFile", "Not connected"); return; }

    auto sizeResp = sendCommand("SIZE " + remotePath);
    qint64 totalSize = (sizeResp.code == 213) ? sizeResp.message.trimmed().toLongLong() : -1;

    QString dataHost; uint16_t dataPort;
    if (!enterPassiveMode(dataHost, dataPort)) { emit operationFailed("downloadFile", "PASV failed"); return; }

    QTcpSocket dataSocket;
    dataSocket.connectToHost(dataHost, dataPort);
    if (!dataSocket.waitForConnected(SocketTimeout)) { emit operationFailed("downloadFile", "Data connection failed"); return; }

    auto retrResp = sendCommand("RETR " + remotePath);
    if (retrResp.code != 150 && retrResp.code != 125) { emit operationFailed("downloadFile", retrResp.message); return; }

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit operationFailed("downloadFile", "Cannot create local file: " + localPath);
        dataSocket.close();
        return;
    }

    qint64 received = 0;
    while (dataSocket.state() == QAbstractSocket::ConnectedState || dataSocket.bytesAvailable() > 0) {
        if (dataSocket.bytesAvailable() > 0) {
            QByteArray chunk = dataSocket.read(ChunkSize);
            file.write(chunk);
            received += chunk.size();
            emit transferProgress(received, totalSize);
        } else {
            dataSocket.waitForReadyRead(DataTimeout);
        }
    }
    if (dataSocket.bytesAvailable() > 0) {
        QByteArray remaining = dataSocket.readAll();
        file.write(remaining);
    }

    dataSocket.close();
    file.close();
    readResponse();
    emit operationCompleted("downloadFile");
}
