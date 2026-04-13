#include "ui/DebugPanel.h"
#include "ui/MemoryBrowserWidget.h"
#include "ui/DebugMonitorWidget.h"

#include <QHBoxLayout>

DebugPanel::DebugPanel(C64Connection *connection, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    m_memoryBrowser = new MemoryBrowserWidget(connection, this);
    m_memoryBrowser->setMinimumWidth(540);
    splitter->addWidget(m_memoryBrowser);

    m_debugMonitor = new DebugMonitorWidget(connection, this);
    m_debugMonitor->setMinimumWidth(200);
    splitter->addWidget(m_debugMonitor);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    layout->addWidget(splitter);
}
