#pragma once

#include <QObject>
#include <QList>
#include <QMap>
#include <QTimer>
#include <QUuid>

#include "models/CrtSettings.h"
#include "models/CrtPreset.h"

struct CustomPreset {
    QUuid id;
    QString name;
    CrtSettings settings;
};

class PresetManager : public QObject {
    Q_OBJECT
public:
    explicit PresetManager(QObject *parent = nullptr);

    // Current selection
    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);
    bool isCustomSelected() const;

    // Query
    CrtSettings settingsForIndex(int index) const;
    QString displayNameForIndex(int index) const;
    int totalCount() const;
    bool isBuiltInModified(int builtInIndex) const;

    // Modification
    void saveOverride(int builtInIndex, const CrtSettings &settings);
    void resetBuiltIn(int builtInIndex);
    QUuid saveAsCustom(const QString &name, const CrtSettings &settings);
    void deleteCustom(const QUuid &id);
    void renameCustom(const QUuid &id, const QString &name);

    // All entries for UI
    struct PresetEntry {
        QString name;
        bool isCustom;
        bool isModified;
    };
    QList<PresetEntry> allEntries() const;

signals:
    void presetsChanged();

private:
    void load();
    void scheduleSave();
    void save();

    static QString presetFilePath();

    QList<CrtPreset> m_builtIn;
    QMap<QString, CrtSettings> m_overrides;  // builtIn id -> overridden settings
    QList<CustomPreset> m_custom;
    int m_selectedIndex = 0;
    QTimer *m_saveTimer;
};
