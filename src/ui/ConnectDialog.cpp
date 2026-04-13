#include "ui/ConnectDialog.h"
#include "network/C64Connection.h"
#include "network/DeviceScanner.h"
#include "app/Log.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QMessageBox>
#include <QNetworkInterface>

ConnectDialog::ConnectDialog(C64Connection *connection, QWidget *parent)
    : QDialog(parent)
    , m_connection(connection)
    , m_scanner(new DeviceScanner(this))
    , m_scanTimer(new QTimer(this))
{
    setWindowTitle("Open Device");
    setFixedSize(420, 380);

    auto *mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    mainLayout->addWidget(m_tabs);

    buildToolboxTab();
    buildViewerTab();

    m_tabs->addTab(m_toolboxTab, "Toolbox");
    m_tabs->addTab(m_viewerTab, "Viewer");

    connect(m_tabs, &QTabWidget::currentChanged, this, &ConnectDialog::onTabChanged);
    connect(m_scanner, &DeviceScanner::deviceFound, this, &ConnectDialog::onDeviceFound);
    connect(m_scanner, &DeviceScanner::scanComplete, this, &ConnectDialog::onScanComplete);

    // Auto-scan every 5 seconds
    connect(m_scanTimer, &QTimer::timeout, this, &ConnectDialog::startScanning);

    // Pre-fill IP from recent
    auto recent = m_connection->recentConnections()->toolboxSessions();
    if (!recent.isEmpty())
        m_ipEdit->setText(recent.first().ipAddress);

    // Start scanning immediately
    startScanning();
    m_scanTimer->start(5000);
}

ConnectDialog::~ConnectDialog()
{
    stopScanning();
}

void ConnectDialog::buildToolboxTab()
{
    m_toolboxTab = new QWidget(this);
    auto *layout = new QVBoxLayout(m_toolboxTab);
    layout->setSpacing(8);

    // Discovered devices section
    auto *headerLayout = new QHBoxLayout;
    auto *headerLabel = new QLabel("Discovered Devices:", m_toolboxTab);
    headerLabel->setStyleSheet("font-weight: 600; font-size: 11pt;");
    headerLayout->addWidget(headerLabel);

    m_scanLabel = new QLabel("Scanning...", m_toolboxTab);
    m_scanLabel->setStyleSheet("color: gray; font-size: 10pt;");
    headerLayout->addWidget(m_scanLabel);
    headerLayout->addStretch();
    layout->addLayout(headerLayout);

    m_deviceList = new QListWidget(m_toolboxTab);
    m_deviceList->setMaximumHeight(120);
    m_deviceList->setAlternatingRowColors(true);
    connect(m_deviceList, &QListWidget::itemDoubleClicked,
            this, &ConnectDialog::onDiscoveredDeviceClicked);
    layout->addWidget(m_deviceList);

    layout->addSpacing(8);

    // Manual connect section
    auto *manualLabel = new QLabel("Manual Connect:", m_toolboxTab);
    manualLabel->setStyleSheet("font-weight: 600; font-size: 11pt;");
    layout->addWidget(manualLabel);

    auto *ipRow = new QHBoxLayout;
    m_ipEdit = new QLineEdit(m_toolboxTab);
    m_ipEdit->setPlaceholderText("192.168.1.24");
    ipRow->addWidget(m_ipEdit);

    m_connectBtn = new QPushButton("Connect", m_toolboxTab);
    m_connectBtn->setDefault(true);
    connect(m_connectBtn, &QPushButton::clicked, this, &ConnectDialog::onConnectClicked);
    ipRow->addWidget(m_connectBtn);
    layout->addLayout(ipRow);

    // Password field
    auto *passRow = new QHBoxLayout;
    auto *passLabel = new QLabel("Password:", m_toolboxTab);
    passRow->addWidget(passLabel);
    m_passwordEdit = new QLineEdit(m_toolboxTab);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("(optional)");
    passRow->addWidget(m_passwordEdit);
    layout->addLayout(passRow);

    m_errorLabel = new QLabel(m_toolboxTab);
    m_errorLabel->setStyleSheet("color: red; font-size: 10pt;");
    m_errorLabel->hide();
    layout->addWidget(m_errorLabel);

    // Connect on Enter in IP field
    connect(m_ipEdit, &QLineEdit::returnPressed, this, &ConnectDialog::onConnectClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &ConnectDialog::onConnectClicked);

    layout->addStretch();

    // Help text
    auto *helpLabel = new QLabel(m_toolboxTab);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("color: gray; font-size: 10pt;");
    helpLabel->setText("Make sure FTP and Web Remote Control are enabled in "
                       "your Ultimate device settings.");
    layout->addWidget(helpLabel);
}

