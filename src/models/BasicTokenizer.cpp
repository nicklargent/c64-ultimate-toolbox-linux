#include "models/BasicTokenizer.h"
#include <QtEndian>

const QList<QPair<QString, uint8_t>> &BasicTokenizer::tokens()
{
    static const QList<QPair<QString, uint8_t>> t = {
        {"restore", 140}, {"input#", 132}, {"return", 142},
        {"verify", 149}, {"print#", 152}, {"right$", 201},
        {"input", 133}, {"gosub", 141}, {"print", 153},
        {"close", 160}, {"left$", 200},
        {"next", 130}, {"data", 131}, {"read", 135},
        {"goto", 137}, {"stop", 144}, {"wait", 146},
        {"load", 147}, {"save", 148}, {"poke", 151},
        {"cont", 154}, {"list", 155}, {"open", 159},
        {"tab(", 163}, {"spc(", 166}, {"then", 167},
        {"step", 169}, {"peek", 194}, {"str$", 196},
        {"chr$", 199}, {"mid$", 202},
        {"end", 128}, {"for", 129}, {"dim", 134},
        {"let", 136}, {"run", 138}, {"rem", 143},
        {"def", 150}, {"clr", 156}, {"cmd", 157},
        {"sys", 158}, {"get", 161}, {"new", 162},
        {"not", 168}, {"and", 175}, {"sgn", 180},
        {"int", 181}, {"abs", 182}, {"usr", 183},
        {"fre", 184}, {"pos", 185}, {"sqr", 186},
        {"rnd", 187}, {"log", 188}, {"exp", 189},
        {"cos", 190}, {"sin", 191}, {"tan", 192},
        {"atn", 193}, {"len", 195}, {"val", 197},
        {"asc", 198},
        {"if", 139}, {"on", 145}, {"to", 164},
        {"fn", 165}, {"or", 176}, {"go", 203},
        {"+", 170}, {"-", 171}, {"*", 172},
        {"/", 173}, {"^", 174}, {">", 177},
        {"=", 178}, {"<", 179},
    };
    return t;
}

const QList<QPair<QString, uint8_t>> &BasicTokenizer::specialCodes()
{
    static const QList<QPair<QString, uint8_t>> c = {
        {"{rvs off}", 0x92}, {"{rvs on}", 0x12},
        {"{up}", 0x91}, {"{down}", 0x11},
        {"{left}", 0x9D}, {"{rght}", 0x1D}, {"{right}", 0x1D},
        {"{clr}", 0x93}, {"{clear}", 0x93}, {"{home}", 0x13},
        {"{del}", 0x14}, {"{inst}", 0x94},
        {"{blk}", 0x90}, {"{wht}", 0x05},
        {"{red}", 0x1C}, {"{cyn}", 0x9F},
        {"{pur}", 0x9C}, {"{grn}", 0x1E},
        {"{blu}", 0x1F}, {"{yel}", 0x9E},
        {"{org}", 0x81}, {"{brn}", 0x95},
        {"{lred}", 0x96}, {"{dgry}", 0x97},
        {"{mgry}", 0x98}, {"{lgrn}", 0x99},
        {"{lblu}", 0x9A}, {"{lgry}", 0x9B},
    };
    return c;
}

const QSet<QString> &BasicTokenizer::keywordSet()
{
    static QSet<QString> s;
    if (s.isEmpty()) {
        for (const auto &[kw, _] : tokens()) {
            if (kw.size() > 1)
                s.insert(kw);
        }
    }
    return s;
}

uint8_t BasicTokenizer::asciiToPetscii(QChar ch)
{
    char c = ch.toLatin1();
    if (c <= 0x40 || c == 0x5B || c == 0x5D)
        return static_cast<uint8_t>(c);
    if (c >= 0x61 && c <= 0x7A) // lowercase a-z
        return static_cast<uint8_t>(c - 0x61 + 0x41);
    if (c >= 0x41 && c <= 0x5A) // uppercase A-Z
        return static_cast<uint8_t>(c - 0x41 + 0xC1);
    return static_cast<uint8_t>(c);
}

QPair<uint8_t, QString> BasicTokenizer::scanToken(const QString &s, bool doTokenize, QString *errorOut)
{
    if (doTokenize) {
        for (const auto &[keyword, value] : tokens()) {
            if (s.startsWith(keyword))
                return {value, s.mid(keyword.size())};
        }
    }

    // {special} escape codes
    if (s.startsWith('{')) {
        for (const auto &[code, value] : specialCodes()) {
            if (s.startsWith(code))
                return {value, s.mid(code.size())};
        }
        if (errorOut)
            *errorOut = QStringLiteral("Unknown escape code near: %1").arg(s.left(20));
        return {0, s.mid(1)};
    }

    // Regular character
    return {asciiToPetscii(s[0]), s.mid(1)};
}

TokenizeResult BasicTokenizer::tokenize(const QString &program, QString *errorOut)
{
    QStringList lines;
    for (const auto &line : program.split('\n')) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            lines.append(trimmed.toLower());
    }

    if (lines.isEmpty()) {
        if (errorOut) *errorOut = "No BASIC lines to tokenize";
        return {};
    }

    struct TokenizedLine {
        uint16_t lineNumber;
        QByteArray bytes;
        uint16_t addr;
    };

    uint16_t addr = 0x0801;
    QList<TokenizedLine> tokenizedLines;

    for (const auto &line : lines) {
        // Parse line number
        int numEnd = 0;
        while (numEnd < line.size() && line[numEnd].isDigit())
            numEnd++;

        if (numEnd == 0) {
            if (errorOut) *errorOut = "Invalid or missing line number: " + line;
            return {};
        }

        uint16_t lineNumber = line.left(numEnd).toUShort();
        QString remaining = line.mid(numEnd).trimmed();

        // Tokenize the rest
        QByteArray bytes;
        bool inQuotes = false;
        bool inRemark = false;

        while (!remaining.isEmpty()) {
            auto [byte, rest] = scanToken(remaining, !(inQuotes || inRemark), errorOut);
            if (errorOut && !errorOut->isEmpty())
                return {};
            bytes.append(static_cast<char>(byte));
            remaining = rest;

            if (byte == '"')
                inQuotes = !inQuotes;
            if (byte == 143) // REM token
                inRemark = true;
        }

        tokenizedLines.append({lineNumber, bytes, addr});
        addr += static_cast<uint16_t>(bytes.size() + 5);
    }

    // Build binary
    QByteArray data;
    for (int i = 0; i < tokenizedLines.size(); i++) {
        uint16_t nextAddr;
        if (i + 1 < tokenizedLines.size())
            nextAddr = tokenizedLines[i + 1].addr;
        else
            nextAddr = tokenizedLines[i].addr + static_cast<uint16_t>(tokenizedLines[i].bytes.size() + 5);

        // Next line pointer LE
        data.append(static_cast<char>(nextAddr & 0xFF));
        data.append(static_cast<char>(nextAddr >> 8));
        // Line number LE
        data.append(static_cast<char>(tokenizedLines[i].lineNumber & 0xFF));
        data.append(static_cast<char>(tokenizedLines[i].lineNumber >> 8));
        // Tokenized bytes
        data.append(tokenizedLines[i].bytes);
        // Null terminator
        data.append('\0');
    }

    // Program end marker
    data.append('\0');
    data.append('\0');

    TokenizeResult result;
    result.programData = data;
    result.endAddress = addr + 2;
    return result;
}
