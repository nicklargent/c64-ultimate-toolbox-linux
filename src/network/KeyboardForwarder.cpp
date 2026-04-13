#include "network/KeyboardForwarder.h"
#include "network/C64ApiClient.h"
#include "app/Log.h"

#include <Qt>

KeyboardForwarder::KeyboardForwarder(C64ApiClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
}

void KeyboardForwarder::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;

    if (enabled) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &KeyboardForwarder::pollAndInject);
        m_pollTimer->start(50);
    } else {
        if (m_pollTimer) {
            m_pollTimer->stop();
            delete m_pollTimer;
            m_pollTimer = nullptr;
        }
        m_keyQueue.clear();
    }

    emit enabledChanged(enabled);
}

void KeyboardForwarder::sendKey(uint8_t petscii)
{
    m_keyQueue.enqueue(petscii);
}

void KeyboardForwarder::handleCharacter(QChar ch)
{
    uint8_t p = charToPetscii(ch);
    if (p != 0)
        sendKey(p);
}

void KeyboardForwarder::handleSpecialKey(int qtKey)
{
    uint8_t p = qtKeyToPetscii(qtKey);
    if (p != 0)
        sendKey(p);
}

uint8_t KeyboardForwarder::charToPetscii(QChar ch)
{
    char c = ch.toLatin1();

    // Return/newline
    if (c == '\r' || c == '\n')
        return 0x0D;

    // Space, digits, basic punctuation (0x20-0x3F)
    if (c >= 0x20 && c <= 0x3F)
        return static_cast<uint8_t>(c);

    // @ [ ] pass-through
    if (c == '@') return 0x40;
    if (c == '[') return 0x5B;
    if (c == ']') return 0x5D;

    // Uppercase A-Z → PETSCII 0xC1-0xDA
    if (c >= 'A' && c <= 'Z')
        return static_cast<uint8_t>(c - 'A' + 0xC1);

    // Lowercase a-z → PETSCII 0x41-0x5A
    if (c >= 'a' && c <= 'z')
        return static_cast<uint8_t>(c - 'a' + 0x41);

    return 0;
}

uint8_t KeyboardForwarder::qtKeyToPetscii(int key)
{
    switch (key) {
    case Qt::Key_Return:    return 0x0D;
    case Qt::Key_Backspace: return 0x14;  // DEL
    case Qt::Key_Escape:    return 0x03;  // RUN/STOP
    case Qt::Key_Home:      return 0x13;
    case Qt::Key_Up:        return 0x91;
    case Qt::Key_Down:      return 0x11;
    case Qt::Key_Left:      return 0x9D;
    case Qt::Key_Right:     return 0x1D;
    case Qt::Key_F1:        return 0x85;
    case Qt::Key_F2:        return 0x89;
    case Qt::Key_F3:        return 0x86;
    case Qt::Key_F4:        return 0x8A;
    case Qt::Key_F5:        return 0x87;
    case Qt::Key_F6:        return 0x8B;
    case Qt::Key_F7:        return 0x88;
    case Qt::Key_F8:        return 0x8C;
    case Qt::Key_Insert:    return 0x94;
    case Qt::Key_Delete:    return 0x14;  // DEL
    default:                return 0;
    }
}

void KeyboardForwarder::pollAndInject()
{
    if (!m_enabled || m_keyQueue.isEmpty() || m_sending)
        return;

    m_sending = true;

    // Read keyboard buffer counter at $00C6
    m_client->readMemory(0x00C6, 1);

    // We need to chain: read → check → write key → write counter
    // Use a one-shot connection for the response
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_client, &C64ApiClient::memoryDataReceived, this,
        [this, conn](uint16_t address, const QByteArray &data) {
            if (address != 0x00C6)
                return;

            QObject::disconnect(*conn);

            if (data.isEmpty()) {
                m_sending = false;
                return;
            }

            int count = static_cast<uint8_t>(data[0]);
            if (count >= 10 || m_keyQueue.isEmpty()) {
                m_sending = false;
                return;
            }

            uint8_t petscii = m_keyQueue.dequeue();
            uint16_t writeAddr = 0x0277 + count;

            // Write the key byte
            m_client->writeMemoryHex(writeAddr, QStringLiteral("%1").arg(petscii, 2, 16, QChar('0')).toUpper());

            // Increment counter
            auto conn2 = std::make_shared<QMetaObject::Connection>();
            *conn2 = connect(m_client, &C64ApiClient::requestSucceeded, this,
                [this, conn2, count](const QString &op) {
                    if (op != "writeMemoryHex")
                        return;
                    QObject::disconnect(*conn2);

                    m_client->writeMemoryHex(0x00C6, QStringLiteral("%1").arg(count + 1, 2, 16, QChar('0')).toUpper());

                    auto conn3 = std::make_shared<QMetaObject::Connection>();
                    *conn3 = connect(m_client, &C64ApiClient::requestSucceeded, this,
                        [this, conn3](const QString &) {
                            QObject::disconnect(*conn3);
                            m_sending = false;
                        });
                });
        });

    // Also handle failure
    auto errConn = std::make_shared<QMetaObject::Connection>();
    *errConn = connect(m_client, &C64ApiClient::requestFailed, this,
        [this, errConn](const QString &, const QString &) {
            QObject::disconnect(*errConn);
            m_sending = false;
        });
}
