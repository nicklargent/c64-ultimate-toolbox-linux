#include "models/ConnectionMode.h"

#include <QJsonDocument>

RecentConnections::RecentConnections(QObject *parent)
    : QObject(parent)
{
    load();
}

void RecentConnections::addViewer(uint16_t videoPort, uint16_t audioPort)
{
    ViewerSession s;
    s.videoPort = videoPort;
    s.audioPort = audioPort;
    s.date = QDateTime::currentDateTime();

    QString id = s.id();
    m_viewerSessions.removeIf([&](const ViewerSession &v) { return v.id() == id; });
    m_viewerSessions.prepend(s);
    while (m_viewerSessions.size() > MaxRecent)
        m_viewerSessions.removeLast();
    save();
    emit changed();
}

void RecentConnections::addToolbox(const QString &ip, const QString &password, bool savePassword)
{
    ToolboxSession s;
    s.ipAddress = ip;
    s.savePassword = savePassword;
    s.password = savePassword ? password : QString();
    s.date = QDateTime::currentDateTime();

    m_toolboxSessions.removeIf([&](const ToolboxSession &t) { return t.id() == ip; });
    m_toolboxSessions.prepend(s);
    while (m_toolboxSessions.size() > MaxRecent)
        m_toolboxSessions.removeLast();
    save();
    emit changed();
}

void RecentConnections::removeViewer(const QString &id)
{
    m_viewerSessions.removeIf([&](const ViewerSession &v) { return v.id() == id; });
    save();
    emit changed();
}

void RecentConnections::removeToolbox(const QString &id)
{
    m_toolboxSessions.removeIf([&](const ToolboxSession &t) { return t.id() == id; });
    save();
    emit changed();
}

void RecentConnections::load()
{
    QSettings settings;

    auto viewerData = settings.value("c64_recent_viewer").toByteArray();
    if (!viewerData.isEmpty()) {
        auto arr = QJsonDocument::fromJson(viewerData).array();
        for (const auto &v : arr)
            m_viewerSessions.append(ViewerSession::fromJson(v.toObject()));
    }

    auto toolboxData = settings.value("c64_recent_toolbox").toByteArray();
    if (!toolboxData.isEmpty()) {
        auto arr = QJsonDocument::fromJson(toolboxData).array();
        for (const auto &v : arr)
            m_toolboxSessions.append(ToolboxSession::fromJson(v.toObject()));
    }
}

void RecentConnections::save()
{
    QSettings settings;

    QJsonArray viewerArr;
    for (const auto &s : m_viewerSessions)
        viewerArr.append(s.toJson());
    settings.setValue("c64_recent_viewer", QJsonDocument(viewerArr).toJson(QJsonDocument::Compact));

    QJsonArray toolboxArr;
    for (const auto &s : m_toolboxSessions)
        toolboxArr.append(s.toJson());
    settings.setValue("c64_recent_toolbox", QJsonDocument(toolboxArr).toJson(QJsonDocument::Compact));
}
