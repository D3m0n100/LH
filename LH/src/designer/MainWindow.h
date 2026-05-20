/**
 * @file MainWindow.h
 * @brief 主窗口类定义
 *
 * ============== 架构设计说明 ==============
 *
 * MainWindow 的主要职责是 **UI 组合和高层协调**，具体模块的业务逻辑
 * 由对应的"控制器类"负责，以降低 MainWindow 的复杂度：
 *
 * - **ProjectController**: 负责项目生命周期管理（新建/打开/关闭/保存），
 *   包括 project_config.json 的读写、最近项目列表、组态合法性校验等。
 *
 * - **BuildController**: 负责 DSL 编译相关逻辑，包括触发编译、管理编译
 *   状态、调用 DSLCompilerInterface、处理编译结果等。
 *
 * - **SettingsController**: 负责应用设置的读取/保存/应用，包括默认项目
 *   路径、日志自动滚动、字体大小等。
 *
 * - **OutputPaneController**: 负责输出窗口（日志面板）的所有逻辑。
 *
 * - **MonitorController**: 负责监控相关逻辑。
 *
 * - **DslScriptEditor**: 封装 DSL 脚本编辑器 UI 和拖拽组态逻辑。
 *
 * 这种结构的优势：
 * 1. 职责分离，便于单独测试各模块
 * 2. 降低 MainWindow 的代码复杂度（从 ~1700 行降至 ~900 行）
 * 3. 后续扩展功能时无需修改 MainWindow 头文件
 * 4. 便于团队协作，不同成员可以并行开发不同模块
 *
 * ============== 功能增强历史 ==============
 *
 * - 编辑菜单（撤销/重做/剪切/复制/粘贴）
 * - 视图菜单（Dock 窗口显示控制）
 * - "最近打开"子菜单
 * - 设置对话框支持
 * - 输出窗口右键菜单
 * - 编辑器光标位置状态栏显示
 * - 函数列表显示/隐藏控制
 * - 编辑器修改状态标记
 * - 导出监控图像动作
 * - DSL 脚本编辑器菜单/工具栏入口
 * - 拖拽式组态数据结构（DslMappingEntry）
 * - 组态合法性校验
 * - 与监控模块的配置同步
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QMdiArea;
class QTextEdit;
class QLabel;
class QProgressBar;
class QCloseEvent;
class QAction;
class QDockWidget;
class QToolBar;
class QMenu;
class QMdiSubWindow;
class QTabWidget;
QT_END_NAMESPACE

#include "DslScriptEditor.h"
#include "common/ConfigTypes.h"
#include "BuildController.h"  // 需要 BuildType 枚举

// 前向声明 - 控制器类
class ProjectController;
class SettingsController;

// 前向声明 - 其他类
class MonitorWidget;
class DownloadDockWidget;
class ICommInterface;
class SampleDataProvider;
class OutputPaneController;
class MonitorController;
class ProgramBlocksWidget;
class ProjectExplorerWidget;
class GlobalStatusBar;
class InspectorPanel;
class ParameterTuningWindow;
class ProblemsPanel;

/**
 * @brief 主窗口类
 *
 * MainWindow 是应用程序的主界面，负责：
 * - 菜单栏、工具栏、状态栏的创建和管理
 * - MDI 工作区的管理
 * - Dock 窗口的布局
 * - 各模块控制器的协调
 * - 将用户操作转发给相应的控制器
 * - 根据控制器的信号更新 UI 显示
 *
 * @note MainWindow 不直接处理细节业务逻辑，而是委托给相应的控制器类
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// 更新通信连接状态显示
    void updateConnectionStatus(bool connected);
    
    /// 获取当前运行时配置
    const ProjectRuntimeConfig& runtimeConfig() const;
    
    /// 应用运行时配置到监控系统
    bool applyRuntimeConfigToMonitor();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // ===== 工程相关（转发给 ProjectController）=====
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onSaveAll();
    void onCloseProject();
    void onRecentProjectTriggered();

    // ===== 编辑相关 =====
    void onUndo();
    void onRedo();
    void onCut();
    void onCopy();
    void onPaste();
    void onSelectAll();
    void onFind();

    // ===== 视图相关 =====
    void onToggleOutputDock(bool checked);
    void onToggleMonitorDock(bool checked);
    void onToggleDownloadDock(bool checked);
    void onToggleFunctionList(bool visible);
    void onToggleExplorerDock(bool checked);
    void onToggleDslEditor(bool checked);
    void onResetLayout();
    void onOpenDownloadWindow();
    void onClearOutput();

    // ===== 设置相关（转发给 SettingsController）=====
    void onOpenSettings();

    // ===== 编译相关（转发给 BuildController）=====
    void onCompileConfiguration();
    void onCompileParameters();
    void onCompileCommunication();
    void onCompileAndRunProject();

    // ===== 运行控制 =====
    void onRunProject();
    void onStopProject();

    // ===== 监控相关 =====
    void onOpenMonitor();
    void onOpenParameterTuningWindow();
    void onStartMonitoring();
    void onStopMonitoring();
    void onExportMonitorData();
    void onExportMonitorImage();

    // ===== 其他 =====
    void onOpenLogDirectory();
    void onOpenDiagnosisWizard();
    void onEditParameterRequested(const QString& parameterName);
    void onApplyParametersRequested();
    void onAbout();

    // ===== 内部辅助槽函数 =====
    void onFocusChanged(QWidget* old, QWidget* now);

    // ===== 输出窗口右键菜单槽函数 =====
    void onOutputContextMenu(const QPoint& pos);
    void onCopySelectedOutput();
    void onCopyAllOutput();
    void onSaveOutputToFile();

    // ===== 编辑器相关槽函数 =====
    void onEditorCursorPositionChanged(int line, int column, int totalLines);
    void onEditorModified(bool modified);
    void onDslEditorSubWindowDestroyed();
    void onSnippetInserted(const DslInsertRecord& record);
    void onDropError(const QString& errorMessage);

    // ===== 控制器信号处理槽函数 =====
    
    // ProjectController 信号处理
    void onProjectCreated(const QString& projectPath, const QString& projectName);
    void onProjectOpened(const ProjectRuntimeConfig& config);
    void onProjectSaved();
    void onProjectClosed();
    void onProjectModifiedChanged(bool modified);
    void onRecentProjectsChanged(const QStringList& projects);
    void onProjectNameRequired(QString& projectName, bool& accepted);
    void onDirectorySelectionRequired(const QString& title, const QString& defaultDir,
                                       QString& selectedDir, bool& accepted);
    void onSaveConfirmationRequired(bool& shouldSave, bool& cancelled);
    void onScriptLoadRequired(const QString& scriptPath, const QString& content);
    void onEditorClearRequired();
    void onValidationFailed(const QStringList& errors);
    
    // BuildController 信号处理
    void onCompileStarted(BuildType type);
    void onCompileSucceeded(BuildType type);
    void onCompileFailed(BuildType type, const QString& errorMessage);
    void onBuildBusyChanged(bool busy);
    void onBuildSaveRequired();
    void onBuildValidationRequired(BuildType type, QStringList& errors, bool& valid);
    
    // SettingsController 信号处理
    void onFontSizeChanged(int pointSize);
    
    // 通用日志消息处理
    void onLogMessage(const QString& message);
    void onErrorOccurred(const QString& title, const QString& message);
    void onWarningOccurred(const QString& title, const QString& message);

    // ===== 监控告警（阈值超限） =====
    void onMonitorThresholdExceeded(const QString& channelName, double value, double thresholdValue);
    void onExplorerFileOpenRequested(const QString& filePath);
    void onLocateCurrentFileInExplorer();

private:
    // ===== UI 构建 =====
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void createDockWidgets();
    void createControllers();
    void initConnections();
    void connectControllerSignals();
    void createWorkspaceTabs();
    void createInspectorDock();
    void createParameterTuningWindow();
    
    void updateStatusBar(const QString& message);
    void updateEditActions();
    QWidget* getCurrentTextEditor() const;
    
    void setCompileActionsEnabled(bool enabled);
    void updateRecentProjectsMenu();
    void appendOutput(const QString& message);
    void updateWindowTitle();
    void updateEditorSubWindowTitle();
    void refreshInspectorPanel();
    void refreshInspectorPanel(InspectorPanel* panel);
    void refreshInspectorPanel(ParameterTuningWindow* window);
    void addProblem(const QString& severity, const QString& source, const QString& message);
    
    void createDslEditorSubWindow();
    void connectDslEditorSignals();
    
    void applyFontSize(int pointSize);
    void showValidationErrors(const QStringList& errors);

    QString resolveExplorerRootPath() const;
    void refreshExplorerRoot();
    void openFileFromExplorer(const QString& filePath);
    bool loadTextFileToEditor(const QString& filePath);
    void openAuxiliaryTextFileInMdi(const QString& filePath);
    bool isSupportedTextFile(const QString& filePath) const;

    // ===== Demo Mode（演示数据模式） =====
    void startDemoModeIfNeeded(const QString& reason);
    void stopDemoMode(const QString& reason);

private:
    // ================= 控制器 =================
    ProjectController*  m_projectController;    ///< 项目控制器
    BuildController*    m_buildController;      ///< 编译控制器
    SettingsController* m_settingsController;   ///< 设置控制器

    // ================= 中心工作区 =================
    QMdiArea*     m_mdiArea;
    DslScriptEditor* m_dslEditor;
    QMdiSubWindow* m_editorSubWindow;

    // ================= Dock 窗口 =================
    QDockWidget*  m_explorerDock = nullptr;
    QDockWidget*  m_logDock;
    QDockWidget*  m_monitorDock;
    QDockWidget*  m_downloadDock;
    QTextEdit*    m_outputViewer;
    QTabWidget*   m_bottomPanels = nullptr;
    QTabWidget*   m_workspaceTabs = nullptr;
    QWidget*      m_workspaceDslPage = nullptr;
    QWidget*      m_workspaceBuildPage = nullptr;
    QWidget*      m_workspaceMonitorPage = nullptr;
    QWidget*      m_workspaceDisplayPage = nullptr;
    DownloadDockWidget* m_downloadWidget = nullptr;
    ProgramBlocksWidget* m_displayBlocksWidget = nullptr;
    QDockWidget*  m_inspectorDock = nullptr;
    InspectorPanel* m_inspectorPanel = nullptr;
    ParameterTuningWindow* m_parameterTuningWindow = nullptr;
    ProblemsPanel* m_problemsPanel = nullptr;
    GlobalStatusBar* m_globalStatusBar = nullptr;

    ProjectExplorerWidget* m_projectExplorerWidget = nullptr;

    // ================= 状态栏组件 =================
    QLabel*       m_statusLabel;
    QLabel*       m_connectionStatusLabel;
    QLabel*       m_editorPositionLabel;
    QProgressBar* m_progressBar;

    // ================= 工具栏 =================
    QToolBar*     m_fileToolBar;
    QToolBar*     m_runToolBar;
    QToolBar*     m_overviewToolBar = nullptr;

    // ================= 文件菜单 =================
    QMenu*        m_recentProjectsMenu;

    // ================= 编辑菜单 QAction =================
    QAction*      m_actUndo;
    QAction*      m_actRedo;
    QAction*      m_actCut;
    QAction*      m_actCopy;
    QAction*      m_actPaste;
    QAction*      m_actSelectAll;
    QAction*      m_actFind;

    // ================= 视图菜单 QAction =================
    QAction*      m_actToggleOutputDock;
    QAction*      m_actToggleMonitorDock;
    QAction*      m_actToggleDownloadDock;
    QAction*      m_actToggleExplorerDock = nullptr;
    QAction*      m_actToggleFunctionList;
    QAction*      m_actToggleDslEditor;
    QAction*      m_actResetLayout;
    QAction*      m_actClearOutput;
    QAction*      m_actOpenDisplayWorkspace = nullptr;

    // ================= 编译 / 运行相关动作 =================
    QAction*      m_actCompileConfig;
    QAction*      m_actCompileParameters;
    QAction*      m_actCompileCommunication;
    QAction*      m_actCompileAndRunProject;
    QAction*      m_actRunProject;
    QAction*      m_actStopProject;
    QAction*      m_actOpenMonitor;

    QAction*      m_actOpenDownload;
    // ================= 监控菜单 QAction =================
    QAction*      m_actStartMonitor;
    QAction*      m_actStopMonitor;
    QAction*      m_actExportMonitorData;
    QAction*      m_actExportMonitorImage;

    // ================= 文件工具栏 QAction =================
    QAction*      m_actNew;
    QAction*      m_actOpen;
    QAction*      m_actSave;
    QAction*      m_actCompile;
    QAction*      m_actOpenDslEditorToolBar;

    // ================= 设置相关 QAction =================
    QAction*      m_actSettings;

    // ================= 模块封装 =================
    MonitorWidget* m_monitorWidget;
    SampleDataProvider* m_sampleDataProvider;
    ICommInterface* m_commInterface = nullptr;
    bool m_projectRunning = false;
    bool m_demoModeActive = false;
    bool m_pendingRunAfterCompile = false;
    bool m_lastBuildSaveSucceeded = true;
    bool m_skipNextBuildSave = false;
    int m_alarmCount = 0;
    QStringList m_runtimeProviderIds;
};

#endif // MAINWINDOW_H
