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
    , m_organization(QStringLiteral("ServoValve"))
    , m_application(QStringLiteral("ControlPlatform"))
    , m_defaultProjectDir(QDir::homePath())
    , m_activeWorkspaceId(QStringLiteral("programming"))
{
    LOG_DEBUG("SettingsController 已创建");
}

SettingsController::~SettingsController()
{
    LOG_DEBUG("SettingsController 已销毁");
}

void SettingsController::setSettingsStorage(const QString& organization, const QString& application)
{
    m_organization = organization;
    m_application = application;
}

void SettingsController::setWindowGeometry(const QByteArray& geometry)
{
    m_windowGeometry = geometry;
    QSettings settings(m_organization, m_application);
    settings.setValue(QStringLiteral("ui/windowGeometry"), geometry);
}

void SettingsController::setActiveWorkspaceId(const QString& id)
{
    m_activeWorkspaceId = id;
    QSettings settings(m_organization, m_application);
    settings.setValue(QStringLiteral("ui/activeWorkspace"), id);
}

QByteArray SettingsController::workspaceLayoutState(const QString& workspaceId) const
{
    if (m_workspaceStates.contains(workspaceId)) {
        return m_workspaceStates.value(workspaceId);
    }
    QSettings settings(m_organization, m_application);
    return settings.value(QString("ui/workspaces/%1/state").arg(workspaceId)).toByteArray();
}

void SettingsController::setWorkspaceLayoutState(const QString& workspaceId, const QByteArray& state)
{
    m_workspaceStates[workspaceId] = state;
    m_layoutVersion = CURRENT_LAYOUT_VERSION;
    QSettings settings(m_organization, m_application);
    if (state.isEmpty()) {
        settings.remove(QString("ui/workspaces/%1").arg(workspaceId));
    } else {
        settings.setValue(QString("ui/workspaces/%1/state").arg(workspaceId), state);
        settings.setValue(QStringLiteral("ui/layoutVersion"), CURRENT_LAYOUT_VERSION);
    }
}

void SettingsController::clearWorkspaceLayouts()
{
    m_workspaceStates.clear();
    m_layoutVersion = 0;
    QSettings settings(m_organization, m_application);
    settings.remove("ui/workspaces");
    settings.remove("ui/layoutVersion");
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
    QSettings settings(m_organization, m_application);
    
    m_defaultProjectDir = settings.value("defaultProjectDir", QDir::homePath()).toString();
    m_autoScrollLog = settings.value("autoScrollLog", true).toBool();
    m_fontSizeIndex = settings.value("fontSizeIndex", 1).toInt();

    m_layoutVersion = settings.value("ui/layoutVersion", 0).toInt();
    m_windowGeometry = settings.value("ui/windowGeometry").toByteArray();
    m_activeWorkspaceId = settings.value("ui/activeWorkspace", QStringLiteral("programming")).toString();

    m_workspaceStates.clear();
    if (m_layoutVersion == CURRENT_LAYOUT_VERSION) {
        settings.beginGroup("ui/workspaces");
        for (const QString& key : settings.childGroups()) {
            settings.beginGroup(key);
            m_workspaceStates[key] = settings.value("state").toByteArray();
            settings.endGroup();
        }
        settings.endGroup();
    }
    
    LOG_DEBUG(QString("已加载应用设置: 项目目录=%1, 自动滚动=%2, 字体索引=%3, 布局版本=%4")
              .arg(m_defaultProjectDir)
              .arg(m_autoScrollLog)
              .arg(m_fontSizeIndex)
              .arg(m_layoutVersion));
}

void SettingsController::saveSettings()
{
    QSettings settings(m_organization, m_application);
    
    settings.setValue("defaultProjectDir", m_defaultProjectDir);
    settings.setValue("autoScrollLog", m_autoScrollLog);
    settings.setValue("fontSizeIndex", m_fontSizeIndex);

    settings.setValue("ui/layoutVersion", CURRENT_LAYOUT_VERSION);
    if (!m_windowGeometry.isEmpty()) {
        settings.setValue("ui/windowGeometry", m_windowGeometry);
    }
    settings.setValue("ui/activeWorkspace", m_activeWorkspaceId);

    settings.beginGroup("ui/workspaces");
    for (auto it = m_workspaceStates.constBegin(); it != m_workspaceStates.constEnd(); ++it) {
        settings.beginGroup(it.key());
        settings.setValue("state", it.value());
        settings.endGroup();
    }
    settings.endGroup();
    
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
