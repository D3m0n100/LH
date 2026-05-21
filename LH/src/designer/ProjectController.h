/**
 * @file ProjectController.h
 * @brief 项目控制器 - 负责项目生命周期管理
 *
 * ============== 职责说明 ==============
 *
 * ProjectController 封装了项目相关的所有业务逻辑，包括：
 * - 项目的新建、打开、保存、关闭
 * - project_config.json 的读写
 * - ProjectRuntimeConfig 的维护
 * - 最近项目列表的管理
 * - 组态合法性校验
 * - DSL 映射的同步
 *
 * ============== 与 MainWindow 的关系 ==============
 *
 * - ProjectController 不持有任何 UI 控件
 * - 通过信号通知 MainWindow 项目状态变化
 * - MainWindow 负责将菜单/按钮动作转发给 ProjectController
 * - MainWindow 负责根据信号更新 UI 显示
 *
 * ============== 使用示例 ==============
 *
 * @code
 * // 在 MainWindow 中
 * m_projectController = new ProjectController(this);
 * connect(m_actNew, &QAction::triggered, 
 *         m_projectController, &ProjectController::createNewProject);
 * connect(m_projectController, &ProjectController::projectOpened,
 *         this, &MainWindow::onProjectOpened);
 * @endcode
 */

#ifndef PROJECTCONTROLLER_H
#define PROJECTCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>

#include "common/ConfigTypes.h"
#include "DslCompletionEngine.h"  // 需要 DslInsertRecord 的完整定义

// 前向声明
class DslScriptEditor;

/**
 * @brief 项目控制器类
 *
 * 封装项目生命周期管理的所有业务逻辑，不直接操作 UI 控件。
 */
class ProjectController : public QObject
{
    Q_OBJECT

public:
    explicit ProjectController(QObject* parent = nullptr);
    ~ProjectController() override;

    // ===== 项目状态查询 =====
    
    /// 获取当前项目路径
    QString currentProjectPath() const { return m_currentProject; }
    
    /// 获取当前脚本文件路径
    QString currentScriptFile() const { return m_currentScriptFile; }

    /// 设置当前脚本文件路径
    void setCurrentScriptFile(const QString& scriptFile);
    QString currentMainScriptFile() const { return m_runtimeConfig.mainScriptPath; }
    QStringList projectScriptFiles() const { return m_runtimeConfig.scriptFiles; }
    
    /// 当前是否有打开的项目
    bool hasOpenProject() const { return !m_currentProject.isEmpty(); }
    
    /// 项目是否已修改
    bool isModified() const { return m_modified; }
    
    /// 设置修改状态
    void setModified(bool modified);
    
    /// 获取运行时配置（只读）
    const ProjectRuntimeConfig& runtimeConfig() const { return m_runtimeConfig; }
    
    /// 获取运行时配置（可修改）
    ProjectRuntimeConfig& runtimeConfig() { return m_runtimeConfig; }

    // ===== 最近项目列表 =====
    
    /// 获取最近项目列表
    QStringList recentProjects() const { return m_recentProjects; }
    
    /// 加载最近项目列表
    void loadRecentProjects();
    
    /// 保存最近项目列表
    void saveRecentProjects();

    // ===== 组态合法性校验 =====
    
    /// 校验项目配置
    bool validateConfiguration(QStringList& errors) const;

    // ===== DSL 映射同步 =====
    
    /// 设置 DSL 编辑器引用（用于同步映射）
    void setDslEditor(DslScriptEditor* editor);
    
    /// 从编辑器同步 DSL 映射
    void syncDslMappingsFromEditor();
    
    /// 同步 DSL 映射到编辑器
    void syncDslMappingsToEditor();

    // ===== 设置默认项目目录 =====
    
    void setDefaultProjectDir(const QString& dir) { m_defaultProjectDir = dir; }
    QString defaultProjectDir() const { return m_defaultProjectDir; }

public slots:
    // ===== 项目操作槽函数 =====
    
