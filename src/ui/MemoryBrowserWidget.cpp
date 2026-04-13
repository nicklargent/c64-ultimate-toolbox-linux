#include "ui/MemoryBrowserWidget.h"
#include "network/C64Connection.h"
#include "network/C64ApiClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

struct MemoryPreset {
    const char *name;
    int address;
};

static const MemoryPreset presets[] = {
    {"Zero Page",  0x0000},
    {"Screen",     0x0400},
    {"BASIC",      0x0801},
    {"Color RAM",  0xD800},
    {"VIC-II",     0xD000},
    {"SID",        0xD400},
    {"CIA 1",      0xDC00},
};

MemoryBrowserWidget::MemoryBrowserWidget(C64Connection *connection, QWidget *parent)
    : QWidget(parent)
    , m_connection(connection)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // Top bar
    auto *topBar = new QHBoxLayout;
    topBar->setSpacing(4);

    auto *prevBtn = new QPushButton("\u25c0", this);
    prevBtn->setFixedWidth(30);
    connect(prevBtn, &QPushButton::clicked, this, &MemoryBrowserWidget::prevPage);
    topBar->addWidget(prevBtn);

    topBar->addWidget(new QLabel("$", this));

    m_addressField = new QLineEdit("0000", this);
    m_addressField->setFont(QFont("monospace", 12));
    m_addressField->setFixedWidth(55);
    m_addressField->setMaxLength(4);
    connect(m_addressField, &QLineEdit::returnPressed, this, &MemoryBrowserWidget::goToAddress);
    topBar->addWidget(m_addressField);

    auto *goBtn = new QPushButton("Go", this);
    connect(goBtn, &QPushButton::clicked, this, &MemoryBrowserWidget::goToAddress);
    topBar->addWidget(goBtn);

    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem("Jump to...");
    for (const auto &p : presets)
        m_presetCombo->addItem(QStringLiteral("%1 ($%2)").arg(p.name, QString::number(p.address, 16).toUpper().rightJustified(4, '0')));
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MemoryBrowserWidget::presetSelected);
    topBar->addWidget(m_presetCombo);

    auto *nextBtn = new QPushButton("\u25b6", this);
    nextBtn->setFixedWidth(30);
    connect(nextBtn, &QPushButton::clicked, this, &MemoryBrowserWidget::nextPage);
    topBar->addWidget(nextBtn);

    layout->addLayout(topBar);

    // Hex dump view
    m_hexView = new QPlainTextEdit(this);
    m_hexView->setReadOnly(true);
    m_hexView->setFont(QFont("monospace", 11));
    m_hexView->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(m_hexView, 1);

    // Bottom bar
    auto *bottomBar = new QHBoxLayout;
    bottomBar->setSpacing(8);

    m_autoRefresh = new QCheckBox("Auto-refresh", this);
    connect(m_autoRefresh, &QCheckBox::toggled, this, &MemoryBrowserWidget::toggleAutoRefresh);
    bottomBar->addWidget(m_autoRefresh);

    bottomBar->addStretch();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: gray; font-size: 10pt;");
    bottomBar->addWidget(m_statusLabel);

    layout->addLayout(bottomBar);

    // Refresh timer
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() { loadMemory(m_currentAddress); });

    // Wire memory response
    if (m_connection->apiClient()) {
        connect(m_connection->apiClient(), &C64ApiClient::memoryDataReceived,
                this, &MemoryBrowserWidget::onMemoryReceived);
    }

    loadMemory(0);
}

void MemoryBrowserWidget::goToAddress()
{
    bool ok;
    int addr = m_addressField->text().trimmed().toInt(&ok, 16);
    if (ok)
        loadMemory(addr);
}

void MemoryBrowserWidget::prevPage()
{
    loadMemory(qMax(0, m_currentAddress - PageSize));
}

void MemoryBrowserWidget::nextPage()
{
    loadMemory(qMin(0xFFFF, m_currentAddress + PageSize));
}

void MemoryBrowserWidget::presetSelected(int index)
{
    int presetIdx = index - 1;
    if (presetIdx >= 0 && presetIdx < static_cast<int>(std::size(presets))) {
        loadMemory(presets[presetIdx].address);
        m_presetCombo->setCurrentIndex(0);
    }
}

void MemoryBrowserWidget::loadMemory(int address)
{
    auto *api = m_connection->apiClient();
    if (!api) return;

    m_currentAddress = qMin(address, 0xFFFF);
    int length = qMin(PageSize, 0x10000 - m_currentAddress);

    m_addressField->setText(QStringLiteral("%1").arg(m_currentAddress, 4, 16, QChar('0')).toUpper());
    m_statusLabel->setText(QStringLiteral("Loading $%1...").arg(m_currentAddress, 4, 16, QChar('0')).toUpper());

    api->readMemory(static_cast<uint16_t>(m_currentAddress), length);
}

void MemoryBrowserWidget::onMemoryReceived(uint16_t address, const QByteArray &data)
{
    if (address != static_cast<uint16_t>(m_currentAddress))
        return;

    m_memoryData = data;
    updateHexView();
    m_statusLabel->setText(QStringLiteral("%1 bytes at $%2")
        .arg(data.size())
        .arg(m_currentAddress, 4, 16, QChar('0')).toUpper());
}

void MemoryBrowserWidget::updateHexView()
{
    m_hexView->setPlainText(formatHexDump(m_currentAddress, m_memoryData));
}

QString MemoryBrowserWidget::formatHexDump(int baseAddr, const QByteArray &data)
{
    QString result;
    for (int row = 0; row < data.size(); row += 16) {
        int addr = baseAddr + row;
        QString hex = QStringLiteral("%1: ").arg(addr, 4, 16, QChar('0')).toUpper();
        QString ascii;

        for (int col = 0; col < 16; col++) {
            int offset = row + col;
            if (offset < data.size()) {
                uint8_t byte = static_cast<uint8_t>(data[offset]);
                hex += QStringLiteral("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
                ascii += (byte >= 0x20 && byte <= 0x7E) ? QChar(byte) : QChar('.');
            } else {
                hex += "   ";
            }
            if (col == 7) hex += ' ';
        }

        result += hex + " |" + ascii + "|\n";
    }
    return result;
}

void MemoryBrowserWidget::toggleAutoRefresh(bool checked)
{
    if (checked)
        m_refreshTimer->start(250);
    else
        m_refreshTimer->stop();
}
