#include "network/C64Connection.h"
#include "network/C64ApiClient.h"
#include "network/FtpClient.h"
#include "network/UdpVideoReceiver.h"
#include "network/UdpAudioReceiver.h"
#include "network/KeyboardForwarder.h"
#include "network/CliServer.h"
#include "video/FrameAssembler.h"
#include "audio/AudioPlayer.h"
#include "app/Log.h"

#include <QNetworkInterface>
#include <QProcess>
#include <QTimer>

C64Connection::C64Connection(QObject *parent)
    : QObject(parent)
    , m_frameAssembler(new FrameAssembler(this))
    , m_videoReceiver(new UdpVideoReceiver(m_frameAssembler, this))
    , m_audioReceiver(new UdpAudioReceiver(this))
    , m_audioPlayer(new AudioPlayer(this))
    , m_recentConnections(new RecentConnections(this))
{
    // Wire audio receiver to player
    connect(m_audioReceiver, &UdpAudioReceiver::audioDataReceived,
            this, [this](const QByteArray &pcmData, uint16_t) {
        m_audioPlayer->appendData(pcmData);
    });

    // Wire frame assembler for FPS counting
    connect(m_frameAssembler, &FrameAssembler::frameReady,
            this, [this](const QByteArray &, int, int) {
        m_frameCount++;
    });

    // FPS timer
    m_fpsTimer = new QTimer(this);
    connect(m_fpsTimer, &QTimer::timeout, this, &C64Connection::updateFps);
}

C64Connection::~C64Connection()
{
    disconnect();
}

void C64Connection::connectToolbox(const QString &ip, const QString &password,
                                    bool savePassword)
{
    disconnect();

    m_mode = ConnectionMode::Toolbox;
    m_pendingIp = ip;
    m_pendingPassword = password;
    m_pendingSavePassword = savePassword;

    m_apiClient = new C64ApiClient(ip, password, this);
    m_keyboardForwarder = new KeyboardForwarder(m_apiClient, this);

    connect(m_apiClient, &C64ApiClient::deviceInfoReceived,
            this, &C64Connection::onDeviceInfoReceived);
    connect(m_apiClient, &C64ApiClient::authenticationRequired,
            this, &C64Connection::onAuthRequired);
    connect(m_apiClient, &C64ApiClient::requestFailed,
            this, &C64Connection::onApiRequestFailed);

    // Fetch device info to verify connection
    m_apiClient->fetchDeviceInfo();
}

void C64Connection::startListening(uint16_t videoPort, uint16_t audioPort)
{
    disconnect();

    m_mode = ConnectionMode::Viewer;
    m_videoPort = videoPort;
    m_audioPort = audioPort;
    m_connected = true;

    startReceivers();
    m_audioPlayer->start();
    m_fpsTimer->start(1000);

    m_recentConnections->addViewer(videoPort, audioPort);

    emit connectionChanged();
    qCInfo(logApp) << "Listening for streams on video:" << videoPort << "audio:" << audioPort;
}

void C64Connection::disconnect()
{
    if (m_cliServer) {
        m_cliServer->stop();
        delete m_cliServer;
        m_cliServer = nullptr;
    }

    stopReceivers();
    m_audioPlayer->stop();
    m_fpsTimer->stop();
    allowSleep();

    if (m_keyboardForwarder) {
        m_keyboardForwarder->setEnabled(false);
        delete m_keyboardForwarder;
        m_keyboardForwarder = nullptr;
    }

    if (m_ftpClient) {
        m_ftpClient->disconnect();
        delete m_ftpClient;
        m_ftpClient = nullptr;
    }

    if (m_apiClient) {
        delete m_apiClient;
        m_apiClient = nullptr;
    }

    m_connected = false;
    m_streamsActive = false;
    m_isPaused = false;
    m_isWaitingForReboot = false;
    m_deviceInfo = {};
    m_frameCount = 0;
    m_fps = 0;

    emit connectionChanged();
}

void C64Connection::onDeviceInfoReceived(const DeviceInfo &info)
{
    m_deviceInfo = info;

    if (!m_connected) {
        // First connection
        m_connected = true;

        // Create FTP client
        m_ftpClient = new FtpClient(m_pendingIp, 21, m_pendingPassword, this);
        m_ftpClient->connectToServer();

        // Start receivers and streams
        startReceivers();
        m_audioPlayer->start();
        m_fpsTimer->start(1000);
        startStreams();

        m_recentConnections->addToolbox(m_pendingIp, m_pendingPassword, m_pendingSavePassword);
        emit connectionChanged();

        // Start CLI server for c64ctl
        m_cliServer = new CliServer(this, this);
        m_cliServer->start();

        qCInfo(logApp) << "Connected to" << info.product << "at" << m_pendingIp;
    } else if (m_isWaitingForReboot) {
        // Device came back after reboot
        m_isWaitingForReboot = false;
        startStreams();
        qCInfo(logApp) << "Device returned after reboot";
    }
}

