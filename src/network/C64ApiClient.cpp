#include "network/C64ApiClient.h"
#include "app/Log.h"

#include <QNetworkReply>
#include <QJsonDocument>
#include <QUrl>
#include <QUrlQuery>

C64ApiClient::C64ApiClient(const QString &host, const QString &password, QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_host(host)
    , m_password(password)
{
    m_baseUrl = QStringLiteral("http://%1").arg(host);
}

void C64ApiClient::setHost(const QString &host)
{
    m_host = host;
    m_baseUrl = QStringLiteral("http://%1").arg(host);
}

void C64ApiClient::setPassword(const QString &password)
{
    m_password = password;
}

void C64ApiClient::addAuth(QNetworkRequest &request)
{
    if (!m_password.isEmpty())
        request.setRawHeader("X-Password", m_password.toUtf8());
}

QString C64ApiClient::encodeQueryValue(const QString &value)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(value, QByteArray(), "/:&=+"));
}

QString C64ApiClient::encodePathComponent(const QString &value)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(value, "/"));
}

QNetworkReply *C64ApiClient::sendGet(const QUrl &url)
{
    QNetworkRequest request(url);
    addAuth(request);
    return m_nam->get(request);
}

QNetworkReply *C64ApiClient::sendPut(const QUrl &url, const QByteArray &body)
{
    QNetworkRequest request(url);
    addAuth(request);
    return m_nam->put(request, body);
}

QNetworkReply *C64ApiClient::sendPost(const QUrl &url, const QByteArray &body)
{
    QNetworkRequest request(url);
    addAuth(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    return m_nam->post(request, body);
}

// --- Device Info ---

void C64ApiClient::fetchDeviceInfo()
{
    auto *reply = sendGet(QUrl(m_baseUrl + "/v1/info"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 403) {
            emit authenticationRequired();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed("fetchDeviceInfo", reply->errorString());
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        emit deviceInfoReceived(DeviceInfo::fromJson(doc.object()));
    });
}

// --- Stream Control ---

void C64ApiClient::startStream(const QString &name, const QString &clientIp, int port)
{
    QString url = QStringLiteral("%1/v1/streams/%2:start?ip=%3:%4")
        .arg(m_baseUrl, name, clientIp).arg(port);
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, name]() {
        reply->deleteLater();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            emit streamStarted(name);
        } else {
            emit requestFailed("startStream", reply->errorString());
        }
    });
}

void C64ApiClient::stopStream(const QString &name)
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/streams/" + name + ":stop"));
    connect(reply, &QNetworkReply::finished, this, [this, reply, name]() {
        reply->deleteLater();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 200 && status < 300) {
            emit streamStopped(name);
        } else {
            emit requestFailed("stopStream", reply->errorString());
        }
    });
}

// --- File Runners (binary upload) ---

void C64ApiClient::runSidFile(const QByteArray &data)
{
    auto *reply = sendPost(QUrl(m_baseUrl + "/v1/runners:sidplay"), data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("runSid");
        else
            emit requestFailed("runSid", reply->errorString());
    });
}

void C64ApiClient::runModFile(const QByteArray &data)
{
    auto *reply = sendPost(QUrl(m_baseUrl + "/v1/runners:modplay"), data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("runMod");
        else
            emit requestFailed("runMod", reply->errorString());
    });
}

void C64ApiClient::runPrgFile(const QByteArray &data)
{
    auto *reply = sendPost(QUrl(m_baseUrl + "/v1/runners:run_prg"), data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("runPrg");
        else
            emit requestFailed("runPrg", reply->errorString());
    });
}

void C64ApiClient::runCrtFile(const QByteArray &data)
{
    auto *reply = sendPost(QUrl(m_baseUrl + "/v1/runners:run_crt"), data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("runCrt");
        else
            emit requestFailed("runCrt", reply->errorString());
    });
}

// --- File Runners (by path) ---

void C64ApiClient::runPrgFromPath(const QString &path)
{
    QString url = QStringLiteral("%1/v1/runners:run_prg?file=%2")
        .arg(m_baseUrl, encodeQueryValue(path));
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("runPrgFromPath");
        else
            emit requestFailed("runPrgFromPath", reply->errorString());
    });
}

