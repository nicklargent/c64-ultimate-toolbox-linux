#include "ui/BasicEditorWidget.h"
#include "models/BasicTokenizer.h"

#include <QRegularExpression>

BasicHighlighter::BasicHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // Line numbers
    m_lineNumberFormat.setForeground(QColor(100, 150, 200));

    // Strings
    m_stringFormat.setForeground(QColor(200, 120, 60));

    // REM comments
    m_remarkFormat.setForeground(QColor(120, 120, 120));
    m_remarkFormat.setFontItalic(true);

    // Keywords
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(QColor(80, 160, 220));
    keywordFormat.setFontWeight(QFont::Bold);

    for (const auto &kw : BasicTokenizer::keywordSet()) {
        Rule rule;
        rule.pattern = QRegularExpression(
            QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(kw)),
            QRegularExpression::CaseInsensitiveOption);
        rule.format = keywordFormat;
        m_rules.append(rule);
    }

    // Special codes {xxx}
    QTextCharFormat specialFormat;
    specialFormat.setForeground(QColor(180, 100, 180));
    Rule specialRule;
    specialRule.pattern = QRegularExpression(R"(\{[a-z ]+\})");
    specialRule.format = specialFormat;
    m_rules.append(specialRule);
}

void BasicHighlighter::highlightBlock(const QString &text)
{
    // Line numbers at start
    QRegularExpression lineNumRe(R"(^\s*(\d+))");
    auto lineMatch = lineNumRe.match(text);
    if (lineMatch.hasMatch()) {
        setFormat(lineMatch.capturedStart(1), lineMatch.capturedLength(1), m_lineNumberFormat);
    }

    // Check for REM — everything after REM token is a comment
    QRegularExpression remRe(R"(\brem\b.*$)", QRegularExpression::CaseInsensitiveOption);
    auto remMatch = remRe.match(text);
    if (remMatch.hasMatch()) {
        setFormat(remMatch.capturedStart(), remMatch.capturedLength(), m_remarkFormat);
        // Only highlight before REM
        QString beforeRem = text.left(remMatch.capturedStart());
        for (const auto &rule : m_rules) {
            auto it = rule.pattern.globalMatch(beforeRem);
            while (it.hasNext()) {
                auto match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
        return;
    }

    // Apply keyword/special rules
    for (const auto &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Strings
    QRegularExpression stringRe(R"("[^"]*"?)");
    auto strIt = stringRe.globalMatch(text);
    while (strIt.hasNext()) {
        auto match = strIt.next();
        setFormat(match.capturedStart(), match.capturedLength(), m_stringFormat);
    }
}

BasicEditorWidget::BasicEditorWidget(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setFont(QFont("monospace", 12));
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);
    setLineWrapMode(QPlainTextEdit::NoWrap);

    m_highlighter = new BasicHighlighter(document());
}

void BasicEditorWidget::setText(const QString &text)
{
    setPlainText(text);
}

int BasicEditorWidget::lineCount() const
{
    return document()->blockCount();
}
