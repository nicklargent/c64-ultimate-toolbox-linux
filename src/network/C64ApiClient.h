#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonArray>

#include "models/DeviceInfo.h"

class C64ApiClient : public QObject {
    Q_OBJECT
public:
    explicit C64ApiClient(const QString &host, const QString &password = {}, QObject *parent = nullptr);

    void setHost(const QString &host);
    void setPassword(const QString &password);

    // Device info
    void fetchDeviceInfo();

    // Stream control
    void startStream(const QString &name, const QString &clientIp, int port);
    void stopStream(const QString &name);

    // File runners (upload binary)
    void runSidFile(const QByteArray &data);
    void runModFile(const QByteArray &data);
    void runPrgFile(const QByteArray &data);
    void runCrtFile(const QByteArray &data);

    // File runners (by device path)
    void runPrgFromPath(const QString &path);
    void loadPrgFromPath(const QString &path);
    void playSidFromPath(const QString &path);
    void playModFromPath(const QString &path);
    void runCrtFromPath(const QString &path);

    // Drives
    void fetchDrives();
    void mountDrive(const QString &drive, const QString &imagePath);
    void removeDrive(const QString &drive);
    void resetDrive(const QString &drive);

    // Disk image creation
    void createD64(const QString &path, int tracks = 35);
    void createD71(const QString &path);
    void createD81(const QString &path);
    void createDnp(const QString &path, int tracks);

    // Configuration
    void fetchConfigCategories();
    void fetchConfig(const QString &category);
    void fetchConfigDetail(const QString &category, const QString &item);
    void setConfig(const QString &category, const QString &item, const QString &value);
    void saveConfigToFlash();
    void loadConfigFromFlash();
    void resetConfigToDefault();

    // Machine control
    void machineReset();
    void machineReboot();
    void machinePowerOff();
    void machineMenuButton();
    void machinePause();
    void machineResume();

    // Memory access
    void readMemory(uint16_t address, int length);
    void writeMemory(uint16_t address, const QByteArray &data);
    void writeMemoryHex(uint16_t address, const QString &hexData);

signals:
    void deviceInfoReceived(const DeviceInfo &info);
    void streamStarted(const QString &name);
    void streamStopped(const QString &name);
    void drivesReceived(const QJsonArray &drives);
    void configCategoriesReceived(const QStringList &categories);
    void configReceived(const QString &category, const QJsonObject &config);
    void configDetailReceived(const QString &category, const QString &item, const QJsonObject &detail);
    void memoryDataReceived(uint16_t address, const QByteArray &data);
    void requestSucceeded(const QString &operation);
    void requestFailed(const QString &operation, const QString &error);
    void authenticationRequired();

private:
    QNetworkReply *sendGet(const QUrl &url);
    QNetworkReply *sendPut(const QUrl &url, const QByteArray &body = {});
    QNetworkReply *sendPost(const QUrl &url, const QByteArray &body);
    void addAuth(QNetworkRequest &request);

    static QString encodeQueryValue(const QString &value);
    static QString encodePathComponent(const QString &value);

    QNetworkAccessManager *m_nam;
    QString m_baseUrl;
    QString m_host;
    QString m_password;
};
