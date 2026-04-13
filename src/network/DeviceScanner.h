#pragma once

#include <QObject>
#include <QNetworkAccessManager>

#include "models/DeviceInfo.h"

struct DiscoveredDevice {
    QString ipAddress;
    DeviceInfo info;
    bool hasInfo = false;
    bool requiresPassword = false;
    bool ftpAvailable = false;
};

class DeviceScanner : public QObject {
    Q_OBJECT
public:
    explicit DeviceScanner(QObject *parent = nullptr);

    void scan();
    void stop();

signals:
    void deviceFound(const DiscoveredDevice &device);
    void scanComplete();
    void scanProgress(int checked, int total);

private:
    void checkHost(const QString &ip);
    void checkFtp(const QString &ip, DiscoveredDevice device);
    static QString getLocalSubnet();

    QNetworkAccessManager *m_nam;
    int m_pending = 0;
    bool m_scanning = false;
};