void C64ApiClient::loadPrgFromPath(const QString &path)
{
    QString url = QStringLiteral("%1/v1/runners:load_prg?file=%2")
        .arg(m_baseUrl, encodeQueryValue(path));
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("loadPrgFromPath");
        else
            emit requestFailed("loadPrgFromPath", reply->errorString());
    });
}

void C64ApiClient::playSidFromPath(const QString &path)
{
    QString url = QStringLiteral("%1/v1/runners:sidplay?file=%2")
        .arg(m_baseUrl, encodeQueryValue(path));
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("playSidFromPath");
        else
            emit requestFailed("playSidFromPath", reply->errorString());
    });
}

void C64ApiClient::playModFromPath(const QString &path)
{
    QString url = QStringLiteral("%1/v1/runners:modplay?file=%2")
        .arg(m_baseUrl, encodeQueryValue(path));
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("playModFromPath");
        else
            emit requestFailed("playModFromPath", reply->errorString());
    });
}

void C64ApiClient::runCrtFromPath(const QString &path)
{
    QString url = QStringLiteral("%1/v1/runners:run_crt?file=%2")
        .arg(m_baseUrl, encodeQueryValue(path));
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("runCrtFromPath");
        else
            emit requestFailed("runCrtFromPath", reply->errorString());
    });
}

// --- Drives ---

void C64ApiClient::fetchDrives()
{
    auto *reply = sendGet(QUrl(m_baseUrl + "/v1/drives"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed("fetchDrives", reply->errorString());
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        emit drivesReceived(doc.object()["drives"].toArray());
    });
}

void C64ApiClient::mountDrive(const QString &drive, const QString &imagePath)
{
    QString url = QStringLiteral("%1/v1/drives/%2:mount?image=%3")
        .arg(m_baseUrl, drive, encodeQueryValue(imagePath));
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("mountDrive");
        else
            emit requestFailed("mountDrive", reply->errorString());
    });
}

void C64ApiClient::removeDrive(const QString &drive)
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/drives/" + drive + ":remove"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("removeDrive");
        else
            emit requestFailed("removeDrive", reply->errorString());
    });
}

void C64ApiClient::resetDrive(const QString &drive)
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/drives/" + drive + ":reset"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("resetDrive");
        else
            emit requestFailed("resetDrive", reply->errorString());
    });
}

// --- Disk Image Creation ---

void C64ApiClient::createD64(const QString &path, int tracks)
{
    QString url = QStringLiteral("%1/v1/files/%2:create_d64?tracks=%3")
        .arg(m_baseUrl, encodePathComponent(path)).arg(tracks);
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("createD64");
        else
            emit requestFailed("createD64", reply->errorString());
    });
}

void C64ApiClient::createD71(const QString &path)
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/files/" + encodePathComponent(path) + ":create_d71"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("createD71");
        else
            emit requestFailed("createD71", reply->errorString());
    });
}

void C64ApiClient::createD81(const QString &path)
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/files/" + encodePathComponent(path) + ":create_d81"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("createD81");
        else
            emit requestFailed("createD81", reply->errorString());
    });
}

void C64ApiClient::createDnp(const QString &path, int tracks)
{
    QString url = QStringLiteral("%1/v1/files/%2:create_dnp?tracks=%3")
        .arg(m_baseUrl, encodePathComponent(path)).arg(tracks);
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("createDnp");
        else
            emit requestFailed("createDnp", reply->errorString());
    });
}

// --- Configuration ---

void C64ApiClient::fetchConfigCategories()
{
    auto *reply = sendGet(QUrl(m_baseUrl + "/v1/configs"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed("fetchConfigCategories", reply->errorString());
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        QStringList categories;
        for (const auto &v : doc.object()["categories"].toArray())
            categories.append(v.toString());
        emit configCategoriesReceived(categories);
    });
}

void C64ApiClient::fetchConfig(const QString &category)
{
    QString url = m_baseUrl + "/v1/configs/" + encodePathComponent(category);
    auto *reply = sendGet(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, category]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed("fetchConfig", reply->errorString());
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        emit configReceived(category, doc.object());
    });
}

