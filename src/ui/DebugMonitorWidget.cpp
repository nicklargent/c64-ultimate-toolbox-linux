#include "ui/DebugMonitorWidget.h"
#include "network/C64Connection.h"
#include "network/C64ApiClient.h"

#include <QVBoxLayout>
#include <QKeyEvent>

DebugMonitorWidget::DebugMonitorWidget(C64Connection *connection, QWidget *parent)
    : QWidget(parent)
    , m_connection(connection)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setFont(QFont("monospace", 12));
    m_textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_textEdit->installEventFilter(this);
    layout->addWidget(m_textEdit);

    appendOutput("Ultimate Toolbox Remote Monitor\nType 'help' for a list of commands.");
    showPrompt();
}

bool DebugMonitorWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_textEdit && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);

        // Prevent editing before prompt
        auto cursor = m_textEdit->textCursor();
        if (cursor.position() < m_inputStart && ke->key() != Qt::Key_Up && ke->key() != Qt::Key_Down) {
            cursor.movePosition(QTextCursor::End);
            m_textEdit->setTextCursor(cursor);
        }

        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            QString input = currentInput().trimmed();
            m_textEdit->appendPlainText("");

            if (!input.isEmpty()) {
                m_history.append(input);
                m_historyIndex = m_history.size();
                executeCommand(input);
            } else {
                showPrompt();
            }
            return true;
        }

        if (ke->key() == Qt::Key_Up) {
            if (m_historyIndex > 0) {
                m_historyIndex--;
                replaceInput(m_history[m_historyIndex]);
            }
            return true;
        }

        if (ke->key() == Qt::Key_Down) {
            if (m_historyIndex < m_history.size() - 1) {
                m_historyIndex++;
                replaceInput(m_history[m_historyIndex]);
            } else {
                m_historyIndex = m_history.size();
                replaceInput("");
            }
            return true;
        }

        if (ke->key() == Qt::Key_Backspace && cursor.position() <= m_inputStart) {
            return true; // block
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DebugMonitorWidget::showPrompt()
{
    m_textEdit->moveCursor(QTextCursor::End);
    m_textEdit->insertPlainText("> ");
    m_inputStart = m_textEdit->toPlainText().length();
    m_textEdit->moveCursor(QTextCursor::End);
}

void DebugMonitorWidget::appendOutput(const QString &text)
{
    m_textEdit->moveCursor(QTextCursor::End);
    m_textEdit->insertPlainText(text + "\n");
}

QString DebugMonitorWidget::currentInput() const
{
    QString full = m_textEdit->toPlainText();
    if (m_inputStart >= full.length()) return {};
    return full.mid(m_inputStart);
}

void DebugMonitorWidget::replaceInput(const QString &text)
{
    QString full = m_textEdit->toPlainText();
    QString before = full.left(m_inputStart);
    m_textEdit->setPlainText(before + text);
    m_textEdit->moveCursor(QTextCursor::End);
}

void DebugMonitorWidget::executeCommand(const QString &input)
{
    auto parts = input.trimmed().split(' ', Qt::SkipEmptyParts);
    if (parts.isEmpty()) { showPrompt(); return; }

    QString cmd = parts[0].toLower();
    QStringList args = parts.mid(1);

    if (cmd == "help" || cmd == "?") {
        showHelp();
        showPrompt();
    } else if (cmd == "m") {
        commandMemoryDump(args);
    } else if (cmd == "r") {
        commandReadByte(args);
    } else if (cmd == "w") {
        commandWriteByte(args);
    } else if (cmd == "f") {
        commandFill(args);
    } else if (cmd == "h") {
        commandHunt(args);
    } else if (cmd == "clear" || cmd == "cls") {
        m_textEdit->clear();
        showPrompt();
    } else {
        appendOutput("Unknown command: " + cmd + ". Type 'help' for commands.");
        showPrompt();
    }
}

void DebugMonitorWidget::showHelp()
{
    appendOutput(
        "Available commands:\n"
        "  m <addr>             Memory dump, 256 bytes\n"
        "  m <start> <end>      Memory dump, range\n"
        "  r <addr>             Read single byte\n"
        "  w <addr> <byte>      Write single byte\n"
        "  h <start> <end> <b>  Hunt for byte pattern\n"
        "  f <start> <end> <b>  Fill range with byte\n"
        "  clear                Clear screen\n"
        "  help                 This help text\n"
        "All values in hexadecimal.");
}

void DebugMonitorWidget::commandMemoryDump(const QStringList &args)
{
    int startAddr = parseHex(args.value(0));
    if (startAddr < 0) {
        appendOutput("Usage: m <addr> [end_addr]");
        showPrompt();
        return;
    }

    int endAddr = args.size() > 1 ? parseHex(args[1]) : startAddr + 0xFF;
    if (endAddr < 0) endAddr = startAddr + 0xFF;
    int length = qMin(endAddr - startAddr + 1, 0x1000);

    auto *api = m_connection->apiClient();
    if (!api) { appendOutput("Error: Not connected"); showPrompt(); return; }

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(api, &C64ApiClient::memoryDataReceived, this,
        [this, conn, startAddr](uint16_t addr, const QByteArray &data) {
            if (addr != static_cast<uint16_t>(startAddr)) return;
            QObject::disconnect(*conn);

            QString output;
            for (int row = 0; row < data.size(); row += 16) {
                int a = startAddr + row;
                QString hex = QStringLiteral("%1: ").arg(a, 4, 16, QChar('0')).toUpper();
                QString ascii;
                for (int col = 0; col < 16 && row + col < data.size(); col++) {
                    uint8_t b = static_cast<uint8_t>(data[row + col]);
                    hex += QStringLiteral("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
                    ascii += (b >= 0x20 && b <= 0x7E) ? QChar(b) : QChar('.');
                }
                output += hex + " " + ascii + "\n";
            }
            appendOutput(output);
            showPrompt();
        });

    api->readMemory(static_cast<uint16_t>(startAddr), length);
}

void DebugMonitorWidget::commandReadByte(const QStringList &args)
{
    int addr = parseHex(args.value(0));
    if (addr < 0) { appendOutput("Usage: r <addr>"); showPrompt(); return; }

    auto *api = m_connection->apiClient();
    if (!api) { appendOutput("Error: Not connected"); showPrompt(); return; }

    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(api, &C64ApiClient::memoryDataReceived, this,
        [this, conn, addr](uint16_t a, const QByteArray &data) {
            if (a != static_cast<uint16_t>(addr)) return;
            QObject::disconnect(*conn);
            if (!data.isEmpty()) {
                uint8_t b = static_cast<uint8_t>(data[0]);
                appendOutput(QStringLiteral("$%1 = $%2 (%3)")
                    .arg(addr, 4, 16, QChar('0')).toUpper()
                    .arg(b, 2, 16, QChar('0')).toUpper()
                    .arg(b));
            }
            showPrompt();
        });

    api->readMemory(static_cast<uint16_t>(addr), 1);
}

void DebugMonitorWidget::commandWriteByte(const QStringList &args)
{
    if (args.size() < 2) { appendOutput("Usage: w <addr> <byte>"); showPrompt(); return; }
    int addr = parseHex(args[0]);
    int byte = parseHex(args[1]);
    if (addr < 0 || byte < 0 || byte > 0xFF) { appendOutput("Invalid address or byte"); showPrompt(); return; }

    auto *api = m_connection->apiClient();
    if (!api) { appendOutput("Error: Not connected"); showPrompt(); return; }

    api->writeMemoryHex(static_cast<uint16_t>(addr),
        QStringLiteral("%1").arg(byte, 2, 16, QChar('0')).toUpper());

    appendOutput(QStringLiteral("Wrote $%1 to $%2")
        .arg(byte, 2, 16, QChar('0')).toUpper()
        .arg(addr, 4, 16, QChar('0')).toUpper());
    showPrompt();
}

void DebugMonitorWidget::commandFill(const QStringList &args)
{
    if (args.size() < 3) { appendOutput("Usage: f <start> <end> <byte>"); showPrompt(); return; }
    int start = parseHex(args[0]);
    int end = parseHex(args[1]);
    int byte = parseHex(args[2]);
    if (start < 0 || end < 0 || byte < 0) { appendOutput("Invalid arguments"); showPrompt(); return; }

    auto *api = m_connection->apiClient();
    if (!api) { appendOutput("Error: Not connected"); showPrompt(); return; }

    int length = end - start + 1;
    QByteArray data(length, static_cast<char>(byte));
    api->writeMemory(static_cast<uint16_t>(start), data);

    appendOutput(QStringLiteral("Filled $%1-$%2 with $%3")
        .arg(start, 4, 16, QChar('0')).toUpper()
        .arg(end, 4, 16, QChar('0')).toUpper()
        .arg(byte, 2, 16, QChar('0')).toUpper());
    showPrompt();
}

void DebugMonitorWidget::commandHunt(const QStringList &args)
{
    if (args.size() < 3) { appendOutput("Usage: h <start> <end> <byte1> [byte2] ..."); showPrompt(); return; }
    int start = parseHex(args[0]);
    int end = parseHex(args[1]);
    if (start < 0 || end < 0) { appendOutput("Invalid range"); showPrompt(); return; }

    QByteArray pattern;
    for (int i = 2; i < args.size(); i++) {
        int b = parseHex(args[i]);
        if (b < 0 || b > 0xFF) { appendOutput("Invalid byte in pattern"); showPrompt(); return; }
        pattern.append(static_cast<char>(b));
    }

    auto *api = m_connection->apiClient();
    if (!api) { appendOutput("Error: Not connected"); showPrompt(); return; }

    int length = end - start + 1;
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(api, &C64ApiClient::memoryDataReceived, this,
        [this, conn, start, pattern](uint16_t addr, const QByteArray &data) {
            if (addr != static_cast<uint16_t>(start)) return;
            QObject::disconnect(*conn);

            QStringList found;
            for (int i = 0; i <= data.size() - pattern.size(); i++) {
                if (data.mid(i, pattern.size()) == pattern)
                    found.append(QStringLiteral("$%1").arg(start + i, 4, 16, QChar('0')).toUpper());
            }

            if (found.isEmpty())
                appendOutput("Pattern not found.");
            else
                appendOutput("Found at: " + found.join(" "));
            showPrompt();
        });

    api->readMemory(static_cast<uint16_t>(start), length);
}

int DebugMonitorWidget::parseHex(const QString &str)
{
    if (str.isEmpty()) return -1;
    QString cleaned = str.startsWith('$') ? str.mid(1) : str;
    bool ok;
    int val = cleaned.toInt(&ok, 16);
    return ok ? val : -1;
}
