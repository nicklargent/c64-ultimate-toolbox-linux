#pragma once

#include <QString>
#include <QJsonObject>

struct DeviceInfo {
    QString product;
    QString firmwareVersion;
    QString fpgaVersion;
    QString coreVersion;
    QString hostname;
    QString uniqueId;

    static DeviceInfo fromJson(const QJsonObject &obj) {
        DeviceInfo info;
        info.product = obj["product"].toString();
        info.firmwareVersion = obj["firmware_version"].toString();
        info.fpgaVersion = obj["fpga_version"].toString();
        info.coreVersion = obj["core_version"].toString();
        info.hostname = obj["hostname"].toString();
        info.uniqueId = obj["unique_id"].toString();
        return info;
    }
};
