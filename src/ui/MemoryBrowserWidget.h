#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QTimer>

class C64Connection;

class MemoryBrowserWidget : public QWidget {
    Q_OBJECT
public:
    explicit MemoryBrowserWidget(C64Connection *connection, QWidget *parent = nullptr);

private slots:
    void goToAddress();
    void prevPage();
    void nextPage();
    void presetSelected(int index);
    void onMemoryReceived(uint16_t address, const QByteArray &data);
    void toggleAutoRefresh(bool checked);

private:
    void loadMemory(int address);
    void updateHexView();
    static QString formatHexDump(int baseAddr, const QByteArray &data);

    C64Connection *m_connection;

    QLineEdit *m_addressField;
    QComboBox *m_presetCombo;
    QPlainTextEdit *m_hexView;
    QCheckBox *m_autoRefresh;
    QLabel *m_statusLabel;
    QTimer *m_refreshTimer;

    int m_currentAddress = 0;
    static constexpr int PageSize = 256;
    QByteArray m_memoryData;
};
