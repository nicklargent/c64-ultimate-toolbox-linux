#include "audio/AudioPlayer.h"
#include "app/Log.h"

#include <QMediaDevices>
#include <cstring>
#include <algorithm>
#include <cmath>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject(parent)
{
    m_format.setSampleRate(48000);
    m_format.setChannelCount(2);
    m_format.setSampleFormat(QAudioFormat::Int16);
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

void AudioPlayer::start()
{
    stop();

    auto device = QMediaDevices::defaultAudioOutput();
    m_sink = new QAudioSink(device, m_format, this);
    m_sink->setBufferSize(48000 * 4 / 5); // ~200ms buffer
    m_ioDevice = m_sink->start();

    if (!m_ioDevice) {
        qCWarning(logAudio) << "Failed to start audio sink";
        delete m_sink;
        m_sink = nullptr;
        return;
    }

    m_playing = true;
    qCInfo(logAudio) << "Audio player started";
}

void AudioPlayer::stop()
{
    m_playing = false;
    if (m_sink) {
        m_sink->stop();
        delete m_sink;
        m_sink = nullptr;
    }
    m_ioDevice = nullptr;
    qCInfo(logAudio) << "Audio player stopped";
}

void AudioPlayer::appendData(const QByteArray &pcmData)
{
    if (!m_playing || !m_ioDevice)
        return;

    emit audioDataForRecording(pcmData);

    QByteArray adjusted = pcmData;
    applyVolumeBalance(adjusted);
    m_ioDevice->write(adjusted);
}

void AudioPlayer::setVolume(float volume)
{
    m_volume = std::clamp(volume, 0.0f, 1.0f);
}

void AudioPlayer::setBalance(float balance)
{
    m_balance = std::clamp(balance, -1.0f, 1.0f);
}

void AudioPlayer::setMuted(bool muted)
{
    m_muted = muted;
}

void AudioPlayer::applyVolumeBalance(QByteArray &data)
{
    if (m_muted) {
        data.fill('\0');
        return;
    }

    float leftAmp = m_volume * std::min(1.0f, 1.0f - m_balance);
    float rightAmp = m_volume * std::min(1.0f, 1.0f + m_balance);

    auto *samples = reinterpret_cast<int16_t *>(data.data());
    int sampleCount = data.size() / 2;
    sampleCount &= ~1; // ensure even (stereo pairs)

    for (int i = 0; i < sampleCount; i += 2) {
        samples[i]     = static_cast<int16_t>(std::clamp(samples[i] * leftAmp, -32768.0f, 32767.0f));
        samples[i + 1] = static_cast<int16_t>(std::clamp(samples[i + 1] * rightAmp, -32768.0f, 32767.0f));
    }
}
