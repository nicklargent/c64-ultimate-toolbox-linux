#include "network/UdpVideoReceiver.h"
#include "video/FrameAssembler.h"
#include "app/Log.h"

#include <QVariant>

UdpVideoReceiver::UdpVideoReceiver(FrameAssembler *assembler, QObject *parent)
    : QObject(parent)
    , m_assembler(assembler)
{
}

UdpVideoReceiver::~UdpVideoReceiver()
{
    stop();
}

void UdpVideoReceiver::start(uint16_t port)
{
    stop();

    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::AnyIPv4, port, QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint)) {
        qCWarning(logVideo) << "Failed to bind video UDP port" << port << m_socket->errorString();
        delete m_socket;
        m_socket = nullptr;
        return;
    }

    // Increase receive buffer for real-time video
    m_socket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, QVariant(1024 * 1024));

    connect(m_socket, &QUdpSocket::readyRead, this, &UdpVideoReceiver::onReadyRead);

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setTimerType(Qt::PreciseTimer);
    connect(m_timeoutTimer, &QTimer::timeout, m_assembler, &FrameAssembler::checkTimeout);
    m_timeoutTimer->start(100);

    m_packetsReceived = 0;
    qCInfo(logVideo) << "Video receiver started on port" << port;
}

void UdpVideoReceiver::stop()
{
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
        delete m_timeoutTimer;
        m_timeoutTimer = nullptr;
    }
    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }
    qCInfo(logVideo) << "Video receiver stopped";
}

void UdpVideoReceiver::onReadyRead()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket->pendingDatagramSize());
        m_socket->readDatagram(datagram.data(), datagram.size());
        m_packetsReceived++;
        m_assembler->processPacket(datagram);
    }
}
