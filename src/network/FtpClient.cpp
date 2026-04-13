#include "network/FtpClient.h"
#include "app/Log.h"

#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QThread>

static constexpr int ChunkSize = 65536;
static constexpr int SocketTimeout = 10000; // 10s for control commands
static constexpr int DataTimeout = 30000;   // 30s for data transfers

FtpClient::FtpClient(const QString &host, uint16_t port, const QString &password, QObject *parent)
    : QObject(parent)
    , m_host(host)
    , m_port(port)
    , m_password(password)
{
}

FtpClient::~FtpClient()
{
    disconnect();
}

// --- Public methods (dispatch to worker thread) ---

void FtpClient::connectToServer()
{
    if (m_workerThread) return;

    m_workerThread = new QThread(this);
    m_workerThread->start();

    // Move work to thread via lambda
    QMetaObject::invokeMethod(this, [this]() { doConnect(); }, Qt::QueuedConnection);
    moveToThread(m_workerThread);
}

void FtpClient::disconnect()
{
    if (m_controlSocket) {
        m_controlSocket->disconnectFromHost();
        delete m_controlSocket;
        m_controlSocket = nullptr;
    }
    m_connected = false;

    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    emit disconnected();
}

void FtpClient::listDirectory(const QString &path)
{
    QMetaObject::invokeMethod(this, [this, path]() { doListDirectory(path); }, Qt::QueuedConnection);
}

void FtpClient::createDirectory(const QString &path)
{
    QMetaObject::invokeMethod(this, [this, path]() { doCreateDirectory(path); }, Qt::QueuedConnection);
}

void FtpClient::deleteFile(const QString &path)
{
    QMetaObject::invokeMethod(this, [this, path]() { doDeleteFile(path); }, Qt::QueuedConnection);
}

void FtpClient::deleteDirectory(const QString &path)
{
    QMetaObject::invokeMethod(this, [this, path]() { doDeleteDirectory(path); }, Qt::QueuedConnection);
}

void FtpClient::rename(const QString &from, const QString &to)
{
    QMetaObject::invokeMethod(this, [this, from, to]() { doRename(from, to); }, Qt::QueuedConnection);
}

void FtpClient::uploadFile(const QString &localPath, const QString &remotePath)
{
    QMetaObject::invokeMethod(this, [this, localPath, remotePath]() { doUploadFile(localPath, remotePath); }, Qt::QueuedConnection);
}

void FtpClient::downloadFile(const QString &remotePath, const QString &localPath)
{
    QMetaObject::invokeMethod(this, [this, remotePath, localPath]() { doDownloadFile(remotePath, localPath); }, Qt::QueuedConnection);
}

void FtpClient::uploadDirectory(const QString &localPath, const QString &remotePath)
{
    QMetaObject::invokeMethod(this, [this, localPath, remotePath]() { doUploadDirectory(localPath, remotePath); }, Qt::QueuedConnection);
}

// --- Protocol implementation ---

FtpClient::FtpResponse FtpClient::readResponse()
{
    FtpResponse resp;
    QString fullResponse;

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
            fullResponse += line + "\n";
            qCDebug(logFtp) << "<" << line;

            // Check if this is the final line (NNN<space>message)
            static QRegularExpression re(R"(^(\d{3})\s)");
            auto match = re.match(line);
            if (match.hasMatch()) {
                resp.code = match.captured(1).toInt();
                resp.message = line.mid(4);
                return resp;
            }
        }
    }
}

FtpClient::FtpResponse FtpClient::sendCommand(const QString &command)
{
    qCDebug(logFtp) << ">" << command;
    m_controlSocket->write((command + "\r\n").toUtf8());
    m_controlSocket->flush();
    return readResponse();
}

bool FtpClient::enterPassiveMode(QString &host, uint16_t &port)
{
    auto resp = sendCommand("PASV");
    if (resp.code != 227)
        return false;

    // Parse (h1,h2,h3,h4,p1,p2)
    static QRegularExpression re(R"(\((\d+),(\d+),(\d+),(\d+),(\d+),(\d+)\))");
    auto match = re.match(resp.message);
    if (!match.hasMatch())
        return false;

    // Ignore h1-h4, use original host (device may report internal IP)
    host = m_host;
    port = static_cast<uint16_t>(match.captured(5).toInt() * 256 + match.captured(6).toInt());
    return true;
}

