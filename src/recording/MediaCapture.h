#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QImage>

class CrtRenderer;
class AudioPlayer;

class MediaCapture : public QObject {
    Q_OBJECT
public:
    explicit MediaCapture(QObject *parent = nullptr);
    ~MediaCapture();

    void setRenderer(CrtRenderer *renderer) { m_renderer = renderer; }
    void setAudioPlayer(AudioPlayer *player) { m_audioPlayer = player; }

    bool isRecording() const { return m_recording; }

    QImage takeScreenshot();

    // Recording (requires FFmpeg at build time — stubbed if unavailable)
    bool startRecording(int width, int height);
    void stopRecording();
    void appendVideoFrame(const QImage &frame);
    void appendAudioData(const QByteArray &pcmData);

signals:
    void recordingStarted();
    void recordingStopped(const QString &filePath);

private:
    CrtRenderer *m_renderer = nullptr;
    AudioPlayer *m_audioPlayer = nullptr;
    bool m_recording = false;
    QElapsedTimer m_recordingTimer;
    QString m_tempFilePath;
};