    /// 创建新项目（会弹出对话框）
    void createNewProject();
    
    /// 打开项目（会弹出对话框）
    void openProject();
    
    /// 打开指定路径的项目
    bool openProjectFromPath(const QString& projectPath);
    
    /// 保存当前项目
    bool saveProject();
    
    /// 关闭当前项目
    void closeProject();
    
    /// 从最近列表打开项目
    void openRecentProject(const QString& path);

signals:
    // ===== 项目状态信号 =====
    
    /// 项目已创建
    void projectCreated(const QString& projectPath, const QString& projectName);
    
    /// 项目已打开
    void projectOpened(const ProjectRuntimeConfig& config);
    
    /// 项目已保存
    void projectSaved();
    
    /// 项目已关闭
    void projectClosed();
    
    /// 项目修改状态变化
    void modifiedChanged(bool modified);
    
    /// 最近项目列表已更新
    void recentProjectsChanged(const QStringList& projects);
    
    /// 日志消息
    void logMessage(const QString& message);
    
    /// 错误消息（需要弹窗）
    void errorOccurred(const QString& title, const QString& message);
    
    /// 警告消息（需要弹窗）
    void warningOccurred(const QString& title, const QString& message);
    
    /// 需要用户输入项目名称
    void projectNameRequired(QString& projectName, bool& accepted);
    
    /// 需要用户选择目录
    void directorySelectionRequired(const QString& title, const QString& defaultDir, 
                                     QString& selectedDir, bool& accepted);
    
    /// 需要用户确认（保存/放弃/取消）
    void saveConfirmationRequired(bool& shouldSave, bool& cancelled);
    
    /// 需要加载脚本到编辑器
    void scriptLoadRequired(const QString& scriptPath, const QString& content);
    
    /// 需要清空编辑器
    void editorClearRequired();
    
    /// 校验错误
    void validationFailed(const QStringList& errors);

private:
    // ===== 内部方法 =====
    
    /// 加载项目配置
    bool loadProjectConfig(const QString& projectDir);
    
    /// 保存项目配置
    bool saveProjectConfig(const QString& projectDir);
    void syncScriptConfigFields();
    
    /// 添加到最近项目列表
    void addToRecentProjects(const QString& path);
    
    /// 创建映射条目
    DslMappingEntry createMappingFromInsertRecord(const DslInsertRecord& record);
    
    /// 校验必填字段
    bool checkRequiredFields(const ProjectRuntimeConfig& cfg, QStringList& errors) const;
    
    /// 校验重复绑定
    bool checkDuplicateChannelBindings(const ProjectRuntimeConfig& cfg, QStringList& errors) const;
    
    /// 校验硬件资源限制
    bool checkHardwareResourceLimits(const ProjectRuntimeConfig& cfg, QStringList& errors) const;
    
    /// 生成时间戳日志消息
    QString timestampedMessage(const QString& msg) const;

private:
    // ===== 项目状态 =====
    QString m_currentProject;           ///< 当前项目路径
    QString m_currentScriptFile;        ///< 当前脚本文件路径
    bool m_modified = false;            ///< 是否已修改
    ProjectRuntimeConfig m_runtimeConfig; ///< 运行时配置
    
    // ===== 最近项目 =====
    QStringList m_recentProjects;       ///< 最近项目列表
    
    // ===== 设置 =====
    QString m_defaultProjectDir;        ///< 默认项目目录
    
    // ===== DSL 编辑器引用 =====
    DslScriptEditor* m_dslEditor = nullptr;
    
    // ===== 常量 =====
    static const int MAX_RECENT_PROJECTS = 5;
    static const int MAX_ANALOG_CHANNELS = 16;
    static const int MAX_DIGITAL_CHANNELS = 32;
    static const int MAX_MONITOR_CHANNELS = 64;
    static const int MIN_SAMPLE_PERIOD_MS = 1;
    static const int MAX_SAMPLE_PERIOD_MS = 10000;
};

#endif // PROJECTCONTROLLER_H
