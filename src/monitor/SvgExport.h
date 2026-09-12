#ifndef LH_SVG_EXPORT_H
#define LH_SVG_EXPORT_H
#include <QWidget>
#include <QPainter>
#include <QSvgGenerator>
#include <QSaveFile>
#include <functional>

namespace SvgExport {
inline bool write(QWidget* widget, const QString& path,
                  const std::function<bool(QSaveFile&)>& commit = {})
{
    if (!widget) return false;
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QSvgGenerator generator;
    generator.setOutputDevice(&file);
    generator.setSize(widget->size());
    generator.setViewBox(widget->rect());
    QPainter painter;
    if (!painter.begin(&generator)) return false;
    widget->render(&painter);
    if (!painter.end() || file.error() != QFileDevice::NoError || file.pos() <= 0) return false;
    return commit ? commit(file) : file.commit();
}
}
#endif
