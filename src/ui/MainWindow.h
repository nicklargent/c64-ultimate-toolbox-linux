#pragma once

#include <QMainWindow>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QSettings>
#include <QLabel>
#include <QTabWidget>

#include "models/ConnectionMode.h"

class C64Connection;
class VideoWidget;
class MenuController;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(C64Connection *connection, QWidget *parent = nullptr);
    ~MainWindow();

    C64Connection *connection() const { return m_connection; }
    VideoWidget *videoWidget() const { return m_videoWidget; }

    void setMode(ConnectionMode mode);

    bool eventFilter(QObject *obj, QEvent *event) override;

protected:
    void closeEvent(QCloseEvent *event) override;

signals:
    void windowClosed();

private slots:
    void onConnectionChanged();
    void onFpsUpdated(int fps);
    void onPauseResume();
    void onResetMachine();
    void onRebootMachine();
    void onPowerOff();
    void onRunFile();
    void onToggleKeyboard();
    void onScreenshot();
    void onToggleDebugPanel();
    void onToggleInspector();
    void onToggleSidebar();

private:
    void buildToolbar();
    void updateToolbarForMode();
    void updateTitle();
    void saveGeometry();
    void restoreGeometryState();

    C64Connection *m_connection;
    ConnectionMode m_mode = ConnectionMode::Toolbox;

    // Layout
    QSplitter *m_mainSplitter;     // horizontal: sidebar | center | inspector
    QSplitter *m_centerSplitter;   // vertical: video | debug
    VideoWidget *m_videoWidget;
    QWidget *m_sidebarPlaceholder;
    QWidget *m_inspectorPlaceholder;
    QWidget *m_debugPlaceholder;

    // Toolbar
    QToolBar *m_toolbar;
    QAction *m_actPauseResume;
    QAction *m_actReset;
    QAction *m_actReboot;
    QAction *m_actPowerOff;
    QAction *m_actRunFile;
    QAction *m_actKeyboard;
    QAction *m_actScreenshot;
    QAction *m_actRecord;
    QAction *m_actDebugPanel;
    QAction *m_actInspector;
    QAction *m_actSidebar;
    QAction *m_actNewScratchpad;

    // Status
    QLabel *m_fpsLabel;
    bool m_keyboardWasEnabled = false;
    MenuController *m_menuController = nullptr;
};
