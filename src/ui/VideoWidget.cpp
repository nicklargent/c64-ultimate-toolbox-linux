#include "ui/VideoWidget.h"
#include "video/CrtRenderer.h"
#include "video/FrameAssembler.h"
#include "ui/StatusBarWidget.h"
#include "ui/KeyStripWidget.h"
#include "network/C64Connection.h"
#include "network/KeyboardForwarder.h"

#include <QVBoxLayout>
#include <QResizeEvent>

VideoWidget::VideoWidget(C64Connection *connection, QWidget *parent)
    : QWidget(parent)
    , m_connection(connection)
{
    setStyleSheet("VideoWidget { background-color: black; }");
    setMinimumSize(200, 150);

    m_renderer = new CrtRenderer(this);

    m_statusBar = new StatusBarWidget(this);

    // Wire frame assembler to renderer
    connect(connection->frameAssembler(), &FrameAssembler::frameReady,
            m_renderer, &CrtRenderer::updateFrame);

    // Status update timer (500ms)
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &VideoWidget::updateStatus);
    m_statusTimer->start(500);
}

void VideoWidget::setKeyboardStripVisible(bool visible)
{
    if (visible && !m_keyStrip && m_connection->keyboardForwarder()) {
        m_keyStrip = new KeyStripWidget(m_connection->keyboardForwarder(), this);
        connect(m_keyStrip, &KeyStripWidget::menuButtonClicked,
                this, &VideoWidget::menuButtonClicked);
        m_keyStrip->show();
    } else if (!visible && m_keyStrip) {
        delete m_keyStrip;
        m_keyStrip = nullptr;
    }

    layoutVideo();
}

void VideoWidget::resizeEvent(QResizeEvent *)
{
    layoutVideo();
}

void VideoWidget::layoutVideo()
{
    int w = width();
    int h = height();

    // Status bar at bottom
    int statusH = m_statusBar->height(); // 24px
    m_statusBar->setGeometry(0, h - statusH, w, statusH);

    // Key strip above status bar (if visible)
    int keyStripH = 0;
    if (m_keyStrip) {
        keyStripH = m_keyStrip->sizeHint().height();
        m_keyStrip->setGeometry(0, h - statusH - keyStripH, w, keyStripH);
    }

    // Available space for video
    int availH = h - statusH - keyStripH;
    int availW = w;

    // Aspect-ratio-correct letterboxing
    int videoW, videoH;
    if (static_cast<float>(availW) / availH > AspectRatio) {
        // Pillarbox
        videoH = availH;
        videoW = static_cast<int>(availH * AspectRatio);
    } else {
        // Letterbox
        videoW = availW;
        videoH = static_cast<int>(availW / AspectRatio);
    }

    int videoX = (availW - videoW) / 2;
    int videoY = (availH - videoH) / 2;

    m_renderer->setGeometry(videoX, videoY, videoW, videoH);
}

void VideoWidget::updateStatus()
{
    bool recording = false; // TODO: wire to MediaCapture
    bool keyboard = m_connection->keyboardForwarder()
                    && m_connection->keyboardForwarder()->isEnabled();
    m_statusBar->update(recording, keyboard);
}
