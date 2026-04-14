#include "ui/MainWindow.h"
#include "ui/VideoWidget.h"
#include "ui/FileManagerWidget.h"
#include "ui/SystemPanel.h"
#include "ui/DisplayAudioPanel.h"
#include "ui/DebugPanel.h"
#include "ui/BasicEditorWindow.h"
#include "network/MenuController.h"
#include "video/CrtRenderer.h"
#include "network/C64Connection.h"
#include "network/KeyboardForwarder.h"
#include "app/Log.h"

#include <QKeyEvent>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QStatusBar>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QVBoxLayout>

MainWindow::MainWindow(C64Connection *connection, QWidget *parent)
    : QMainWindow(parent)
    , m_connection(connection)
{
    setMinimumSize(700, 450);
    resize(1200, 750);
    setWindowTitle("C64 Ultimate Toolbox");

    // FPS label for status bar
    m_fpsLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_fpsLabel);

    // Build the 3-column splitter layout
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Sidebar: File Manager
    m_sidebarPlaceholder = new FileManagerWidget(connection, this);
    m_sidebarPlaceholder->setMinimumWidth(300);
    m_sidebarPlaceholder->setMaximumWidth(500);

    // Center: vertical splitter (video on top, debug on bottom)
    m_centerSplitter = new QSplitter(Qt::Vertical, this);

    m_videoWidget = new VideoWidget(connection, this);
    m_centerSplitter->addWidget(m_videoWidget);

    // Debug panel: Memory Browser + Monitor
    m_debugPlaceholder = new DebugPanel(connection, this);
    m_debugPlaceholder->setMinimumHeight(200);
    m_centerSplitter->addWidget(m_debugPlaceholder);
    m_debugPlaceholder->hide();

    m_centerSplitter->setStretchFactor(0, 1); // Video stretches
    m_centerSplitter->setStretchFactor(1, 0); // Debug fixed

    // Inspector: tabbed System + Display/Audio panels
    auto *inspectorTabs = new QTabWidget(this);
    inspectorTabs->addTab(new SystemPanel(connection, this), "Device Info");
    inspectorTabs->addTab(new DisplayAudioPanel(connection, m_videoWidget->renderer(), this), "App Settings");
    inspectorTabs->setMinimumWidth(350);
    inspectorTabs->setMaximumWidth(700);
    m_inspectorPlaceholder = inspectorTabs;

    m_mainSplitter->addWidget(m_sidebarPlaceholder);
    m_mainSplitter->addWidget(m_centerSplitter);
    m_mainSplitter->addWidget(m_inspectorPlaceholder);

    // Default sizes: sidebar 0 (hidden), center stretches, inspector 0 (hidden)
    m_mainSplitter->setSizes({0, 1, 0});
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setStretchFactor(2, 0);

    setCentralWidget(m_mainSplitter);

    // Hide sidebar and inspector by default
    m_sidebarPlaceholder->hide();
    m_inspectorPlaceholder->hide();

    buildToolbar();

    // Connect signals
    connect(m_connection, &C64Connection::connectionChanged,
            this, &MainWindow::onConnectionChanged);
    connect(m_connection, &C64Connection::fpsUpdated,
            this, &MainWindow::onFpsUpdated);

    // Menu button from keyboard strip — toggle on-screen Ultimate menu
    connect(m_videoWidget, &VideoWidget::menuButtonClicked, this, [this]() {
        if (m_connection->apiClient())
            m_connection->machineAction(C64Connection::MachineAction::MenuButton);
    });

    restoreGeometryState();

    // Install app-wide event filter to capture keys for keyboard forwarding
    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    saveGeometry();
}

void MainWindow::setMode(ConnectionMode mode)
{
    m_mode = mode;
    updateToolbarForMode();
    updateTitle();

    // Sidebar hidden in viewer mode
    m_sidebarPlaceholder->setVisible(mode == ConnectionMode::Toolbox);

    // Default keyboard forwarding ON in Toolbox mode
    if (mode == ConnectionMode::Toolbox) {
        auto *fwd = m_connection->keyboardForwarder();
        if (fwd) {
            fwd->setEnabled(true);
            m_actKeyboard->setChecked(true);
            m_videoWidget->setKeyboardStripVisible(true);
        }
    }
}

