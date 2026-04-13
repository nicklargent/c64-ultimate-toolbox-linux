#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>

class C64Connection;
class DeviceScanner;
struct DiscoveredDevice;

class ConnectDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConnectDialog(C64Connection *connection, QWidget *parent = nullptr);
    ~ConnectDialog();

signals:
    void connectedToolbox();
    void connectedViewer();

private slots:
    void onTabChanged(int index);
    void onConnectClicked();
    void onListenClicked();
    void onDeviceFound(const DiscoveredDevice &device);
    void onScanComplete();
    void onDiscoveredDeviceClicked(QListWidgetItem *item);
    void onRecentViewerClicked(QListWidgetItem *item);

private:
    void buildToolboxTab();
    void buildViewerTab();
    void startScanning();
    void stopScanning();
    void refreshRecentViewers();
    void attemptConnection(const QString &ip, const QString &password, bool savePassword);
    QString promptPassword(const QString &ip);

    C64Connection *m_connection;
    DeviceScanner *m_scanner;
    QTimer *m_scanTimer;

    QTabWidget *m_tabs;

    // Toolbox tab
    QWidget *m_toolboxTab;
    QListWidget *m_deviceList;
    QLabel *m_scanLabel;
    QLineEdit *m_ipEdit;
    QLineEdit *m_passwordEdit;
    QPushButton *m_connectBtn;
    QLabel *m_errorLabel;
    bool m_firstScanDone = false;

    // Viewer tab
    QWidget *m_viewerTab;
    QListWidget *m_recentList;
    QSpinBox *m_videoPortSpin;
    QSpinBox *m_audioPortSpin;
    QPushButton *m_listenBtn;
};
