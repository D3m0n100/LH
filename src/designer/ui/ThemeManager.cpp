#include "ThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QString>
#include <QHash>
#include <QRegularExpression>
#include <QEvent>
#include <QPointer>
#include <QTimer>
#include <QWidget>

namespace {
QString darkStyle(QString css)
{
    static const QHash<QString, QString> colors = {
        {"#f3f3f3", "#20252b"}, {"#ffffff", "#171c22"}, {"#f8f8f8", "#262d35"},
        {"#fafafa", "#202730"}, {"#ececec", "#262d35"}, {"#f6f8fa", "#252b33"},
        {"#d0d7de", "#46515e"}, {"#c8c8c8", "#46515e"}, {"#cecece", "#46515e"},
        {"#dddddd", "#3b4652"}, {"#e5e5e5", "#35414e"}, {"#a0a0a0", "#637386"},
        {"#1f1f1f", "#e6edf3"}, {"#24292f", "#e6edf3"}, {"#3b3b3b", "#c9d4df"},
        {"#57606a", "#b7c4d2"}, {"#5f6a72", "#b7c4d2"}, {"#9a9a9a", "#8b99a9"},
        {"#e8f3ff", "#253a50"}, {"#cce7ff", "#294b6a"}, {"#dbeeff", "#294b6a"},
        {"#99c9ef", "#4882b3"}, {"#005a9e", "#7bc3ff"}, {"#16825d", "#73d6a3"},
        {"#8a6d00", "#f2cf66"}, {"#c42b1c", "#ffaaa5"}, {"#116329", "#91e0b1"},
        {"#dafbe1", "#193b2a"}, {"#aceebb", "#34764c"}, {"#7d4e00", "#f2cf66"},
        {"#fff8c5", "#3b3320"}, {"#f0d98c", "#856b35"}, {"#cf222e", "#ffaaa5"},
        {"#ffebe9", "#44272b"}, {"#ff8182", "#9c4d59"}, {"#8c959f", "#a8b6c5"},
        {"#eaeef2", "#303944"}, {"#afb8c1", "#607082"}, {"#d8dee4", "#394653"},
        {"#ddf4ff", "#253a50"}
    };
    const QRegularExpression colorPattern(QStringLiteral("#[0-9a-fA-F]{6}"));
    auto matches = colorPattern.globalMatch(css);
    QString result;
    int previous = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += css.mid(previous, match.capturedStart() - previous);
        const QString color = match.captured().toLower();
        const int propertyStart = qMax(css.lastIndexOf(';', match.capturedStart()),
                                      css.lastIndexOf('{', match.capturedStart())) + 1;
        const auto property = css.mid(propertyStart, match.capturedStart() - propertyStart).trimmed();
        const bool foreground = property == "color:" || property == "selection-color:";
        result += color == "#ffffff" && foreground ? QStringLiteral("#f0f3f6")
            : color == "#0969da" && foreground ? QStringLiteral("#7bc3ff") : colors.value(color, color);
        previous = match.capturedEnd();
    }
    return result + css.mid(previous);
}

// Inline styles otherwise override the application theme. Keep their original CSS
// so both live changes and Light/Dark round trips preserve the widget's semantics.
class InlineThemeStyles final : public QObject {
public:
    explicit InlineThemeStyles(QApplication* app) : QObject(app) { app->installEventFilter(this); }
    void refresh() { for (auto* widget : QApplication::allWidgets()) restyle(widget); }
protected:
    bool eventFilter(QObject* object, QEvent* event) override {
        if (!m_applying && (event->type() == QEvent::Polish || event->type() == QEvent::StyleChange)) {
            if (auto* widget = qobject_cast<QWidget*>(object)) {
                remember(widget);
                if (!widget->property("_lh_styleQueued").toBool()) {
                    widget->setProperty("_lh_styleQueued", true);
                    const QPointer<QWidget> alive(widget);
                    QTimer::singleShot(0, widget, [this, alive] {
                        if (alive) { alive->setProperty("_lh_styleQueued", false); restyle(alive); }
                    });
                }
            }
        }
        return false;
    }
private:
    void remember(QWidget* widget) {
        if (!widget->property("_lh_styleSource").isValid()
            || widget->styleSheet() != widget->property("_lh_styleApplied").toString())
            widget->setProperty("_lh_styleSource", widget->styleSheet());
    }
    void restyle(QWidget* widget) {
        remember(widget);
        const auto original = widget->property("_lh_styleSource").toString();
        const auto css = qApp->palette().color(QPalette::Window).lightness() < 128 ? darkStyle(original) : original;
        widget->setProperty("_lh_styleApplied", css);
        m_applying = true;
        if (widget->styleSheet() != css) widget->setStyleSheet(css);
        m_applying = false;
    }
    bool m_applying = false;
};
}