void ConnectDialog::buildViewerTab()
{
    m_viewerTab = new QWidget(this);
    auto *layout = new QVBoxLayout(m_viewerTab);
    layout->setSpacing(8);

    // Recent sessions
    auto *recentLabel = new QLabel("Recent Sessions:", m_viewerTab);
    recentLabel->setStyleSheet("font-weight: 600; font-size: 11pt;");
    layout->addWidget(recentLabel);

    m_recentList = new QListWidget(m_viewerTab);
    m_recentList->setMaximumHeight(100);
    connect(m_recentList, &QListWidget::itemDoubleClicked,
            this, &ConnectDialog::onRecentViewerClicked);
    layout->addWidget(m_recentList);

    refreshRecentViewers();

    layout->addSpacing(8);

    // Manual listen
    auto *listenLabel = new QLabel("Manual Listen:", m_viewerTab);
    listenLabel->setStyleSheet("font-weight: 600; font-size: 11pt;");
    layout->addWidget(listenLabel);

    auto *portRow = new QHBoxLayout;
    portRow->setSpacing(6);

    portRow->addWidget(new QLabel("Video Port:", m_viewerTab));
    m_videoPortSpin = new QSpinBox(m_viewerTab);
    m_videoPortSpin->setRange(1024, 65535);
    m_videoPortSpin->setValue(11000);
    portRow->addWidget(m_videoPortSpin);

    portRow->addWidget(new QLabel("Audio Port:", m_viewerTab));
    m_audioPortSpin = new QSpinBox(m_viewerTab);
    m_audioPortSpin->setRange(1024, 65535);
    m_audioPortSpin->setValue(11001);
    portRow->addWidget(m_audioPortSpin);

    m_listenBtn = new QPushButton("Listen", m_viewerTab);
    connect(m_listenBtn, &QPushButton::clicked, this, &ConnectDialog::onListenClicked);
    portRow->addWidget(m_listenBtn);

    layout->addLayout(portRow);

    layout->addStretch();

    // Help text with local IP
    auto *helpLabel = new QLabel(m_viewerTab);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("color: gray; font-size: 10pt;");

    QString localIp = "unknown";
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack) ||
            !iface.flags().testFlag(QNetworkInterface::IsUp))
            continue;
        for (const auto &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                localIp = entry.ip().toString();
                goto found;
            }
        }
    }
    found:
    helpLabel->setText(QStringLiteral(
        "Your IP: %1\n"
        "Enable data streams on your Ultimate device and enter this IP "
        "in the Ultimate Menu \u2192 C64 and Cartridge Settings \u2192 Data streams.").arg(localIp));
    layout->addWidget(helpLabel);
}

void ConnectDialog::onTabChanged(int index)
{
    if (index == 0) {
        // Toolbox tab — start scanning
        startScanning();
        m_scanTimer->start(5000);
    } else {
        // Viewer tab — stop scanning
        stopScanning();
        m_scanTimer->stop();
        refreshRecentViewers();
    }
}

void ConnectDialog::startScanning()
{
    m_scanLabel->setText("Scanning...");
    m_scanLabel->show();
    m_scanner->scan();
}

void ConnectDialog::stopScanning()
{
    m_scanner->stop();
    m_scanTimer->stop();
}

void ConnectDialog::onDeviceFound(const DiscoveredDevice &device)
{
    // Check for duplicate
    for (int i = 0; i < m_deviceList->count(); i++) {
        if (m_deviceList->item(i)->data(Qt::UserRole).toString() == device.ipAddress)
            return;
    }

    QString text;
    if (device.hasInfo) {
        text = QStringLiteral("%1 \u2014 %2 (%3)")
            .arg(device.info.product, device.info.hostname, device.ipAddress);
    } else {
        text = device.ipAddress;
    }

    if (device.requiresPassword)
        text += QString::fromUtf8(" \xf0\x9f\x94\x92"); // 🔒
    if (!device.ftpAvailable)
        text += QString::fromUtf8(" \xe2\x9a\xa0\xef\xb8\x8f FTP disabled"); // ⚠️

    auto *item = new QListWidgetItem(text, m_deviceList);
    item->setData(Qt::UserRole, device.ipAddress);
    item->setData(Qt::UserRole + 1, device.requiresPassword);

    if (!device.ftpAvailable) {
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        item->setToolTip("FTP is not available on this device. Enable it in the device settings.");
    }
}

