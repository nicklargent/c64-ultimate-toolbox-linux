#pragma once

#include <QObject>
#include <QMutex>
#include <QByteArray>
#include <QElapsedTimer>
#include <QMap>

class FrameAssembler : public QObject {
    Q_OBJECT
public:
    static constexpr int PixelsPerLine = 384;
    static constexpr int BytesPerLine = 192;
    static constexpr int LinesPerPacket = 4;
    static constexpr int VideoHeaderSize = 12;
    static constexpr int VideoPacketSize = 780;
    static constexpr int PalHeight = 272;
    static constexpr int NtscHeight = 240;

    explicit FrameAssembler(QObject *parent = nullptr);

    void processPacket(const QByteArray &data);
    void checkTimeout();

signals:
    void frameReady(const QByteArray &rgbaData, int width, int height);

private:
    struct PacketHeader {
        uint16_t sequenceNum;
        uint16_t frameNum;
        uint16_t lineNum;
        bool isLastPacket;
    };

    static PacketHeader parseHeader(const char *data);
    void assembleAndEmit();

    QMutex m_mutex;
    uint16_t m_currentFrameNum = 0;
    QMap<uint16_t, QByteArray> m_receivedLines; // lineNum -> payload (4 lines * 192 bytes)
    bool m_lastPacketReceived = false;
    QElapsedTimer m_frameTimer;
    uint16_t m_maxLineReceived = 0;
    bool m_hasData = false;
};
