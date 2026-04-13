#pragma once

#include <QObject>
#include <QUdpSocket>
#include <atomic>

class UdpAudioReceiver : public QObject {
    Q_OBJECT
public:
    static constexpr int AudioPacketSize = 770;
    static constexpr int AudioHeaderSize = 2;
    static constexpr int SamplesPerPacket = 192;

    explicit UdpAudioReceiver(QObject *parent = nullptr);
    ~UdpAudioReceiver();

    void start(uint16_t port);
    void stop();

    uint64_t packetsReceived() const { return m_packetsReceived.load(); }

signals:
    void audioDataReceived(const QByteArray &pcmData, uint16_t sequenceNum);

private slots:
    void onReadyRead();

private:
    QUdpSocket *m_socket = nullptr;
    std::atomic<uint64_t> m_packetsReceived{0};
};
