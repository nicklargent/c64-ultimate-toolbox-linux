#pragma once

#include <QObject>
#include <QTimer>
#include <QQueue>

class C64ApiClient;

class KeyboardForwarder : public QObject {
    Q_OBJECT
public:
    explicit KeyboardForwarder(C64ApiClient *client, QObject *parent = nullptr);

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    void sendKey(uint8_t petscii);
    void handleCharacter(QChar ch);
    void handleSpecialKey(int qtKey);

    static uint8_t charToPetscii(QChar ch);
    static uint8_t qtKeyToPetscii(int key);

signals:
    void enabledChanged(bool enabled);

private slots:
    void pollAndInject();

private:
    C64ApiClient *m_client;
    QTimer *m_pollTimer = nullptr;
    QQueue<uint8_t> m_keyQueue;
    bool m_enabled = false;
    bool m_sending = false;
};