void MainWindow::buildToolbar()
{
    m_toolbar = addToolBar("Main");
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(20, 20));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_actSidebar = m_toolbar->addAction("Sidebar");
    m_actSidebar->setCheckable(true);
    m_actSidebar->setToolTip("Toggle Sidebar");
    connect(m_actSidebar, &QAction::triggered, this, &MainWindow::onToggleSidebar);

    m_toolbar->addSeparator();

    m_actPauseResume = m_toolbar->addAction("Pause");
    m_actPauseResume->setToolTip("Pause/Resume");
    connect(m_actPauseResume, &QAction::triggered, this, &MainWindow::onPauseResume);

    m_actReset = m_toolbar->addAction("Reset");
    m_actReset->setToolTip("Reset Machine");
    connect(m_actReset, &QAction::triggered, this, &MainWindow::onResetMachine);

    m_actReboot = m_toolbar->addAction("Reboot");
    m_actReboot->setToolTip("Reboot Device");
    connect(m_actReboot, &QAction::triggered, this, &MainWindow::onRebootMachine);

    m_actPowerOff = m_toolbar->addAction("Power Off");
    m_actPowerOff->setToolTip("Power Off Device");
    connect(m_actPowerOff, &QAction::triggered, this, &MainWindow::onPowerOff);

    m_toolbar->addSeparator();

    m_actNewScratchpad = m_toolbar->addAction("BASIC");
    m_actNewScratchpad->setToolTip("New BASIC Scratchpad");
    connect(m_actNewScratchpad, &QAction::triggered, this, [this]() {
        auto *editor = new BasicEditorWindow(m_connection);
        editor->setAttribute(Qt::WA_DeleteOnClose);
        editor->show();
    });

    m_actRunFile = m_toolbar->addAction("Run File");
    m_actRunFile->setToolTip("Run File on Device");
    connect(m_actRunFile, &QAction::triggered, this, &MainWindow::onRunFile);

    m_toolbar->addSeparator();

    m_actKeyboard = m_toolbar->addAction("Keyboard");
    m_actKeyboard->setCheckable(true);
    m_actKeyboard->setToolTip("Send Keyboard Input");
    connect(m_actKeyboard, &QAction::triggered, this, &MainWindow::onToggleKeyboard);

    m_toolbar->addSeparator();

    m_actScreenshot = m_toolbar->addAction("Screenshot");
    m_actScreenshot->setToolTip("Take Screenshot");
    connect(m_actScreenshot, &QAction::triggered, this, &MainWindow::onScreenshot);

    m_actRecord = m_toolbar->addAction("Record");
    m_actRecord->setCheckable(true);
    m_actRecord->setToolTip("Toggle Recording");

    m_toolbar->addSeparator();

    m_actDebugPanel = m_toolbar->addAction("Debug");
    m_actDebugPanel->setCheckable(true);
    m_actDebugPanel->setToolTip("Toggle Debug Panel");
    connect(m_actDebugPanel, &QAction::triggered, this, &MainWindow::onToggleDebugPanel);

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    m_actInspector = m_toolbar->addAction("Inspector");
    m_actInspector->setCheckable(true);
    m_actInspector->setToolTip("Toggle Inspector Panel");
    connect(m_actInspector, &QAction::triggered, this, &MainWindow::onToggleInspector);
}

void MainWindow::updateToolbarForMode()
{
    bool toolbox = (m_mode == ConnectionMode::Toolbox);

    m_actSidebar->setVisible(toolbox);
    m_actPauseResume->setVisible(toolbox);
    m_actReset->setVisible(toolbox);
    m_actReboot->setVisible(toolbox);
    m_actPowerOff->setVisible(toolbox);
    m_actNewScratchpad->setVisible(toolbox);
    m_actRunFile->setVisible(toolbox);
    m_actKeyboard->setVisible(toolbox);
    m_actDebugPanel->setVisible(toolbox);
}