void ThemeManager::applyModernTheme(QApplication* app)
{
    applyTheme(app, ThemeMode::Light);
}

void ThemeManager::applyTheme(QApplication* app, ThemeMode mode)
{
    if (!app) {
        return;
    }
    auto* styles = app->findChild<InlineThemeStyles*>(QStringLiteral("LHInlineThemeStyles"), Qt::FindDirectChildrenOnly);
    if (!styles) { styles = new InlineThemeStyles(app); styles->setObjectName(QStringLiteral("LHInlineThemeStyles")); }

    switch (mode) {
    case ThemeMode::Light:
        applyLightPalette(app);
        app->setStyleSheet(buildLightStyleSheet());
        break;
    case ThemeMode::Dark:
        applyDarkPalette(app);
        app->setStyleSheet(buildDarkStyleSheet());
        break;
    }
    styles->refresh();
}

void ThemeManager::applyLightPalette(QApplication* app)
{
    QPalette palette = app->palette();
    palette.setColor(QPalette::Window, QColor("#f3f3f3"));
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::AlternateBase, QColor("#f8f8f8"));
    palette.setColor(QPalette::WindowText, QColor("#1f1f1f"));
    palette.setColor(QPalette::Text, QColor("#1f1f1f"));
    palette.setColor(QPalette::Button, QColor("#f8f8f8"));
    palette.setColor(QPalette::ButtonText, QColor("#1f1f1f"));
    palette.setColor(QPalette::Highlight, QColor("#007acc"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::ToolTipBase, QColor("#fffbe8"));
    palette.setColor(QPalette::ToolTipText, QColor("#1f1f1f"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#767676"));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#767676"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#767676"));
    app->setPalette(palette);
}

void ThemeManager::applyDarkPalette(QApplication* app)
{
    QPalette palette = app->palette();
    for (const auto role : {QPalette::Window, QPalette::Button}) palette.setColor(role, QColor("#20252b"));
    palette.setColor(QPalette::Base, QColor("#171c22"));
    palette.setColor(QPalette::AlternateBase, QColor("#252b33"));
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText,
                           QPalette::ToolTipText}) palette.setColor(role, QColor("#e6edf3"));
    palette.setColor(QPalette::ToolTipBase, QColor("#262d35"));
    palette.setColor(QPalette::Highlight, QColor("#007acc"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    for (const auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, QColor("#8b99a9"));
    app->setPalette(palette);
}

QString ThemeManager::buildDarkStyleSheet()
{
    return darkStyle(buildLightStyleSheet());
}

QString ThemeManager::groupBoxStyleSheet(bool muted)
{
    const auto css = QStringLiteral(R"(
QGroupBox {
    border: 1px solid #d0d7de;
    border-radius: 6px;
    margin-top: %1px;
    padding: 12px 10px 10px 10px;
    background: %2;
    font-weight: 600;
    color: #24292f;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 8px;
    padding: 0 4px;
    background: #ffffff;
    color: #24292f;
}
)").arg(muted ? 12 : 10).arg(muted ? QStringLiteral("#f6f8fa") : QStringLiteral("#ffffff"));
    return css; // The theme manager adapts application and widget-local CSS from this Light source.
}

QString ThemeManager::buildLightStyleSheet()
{
    return QString::fromUtf8(R"(
* {
    font-family: "Microsoft YaHei UI", "Segoe UI", Arial, sans-serif;
    font-size: 12px;
}
QMainWindow {
    background: #f3f3f3;
}
QToolBar {
    spacing: 4px;
    border: none;
    border-bottom: 1px solid #d0d7de;
    background: #f3f3f3;
    padding: 3px 6px;
}
QToolBar::separator {
    width: 1px;
    background: #d0d7de;
    margin: 5px 6px;
}
QWidget#GlobalStatusBar {
    background: transparent;
}
QLabel#GlobalStatusItem {
    color: #3b3b3b;
    background: transparent;
    border-right: 1px solid #d0d7de;
    padding: 3px 10px;
    font-weight: 600;
}
QLabel#GlobalStatusItem[state="active"] {
    color: #005a9e;
}
QLabel#GlobalStatusItem[state="success"] {
    color: #16825d;
}
QLabel#GlobalStatusItem[state="warning"] {
    color: #8a6d00;
}
QLabel#GlobalStatusItem[state="error"] {
    color: #c42b1c;
}
QLabel#GlobalStatusItem[state="muted"] {
    color: #5f6a72;
}
QStatusBar QWidget#GlobalStatusBar {
    background: transparent;
}
QStatusBar QLabel#GlobalStatusItem {
    color: #ffffff;
    background: transparent;
    border-right: 1px solid rgba(255, 255, 255, 0.25);
    padding: 2px 8px;
    font-weight: 600;
}
QStatusBar QLabel#GlobalStatusItem[state="active"] {
    color: #fffb8f;
}
QStatusBar QLabel#GlobalStatusItem[state="success"] {
    color: #dff6dd;
}
QStatusBar QLabel#GlobalStatusItem[state="warning"] {
    color: #ffe066;
}
QStatusBar QLabel#GlobalStatusItem[state="error"] {
    color: #ffcccc;
}
QStatusBar QLabel#GlobalStatusItem[state="muted"] {
    color: #d0e6f8;
}
QStatusBar QLabel#GlobalStatusProblemItem:hover,
QStatusBar QLabel#GlobalStatusAlarmItem:hover {
    background: rgba(255, 255, 255, 0.2);
    border-radius: 3px;
}
QStatusBar QToolButton#GlobalStatusDetailsToggle {
    color: #ffffff;
    background: transparent;
    border: none;
    padding: 2px 6px;
    font-weight: 600;
}
QStatusBar QToolButton#GlobalStatusDetailsToggle:hover {
    background: rgba(255, 255, 255, 0.2);
    border-radius: 3px;
}
QDockWidget::title {
    background: #f3f3f3;
    padding: 5px 8px;
    border-bottom: 1px solid #d0d7de;
    color: #1f1f1f;
    font-weight: 600;
}
QStatusBar {
    background: #007acc;
    color: #ffffff;
    border-top: 1px solid #0065a9;
}
QStatusBar QLabel {
    color: #ffffff;
    padding: 0 6px;
}
QLabel#ConnectionStatusLabel[connected="true"] {
    color: #dff6dd;
    font-weight: 700;
}
QLabel#ConnectionStatusLabel[connected="false"] {
    color: #e8f3ff;
    font-weight: 700;
}
QTabWidget::pane {
    border: 1px solid #d0d7de;
    background: #ffffff;
}
QTabBar::tab {
    background: #ececec;
    border: 1px solid #d0d7de;
    border-bottom: none;
    padding: 6px 12px;
    margin-right: 1px;
    color: #3b3b3b;
}
QTabBar::tab:selected {
    background: #ffffff;
    color: #1f1f1f;
    border-top: 2px solid #007acc;
}
QToolButton, QPushButton {
    background: #f8f8f8;
    color: #1f1f1f;
    border: 1px solid #c8c8c8;
    border-radius: 3px;
    padding: 4px 9px;
    min-height: 22px;
}
QToolButton:hover, QPushButton:hover {
    background: #e8f3ff;
    border-color: #99c9ef;
}
QToolButton:pressed, QPushButton:pressed {
    background: #cce7ff;
    border-color: #007acc;
}
QToolButton:checked, QPushButton:checked {
    background: #dbeeff;
    border-color: #007acc;
}
QPushButton#PrimaryButton {
    background: #007acc;
    color: #ffffff;
    border-color: #007acc;
    font-weight: 600;
}
QPushButton#PrimaryButton:hover {
    background: #006db8;
}
QToolButton:disabled, QPushButton:disabled {
    background: #f3f3f3;
    color: #9a9a9a;
    border-color: #dddddd;
}
QLineEdit, QTextEdit, QPlainTextEdit, QTableWidget, QListWidget, QTreeView, QComboBox, QSpinBox {
    border: 1px solid #cecece;
    border-radius: 3px;
    background: #ffffff;
    color: #1f1f1f;
    selection-background-color: #007acc;
    selection-color: #ffffff;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus {
    border-color: #007acc;
}
QHeaderView::section {
    background: #f3f3f3;
    color: #3b3b3b;
    border: none;
    border-right: 1px solid #d0d7de;
    border-bottom: 1px solid #d0d7de;
    padding: 5px 7px;
    font-weight: 600;
}
QTableWidget {
    gridline-color: #e5e5e5;
    alternate-background-color: #fafafa;
}
QTreeView::item, QListWidget::item {
    min-height: 22px;
    padding: 2px 4px;
}
QTreeView::item:selected, QListWidget::item:selected {
    background: #cce7ff;
    color: #1f1f1f;
}
QTreeView::item:hover, QListWidget::item:hover {
    background: #e8f3ff;
}
QSplitter::handle {
    background: #d0d7de;
}
QSplitter::handle:hover {
    background: #99c9ef;
}
QMenu {
    background: #ffffff;
    border: 1px solid #d0d7de;
}
QMenu::item {
    padding: 5px 24px 5px 24px;
}
QMenu::item:selected {
    background: #e8f3ff;
    color: #1f1f1f;
}
QPushButton:focus, QToolButton:focus {
    border-color: #007acc;
}
QLabel[badgeState="success"] {
    color: #116329;
    background: #dafbe1;
    border: 1px solid #aceebb;
    border-radius: 4px;
    padding: 2px 8px;
    font-weight: bold;
}
QLabel[badgeState="warning"] {
    color: #7d4e00;
    background: #fff8c5;
    border: 1px solid #f0d98c;
    border-radius: 4px;
    padding: 2px 8px;
    font-weight: bold;
}
QLabel[badgeState="error"] {
    color: #cf222e;
    background: #ffebe9;
    border: 1px solid #ff8182;
    border-radius: 4px;
    padding: 2px 8px;
    font-weight: bold;
}
QLabel[badgeState="muted"], QLabel[badgeState="idle"] {
    color: #57606a;
    background: #f6f8fa;
    border: 1px solid #d0d7de;
    border-radius: 4px;
    padding: 2px 8px;
    font-weight: bold;
}
QScrollBar:vertical {
    background: #f3f3f3;
    width: 8px;
    margin: 0px;
}
QScrollBar::handle:vertical {
    background: #c8c8c8;
    min-height: 20px;
    border-radius: 4px;
}
QScrollBar::handle:vertical:hover {
    background: #a0a0a0;
}
QScrollBar:horizontal {
    background: #f3f3f3;
    height: 8px;
    margin: 0px;
}
QScrollBar::handle:horizontal {
    background: #c8c8c8;
    min-width: 20px;
    border-radius: 4px;
}
QScrollBar::handle:horizontal:hover {
    background: #a0a0a0;
}
)") + groupBoxStyleSheet();
}
