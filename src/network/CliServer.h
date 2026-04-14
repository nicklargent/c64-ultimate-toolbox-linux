#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

class C64Connection;

class CliServer : public QObject {
    Q_OBJECT
public:
    explicit CliServer(C64Connection *connection, QObject *parent = nullptr);
    ~CliServer();

    bool start();
    void stop();
    uint16_t port() const;

private slots:
    void onNewConnection();

private:
    void handleRequest(QTcpSocket *socket);
    void sendResponse(QTcpSocket *socket, int status, const QString &contentType, const QByteArray &body);

    // Handlers
    QByteArray handleInfo();
    QByteArray handleScreen();
    QByteArray handleScreenRaw();
    QByteArray handleType(const QByteArray &body);
    QByteArray handleBasic(const QByteArray &body);
    QByteArray handlePeek(const QString &addr, const QString &len);
    QByteArray handlePoke(const QString &addr, const QByteArray &body);
    QByteArray handleReset();

    // Helpers
    static QString screenCodeToAscii(uint8_t code);
    QByteArray readMemorySync(uint16_t address, int length);
    void writeMemorySync(uint16_t address, const QByteArray &data);
    void writeMemoryHexSync(uint16_t address, const QString &hex);
    void injectKeys(const QByteArray &petsciiBytes);

    C64Connection *m_connection;
    QTcpServer *m_server = nullptr;
    static constexpr const char *PortFile = "/tmp/c64ctl.port";
};
