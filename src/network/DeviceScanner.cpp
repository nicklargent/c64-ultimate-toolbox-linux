#include "network/DeviceScanner.h"
#include "app/Log.h"

#include <QNetworkReply>
#include <QNetworkInterface>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QTimer>

DeviceScanner::DeviceScanner(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

QString DeviceScanner::getLocalSubnet()
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : interfaces) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning))
            continue;

        for (const auto &entry : iface.addressEntries()) {
            auto addr = entry.ip();
            if (addr.protocol() != QAbstractSocket::IPv4Protocol)
                continue;

            QString ip = addr.toString();
            int lastDot = ip.lastIndexOf('.');
            if (lastDot > 0) {
                qCInfo(logNetwork) << "Using subnet from interface" << iface.name() << ip;
                return ip.left(lastDot);
            }
        }
    }

    qCWarning(logNetwork) << "Could not detect subnet, using fallback 192.168.1";
    return "192.168.1";
}

void DeviceScanner::scan()
{
    if (m_scanning)
        return;

    m_scanning = true;
    m_pending = 254;

    QString subnet = getLocalSubnet();
    qCInfo(logNetwork) << "Scanning subnet" << subnet << ".1-.254";

    for (int i = 1; i <= 254; i++) {
        QString ip = QStringLiteral("%1.%2").arg(subnet).arg(i);
        checkHost(ip);
    }
}

void DeviceScanner::stop()
{
    m_scanning = false;
}

void DeviceScanner::checkHost(const QString &ip)
{
    QUrl url(QStringLiteral("http://%1/v1/info").arg(ip));
    QNetworkRequest request(url);
    request.setTransferTimeout(1500);

    auto *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, ip]() {
        reply->deleteLater();

        if (!m_scanning) {
            m_pending--;
            return;
        }

        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (status >= 200 && status < 300) {
            // Device found, no password
            auto doc = QJsonDocument::fromJson(reply->readAll());
            DiscoveredDevice dev;
            dev.ipAddress = ip;
            dev.info = DeviceInfo::fromJson(doc.object());
            dev.hasInfo = true;
            dev.requiresPassword = false;
            checkFtp(ip, dev);
        } else if (status == 403) {
            // Device found, password required
            DiscoveredDevice dev;
            dev.ipAddress = ip;
            dev.requiresPassword = true;
            checkFtp(ip, dev);
        } else {
            m_pending--;
            emit scanProgress(254 - m_pending, 254);
            if (m_pending <= 0) {
                m_scanning = false;
                emit scanComplete();
            }
        }
    });
}

void DeviceScanner::checkFtp(const QString &ip, DiscoveredDevice device)
{
    auto *socket = new QTcpSocket(this);
    socket->connectToHost(ip, 21);

    // Use a timer for timeout
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);

    connect(socket, &QTcpSocket::connected, this, [this, socket, timer, device]() mutable {
        timer->stop();
        device.ftpAvailable = true;
        socket->disconnectFromHost();
        socket->deleteLater();
        timer->deleteLater();

        emit deviceFound(device);
        m_pending--;
        emit scanProgress(254 - m_pending, 254);
        if (m_pending <= 0) {
            m_scanning = false;
            emit scanComplete();
        }
    });

    connect(socket, &QTcpSocket::errorOccurred, this, [this, socket, timer, device]() mutable {
        timer->stop();
        device.ftpAvailable = false;
        socket->deleteLater();
        timer->deleteLater();

        emit deviceFound(device);
        m_pending--;
        emit scanProgress(254 - m_pending, 254);
        if (m_pending <= 0) {
            m_scanning = false;
            emit scanComplete();
        }
    });

    connect(timer, &QTimer::timeout, this, [this, socket, timer, device]() mutable {
        device.ftpAvailable = false;
        socket->abort();
        socket->deleteLater();
        timer->deleteLater();

        emit deviceFound(device);
        m_pending--;
        emit scanProgress(254 - m_pending, 254);
        if (m_pending <= 0) {
            m_scanning = false;
            emit scanComplete();
        }
    });

    timer->start(1500);
}
