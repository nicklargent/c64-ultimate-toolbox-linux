#include "video/CrtRenderer.h"
#include "app/Log.h"

#include <QFile>
#include <QImage>

static QString loadShaderSource(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(logVideo) << "Cannot load shader:" << path;
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

CrtRenderer::CrtRenderer(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // Request OpenGL 3.3 Core Profile
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapInterval(1);
    setFormat(fmt);

    m_frameTimer.start();
}

CrtRenderer::~CrtRenderer()
{
    makeCurrent();

    if (m_sourceTexture) glDeleteTextures(1, &m_sourceTexture);
    if (m_accumTextures[0]) glDeleteTextures(2, m_accumTextures);
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);

    delete m_crtProgram;
    delete m_blitProgram;

    doneCurrent();
}

void CrtRenderer::updateFrame(const QByteArray &rgbaData, int width, int height)
{
    m_pendingFrame = rgbaData;
    m_pendingWidth = width;
    m_pendingHeight = height;
    m_hasPendingFrame = true;

    update(); // Schedule repaint
}

void CrtRenderer::setCrtSettings(const CrtSettings &settings)
{
    m_settings = settings;
    update();
}

QImage CrtRenderer::captureFrame()
{
    makeCurrent();

    int w = width() * devicePixelRatio();
    int h = height() * devicePixelRatio();

    QImage img(w, h, QImage::Format_RGBA8888);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());

    doneCurrent();

    return img.mirrored(); // OpenGL is bottom-up
}

QSize CrtRenderer::currentRenderSize() const
{
    return QSize(width() * devicePixelRatio(), height() * devicePixelRatio());
}

void CrtRenderer::initializeGL()
{
    initializeOpenGLFunctions();

    qCInfo(logVideo) << "OpenGL:" << reinterpret_cast<const char*>(glGetString(GL_VERSION));
    qCInfo(logVideo) << "GLSL:" << reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Create empty VAO (we use gl_VertexID)
    glGenVertexArrays(1, &m_vao);

    // Create FBO for off-screen CRT pass
    glGenFramebuffers(1, &m_fbo);

    if (!loadShaders()) {
        qCCritical(logVideo) << "Failed to load shaders";
        return;
    }

    m_initialized = true;
}

bool CrtRenderer::loadShaders()
{
    // Find shader source files relative to the binary or use embedded paths
    // Try several locations
    QString crtVertSrc = loadShaderSource(":/shaders/crt.vert");
    QString crtFragSrc = loadShaderSource(":/shaders/crt.frag");
    QString blitVertSrc = loadShaderSource(":/shaders/blit.vert");
    QString blitFragSrc = loadShaderSource(":/shaders/blit.frag");

    if (crtVertSrc.isEmpty() || crtFragSrc.isEmpty() ||
        blitVertSrc.isEmpty() || blitFragSrc.isEmpty()) {
        qCCritical(logVideo) << "Could not load shader resources";
        return false;
    }

    m_crtProgram = new QOpenGLShaderProgram(this);
    if (!m_crtProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, crtVertSrc) ||
        !m_crtProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, crtFragSrc) ||
        !m_crtProgram->link()) {
        qCCritical(logVideo) << "CRT shader error:" << m_crtProgram->log();
        return false;
    }

    m_blitProgram = new QOpenGLShaderProgram(this);
    if (!m_blitProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, blitVertSrc) ||
        !m_blitProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, blitFragSrc) ||
        !m_blitProgram->link()) {
        qCCritical(logVideo) << "Blit shader error:" << m_blitProgram->log();
        return false;
    }

    return true;
}

GLuint CrtRenderer::createTexture(int width, int height)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

