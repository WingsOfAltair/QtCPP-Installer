#pragma once
#include <QString>

inline QString humanSize(qint64 bytes) {
    double size = bytes;
    QString unit = "B";
    if (size > 1024) { size /= 1024; unit = "KB"; }
    if (size > 1024) { size /= 1024; unit = "MB"; }
    if (size > 1024) { size /= 1024; unit = "GB"; }
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(unit);
}
