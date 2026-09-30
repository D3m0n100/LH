#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

namespace LogSafety {
inline bool isSensitiveKey(const QString& key)
{
    QString normalized;
    for (QChar character : key) if (character.isLetterOrNumber()) normalized.append(character.toLower());
    static const QSet<QString> names = {
        "password", "passwd", "pwd", "secret", "token", "accesstoken", "refreshtoken",
        "apikey", "accesskey", "secretkey", "privatekey", "credential", "credentials",
        "authorization", "cookie", "sessioncookie", "connectionstring", "passphrase"
    };
    for (const QString& name : names)
        if (normalized == name || (normalized.size() > name.size() && normalized.endsWith(name))) return true;
    return false;
}

// Free text supports common key=value credentials, Bearer tokens and URI
// passwords. Arbitrary prose cannot be classified; callers must not log secrets.
inline QString redactText(QString text)
{
    static const QRegularExpression credential(
        QStringLiteral(R"((\b[\w-]*(?:password|passwd|pwd|secret|token|api[_-]?key|authorization|cookie|connection[_-]?string|private[_-]?key)\b\s*[:=]\s*)(?:"[^"]*"|'[^']*'|[^\s;,]+))"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression bearer(QStringLiteral(R"((\bBearer\s+)[A-Za-z0-9._~+/-]+=*)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression uri(QStringLiteral(R"((\b[a-z][a-z0-9+.-]*://[^\s:/@]+:)[^\s/@]+(@))"), QRegularExpression::CaseInsensitiveOption);
    text.replace(bearer, QStringLiteral("\\1[REDACTED]"));
    text.replace(credential, QStringLiteral("\\1[REDACTED]"));
    text.replace(uri, QStringLiteral("\\1[REDACTED]\\2"));
    return text;
}

inline QJsonValue redact(const QJsonValue& value)
{
    if (value.isObject()) {
        QJsonObject result;
        const auto object = value.toObject();
        for (auto it = object.begin(); it != object.end(); ++it)
            result.insert(it.key(), isSensitiveKey(it.key()) ? QJsonValue(QStringLiteral("[REDACTED]")) : redact(it.value()));
        return result;
    }
    if (value.isArray()) {
        QJsonArray result;
        for (const auto& item : value.toArray()) result.append(redact(item));
        return result;
    }
    return value.isString() ? QJsonValue(redactText(value.toString())) : value;
}

inline QString escapeLine(const QString& text)
{
    QString result;
    result.reserve(text.size());
    for (const QChar character : text) {
        const ushort code = character.unicode();
        if (character == QLatin1Char('\\')) result += QStringLiteral("\\\\");
        else if (character == QLatin1Char('\n')) result += QStringLiteral("\\n");
        else if (character == QLatin1Char('\r')) result += QStringLiteral("\\r");
        else if (character == QLatin1Char('\t')) result += QStringLiteral("\\t");
        else if (code < 32 || (code >= 127 && code <= 159) || code == 0x2028 || code == 0x2029)
            result += QStringLiteral("\\u%1").arg(code, 4, 16, QLatin1Char('0'));
        else result += character;
    }
    return result;
}
}
