#include "recording/MediaCapture.h"
#include "video/CrtRenderer.h"
#include "audio/AudioPlayer.h"
#include "app/Log.h"

#include <QDateTime>
#include <QStandardPaths>
#include <QDir>

MediaCapture::MediaCapture(QObject *parent)
    : QObject(parent)
{
}

MediaCapture::~MediaCapture()
{
    if (m_recording)
        stopRecording();
}

QImage MediaCapture::takeScreenshot()
{
    if (!m_renderer)
        return {};

    return m_renderer->captureFrame();
}

bool MediaCapture::startRecording(int width, int height)
{
    Q_UNUSED(width)
    Q_UNUSED(height)

    // FFmpeg recording is optional — stub for now
    // A full implementation would use libavcodec/libavformat to encode
    // H.264 video + AAC audio to MP4/MKV

    qCWarning(logVideo) << "Video recording not yet implemented (requires FFmpeg libraries)";

    // Create temp directory for frame capture (fallback: frame-by-frame PNG export)
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_tempFilePath = tempDir + "/c64-recording-" +
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QDir().mkpath(m_tempFilePath);

    m_recording = true;
    m_recordingTimer.start();
    emit recordingStarted();
    return true;
}

void MediaCapture::stopRecording()
{
    if (!m_recording)
        return;

    m_recording = false;
    emit recordingStopped(m_tempFilePath);
}

void MediaCapture::appendVideoFrame(const QImage &frame)
{
    if (!m_recording || frame.isNull())
        return;

    // Stub: save individual frames (for future FFmpeg encoding)
    // In production, these would be fed to libavcodec
    Q_UNUSED(frame)
}

void MediaCapture::appendAudioData(const QByteArray &pcmData)
{
    if (!m_recording)
        return;

    // Stub: buffer audio data for future FFmpeg encoding
    Q_UNUSED(pcmData)
}
