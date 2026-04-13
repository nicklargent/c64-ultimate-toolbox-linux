#pragma once

#include <QString>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>

enum class ConnectionMode {
    Viewer,
    Toolbox
};

struct ViewerSession {
    uint16_t videoPort = 11000;
    uint16_t audioPort = 11001;
    QDateTime date;

    QString id() const { return QStringLiteral("%1-%2").arg(videoPort).arg(audioPort); }

    QJsonObject toJson() const {
        return {
            {"videoPort", videoPort},
            {"audioPort", audioPort},
            {"date", date.toString(Qt::ISODate)}
        };
    }

    static ViewerSession fromJson(const QJsonObject &obj) {
        ViewerSession s;
        s.videoPort = static_cast<uint16_t>(obj["videoPort"].toInt(11000));
        s.audioPort = static_cast<uint16_t>(obj["audioPort"].toInt(11001));
        s.date = QDateTime::fromString(obj["date"].toString(), Qt::ISODate);
        return s;
    }
};

struct ToolboxSession {
    QString ipAddress;
    bool savePassword = false;
    QString password;
    QDateTime date;

    QString id() const { return ipAddress; }

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["ipAddress"] = ipAddress;
        obj["savePassword"] = savePassword;
        if (savePassword)
            obj["password"] = password;
        obj["date"] = date.toString(Qt::ISODate);
        return obj;
    }

    static ToolboxSession fromJson(const QJsonObject &obj) {
        ToolboxSession s;
        s.ipAddress = obj["ipAddress"].toString();
        s.savePassword = obj["savePassword"].toBool();
        if (s.savePassword)
            s.password = obj["password"].toString();
        s.date = QDateTime::fromString(obj["date"].toString(), Qt::ISODate);
        return s;
    }
};

class RecentConnections : public QObject {
    Q_OBJECT
public:
    explicit RecentConnections(QObject *parent = nullptr);

    QList<ViewerSession> viewerSessions() const { return m_viewerSessions; }
    QList<ToolboxSession> toolboxSessions() const { return m_toolboxSessions; }

    void addViewer(uint16_t videoPort, uint16_t audioPort);
    void addToolbox(const QString &ip, const QString &password, bool savePassword);
    void removeViewer(const QString &id);
    void removeToolbox(const QString &id);

signals:
    void changed();

private:
    void load();
    void save();

    static constexpr int MaxRecent = 5;
    QList<ViewerSession> m_viewerSessions;
    QList<ToolboxSession> m_toolboxSessions;
};
