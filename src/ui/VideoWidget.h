#pragma once

#include <QWidget>
#include <QTimer>

class CrtRenderer;
class StatusBarWidget;
class KeyStripWidget;
class C64Connection;

class VideoWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoWidget(C64Connection *connection, QWidget *parent = nullptr);

    CrtRenderer *renderer() const { return m_renderer; }

    void setKeyboardStripVisible(bool visible);
    bool isKeyboardStripVisible() const { return m_keyStrip != nullptr; }

signals:
    void menuButtonClicked();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateStatus();

private:
    void layoutVideo();

    C64Connection *m_connection;
    CrtRenderer *m_renderer;
    StatusBarWidget *m_statusBar;
    KeyStripWidget *m_keyStrip = nullptr;
    QTimer *m_statusTimer;

    static constexpr float AspectRatio = 384.0f / 272.0f;
};
