#include "settings/PresetManager.h"
#include "app/Log.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

PresetManager::PresetManager(QObject *parent)
    : QObject(parent)
    , m_builtIn(CrtPreset::builtInPresets())
{
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(500);
    connect(m_saveTimer, &QTimer::timeout, this, &PresetManager::save);

    load();
}

int PresetManager::totalCount() const
{
    return m_builtIn.size() + m_custom.size();
}

bool PresetManager::isCustomSelected() const
{
    return m_selectedIndex >= m_builtIn.size();
}

void PresetManager::setSelectedIndex(int index)
{
    if (index >= 0 && index < totalCount()) {
        m_selectedIndex = index;
        scheduleSave();
    }
}

CrtSettings PresetManager::settingsForIndex(int index) const
{
    if (index < m_builtIn.size()) {
        auto it = m_overrides.find(m_builtIn[index].id);
        return (it != m_overrides.end()) ? it.value() : m_builtIn[index].settings;
    }
    int customIdx = index - m_builtIn.size();
    if (customIdx >= 0 && customIdx < m_custom.size())
        return m_custom[customIdx].settings;
    return {};
}

QString PresetManager::displayNameForIndex(int index) const
{
    if (index < m_builtIn.size()) {
        QString name = m_builtIn[index].name;
        if (isBuiltInModified(index))
            name += " *";
        return name;
    }
    int customIdx = index - m_builtIn.size();
    if (customIdx >= 0 && customIdx < m_custom.size())
        return m_custom[customIdx].name;
    return "Unknown";
}

bool PresetManager::isBuiltInModified(int builtInIndex) const
{
    if (builtInIndex < 0 || builtInIndex >= m_builtIn.size())
        return false;
    return m_overrides.contains(m_builtIn[builtInIndex].id);
}

QList<PresetManager::PresetEntry> PresetManager::allEntries() const
{
    QList<PresetEntry> entries;
    for (int i = 0; i < m_builtIn.size(); i++) {
        bool mod = isBuiltInModified(i);
        entries.append({mod ? m_builtIn[i].name + " *" : m_builtIn[i].name, false, mod});
    }
    for (const auto &c : m_custom)
        entries.append({c.name, true, false});
    return entries;
}

void PresetManager::saveOverride(int builtInIndex, const CrtSettings &settings)
{
    if (builtInIndex < 0 || builtInIndex >= m_builtIn.size())
        return;
    m_overrides[m_builtIn[builtInIndex].id] = settings;
    scheduleSave();
    emit presetsChanged();
}

void PresetManager::resetBuiltIn(int builtInIndex)
{
    if (builtInIndex < 0 || builtInIndex >= m_builtIn.size())
        return;
    m_overrides.remove(m_builtIn[builtInIndex].id);
    scheduleSave();
    emit presetsChanged();
}

QUuid PresetManager::saveAsCustom(const QString &name, const CrtSettings &settings)
{
    CustomPreset p;
    p.id = QUuid::createUuid();
    p.name = name;
    p.settings = settings;
    m_custom.append(p);
    scheduleSave();
    emit presetsChanged();
    return p.id;
}

void PresetManager::deleteCustom(const QUuid &id)
{
    m_custom.removeIf([&](const CustomPreset &p) { return p.id == id; });

    if (isCustomSelected() && m_selectedIndex >= totalCount())
        m_selectedIndex = 0;

    scheduleSave();
    emit presetsChanged();
}

void PresetManager::renameCustom(const QUuid &id, const QString &name)
{
    for (auto &p : m_custom) {
        if (p.id == id) {
            p.name = name;
            scheduleSave();
            emit presetsChanged();
            return;
        }
    }
}

QString PresetManager::presetFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/c64-ultimate-toolbox";
    QDir().mkpath(dir);
    return dir + "/presets.json";
}

void PresetManager::scheduleSave()
{
    m_saveTimer->start();
}

void PresetManager::save()
{
    QJsonObject root;

    // Custom presets
    QJsonArray customArr;
    for (const auto &p : m_custom) {
        QJsonObject obj;
        obj["id"] = p.id.toString();
        obj["name"] = p.name;
        obj["settings"] = p.settings.toJson();
        customArr.append(obj);
    }
    root["customPresets"] = customArr;

    // Built-in overrides
    QJsonObject overrides;
    for (auto it = m_overrides.begin(); it != m_overrides.end(); ++it)
        overrides[it.key()] = it.value().toJson();
    root["builtInOverrides"] = overrides;

    // Selected
    root["selectedIndex"] = m_selectedIndex;

    QFile file(presetFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        qCDebug(logApp) << "Presets saved";
    }
}

void PresetManager::load()
{
    QFile file(presetFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    auto doc = QJsonDocument::fromJson(file.readAll());
    auto root = doc.object();

    // Custom presets
    m_custom.clear();
    for (const auto &v : root["customPresets"].toArray()) {
        auto obj = v.toObject();
        CustomPreset p;
        p.id = QUuid(obj["id"].toString());
        p.name = obj["name"].toString();
        p.settings = CrtSettings::fromJson(obj["settings"].toObject());
        m_custom.append(p);
    }

    // Built-in overrides
    m_overrides.clear();
    auto overrides = root["builtInOverrides"].toObject();
    for (auto it = overrides.begin(); it != overrides.end(); ++it)
        m_overrides[it.key()] = CrtSettings::fromJson(it.value().toObject());

    // Selected
    m_selectedIndex = root["selectedIndex"].toInt(0);
    if (m_selectedIndex >= totalCount())
        m_selectedIndex = 0;

    qCInfo(logApp) << "Loaded" << m_custom.size() << "custom presets," << m_overrides.size() << "overrides";
}
