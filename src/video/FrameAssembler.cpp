#include "video/FrameAssembler.h"
#include "video/ColorPalette.h"
#include "app/Log.h"

#include <cstring>
#include <QtEndian>

FrameAssembler::FrameAssembler(QObject *parent)
    : QObject(parent)
{
    m_frameTimer.start();
}

FrameAssembler::PacketHeader FrameAssembler::parseHeader(const char *data)
{
    PacketHeader h;
    uint16_t raw;

    memcpy(&h.sequenceNum, data + 0, 2);
    h.sequenceNum = qFromLittleEndian(h.sequenceNum);

    memcpy(&h.frameNum, data + 2, 2);
    h.frameNum = qFromLittleEndian(h.frameNum);

    memcpy(&raw, data + 4, 2);
    raw = qFromLittleEndian(raw);
    h.lineNum = raw & 0x7FFF;
    h.isLastPacket = (raw & 0x8000) != 0;

    return h;
}

void FrameAssembler::processPacket(const QByteArray &data)
{
    if (data.size() != VideoPacketSize)
        return;

    auto header = parseHeader(data.constData());

    QMutexLocker lock(&m_mutex);

    // New frame detected — assemble and emit previous
    if (m_hasData && header.frameNum != m_currentFrameNum) {
        assembleAndEmit();
    }

    // Start or continue frame
    if (!m_hasData || header.frameNum != m_currentFrameNum) {
        m_currentFrameNum = header.frameNum;
        m_receivedLines.clear();
        m_lastPacketReceived = false;
        m_maxLineReceived = 0;
        m_frameTimer.restart();
        m_hasData = true;
    }

    // Store payload (768 bytes after 12-byte header)
    m_receivedLines[header.lineNum] = data.mid(VideoHeaderSize);

    if (header.lineNum > m_maxLineReceived)
        m_maxLineReceived = header.lineNum;

    if (header.isLastPacket) {
        m_lastPacketReceived = true;
        assembleAndEmit();
    }
}

void FrameAssembler::checkTimeout()
{
    QMutexLocker lock(&m_mutex);
    if (m_hasData && m_frameTimer.elapsed() > 100) {
        assembleAndEmit();
    }
}

void FrameAssembler::assembleAndEmit()
{
    // Determine frame height
    int totalLines = static_cast<int>(m_maxLineReceived) + LinesPerPacket;
    int height = (totalLines <= NtscHeight) ? NtscHeight : PalHeight;
    int width = PixelsPerLine;

    // Build indexed pixel buffer (height * BytesPerLine)
    QByteArray indexed(height * BytesPerLine, '\0');
    char *dst = indexed.data();

    // Fill lines from received packets
    QByteArray lastValidLine(BytesPerLine * LinesPerPacket, '\0');

    for (int line = 0; line < height; line += LinesPerPacket) {
        uint16_t lineKey = static_cast<uint16_t>(line);
        auto it = m_receivedLines.find(lineKey);
        if (it != m_receivedLines.end()) {
            lastValidLine = it.value();
        }
        // Copy up to LinesPerPacket lines from this packet
        int linesToCopy = qMin(LinesPerPacket, height - line);
        memcpy(dst + line * BytesPerLine, lastValidLine.constData(), linesToCopy * BytesPerLine);
    }

    // Convert indexed to RGBA using LUT
    const auto &lut = ColorPalette::pairLUT();
    QByteArray rgba(width * height * 4, '\0');
    auto *rgbaPtr = reinterpret_cast<uint32_t *>(rgba.data());
    const auto *idxPtr = reinterpret_cast<const uint8_t *>(indexed.constData());

    int totalBytes = height * BytesPerLine;
    for (int i = 0; i < totalBytes; i++) {
        const auto &pair = lut[idxPtr[i]];
        rgbaPtr[i * 2]     = pair.left;
        rgbaPtr[i * 2 + 1] = pair.right;
    }

    m_hasData = false;
    m_receivedLines.clear();

    emit frameReady(rgba, width, height);
}
