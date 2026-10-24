#pragma once
#include <QPixmap>
#include <QSaveFile>

namespace AtomicPixmap {
inline bool save(const QPixmap& pixmap, const QString& path,
                 const char* format, int quality = -1)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (!pixmap.save(&file, format, quality)) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}
}
