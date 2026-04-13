#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QElapsedTimer>
#include <QByteArray>

#include "models/CrtSettings.h"

class CrtRenderer : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit CrtRenderer(QWidget *parent = nullptr);
    ~CrtRenderer();

    void updateFrame(const QByteArray &rgbaData, int width, int height);
    void setCrtSettings(const CrtSettings &settings);
    CrtSettings crtSettings() const { return m_settings; }

    QImage captureFrame();

    QSize currentRenderSize() const;

signals:
    void frameCaptured(const QImage &image);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    bool loadShaders();
    GLuint createTexture(int width, int height);
    void ensureAccumTextures(int w, int h);

    QOpenGLShaderProgram *m_crtProgram = nullptr;
    QOpenGLShaderProgram *m_blitProgram = nullptr;

    GLuint m_vao = 0;
    GLuint m_sourceTexture = 0;
    GLuint m_accumTextures[2] = {0, 0};
    GLuint m_fbo = 0;
    int m_accumIndex = 0;

    int m_sourceWidth = 0;
    int m_sourceHeight = 0;
    int m_accumWidth = 0;
    int m_accumHeight = 0;

    CrtSettings m_settings;
    QElapsedTimer m_frameTimer;

    QByteArray m_pendingFrame;
    int m_pendingWidth = 0;
    int m_pendingHeight = 0;
    bool m_hasPendingFrame = false;

    bool m_captureNext = false;
    bool m_initialized = false;
};
