/**
 * @file SettingsController.cpp
 * @brief 设置控制器实现
 */

#include "SettingsController.h"
#include "SettingsDialog.h"
#include "Common.h"

#include <QSettings>
#include <QDir>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QFont>

// ================= 构造 / 析构 =================

SettingsController::SettingsController(QObject* parent)
    : QObject(parent)
    , m_defaultProjectDir(QDir::homePath())
{
    LOG_DEBUG("SettingsController 已创建");
}

SettingsController::~SettingsController()
{
    LOG_DEBUG("SettingsController 已销毁");
}

// ================= 设置项访问 =================

void SettingsController::setDefaultProjectDir(const QString& dir)
{
    if (m_defaultProjectDir != dir) {
        m_defaultProjectDir = dir;
        emit defaultProjectDirChanged(dir);
    }
}

void SettingsController::setAutoScrollLog(bool enabled)
{
    if (m_autoScrollLog != enabled) {
        m_autoScrollLog = enabled;
        emit autoScrollLogChanged(enabled);
    }
}

void SettingsController::setFontSizeIndex(int index)
{
    index = qBound(0, index, 2);
    if (m_fontSizeIndex != index) {
        m_fontSizeIndex = index;
        emit fontSizeChanged(fontPointSize(index));
    }
}

int SettingsController::fontPointSize(int index)
{
    switch (index) {
        case 0: return FONT_SIZE_SMALL;
        case 2: return FONT_SIZE_LARGE;
        default: return FONT_SIZE_MEDIUM;
    }
}

int SettingsController::currentFontPointSize() const
{
    return fontPointSize(m_fontSizeIndex);
}

// ================= 设置持久化 =================

void SettingsController::loadSettings()
{
    QSettings settings("ServoValve", "ControlPlatform");
    
    m_defaultProjectDir = settings.value("defaultProjectDir", QDir::homePath()).toString();
    m_autoScrollLog = settings.value("autoScrollLog", true).toBool();
    m_fontSizeIndex = settings.value("fontSizeIndex", 1).toInt();
    
    LOG_DEBUG(QString("已加载应用设置: 项目目录=%1, 自动滚动=%2, 字体索引=%3")
              .arg(m_defaultProjectDir)
              .arg(m_autoScrollLog)
              .arg(m_fontSizeIndex));
}

void SettingsController::saveSettings()
{
    QSettings settings("ServoValve", "ControlPlatform");
    
    settings.setValue("defaultProjectDir", m_defaultProjectDir);
    settings.setValue("autoScrollLog", m_autoScrollLog);
    settings.setValue("fontSizeIndex", m_fontSizeIndex);
    
    LOG_DEBUG("已保存应用设置");
}

// ================= 应用设置到控件 =================

void SettingsController::applyFontToEditor(QPlainTextEdit* editor)
{
    if (!editor) {
        return;
    }
    
    QFont font;
    font.setPointSize(currentFontPointSize());
    font.setFamily("Consolas");
    editor->setFont(font);
}

void SettingsController::applyFontToViewer(QTextEdit* viewer)
{
    if (!viewer) {
        return;
    }
    
    QFont font;
    font.setPointSize(currentFontPointSize());
    font.setFamily("Consolas");
    viewer->setFont(font);
}

void SettingsController::applyAllSettings(QPlainTextEdit* editor, QTextEdit* outputViewer)
{
    applyFontToEditor(editor);
    applyFontToViewer(outputViewer);
    emit settingsApplied();
}

// ================= 设置对话框 =================

void SettingsController::openSettingsDialog(QWidget* parent)
{
    SettingsDialog dialog(parent);
    
    // 设置当前值
    dialog.setDefaultProjectDir(m_defaultProjectDir);
    dialog.setAutoScrollLog(m_autoScrollLog);
    dialog.setFontSizeIndex(m_fontSizeIndex);
    
    if (dialog.exec() == QDialog::Accepted) {
        // 获取新值
        setDefaultProjectDir(dialog.defaultProjectDir());
        setAutoScrollLog(dialog.autoScrollLog());
        setFontSizeIndex(dialog.fontSizeIndex());
        
        // 保存设置
        saveSettings();
        
        // 发出字体变化信号，让 MainWindow 应用到控件
        emit fontSizeChanged(currentFontPointSize());
        
        emit logMessage(QString("[%1] 设置已更新")
                        .arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    }
}
