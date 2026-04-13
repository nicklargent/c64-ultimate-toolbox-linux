#include "network/UdpAudioReceiver.h"
#include "app/Log.h"

#include <QVariant>
#include <QtEndian>

UdpAudioReceiver::UdpAudioReceiver(QObject *parent)
    : QObject(parent)
{
}

UdpAudioReceiver::~UdpAudioReceiver()
{
    stop();
}

void UdpAudioReceiver::start(uint16_t port)
{
    stop();

    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::AnyIPv4, port, QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint)) {
        qCWarning(logAudio) << "Failed to bind audio UDP port" << port << m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        return;
    }

    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, QVariant(512 * 1024));

    connect(m_socket, &QUdpSocket::readyRead, this, &UdpAudioReceiver::onReadyRead);

    m_packetsReceived = 0;
    qCInfo(logAudio) << "Audio receiver started on port" << port;
}

void UdpAudioReceiver::stop()
{
    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }
    qCInfo(logAudio) << "Audio receiver stopped";
}

void UdpAudioReceiver::onReadyRead()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());
        m_socket->readDatagram(datagram.data(), datagram.size());

        if (datagram.size() != AudioPacketSize)
            continue;

        m_packetsReceived++;

        uint16_t seqNum;
        memcpy(&seqNum, datagram.constData(), 2);
        seqNum = qFromLittleEndian(seqNum);

        emit audioDataReceived(datagram.mid(AudioHeaderSize), seqNum);
    }
}