void MainWindow::updateTitle()
{
    if (m_mode == ConnectionMode::Viewer) {
        setWindowTitle("C64 Ultimate Viewer");
    } else {
        auto info = m_connection->deviceInfo();
        if (!info.product.isEmpty()) {
            setWindowTitle(QStringLiteral("%1 \u2014 %2").arg(info.product, info.hostname));
        } else {
            setWindowTitle("C64 Ultimate Toolbox");
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveGeometry();
    m_connection->disconnect();
    emit windowClosed();
    event->accept();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *fwd = m_connection->keyboardForwarder();
        if (!fwd || !fwd->isEnabled())
            return QMainWindow::eventFilter(obj, event);

        // Don't intercept keys when a text input widget has focus
        auto *focusWidget = QApplication::focusWidget();
        if (focusWidget) {
            if (qobject_cast<QLineEdit *>(focusWidget) ||
                qobject_cast<QPlainTextEdit *>(focusWidget) ||
                qobject_cast<QTextEdit *>(focusWidget) ||
                qobject_cast<QSpinBox *>(focusWidget) ||
                qobject_cast<QComboBox *>(focusWidget)) {
                return QMainWindow::eventFilter(obj, event);
            }
            // Don't intercept if focus is in a different window (e.g. BASIC editor)
            if (focusWidget->window() != this)
                return QMainWindow::eventFilter(obj, event);
        }

        // Don't intercept Ctrl+ or Alt+ combos (app shortcuts)
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier))
            return QMainWindow::eventFilter(obj, event);

        // Forward to C64 keyboard buffer
        uint8_t petscii = KeyboardForwarder::qtKeyToPetscii(ke->key());
        if (petscii != 0) {
            fwd->sendKey(petscii);
            return true;
        }

        QString text = ke->text();
        if (!text.isEmpty()) {
            for (const QChar &ch : text) {
                fwd->handleCharacter(ch);
            }
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::onConnectionChanged()
{
    updateTitle();
    updateToolbarForMode();
}

void MainWindow::onFpsUpdated(int fps)
{
    m_fpsLabel->setText(QStringLiteral("%1 FPS").arg(fps));
}

void MainWindow::onPauseResume()
{
    if (m_connection->isPaused()) {
        m_connection->machineAction(C64Connection::MachineAction::Resume);
        m_actPauseResume->setText("Pause");
    } else {
        m_connection->machineAction(C64Connection::MachineAction::Pause);
        m_actPauseResume->setText("Resume");
    }
}

void MainWindow::onResetMachine()
{
    auto ret = QMessageBox::question(this, "Reset Machine",
        "Reset the C64? This will restart the machine.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes)
        m_connection->machineAction(C64Connection::MachineAction::Reset);
}

void MainWindow::onRebootMachine()
{
    auto ret = QMessageBox::question(this, "Reboot Device",
        "Reboot the Ultimate device? Streams will reconnect automatically.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes)
        m_connection->machineAction(C64Connection::MachineAction::Reboot);
}

void MainWindow::onPowerOff()
{
    auto ret = QMessageBox::question(this, "Power Off",
        "Power off the Ultimate device? You will be disconnected.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes)
        m_connection->machineAction(C64Connection::MachineAction::PowerOff);
}

void MainWindow::onRunFile()
{
    QString filter = "C64 Files (*.sid *.mod *.prg *.crt);;SID Files (*.sid);;MOD Files (*.mod);;PRG Files (*.prg);;CRT Files (*.crt);;All Files (*)";
    QString path = QFileDialog::getOpenFileName(this, "Run File on Device", {}, filter);
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Cannot open file: " + path);
        return;
    }

    QByteArray data = file.readAll();
    QString ext = path.section('.', -1).toLower();

    C64Connection::RunnerType type;
    if (ext == "sid") type = C64Connection::RunnerType::Sid;
    else if (ext == "mod") type = C64Connection::RunnerType::Mod;
    else if (ext == "crt") type = C64Connection::RunnerType::Crt;
    else type = C64Connection::RunnerType::Prg;

    m_connection->runFile(type, data);
}

void MainWindow::onToggleKeyboard()
{
    auto *fwd = m_connection->keyboardForwarder();
    if (!fwd)
        return;

    bool enable = !fwd->isEnabled();
    fwd->setEnabled(enable);
    m_actKeyboard->setChecked(enable);
    m_videoWidget->setKeyboardStripVisible(enable);
}

void MainWindow::onScreenshot()
{
    QImage img = m_videoWidget->renderer()->captureFrame();
    if (img.isNull())
        return;

    QString path = QFileDialog::getSaveFileName(this, "Save Screenshot", "screenshot.png",
                                                 "PNG (*.png);;All Files (*)");
    if (!path.isEmpty())
        img.save(path);
}

void MainWindow::onToggleDebugPanel()
{
    bool show = !m_debugPlaceholder->isVisible();
    m_debugPlaceholder->setVisible(show);
    m_actDebugPanel->setChecked(show);
}

void MainWindow::onToggleInspector()
{
    bool show = !m_inspectorPlaceholder->isVisible();
    m_inspectorPlaceholder->setVisible(show);
    m_actInspector->setChecked(show);

    if (show) {
        // Give inspector a reasonable width
        auto sizes = m_mainSplitter->sizes();
        if (sizes.size() == 3 && sizes[2] == 0) {
            int total = sizes[0] + sizes[1] + sizes[2];
            sizes[2] = 400;
            sizes[1] = total - sizes[0] - sizes[2];
            m_mainSplitter->setSizes(sizes);
        }
    }
}

void MainWindow::onToggleSidebar()
{
    bool show = !m_sidebarPlaceholder->isVisible();
    m_sidebarPlaceholder->setVisible(show);
    m_actSidebar->setChecked(show);

    if (show) {
        auto sizes = m_mainSplitter->sizes();
        if (sizes.size() == 3 && sizes[0] == 0) {
            int total = sizes[0] + sizes[1] + sizes[2];
            sizes[0] = 300;
            sizes[1] = total - sizes[0] - sizes[2];
            m_mainSplitter->setSizes(sizes);
        }
    }
}

void MainWindow::saveGeometry()
{
    QSettings settings;
    settings.setValue("window/geometry", QMainWindow::saveGeometry());
    settings.setValue("window/splitterState", m_mainSplitter->saveState());
}

void MainWindow::restoreGeometryState()
{
    QSettings settings;
    auto geom = settings.value("window/geometry").toByteArray();
    if (!geom.isEmpty())
        QMainWindow::restoreGeometry(geom);

    auto splitterState = settings.value("window/splitterState").toByteArray();
    if (!splitterState.isEmpty())
        m_mainSplitter->restoreState(splitterState);
}
