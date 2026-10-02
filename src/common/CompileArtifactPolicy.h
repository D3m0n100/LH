#ifndef LH_COMPILE_ARTIFACT_POLICY_H
#define LH_COMPILE_ARTIFACT_POLICY_H

#include <QByteArray>
#include <QFile>
#include <QString>

namespace CompileArtifactPolicy {
inline QString operationUnavailableMessage()
{
    return QStringLiteral("此编译结果不适用于控制器下载或运行。");
}

inline bool containsMarker(const QByteArray& contents)
{
    return contents.contains("// LH-EXECUTION-UNCONFIRMED: scalar-assignment-v1")
            || contents.contains("// LH-OFFLINE-COMPATIBILITY: legacy-constants-v1");
}

inline bool isFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && containsMarker(file.read(4096));
}
}

#endif
