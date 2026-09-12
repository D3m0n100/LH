#include "ThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QString>

void ThemeManager::applyModernTheme(QApplication* app)
{
    applyTheme(app, ThemeMode::Light);
}

void ThemeManager::applyTheme(QApplication* app, ThemeMode mode)
{
    if (!app) {
        return;
    }

    switch (mode) {
    case ThemeMode::Light:
        applyLightPalette(app);
        app->setStyleSheet(buildLightStyleSheet());
        break;
    case ThemeMode::Dark:
        applyLightPalette(app);
        app->setStyleSheet(buildLightStyleSheet());
        break;
    }
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
    app->setPalette(palette);
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
)");
}
