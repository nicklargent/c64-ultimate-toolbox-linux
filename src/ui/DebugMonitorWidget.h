#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QStringList>

class C64Connection;

class DebugMonitorWidget : public QWidget {
    Q_OBJECT
public:
    explicit DebugMonitorWidget(C64Connection *connection, QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void executeCommand(const QString &input);
    void showPrompt();
    void appendOutput(const QString &text);
    QString currentInput() const;
    void replaceInput(const QString &text);

    // Commands
    void showHelp();
    void commandMemoryDump(const QStringList &args);
    void commandReadByte(const QStringList &args);
    void commandWriteByte(const QStringList &args);
    void commandFill(const QStringList &args);
    void commandHunt(const QStringList &args);

    static int parseHex(const QString &str);
    static QString disassemble(const QByteArray &data, int startAddress);

    C64Connection *m_connection;
    QPlainTextEdit *m_textEdit;
    QStringList m_history;
    int m_historyIndex = -1;
    int m_inputStart = 0;
};
