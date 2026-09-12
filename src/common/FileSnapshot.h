#ifndef LH_FILE_SNAPSHOT_H
#define LH_FILE_SNAPSHOT_H
#include <QIODevice>
#include <QByteArray>
#include <utility>

namespace FileSnapshot {
// Publish a snapshot only after reaching EOF without a read error.
inline bool readComplete(QIODevice& device, QByteArray* result)
{
    QByteArray bytes;
    char chunk[64 * 1024];
    for (;;) {
        const qint64 count = device.read(chunk, sizeof(chunk));
        if (count < 0) return false;
        if (count == 0) {
            if (!device.atEnd()) return false;
            *result = std::move(bytes);
            return true;
        }
        bytes.append(chunk, static_cast<int>(count));
    }
}
}
#endif
