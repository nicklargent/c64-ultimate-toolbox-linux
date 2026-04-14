#include "network/CliServer.h"
#include "network/C64Connection.h"
#include "network/C64ApiClient.h"
#include "models/BasicTokenizer.h"
#include "app/Log.h"

#include <QFile>
#include <QUrl>
#include <QUrlQuery>
#include <QEventLoop>
#include <QTimer>

CliServer::CliServer(C64Connection *connection, QObject *parent)
    : QObject(parent)
    , m_connection(connection)
{
}

CliServer::~CliServer()
{
    stop();
}

bool CliServer::start()
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &CliServer::onNewConnection);

    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        qCWarning(logApp) << "CliServer: failed to listen:" << m_server->errorString();
        return false;
    }

    // Write port file
    QFile portFile(PortFile);
    if (portFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        portFile.write(QByteArray::number(m_server->serverPort()));
        portFile.close();
    }

    qCInfo(logApp) << "CliServer listening on port" << m_server->serverPort();
    return true;
}

void CliServer::stop()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        m_server = nullptr;
    }
    QFile::remove(PortFile);
}

uint16_t CliServer::port() const
{
    return m_server ? m_server->serverPort() : 0;
}

void CliServer::onNewConnection()
{
    while (auto *socket = m_server->nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            handleRequest(socket);
        });
        // Timeout cleanup
        QTimer::singleShot(10000, socket, [socket]() {
            if (socket->state() == QAbstractSocket::ConnectedState)
                socket->disconnectFromHost();
            socket->deleteLater();
        });
    }
}

void CliServer::handleRequest(QTcpSocket *socket)
{
    QByteArray raw = socket->readAll();
    QString request = QString::fromUtf8(raw);

    // Minimal HTTP parser
    int headerEnd = request.indexOf("\r\n\r\n");
    QString headers = (headerEnd >= 0) ? request.left(headerEnd) : request;
    QByteArray body = (headerEnd >= 0) ? raw.mid(headerEnd + 4) : QByteArray();

    QStringList firstLine = headers.section("\r\n", 0, 0).split(' ');
    if (firstLine.size() < 2) {
        sendResponse(socket, 400, "text/plain", "Bad request");
        return;
    }

    QString method = firstLine[0];
    QUrl url(firstLine[1]);
    QString path = url.path();
    QUrlQuery query(url);

    QByteArray responseBody;
    QString contentType = "text/plain";

    if (path == "/info" && method == "GET") {
        responseBody = handleInfo();
        contentType = "application/json";
    } else if (path == "/screen" && method == "GET") {
        responseBody = handleScreen();
    } else if (path == "/screen/raw" && method == "GET") {
        responseBody = handleScreenRaw();
    } else if (path == "/type" && method == "POST") {
        responseBody = handleType(body);
    } else if (path == "/basic" && method == "POST") {
        responseBody = handleBasic(body);
    } else if (path == "/peek" && method == "GET") {
        responseBody = handlePeek(query.queryItemValue("addr"), query.queryItemValue("len"));
    } else if (path == "/poke" && method == "POST") {
        responseBody = handlePoke(query.queryItemValue("addr"), body);
    } else if (path == "/reset" && method == "POST") {
        responseBody = handleReset();
    } else {
        sendResponse(socket, 404, "text/plain", "Not found");
        return;
    }

    sendResponse(socket, 200, contentType, responseBody);
}

