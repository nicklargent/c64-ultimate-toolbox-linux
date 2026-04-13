#pragma once

#include <QWidget>
#include <QSplitter>

class C64Connection;
class MemoryBrowserWidget;
class DebugMonitorWidget;

class DebugPanel : public QWidget {
    Q_OBJECT
public:
    explicit DebugPanel(C64Connection *connection, QWidget *parent = nullptr);

private:
    MemoryBrowserWidget *m_memoryBrowser;
    DebugMonitorWidget *m_debugMonitor;
};
