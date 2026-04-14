#pragma once

#include <QObject>
#include <QTcpSocket>

class MenuController : public QObject {
    Q_OBJECT
public:
    explicit MenuController(const QString &host, QObject *parent = nullptr);
    ~MenuController();

    bool isConnected() const { return m_connected; }
    bool isMenuOpen() const { return m_menuOpen; }

    void openMenu();
    void closeMenu();
    void sendKey(int qtKey);

signals:
    void menuOpened();
    void menuClosed();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();

private:
    void connectToDevice();
    void sendBytes(const QByteArray &data);

    QString m_host;
    QTcpSocket *m_socket = nullptr;
    bool m_connected = false;
    bool m_menuOpen = false;
    QList<int> m_pendingKeys;
};
