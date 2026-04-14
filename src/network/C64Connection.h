#pragma once

#include <QObject>
#include <QTimer>

#include "models/ConnectionMode.h"
#include "models/DeviceInfo.h"

class C64ApiClient;
class FtpClient;
class UdpVideoReceiver;
class UdpAudioReceiver;
class FrameAssembler;
class AudioPlayer;
class KeyboardForwarder;
class CliServer;

class C64Connection : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(ConnectionMode mode READ mode NOTIFY connectionChanged)
public:
    enum class MachineAction { Reset, Reboot, PowerOff, MenuButton, Pause, Resume };
    Q_ENUM(MachineAction)

    enum class RunnerType { Sid, Mod, Prg, Crt };

    explicit C64Connection(QObject *parent = nullptr);
    ~C64Connection();

    // Connection
    void connectToolbox(const QString &ip, const QString &password = {},
                        bool savePassword = false);
    void startListening(uint16_t videoPort = 11000, uint16_t audioPort = 11001);
    void disconnect();

    bool isConnected() const { return m_connected; }
    ConnectionMode mode() const { return m_mode; }
    DeviceInfo deviceInfo() const { return m_deviceInfo; }

    // Component access
    C64ApiClient *apiClient() const { return m_apiClient; }
    FtpClient *ftpClient() const { return m_ftpClient; }
    FrameAssembler *frameAssembler() const { return m_frameAssembler; }
    AudioPlayer *audioPlayer() const { return m_audioPlayer; }
    KeyboardForwarder *keyboardForwarder() const { return m_keyboardForwarder; }
    RecentConnections *recentConnections() const { return m_recentConnections; }

    // Stream control
    void startStreams();
    void stopStreams();
    bool streamsActive() const { return m_streamsActive; }

    // Machine control
    void machineAction(MachineAction action);

    // File execution
    void runFile(RunnerType type, const QByteArray &data);

    // State
    bool isPaused() const { return m_isPaused; }
    int framesPerSecond() const { return m_fps; }
    uint16_t videoPort() const { return m_videoPort; }
    uint16_t audioPort() const { return m_audioPort; }
    QString deviceIp() const { return m_pendingIp; }

signals:
    void connectionChanged();
    void connectionError(const QString &error);
    void fpsUpdated(int fps);
    void streamsStateChanged(bool active);

private slots:
    void onDeviceInfoReceived(const DeviceInfo &info);
    void onAuthRequired();
    void onApiRequestFailed(const QString &op, const QString &error);
    void updateFps();

private:
    void startReceivers();
    void stopReceivers();
    void waitForDeviceAndRestartStreams();
    QString getLocalIp() const;
    void preventSleep();
    void allowSleep();

    // Components
    C64ApiClient *m_apiClient = nullptr;
    FtpClient *m_ftpClient = nullptr;
    FrameAssembler *m_frameAssembler = nullptr;
    UdpVideoReceiver *m_videoReceiver = nullptr;
    UdpAudioReceiver *m_audioReceiver = nullptr;
    AudioPlayer *m_audioPlayer = nullptr;
    KeyboardForwarder *m_keyboardForwarder = nullptr;
    RecentConnections *m_recentConnections = nullptr;

    // State
    bool m_connected = false;
    ConnectionMode m_mode = ConnectionMode::Viewer;
    DeviceInfo m_deviceInfo;
    bool m_streamsActive = false;
    bool m_isPaused = false;
    bool m_isWaitingForReboot = false;
    uint16_t m_videoPort = 11000;
    uint16_t m_audioPort = 11001;

    // FPS tracking
    QTimer *m_fpsTimer = nullptr;
    int m_frameCount = 0;
    int m_fps = 0;

    // CLI server
    CliServer *m_cliServer = nullptr;

    // Sleep inhibit
    uint32_t m_sleepCookie = 0;

    // Pending connection info
    QString m_pendingIp;
    QString m_pendingPassword;
    bool m_pendingSavePassword = false;
};
