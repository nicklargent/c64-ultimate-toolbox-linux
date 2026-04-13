#pragma once

#include <QByteArray>
#include <QString>
#include <QList>
#include <QPair>
#include <QSet>

struct TokenizeResult {
    QByteArray programData;
    uint16_t endAddress = 0;
};

class BasicTokenizer {
public:
    static TokenizeResult tokenize(const QString &program, QString *errorOut = nullptr);

    static const QList<QPair<QString, uint8_t>> &tokens();
    static const QList<QPair<QString, uint8_t>> &specialCodes();
    static const QSet<QString> &keywordSet();

private:
    static QPair<uint8_t, QString> scanToken(const QString &s, bool doTokenize, QString *errorOut);
    static uint8_t asciiToPetscii(QChar ch);
};
