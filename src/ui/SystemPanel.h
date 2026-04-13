#pragma once

#include <QWidget>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>

class C64Connection;

class SystemPanel : public QWidget {
    Q_OBJECT
public:
    explicit SystemPanel(C64Connection *connection, QWidget *parent = nullptr);

private slots:
    void updateStreamStatus();
    void refreshDriveStatus();
    void loadAllConfigs();
    void onConfigCategoriesReceived(const QStringList &categories);
    void onConfigReceived(const QString &category, const QJsonObject &config);
    void onConfigItemClicked(QTreeWidgetItem *item, int column);
    void onSaveToFlash();

private:
    void addInfoRow(QVBoxLayout *layout, const QString &label, const QString &value);

    C64Connection *m_connection;

    // Stream status
    QLabel *m_streamStatusLabel;
    QLabel *m_streamFpsLabel;
    QTimer *m_streamTimer;

    // Drives
    QLabel *m_driveALabel;
    QLabel *m_driveBLabel;

    // Config
    QTreeWidget *m_configTree;
    QLineEdit *m_searchField;
    QLabel *m_configStatus;
};
