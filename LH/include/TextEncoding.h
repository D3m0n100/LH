#ifndef TEXTENCODING_H
#define TEXTENCODING_H

#include <QByteArray>
#include <QString>
#include <QTextCodec>
#include <QTextStream>

namespace TextEncoding {

inline QString decodeUtf8WithLocalFallback(const QByteArray& data)
{
    QTextCodec* utf8 = QTextCodec::codecForName("UTF-8");
    QTextCodec::ConverterState state;
    const QString text = utf8->toUnicode(data.constData(), data.size(), &state);
    if (state.invalidChars == 0) {
        return text;
    }
    return QString::fromLocal8Bit(data);
}

inline void setUtf8(QTextStream& stream)
{
    stream.setCodec("UTF-8");
}

} // namespace TextEncoding

#endif // TEXTENCODING_H
