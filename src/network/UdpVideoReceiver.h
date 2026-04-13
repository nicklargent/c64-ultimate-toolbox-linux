#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <atomic>

class FrameAssembler;

class UdpVideoReceiver : public QObject {
    Q_OBJECT
public:
    explicit UdpVideoReceiver(FrameAssembler *assembler, QObject *parent = nullptr);
    ~UdpVideoReceiver();

    void start(uint16_t port);
    void stop();

    uint64_t packetsReceived() const { return m_packetsReceived.load(); }

private slots:
    void onReadyRead();

private:
    FrameAssembler *m_assembler;
    QUdpSocket *m_socket = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    std::atomic<uint64_t> m_packetsReceived{0};
};
