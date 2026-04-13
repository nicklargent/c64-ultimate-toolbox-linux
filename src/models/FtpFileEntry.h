#pragma once

#include <QString>
#include <QDateTime>

struct FtpFileEntry {
    QString name;
    QString path;
    uint64_t size = 0;
    bool isDirectory = false;
    QDateTime modificationDate;
};