QByteArray FtpClient::readDataConnection(QTcpSocket &dataSocket)
{
    QByteArray result;
    while (dataSocket.state() == QAbstractSocket::ConnectedState || dataSocket.bytesAvailable() > 0) {
        if (dataSocket.bytesAvailable() > 0) {
            result.append(dataSocket.readAll());
        } else {
            dataSocket.waitForReadyRead(DataTimeout);
        }
    }
    // Read any remaining
    if (dataSocket.bytesAvailable() > 0)
        result.append(dataSocket.readAll());
    return result;
}

QList<FtpFileEntry> FtpClient::parseDirectoryListing(const QString &path, const QString &listing)
{
    QList<FtpFileEntry> entries;
    const auto lines = listing.split('\n', Qt::SkipEmptyParts);

    for (const auto &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;

        // Unix ls -l format: perms links owner group size month day time name
        auto parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() < 9)
            continue;

        FtpFileEntry entry;
        entry.isDirectory = trimmed.startsWith('d');
        entry.size = parts[4].toULongLong();

        // Name is everything from field 8 onwards (may contain spaces)
        entry.name = QStringList(parts.mid(8)).join(' ');

        if (entry.name == "." || entry.name == "..")
            continue;

        entry.path = path;
        if (!entry.path.endsWith('/'))
            entry.path += '/';
        entry.path += entry.name;

        entries.append(entry);
    }

    // Sort: directories first, then alphabetically
    std::sort(entries.begin(), entries.end(), [](const FtpFileEntry &a, const FtpFileEntry &b) {
        if (a.isDirectory != b.isDirectory)
            return a.isDirectory > b.isDirectory;
        return a.name.toLower() < b.name.toLower();
    });

    return entries;
}

// --- Worker thread implementations ---

void FtpClient::doConnect()
{
    m_controlSocket = new QTcpSocket();
    m_controlSocket->connectToHost(m_host, m_port);

    if (!m_controlSocket->waitForConnected(SocketTimeout)) {
        emit operationFailed("connect", "Connection timed out");
        return;
    }

    // Read greeting
    auto greeting = readResponse();
    if (greeting.code != 220) {
        emit operationFailed("connect", QStringLiteral("Unexpected greeting: %1").arg(greeting.code));
        return;
    }

    // Login
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

    // Binary mode
    sendCommand("TYPE I");

    m_connected = true;
    emit connected();
    qCInfo(logFtp) << "FTP connected to" << m_host;
}

void FtpClient::doListDirectory(const QString &path)
{
    if (!m_connected) {
        emit operationFailed("listDirectory", "Not connected");
        return;
    }

    // Change directory
    auto cwdResp = sendCommand("CWD " + path);
    if (cwdResp.code != 250) {
        emit operationFailed("listDirectory", QStringLiteral("CWD failed: %1").arg(cwdResp.message));
        return;
    }

    // Enter passive mode
    QString dataHost;
    uint16_t dataPort;
    if (!enterPassiveMode(dataHost, dataPort)) {
        emit operationFailed("listDirectory", "PASV failed");
        return;
    }

    // Connect data channel
    QTcpSocket dataSocket;
    dataSocket.connectToHost(dataHost, dataPort);
    if (!dataSocket.waitForConnected(SocketTimeout)) {
        emit operationFailed("listDirectory", "Data connection failed");
        return;
    }

    // Send LIST
    auto listResp = sendCommand("LIST");
    if (listResp.code != 150 && listResp.code != 125) {
        emit operationFailed("listDirectory", QStringLiteral("LIST failed: %1").arg(listResp.message));
        return;
    }

    // Read data
    QByteArray data = readDataConnection(dataSocket);
    dataSocket.close();

    // Read completion
    readResponse();

    auto entries = parseDirectoryListing(path, QString::fromUtf8(data));
    emit directoryListed(path, entries);
}

void FtpClient::doCreateDirectory(const QString &path)
{
    if (!m_connected) { emit operationFailed("createDirectory", "Not connected"); return; }

    auto resp = sendCommand("MKD " + path);
    if (resp.code == 257)
        emit operationCompleted("createDirectory");
    else
        emit operationFailed("createDirectory", resp.message);
}

void FtpClient::doDeleteFile(const QString &path)
{
    if (!m_connected) { emit operationFailed("deleteFile", "Not connected"); return; }

    auto resp = sendCommand("DELE " + path);
    if (resp.code == 250)
        emit operationCompleted("deleteFile");
    else
        emit operationFailed("deleteFile", resp.message);
}