void ConnectDialog::onScanComplete()
{
    m_firstScanDone = true;
    if (m_deviceList->count() == 0) {
        m_scanLabel->setText("No devices found");
    } else {
        m_scanLabel->setText(QStringLiteral("%1 found").arg(m_deviceList->count()));
    }
}

void ConnectDialog::onDiscoveredDeviceClicked(QListWidgetItem *item)
{
    QString ip = item->data(Qt::UserRole).toString();
    bool needsPassword = item->data(Qt::UserRole + 1).toBool();

    m_ipEdit->setText(ip);

    if (needsPassword) {
        QString pass = promptPassword(ip);
        if (pass.isEmpty())
            return;
        m_passwordEdit->setText(pass);
    }

    onConnectClicked();
}

void ConnectDialog::onRecentViewerClicked(QListWidgetItem *item)
{
    int videoPort = item->data(Qt::UserRole).toInt();
    int audioPort = item->data(Qt::UserRole + 1).toInt();

    m_videoPortSpin->setValue(videoPort);
    m_audioPortSpin->setValue(audioPort);
    onListenClicked();
}

void ConnectDialog::onConnectClicked()
{
    QString ip = m_ipEdit->text().trimmed();
    if (ip.isEmpty()) {
        m_errorLabel->setText("Please enter an IP address.");
        m_errorLabel->show();
        return;
    }

    m_errorLabel->hide();
    m_connectBtn->setEnabled(false);
    m_connectBtn->setText("Connecting...");

    QString password = m_passwordEdit->text();
    bool savePassword = !password.isEmpty(); // save if provided

    attemptConnection(ip, password, savePassword);
}

void ConnectDialog::attemptConnection(const QString &ip, const QString &password, bool savePassword)
{
    // Connect signals for this attempt
    auto connOk = std::make_shared<QMetaObject::Connection>();
    auto connErr = std::make_shared<QMetaObject::Connection>();

    *connOk = connect(m_connection, &C64Connection::connectionChanged, this,
        [this, connOk, connErr]() {
            QObject::disconnect(*connOk);
            QObject::disconnect(*connErr);

            if (m_connection->isConnected()) {
                stopScanning();
                emit connectedToolbox();
                accept();
            }
        });

    *connErr = connect(m_connection, &C64Connection::connectionError, this,
        [this, connOk, connErr](const QString &error) {
            QObject::disconnect(*connOk);
            QObject::disconnect(*connErr);

            m_errorLabel->setText(error);
            m_errorLabel->show();
            m_connectBtn->setEnabled(true);
            m_connectBtn->setText("Connect");

            // If auth required, prompt for password
            if (error.contains("password", Qt::CaseInsensitive)) {
                QString ip = m_ipEdit->text().trimmed();
                QString pass = promptPassword(ip);
                if (!pass.isEmpty()) {
                    m_passwordEdit->setText(pass);
                    attemptConnection(ip, pass, true);
                }
            }
        });

    m_connection->connectToolbox(ip, password, savePassword);
}

QString ConnectDialog::promptPassword(const QString &ip)
{
    bool ok;
    QString pass = QInputDialog::getText(this, "Password Required",
        QStringLiteral("The device at %1 requires a password.").arg(ip),
        QLineEdit::Password, {}, &ok);
    return ok ? pass : QString();
}

void ConnectDialog::onListenClicked()
{
    uint16_t videoPort = static_cast<uint16_t>(m_videoPortSpin->value());
    uint16_t audioPort = static_cast<uint16_t>(m_audioPortSpin->value());

    m_connection->startListening(videoPort, audioPort);

    stopScanning();
    emit connectedViewer();
    accept();
}

void ConnectDialog::refreshRecentViewers()
{
    m_recentList->clear();
    auto sessions = m_connection->recentConnections()->viewerSessions();

    for (const auto &s : sessions) {
        QString text = QStringLiteral("Video Port: %1 \u00b7 Audio Port: %2")
            .arg(s.videoPort).arg(s.audioPort);
        auto *item = new QListWidgetItem(text, m_recentList);
        item->setData(Qt::UserRole, s.videoPort);
        item->setData(Qt::UserRole + 1, s.audioPort);
    }

    if (sessions.isEmpty()) {
        m_recentList->addItem("No recent sessions");
        m_recentList->item(0)->setFlags(Qt::NoItemFlags);
    }
}
