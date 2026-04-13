#pragma once

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

class BasicHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit BasicHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<Rule> m_rules;
    QTextCharFormat m_lineNumberFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_remarkFormat;
};

class BasicEditorWidget : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit BasicEditorWidget(QWidget *parent = nullptr);

    void setText(const QString &text);
    int lineCount() const;

private:
    BasicHighlighter *m_highlighter;
};
