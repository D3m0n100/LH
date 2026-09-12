/**
 * @file SettingsController.h
 * @brief 设置控制器 - 负责应用设置管理
 *
 * ============== 职责说明 ==============
 *
 * SettingsController 封装了应用设置相关的所有业务逻辑，包括：
 * - 从 QSettings 读取/保存应用设置
 * - 提供设置项的获取/更新接口
 * - 将设置应用到各个子模块（编辑器、监控窗口等）
 * - 管理设置对话框的显示
 *
 * ============== 与 MainWindow 的关系 ==============
 *
 * - SettingsController 不持有 UI 控件的所有权
 * - 通过 applyTo*() 方法将设置应用到传入的控件
 * - MainWindow 负责将设置菜单动作转发给 SettingsController
 * - 设置变化时通过信号通知 MainWindow
 *
 * ============== 设置项列表 ==============
 *
 * - defaultProjectDir: 默认项目目录
 * - autoScrollLog: 日志自动滚动
 * - fontSizeIndex: 字体大小索引 (0=小, 1=中, 2=大)
 * - theme: 主题 (预留)
 */

#ifndef SETTINGSCONTROLLER_H
#define SETTINGSCONTROLLER_H

#include <QObject>
#include <QString>
#include <QFont>

// 前向声明
class QTextEdit;
class QPlainTextEdit;
class QWidget;

/**
 * @brief 设置控制器类
 *
 * 封装应用设置的读取、保存和应用逻辑。
 */
class SettingsController : public QObject
{
    Q_OBJECT

public:
    explicit SettingsController(QObject* parent = nullptr);
    ~SettingsController() override;

    // ===== 设置项访问 =====
    
    /// 获取默认项目目录
    QString defaultProjectDir() const { return m_defaultProjectDir; }
    
    /// 设置默认项目目录
    void setDefaultProjectDir(const QString& dir);
    
    /// 获取日志自动滚动设置
    bool autoScrollLog() const { return m_autoScrollLog; }
    
    /// 设置日志自动滚动
    void setAutoScrollLog(bool enabled);
    
    /// 获取字体大小索引
    int fontSizeIndex() const { return m_fontSizeIndex; }
    
    /// 设置字体大小索引
    void setFontSizeIndex(int index);
    
    /// 根据索引获取字体磅值
    static int fontPointSize(int index);
    
    /// 获取当前字体磅值
    int currentFontPointSize() const;

    // ===== 设置持久化 =====
    
    /// 从 QSettings 加载设置
    void loadSettings();
    
    /// 保存设置到 QSettings
    void saveSettings();

    // ===== 应用设置到控件 =====
    
    /// 应用字体设置到文本编辑器
    void applyFontToEditor(QPlainTextEdit* editor);
    
    /// 应用字体设置到文本查看器
    void applyFontToViewer(QTextEdit* viewer);
    
    /// 应用所有设置到指定控件
    void applyAllSettings(QPlainTextEdit* editor, QTextEdit* outputViewer);

public slots:
    /// 打开设置对话框
    void openSettingsDialog(QWidget* parent);

signals:
    // ===== 设置变化信号 =====
    
    /// 默认项目目录已变化
    void defaultProjectDirChanged(const QString& dir);
    
    /// 日志自动滚动设置已变化
    void autoScrollLogChanged(bool enabled);
    
    /// 字体大小已变化
    void fontSizeChanged(int pointSize);
    
    /// 所有设置已应用
    void settingsApplied();
    
    /// 日志消息
    void logMessage(const QString& message);

private:
    // ===== 设置项 =====
    QString m_defaultProjectDir;    ///< 默认项目目录
    bool m_autoScrollLog = true;    ///< 日志自动滚动
    int m_fontSizeIndex = 1;        ///< 字体大小索引 (0=小, 1=中, 2=大)
    
    // ===== 常量 =====
    static const int FONT_SIZE_SMALL = 9;
    static const int FONT_SIZE_MEDIUM = 11;
    static const int FONT_SIZE_LARGE = 14;
};

#endif // SETTINGSCONTROLLER_H
