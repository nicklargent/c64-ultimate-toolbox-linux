#pragma once

#include <QObject>
#include <QAudioSink>
#include <QAudioFormat>
#include <QIODevice>
#include <QMutex>

class AudioPlayer : public QObject {
    Q_OBJECT
public:
    explicit AudioPlayer(QObject *parent = nullptr);
    ~AudioPlayer();

    void start();
    void stop();
    void appendData(const QByteArray &pcmData);

    void setVolume(float volume);
    void setBalance(float balance);
    void setMuted(bool muted);

    float volume() const { return m_volume; }
    float balance() const { return m_balance; }
    bool isMuted() const { return m_muted; }

signals:
    void audioDataForRecording(const QByteArray &pcmData);

private:
    void applyVolumeBalance(QByteArray &data);

    QAudioFormat m_format;
    QAudioSink *m_sink = nullptr;
    QIODevice *m_ioDevice = nullptr;

    float m_volume = 0.2f;
    float m_balance = 0.0f;
    bool m_muted = false;
    bool m_playing = false;
};