void CliServer::sendResponse(QTcpSocket *socket, int status, const QString &contentType, const QByteArray &body)
{
    QString statusText = (status == 200) ? "OK" : (status == 404) ? "Not Found" : "Error";
    QByteArray response;
    response.append(QStringLiteral("HTTP/1.1 %1 %2\r\n").arg(status).arg(statusText).toUtf8());
    response.append(QStringLiteral("Content-Type: %1\r\n").arg(contentType).toUtf8());
    response.append(QStringLiteral("Content-Length: %1\r\n").arg(body.size()).toUtf8());
    response.append("Connection: close\r\n");
    response.append("\r\n");
    response.append(body);

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

// --- Synchronous memory operations ---

QByteArray CliServer::readMemorySync(uint16_t address, int length)
{
    auto *api = m_connection->apiClient();
    if (!api) return {};

    QByteArray result;
    QEventLoop loop;
    bool done = false;

    auto conn = connect(api, &C64ApiClient::memoryDataReceived, this,
        [&](uint16_t addr, const QByteArray &data) {
            if (addr == address) {
                result = data;
                done = true;
                loop.quit();
            }
        });

    auto errConn = connect(api, &C64ApiClient::requestFailed, this,
        [&](const QString &, const QString &) {
            done = true;
            loop.quit();
        });

    api->readMemory(address, length);
    if (!done) {
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
    }

    disconnect(conn);
    disconnect(errConn);
    return result;
}

void CliServer::writeMemorySync(uint16_t address, const QByteArray &data)
{
    auto *api = m_connection->apiClient();
    if (!api) return;

    QEventLoop loop;
    bool done = false;

    auto conn = connect(api, &C64ApiClient::requestSucceeded, this,
        [&](const QString &) { done = true; loop.quit(); });
    auto errConn = connect(api, &C64ApiClient::requestFailed, this,
        [&](const QString &, const QString &) { done = true; loop.quit(); });

    api->writeMemory(address, data);
    if (!done) {
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
    }

    disconnect(conn);
    disconnect(errConn);
}

void CliServer::writeMemoryHexSync(uint16_t address, const QString &hex)
{
    auto *api = m_connection->apiClient();
    if (!api) return;

    QEventLoop loop;
    bool done = false;

    auto conn = connect(api, &C64ApiClient::requestSucceeded, this,
        [&](const QString &) { done = true; loop.quit(); });
    auto errConn = connect(api, &C64ApiClient::requestFailed, this,
        [&](const QString &, const QString &) { done = true; loop.quit(); });

    api->writeMemoryHex(address, hex);
    if (!done) {
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
    }

    disconnect(conn);
    disconnect(errConn);
}

// --- Screen code to ASCII ---

QString CliServer::screenCodeToAscii(uint8_t code)
{
    // Strip reverse video bit
    uint8_t c = code & 0x7F;

    if (c == 0x00) return "@";
    if (c >= 0x01 && c <= 0x1A) return QString(QChar('A' + c - 1));
    if (c == 0x1B) return "[";
    if (c == 0x1C) return "\\";  // £ → backslash for terminal compat
    if (c == 0x1D) return "]";
    if (c == 0x1E) return "^";   // ↑
    if (c == 0x1F) return "<";   // ←
    if (c == 0x20) return " ";
    if (c >= 0x21 && c <= 0x3F) return QString(QChar(c));
    // 0x40-0x5F repeats 0x00-0x1F
    if (c >= 0x40 && c <= 0x5F) return screenCodeToAscii(c - 0x40);
    // 0x60-0x7F are graphics characters
    return ".";
}

// --- Handlers ---

QByteArray CliServer::handleInfo()
{
    auto info = m_connection->deviceInfo();
    return QStringLiteral(
        "{ \"product\": \"%1\", \"firmware\": \"%2\", \"hostname\": \"%3\", "
        "\"fpga\": \"%4\", \"core\": \"%5\" }")
        .arg(info.product, info.firmwareVersion, info.hostname,
             info.fpgaVersion, info.coreVersion).toUtf8();
}

QByteArray CliServer::handleScreen()
{
    QByteArray screenRam = readMemorySync(0x0400, 1000);
    if (screenRam.isEmpty())
        return "Error: could not read screen RAM\n";

    QString result;
    for (int row = 0; row < 25; row++) {
        QString line;
        for (int col = 0; col < 40; col++) {
            int offset = row * 40 + col;
            if (offset < screenRam.size())
                line += screenCodeToAscii(static_cast<uint8_t>(screenRam[offset]));
        }
        // Trim trailing spaces
        while (line.endsWith(' '))
            line.chop(1);
        result += line + "\n";
    }
    return result.toUtf8();
}

QByteArray CliServer::handleScreenRaw()
{
    QByteArray screenRam = readMemorySync(0x0400, 1000);
    if (screenRam.isEmpty())
        return "Error: could not read screen RAM\n";

    QString result;
    for (int row = 0; row < 25; row++) {
        int addr = 0x0400 + row * 40;
        result += QStringLiteral("%1: ").arg(addr, 4, 16, QChar('0')).toUpper();
        for (int col = 0; col < 40; col++) {
            int offset = row * 40 + col;
            if (offset < screenRam.size())
                result += QStringLiteral("%1 ").arg(static_cast<uint8_t>(screenRam[offset]), 2, 16, QChar('0')).toUpper();
        }
        result += "\n";
    }
    return result.toUtf8();
}

void CliServer::injectKeys(const QByteArray &petsciiBytes)
{
    int pos = 0;
    while (pos < petsciiBytes.size()) {
        // Wait for keyboard buffer to be empty
        for (int tries = 0; tries < 100; tries++) {
            QByteArray counter = readMemorySync(0x00C6, 1);
            if (!counter.isEmpty() && static_cast<uint8_t>(counter[0]) == 0)
                break;
            // Small delay
            QEventLoop wait;
            QTimer::singleShot(50, &wait, &QEventLoop::quit);
            wait.exec();
        }

        // Write up to 10 bytes
        int chunk = qMin(10, petsciiBytes.size() - pos);
        QByteArray batch = petsciiBytes.mid(pos, chunk);

        writeMemoryHexSync(0x0277, batch.toHex().toUpper());

        writeMemoryHexSync(0x00C6, QStringLiteral("%1").arg(chunk, 2, 16, QChar('0')).toUpper());

        pos += chunk;
    }
}

QByteArray CliServer::handleType(const QByteArray &body)
{
    QString text = QString::fromUtf8(body);

    // Convert to PETSCII
    QByteArray petscii;
    for (const QChar &ch : text) {
        uint8_t p = 0;
        char c = ch.toLatin1();

        if (c == '\n' || c == '\r') p = 0x0D;
        else if (c >= 0x20 && c <= 0x3F) p = static_cast<uint8_t>(c);
        else if (c == '@') p = 0x40;
        else if (c >= 'A' && c <= 'Z') p = static_cast<uint8_t>(c - 'A' + 0x41);
        else if (c >= 'a' && c <= 'z') p = static_cast<uint8_t>(c - 'a' + 0x41);

        if (p != 0)
            petscii.append(static_cast<char>(p));
    }

    // Append RETURN if not already ending with one
    if (petscii.isEmpty() || static_cast<uint8_t>(petscii.back()) != 0x0D)
        petscii.append('\x0D');

    injectKeys(petscii);
    return "OK\n";
}

QByteArray CliServer::handleBasic(const QByteArray &body)
{
    QString program = QString::fromUtf8(body);
    QString error;
    auto result = BasicTokenizer::tokenize(program, &error);

    if (!error.isEmpty())
        return ("Error: " + error + "\n").toUtf8();

    if (result.programData.isEmpty())
        return "Error: empty program\n";

    auto *api = m_connection->apiClient();
    if (!api)
        return "Error: not connected\n";

    // Write program to memory at $0801 using hex writes (binary POST not supported)
    const int chunkSize = 128; // hex string will be 2x this
    for (int offset = 0; offset < result.programData.size(); offset += chunkSize) {
        int len = qMin(chunkSize, result.programData.size() - offset);
        QString hex = result.programData.mid(offset, len).toHex().toUpper();
        writeMemoryHexSync(0x0801 + offset, hex);
    }

    // Set BASIC pointers ($002D/$002F/$0031) to end of program
    QString endHex = QStringLiteral("%1%2")
        .arg(result.endAddress & 0xFF, 2, 16, QChar('0'))
        .arg(result.endAddress >> 8, 2, 16, QChar('0')).toUpper();
    writeMemoryHexSync(0x002D, endHex);
    writeMemoryHexSync(0x002F, endHex);
    writeMemoryHexSync(0x0031, endHex);

    // Inject RUN command
    QByteArray runCmd;
    runCmd.append('\x52'); runCmd.append('\x55');
    runCmd.append('\x4E'); runCmd.append('\x0D');
    injectKeys(runCmd);

    return QStringLiteral("OK: %1 bytes\n").arg(result.programData.size()).toUtf8();
}

QByteArray CliServer::handlePeek(const QString &addrStr, const QString &lenStr)
{
    bool ok;
    uint16_t addr = addrStr.toUShort(&ok, 16);
    if (!ok) return "Error: invalid address\n";

    int len = lenStr.isEmpty() ? 256 : lenStr.toInt();
    len = qBound(1, len, 4096);

    QByteArray data = readMemorySync(addr, len);
    if (data.isEmpty())
        return "Error: could not read memory\n";

    QString result;
    for (int row = 0; row < data.size(); row += 16) {
        int a = addr + row;
        QString hex = QStringLiteral("%1: ").arg(a, 4, 16, QChar('0')).toUpper();
        QString ascii;
        for (int col = 0; col < 16 && row + col < data.size(); col++) {
            uint8_t b = static_cast<uint8_t>(data[row + col]);
            hex += QStringLiteral("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
            ascii += (b >= 0x20 && b <= 0x7E) ? QChar(b) : QChar('.');
            if (col == 7) hex += ' ';
        }
        result += hex + " |" + ascii + "|\n";
    }
    return result.toUtf8();
}

QByteArray CliServer::handlePoke(const QString &addrStr, const QByteArray &body)
{
    bool ok;
    uint16_t addr = addrStr.toUShort(&ok, 16);
    if (!ok) return "Error: invalid address\n";

    // Body is hex string
    QString hexStr = QString::fromUtf8(body).trimmed();
    writeMemoryHexSync(addr, hexStr);
    return "OK\n";
}

QByteArray CliServer::handleReset()
{
    auto *api = m_connection->apiClient();
    if (!api) return "Error: not connected\n";
    api->machineReset();
    return "OK\n";
}