void C64ApiClient::fetchConfigDetail(const QString &category, const QString &item)
{
    QString url = m_baseUrl + "/v1/configs/" + encodePathComponent(category)
                  + "/" + encodePathComponent(item);
    auto *reply = sendGet(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, category, item]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed("fetchConfigDetail", reply->errorString());
            return;
        }
        auto doc = QJsonDocument::fromJson(reply->readAll());
        emit configDetailReceived(category, item, doc.object());
    });
}

void C64ApiClient::setConfig(const QString &category, const QString &item, const QString &value)
{
    QString url = QStringLiteral("%1/v1/configs/%2/%3?value=%4")
        .arg(m_baseUrl, encodePathComponent(category), encodePathComponent(item), encodeQueryValue(value));
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("setConfig");
        else
            emit requestFailed("setConfig", reply->errorString());
    });
}

void C64ApiClient::saveConfigToFlash()
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/configs:save_to_flash"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("saveConfigToFlash");
        else
            emit requestFailed("saveConfigToFlash", reply->errorString());
    });
}

void C64ApiClient::loadConfigFromFlash()
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/configs:load_from_flash"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("loadConfigFromFlash");
        else
            emit requestFailed("loadConfigFromFlash", reply->errorString());
    });
}

void C64ApiClient::resetConfigToDefault()
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/configs:reset_to_default"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        // 502 is treated as success (device may reboot)
        if (reply->error() == QNetworkReply::NoError || status == 502)
            emit requestSucceeded("resetConfigToDefault");
        else
            emit requestFailed("resetConfigToDefault", reply->errorString());
    });
}

// --- Machine Control ---

void C64ApiClient::machineReset()
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/machine:reset"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("machineReset");
        else
            emit requestFailed("machineReset", reply->errorString());
    });
}

void C64ApiClient::machineReboot()
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/machine:reboot"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        // May timeout — treat as success
        emit requestSucceeded("machineReboot");
    });
}

void C64ApiClient::machinePowerOff()
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/machine:poweroff"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        emit requestSucceeded("machinePowerOff");
    });
}

void C64ApiClient::machineMenuButton()
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/machine:menu_button"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("machineMenuButton");
        else
            emit requestFailed("machineMenuButton", reply->errorString());
    });
}

void C64ApiClient::machinePause()
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/machine:pause"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("machinePause");
        else
            emit requestFailed("machinePause", reply->errorString());
    });
}

void C64ApiClient::machineResume()
{
    auto *reply = sendPut(QUrl(m_baseUrl + "/v1/machine:resume"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("machineResume");
        else
            emit requestFailed("machineResume", reply->errorString());
    });
}

// --- Memory Access ---

void C64ApiClient::readMemory(uint16_t address, int length)
{
    QString url = QStringLiteral("%1/v1/machine:readmem?address=%2&length=%3")
        .arg(m_baseUrl, QStringLiteral("%1").arg(address, 4, 16, QChar('0')).toUpper()).arg(length);
    auto *reply = sendGet(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, address]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed("readMemory", reply->errorString());
            return;
        }
        emit memoryDataReceived(address, reply->readAll());
    });
}

void C64ApiClient::writeMemory(uint16_t address, const QByteArray &data)
{
    QString url = QStringLiteral("%1/v1/machine:writemem?address=%2")
        .arg(m_baseUrl, QStringLiteral("%1").arg(address, 4, 16, QChar('0')).toUpper());
    auto *reply = sendPost(QUrl(url), data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("writeMemory");
        else
            emit requestFailed("writeMemory", reply->errorString());
    });
}

void C64ApiClient::writeMemoryHex(uint16_t address, const QString &hexData)
{
    QString url = QStringLiteral("%1/v1/machine:writemem?address=%2&data=%3")
        .arg(m_baseUrl, QStringLiteral("%1").arg(address, 4, 16, QChar('0')).toUpper(), hexData);
    auto *reply = sendPut(QUrl(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError)
            emit requestSucceeded("writeMemoryHex");
        else
            emit requestFailed("writeMemoryHex", reply->errorString());
    });
}
