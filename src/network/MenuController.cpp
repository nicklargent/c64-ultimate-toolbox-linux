#include "network/MenuController.h"
#include "app/Log.h"

#include <Qt>
#include <QTimer>

MenuController::MenuController(const QString &host, QObject *parent)
    : QObject(parent)
    , m_host(host)
{
}

MenuController::~MenuController()
{
    if (m_socket) {
        m_socket->disconnectFromHost();
        delete m_socket;
    }
}

void MenuController::connectToDevice()
{
    if (m_socket)
        return;

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &MenuController::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &MenuController::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &MenuController::onReadyRead);
    m_socket->connectToHost(m_host, 23);
}

void MenuController::onConnected()
{
    m_connected = true;
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1); // TCP_NODELAY
    qCInfo(logNetwork) << "Telnet menu connected";

    // Wait for initial telnet negotiation data to arrive and be drained,
    // then flush pending keys
    QTimer::singleShot(500, this, [this]() {
        // Drain any remaining data
        if (m_socket)
            m_socket->readAll();

        for (int key : m_pendingKeys)
            sendKey(key);
        m_pendingKeys.clear();

        emit menuOpened();
    });
}

void MenuController::onDisconnected()
{
    m_connected = false;
    m_menuOpen = false;
    m_socket->deleteLater();
    m_socket = nullptr;
    qCInfo(logNetwork) << "Telnet menu disconnected";
    emit menuClosed();
}

void MenuController::onReadyRead()
{
    // Consume all data — don't respond to telnet negotiation.
    // The device works fine without responses (verified with raw Python socket).
    m_socket->readAll();
}

void MenuController::openMenu()
{
    if (m_menuOpen)
        return;
    m_menuOpen = true;
    m_pendingKeys.clear();
    connectToDevice();
}

void MenuController::closeMenu()
{
    if (!m_menuOpen || !m_socket)
        return;

    // Send Escape to close the menu, then disconnect
    sendBytes(QByteArray(1, 0x1B));

    // Give the device a moment to process, then disconnect
    QTimer::singleShot(200, this, [this]() {
        if (m_socket) {
            m_socket->disconnectFromHost();
        }
        m_menuOpen = false;
        emit menuClosed();
    });
}

void MenuController::sendKey(int qtKey)
{
    if (!m_connected || !m_socket) {
        if (m_menuOpen)
            m_pendingKeys.append(qtKey);
        return;
    }

    switch (qtKey) {
    case Qt::Key_Up:     sendBytes("\x1b[A"); break;
    case Qt::Key_Down:   sendBytes("\x1b[B"); break;
    case Qt::Key_Right:  sendBytes("\x1b[C"); break;
    case Qt::Key_Left:   sendBytes("\x1b[D"); break;
    case Qt::Key_Return:
    case Qt::Key_Enter:  sendBytes("\r"); break;
    case Qt::Key_Escape: closeMenu(); break;
    case Qt::Key_Home:   sendBytes("\x1b[H"); break;
    case Qt::Key_End:    sendBytes("\x1b[F"); break;
    case Qt::Key_F1:     sendBytes("\x1b[11~"); break;
    case Qt::Key_F2:     sendBytes("\x1b[12~"); break;
    case Qt::Key_F3:     sendBytes("\x1b[13~"); break;
    case Qt::Key_F7:     sendBytes("\x1b[18~"); break;  // F7=HELP on Ultimate
    default: {
        // Send printable characters directly
        QByteArray ch;
        ch.append(static_cast<char>(qtKey & 0xFF));
        if (qtKey >= 0x20 && qtKey <= 0x7E)
            sendBytes(ch);
        break;
    }
    }
}

void MenuController::sendBytes(const QByteArray &data)
{
    if (m_socket && m_connected) {
        qint64 written = m_socket->write(data);
        m_socket->flush();
        m_socket->waitForBytesWritten(100);
        Q_UNUSED(written);
    }
}
