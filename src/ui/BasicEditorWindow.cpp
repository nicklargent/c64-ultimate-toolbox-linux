#include "ui/BasicEditorWindow.h"
#include "ui/BasicEditorWidget.h"
#include "models/BasicTokenizer.h"
#include "models/BasicSamples.h"
#include "network/C64Connection.h"
#include "network/C64ApiClient.h"
#include "app/Log.h"

#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>

BasicEditorWindow::BasicEditorWindow(C64Connection *connection, QWidget *parent)
    : QMainWindow(parent)
    , m_connection(connection)
{
    setWindowTitle("BASIC Editor");
    resize(600, 500);

    m_editor = new BasicEditorWidget(this);
    setCentralWidget(m_editor);

    connect(m_editor, &QPlainTextEdit::textChanged, this, [this]() {
        m_dirty = true;
        updateTitle();
        updateLineCount();
    });

    // Menu bar
    auto *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("New", QKeySequence::New, this, &BasicEditorWindow::newFile);
    fileMenu->addAction("Open...", QKeySequence::Open, this, &BasicEditorWindow::openFile);
    fileMenu->addAction("Save", QKeySequence::Save, this, &BasicEditorWindow::saveFile);
    fileMenu->addAction("Save As...", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), this, &BasicEditorWindow::saveFileAs);
    fileMenu->addSeparator();

    auto *templateMenu = fileMenu->addMenu("Templates");
    int idx = 0;
    for (const auto &sample : basicSamples()) {
        auto *action = templateMenu->addAction(sample.name);
        connect(action, &QAction::triggered, this, [this, idx]() { onTemplateSelected(idx); });
        idx++;
    }

    auto *deviceMenu = menuBar()->addMenu("Device");
    deviceMenu->addAction("Upload to Device", QKeySequence(Qt::CTRL | Qt::Key_U), this, &BasicEditorWindow::uploadToDevice);

    // Status bar
    m_statusLabel = new QLabel(this);
    statusBar()->addWidget(m_statusLabel);

    m_lineCountLabel = new QLabel("0 lines", this);
    statusBar()->addPermanentWidget(m_lineCountLabel);

    updateTitle();
}

void BasicEditorWindow::newFile()
{
    m_editor->clear();
    m_currentFilePath.clear();
    m_dirty = false;
    updateTitle();
}

void BasicEditorWindow::openFile()
{
    QString path = QFileDialog::getOpenFileName(this, "Open BASIC File", {},
        "BASIC Files (*.bas *.txt);;All Files (*)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open file: " + path);
        return;
    }

    m_editor->setText(QString::fromUtf8(file.readAll()));
    m_currentFilePath = path;
    m_dirty = false;
    updateTitle();
}

void BasicEditorWindow::saveFile()
{
    if (m_currentFilePath.isEmpty()) {
        saveFileAs();
        return;
    }

    QFile file(m_currentFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot save file: " + m_currentFilePath);
        return;
    }

    file.write(m_editor->toPlainText().toUtf8());
    m_dirty = false;
    updateTitle();
    m_statusLabel->setText("Saved");
}

void BasicEditorWindow::saveFileAs()
{
    QString path = QFileDialog::getSaveFileName(this, "Save BASIC File", "untitled.bas",
        "BASIC Files (*.bas);;All Files (*)");
    if (path.isEmpty())
        return;

    m_currentFilePath = path;
    saveFile();
}

void BasicEditorWindow::uploadToDevice()
{
    auto *api = m_connection->apiClient();
    if (!api) {
        m_statusLabel->setText("Not connected to device");
        return;
    }

    QString error;
    auto result = BasicTokenizer::tokenize(m_editor->toPlainText(), &error);

    if (!error.isEmpty()) {
        m_statusLabel->setText("Error: " + error);
        QMessageBox::warning(this, "Tokenization Error", error);
        return;
    }

    if (result.programData.isEmpty()) {
        m_statusLabel->setText("Nothing to upload");
        return;
    }

    m_statusLabel->setText(QStringLiteral("Uploading %1 bytes...").arg(result.programData.size()));
    api->runPrgFile(result.programData);

    connect(api, &C64ApiClient::requestSucceeded, this, [this](const QString &op) {
        if (op == "runPrg")
            m_statusLabel->setText("Uploaded successfully");
    }, Qt::SingleShotConnection);

    connect(api, &C64ApiClient::requestFailed, this, [this](const QString &op, const QString &err) {
        if (op == "runPrg")
            m_statusLabel->setText("Upload failed: " + err);
    }, Qt::SingleShotConnection);
}

void BasicEditorWindow::onTemplateSelected(int index)
{
    auto samples = basicSamples();
    if (index < 0 || index >= samples.size())
        return;

    m_editor->setText(samples[index].code);
    m_currentFilePath.clear();
    m_dirty = false;
    updateTitle();
    m_statusLabel->setText("Loaded template: " + samples[index].name);
}

void BasicEditorWindow::updateTitle()
{
    QString filename = m_currentFilePath.isEmpty() ? "Untitled" : QFileInfo(m_currentFilePath).fileName();
    QString dirty = m_dirty ? " *" : "";
    setWindowTitle(QStringLiteral("BASIC \u2014 %1%2").arg(filename, dirty));
}

void BasicEditorWindow::updateLineCount()
{
    m_lineCountLabel->setText(QStringLiteral("%1 lines").arg(m_editor->lineCount()));
}