void CrtRenderer::ensureAccumTextures(int w, int h)
{
    if (m_accumWidth == w && m_accumHeight == h && m_accumTextures[0])
        return;

    if (m_accumTextures[0])
        glDeleteTextures(2, m_accumTextures);

    m_accumTextures[0] = createTexture(w, h);
    m_accumTextures[1] = createTexture(w, h);
    m_accumWidth = w;
    m_accumHeight = h;
    m_accumIndex = 0;

    // Clear both accum textures to black
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    for (int i = 0; i < 2; i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_accumTextures[i], 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
}

void CrtRenderer::resizeGL(int w, int h)
{
    int dprW = w * devicePixelRatio();
    int dprH = h * devicePixelRatio();
    ensureAccumTextures(dprW, dprH);
}

void CrtRenderer::paintGL()
{
    if (!m_initialized)
        return;

    // Upload pending frame to source texture
    if (m_hasPendingFrame) {
        if (!m_sourceTexture || m_sourceWidth != m_pendingWidth || m_sourceHeight != m_pendingHeight) {
            if (m_sourceTexture)
                glDeleteTextures(1, &m_sourceTexture);

            m_sourceWidth = m_pendingWidth;
            m_sourceHeight = m_pendingHeight;

            glGenTextures(1, &m_sourceTexture);
            glBindTexture(GL_TEXTURE_2D, m_sourceTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_sourceWidth, m_sourceHeight, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, m_pendingFrame.constData());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } else {
            glBindTexture(GL_TEXTURE_2D, m_sourceTexture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_sourceWidth, m_sourceHeight,
                            GL_RGBA, GL_UNSIGNED_BYTE, m_pendingFrame.constData());
        }
        m_hasPendingFrame = false;
    }

    if (!m_sourceTexture)
        return;

    // Calculate delta time
    float dtMs = m_frameTimer.elapsed();
    if (dtMs < 1.0f) dtMs = 16.67f;
    m_frameTimer.restart();

    int drawW = width() * devicePixelRatio();
    int drawH = height() * devicePixelRatio();

    ensureAccumTextures(drawW, drawH);
    glBindVertexArray(m_vao);

    bool hasCRT = m_settings.hasEffects();

    if (hasCRT && m_accumTextures[0]) {
        GLuint readAccum = m_accumTextures[m_accumIndex];
        GLuint writeAccum = m_accumTextures[1 - m_accumIndex];

        // Pass 1: CRT shader → accumulation texture
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, writeAccum, 0);
        glViewport(0, 0, drawW, drawH);

        m_crtProgram->bind();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sourceTexture);
        m_crtProgram->setUniformValue("sourceTexture", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, readAccum);
        m_crtProgram->setUniformValue("prevAccum", 1);

        m_crtProgram->setUniformValue("scanlineIntensity", m_settings.scanlineIntensity);
        m_crtProgram->setUniformValue("scanlineWidth", m_settings.scanlineWidth);
        m_crtProgram->setUniformValue("blurRadius", m_settings.blurRadius);
        m_crtProgram->setUniformValue("bloomIntensity", m_settings.bloomIntensity);
        m_crtProgram->setUniformValue("bloomRadius", m_settings.bloomRadius);
        m_crtProgram->setUniformValue("afterglowStrength", m_settings.afterglowStrength);
        m_crtProgram->setUniformValue("afterglowDecaySpeed", m_settings.afterglowDecaySpeed);
        m_crtProgram->setUniformValue("tintMode", m_settings.tintMode);
        m_crtProgram->setUniformValue("tintStrength", m_settings.tintStrength);
        m_crtProgram->setUniformValue("maskType", m_settings.maskType);
        m_crtProgram->setUniformValue("maskIntensity", m_settings.maskIntensity);
        m_crtProgram->setUniformValue("curvatureAmount", m_settings.curvatureAmount);
        m_crtProgram->setUniformValue("vignetteStrength", m_settings.vignetteStrength);
        m_crtProgram->setUniformValue("dtMs", dtMs);
        m_crtProgram->setUniformValue("outputSize", QVector2D(drawW, drawH));
        m_crtProgram->setUniformValue("sourceSize", QVector2D(m_sourceWidth, m_sourceHeight));

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Pass 2: Blit accumulation to screen
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        glViewport(0, 0, drawW, drawH);

        m_blitProgram->bind();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, writeAccum);
        m_blitProgram->setUniformValue("blitTexture", 0);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        m_accumIndex = 1 - m_accumIndex;
    } else {
        // No CRT effects: blit source directly
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        glViewport(0, 0, drawW, drawH);

        m_blitProgram->bind();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sourceTexture);
        m_blitProgram->setUniformValue("blitTexture", 0);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glBindVertexArray(0);
}
