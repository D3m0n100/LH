#pragma once
#include "common/ConfigTypes.h"
#include <QStringList>

namespace PidParameterUtils {
inline bool containsSeparatedToken(const QString& text, const QString& token)
{
    if (text.isEmpty() || token.isEmpty()) {
        return false;
    }

    const QString lower = text.toLower();
    int index = lower.indexOf(token);
    while (index >= 0) {
        const int beforeIndex = index - 1;
        const int afterIndex = index + token.size();
        const bool beforeOk = beforeIndex < 0 || !lower.at(beforeIndex).isLetterOrNumber();
        const bool afterOk = afterIndex >= lower.size() || !lower.at(afterIndex).isLetterOrNumber();
        if (beforeOk && afterOk) {
            return true;
        }
        index = lower.indexOf(token, index + 1);
    }
    return false;
}

inline bool isPidParameter(const ParameterDefinition& parameter)
{
    const QString name = parameter.name.trimmed().toLower();
    const QString dataType = parameter.dataType.trimmed().toLower();
    const QString unit = parameter.unit.trimmed().toLower();
    const QString kind = parameter.metadata.value("kind").toString().trimmed().toLower();
    const QString role = parameter.metadata.value("role").toString().trimmed().toLower();
    const QString category = parameter.metadata.value("category").toString().trimmed().toLower();

    const QString combined = QStringList{ name, dataType, unit, kind, role, category }.join(' ');
    if (combined.contains("pid")) {
        return true;
    }

    if (containsSeparatedToken(combined, "kp") ||
        containsSeparatedToken(combined, "ki") ||
        containsSeparatedToken(combined, "kd")) {
        return true;
    }

    return false;
}

} // namespace PidParameterUtils