void C64Connection::onAuthRequired()
{
    emit connectionError("Incorrect password");
}

void C64Connection::onApiRequestFailed(const QString &op, const QString &error)
{
    if (!m_connected && op == "fetchDeviceInfo") {
        emit connectionError(QStringLiteral("Cannot connect: %1").arg(error));
    } else {
        qCWarning(logNetwork) << "API request failed:" << op << error;
    }
}

void C64Connection::startReceivers()
{
    m_videoReceiver->start(m_videoPort);
    m_audioReceiver->start(m_audioPort);
}

void C64Connection::stopReceivers()
{
    m_videoReceiver->stop();
    m_audioReceiver->stop();
}

void C64Connection::startStreams()
{
    if (!m_apiClient || m_mode != ConnectionMode::Toolbox)
        return;

    QString localIp = getLocalIp();
    if (localIp.isEmpty()) {
        emit connectionError("Unable to start streams. Could not determine local IP.");
        return;
    }

    // Connect error handler to report stream start failures
    auto errConn = std::make_shared<QMetaObject::Connection>();
    *errConn = connect(m_apiClient, &C64ApiClient::requestFailed, this,
        [this, errConn](const QString &op, const QString &error) {
            if (op != "startStream") return;
            QObject::disconnect(*errConn);

            qCWarning(logNetwork) << "Stream start failed:" << error;
            emit connectionError(
                "Unable to start streams. Make sure Data Streams are enabled "
                "in your Ultimate device settings and that Ethernet is connected.");
        });

    m_apiClient->startStream("video", localIp, m_videoPort);
    m_apiClient->startStream("audio", localIp, m_audioPort);
    m_streamsActive = true;
    preventSleep();
    emit streamsStateChanged(true);
}

void C64Connection::stopStreams()
{
    if (!m_apiClient)
        return;

    m_apiClient->stopStream("video");
    m_apiClient->stopStream("audio");
    m_streamsActive = false;
    allowSleep();
    emit streamsStateChanged(false);
}

void C64Connection::machineAction(MachineAction action)
{
    if (!m_apiClient)
        return;

    switch (action) {
    case MachineAction::Reset:
        m_apiClient->machineReset();
        break;
    case MachineAction::Reboot:
        m_apiClient->machineReboot();
        waitForDeviceAndRestartStreams();
        break;
    case MachineAction::PowerOff:
        m_apiClient->machinePowerOff();
        disconnect();
        break;
    case MachineAction::MenuButton:
        m_apiClient->machineMenuButton();
        break;
    case MachineAction::Pause:
        m_apiClient->machinePause();
        m_isPaused = true;
        break;
    case MachineAction::Resume:
        m_apiClient->machineResume();
        m_isPaused = false;
        break;
    }
}

void C64Connection::runFile(RunnerType type, const QByteArray &data)
{
    if (!m_apiClient)
        return;

    switch (type) {
    case RunnerType::Sid: m_apiClient->runSidFile(data); break;
    case RunnerType::Mod: m_apiClient->runModFile(data); break;
    case RunnerType::Prg: m_apiClient->runPrgFile(data); break;
    case RunnerType::Crt: m_apiClient->runCrtFile(data); break;
    }
}

void C64Connection::waitForDeviceAndRestartStreams()
{
    m_isWaitingForReboot = true;
    stopStreams();

    // Wait 3 seconds for device to go down, then poll
    QTimer::singleShot(3000, this, [this]() {
        if (!m_connected || !m_isWaitingForReboot)
            return;

        auto *pollTimer = new QTimer(this);
        auto attempts = std::make_shared<int>(0);

        connect(pollTimer, &QTimer::timeout, this, [this, pollTimer, attempts]() {
            if (!m_connected || !m_isWaitingForReboot || !m_apiClient || *attempts >= 30) {
                pollTimer->stop();
                pollTimer->deleteLater();
                m_isWaitingForReboot = false;
                return;
            }
            (*attempts)++;
            m_apiClient->fetchDeviceInfo(); // onDeviceInfoReceived handles the rest
        });

        pollTimer->start(2000);
    });
}

QString C64Connection::getLocalIp() const
{
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : interfaces) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning))
            continue;

        for (const auto &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                return entry.ip().toString();
        }
    }
    return {};
}

void C64Connection::updateFps()
{
    m_fps = m_frameCount;
    m_frameCount = 0;
    emit fpsUpdated(m_fps);
}

void C64Connection::preventSleep()
{
    // Use D-Bus to inhibit screen blanking on Linux
    QProcess::startDetached("xdg-screensaver", {"suspend", QString::number(0)});
    // Note: proper implementation would use org.freedesktop.ScreenSaver D-Bus interface
    // This is a placeholder — will be refined in Polish phase
}

void C64Connection::allowSleep()
{
    QProcess::startDetached("xdg-screensaver", {"resume", QString::number(0)});
}