void FtpClient::doDeleteDirectory(const QString &path)
{
    if (!m_connected) { emit operationFailed("deleteDirectory", "Not connected"); return; }

    auto resp = sendCommand("RMD " + path);
    if (resp.code == 250)
        emit operationCompleted("deleteDirectory");
    else
        emit operationFailed("deleteDirectory", resp.message);
}

void FtpClient::doRename(const QString &from, const QString &to)
{
    if (!m_connected) { emit operationFailed("rename", "Not connected"); return; }

    auto rnfr = sendCommand("RNFR " + from);
    if (rnfr.code != 350) {
        emit operationFailed("rename", rnfr.message);
        return;
    }

    auto rnto = sendCommand("RNTO " + to);
    if (rnto.code == 250)
        emit operationCompleted("rename");
    else
        emit operationFailed("rename", rnto.message);
}

void FtpClient::doUploadFile(const QString &localPath, const QString &remotePath)
{
    if (!m_connected) { emit operationFailed("uploadFile", "Not connected"); return; }

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit operationFailed("uploadFile", "Cannot open local file: " + localPath);
        return;
    }

    qint64 totalSize = file.size();

    QString dataHost;
    uint16_t dataPort;
    if (!enterPassiveMode(dataHost, dataPort)) {
        emit operationFailed("uploadFile", "PASV failed");
        return;
    }

    QTcpSocket dataSocket;
    dataSocket.connectToHost(dataHost, dataPort);
    if (!dataSocket.waitForConnected(SocketTimeout)) {
        emit operationFailed("uploadFile", "Data connection failed");
        return;
    }

    auto storResp = sendCommand("STOR " + remotePath);
    if (storResp.code != 150 && storResp.code != 125) {
        emit operationFailed("uploadFile", storResp.message);
        return;
    }

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

void FtpClient::doDownloadFile(const QString &remotePath, const QString &localPath)
{
    if (!m_connected) { emit operationFailed("downloadFile", "Not connected"); return; }

    // Get file size
    auto sizeResp = sendCommand("SIZE " + remotePath);
    qint64 totalSize = (sizeResp.code == 213) ? sizeResp.message.trimmed().toLongLong() : -1;

    QString dataHost;
    uint16_t dataPort;
    if (!enterPassiveMode(dataHost, dataPort)) {
        emit operationFailed("downloadFile", "PASV failed");
        return;
    }

    QTcpSocket dataSocket;
    dataSocket.connectToHost(dataHost, dataPort);
    if (!dataSocket.waitForConnected(SocketTimeout)) {
        emit operationFailed("downloadFile", "Data connection failed");
        return;
    }

    auto retrResp = sendCommand("RETR " + remotePath);
    if (retrResp.code != 150 && retrResp.code != 125) {
        emit operationFailed("downloadFile", retrResp.message);
        return;
    }

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
        received += remaining.size();
        emit transferProgress(received, totalSize);
    }

    dataSocket.close();
    file.close();
    readResponse();

    emit operationCompleted("downloadFile");
}

void FtpClient::doUploadDirectory(const QString &localPath, const QString &remotePath)
{
    if (!m_connected) { emit operationFailed("uploadDirectory", "Not connected"); return; }

    QDir dir(localPath);
    if (!dir.exists()) {
        emit operationFailed("uploadDirectory", "Local directory not found: " + localPath);
        return;
    }

    // Create remote directory
    sendCommand("MKD " + remotePath); // May already exist, ignore error

    // Enumerate files
    QDirIterator it(localPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    QStringList files;
    QStringList dirs;

    while (it.hasNext()) {
        it.next();
        QString relPath = dir.relativeFilePath(it.filePath());
        if (it.fileInfo().isDir())
            dirs.append(relPath);
        else
            files.append(relPath);
    }

    // Sort dirs to create parents first
    std::sort(dirs.begin(), dirs.end());

    // Create directories
    for (const auto &d : dirs) {
        sendCommand("MKD " + remotePath + "/" + d);
    }

    // Upload files
    int total = files.size();
    int current = 0;
    for (const auto &f : files) {
        current++;
        doUploadFile(localPath + "/" + f, remotePath + "/" + f);
    }

    emit operationCompleted("uploadDirectory");
}
