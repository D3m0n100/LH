/**
 * @file BuildController.h
 * @brief 编译控制器 - 负责 DSL 编译流程管理
 *
 * ============== 职责说明 ==============
 *
 * BuildController 封装了 DSL 编译相关的所有业务逻辑，包括：
 * - 触发各类编译（组态、参数、通信）
 * - 管理编译过程中的状态（busy / idle）
 * - 处理编译成功/失败结果
 * - 发出编译日志信息
 *
 * ============== 与 MainWindow 的关系 ==============
 *
 * - BuildController 不持有任何 UI 控件
 * - 通过信号通知 MainWindow 编译状态变化
 * - MainWindow 负责将菜单/按钮动作转发给 BuildController
 * - MainWindow 负责根据信号更新 UI 显示（进度条、按钮状态等）
 *
 * ============== 编译状态机 ==============
 *
 * Idle --> [startCompile] --> Compiling --> [compileFinished] --> Idle
 *                                      \--> [compileFailed]   --> Idle
 *
 * 在 Compiling 状态下，重复调用 startCompile 会被忽略。
 */

#ifndef BUILDCONTROLLER_H
#define BUILDCONTROLLER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include "common/ConfigTypes.h"
#include "compiler/DSLCompilerInterface.h"

/**
 * @brief 编译类型枚举
 */
enum class BuildType {
    Configuration,  ///< 组态编译
    Parameters,     ///< 参数编译
    Communication   ///< 通信编译
};

/**
 * @brief 编译控制器类
 *
 * 封装 DSL 编译流程的所有业务逻辑，不直接操作 UI 控件。
 */
class BuildController : public QObject
{
    Q_OBJECT

public:
    explicit BuildController(QObject* parent = nullptr);
    ~BuildController() override;

    // ===== 状态查询 =====
    
    /// 是否正在编译
    bool isBusy() const { return m_busy; }
    
    /// 获取当前编译类型
    BuildType currentBuildType() const { return m_currentBuildType; }
    CompileResult lastCompileResult() const { return m_lastCompileResult; }

public slots:
    // ===== 编译操作 =====
    
    /// 编译组态
    void compileConfiguration(const QString& projectPath, 
                               const ProjectRuntimeConfig& config);
    
    /// 编译参数
    void compileParameters(const QString& projectPath,
                           const ProjectRuntimeConfig& config);
    
    /// 编译通信
    void compileCommunication(const QString& projectPath,
                              const ProjectRuntimeConfig& config);
    
    /// 取消当前编译（如果支持）
    void cancelCompile();

signals:
    // ===== 编译状态信号 =====
    
    /// 编译开始
    void compileStarted(BuildType type);
    
    /// 编译进度（0-100）
    void compileProgress(int percent);
    
    /// 编译成功完成
    void compileSucceeded(BuildType type);
    
    /// 编译失败
    void compileFailed(BuildType type, const QString& errorMessage);
    
    /// 编译状态变化（busy/idle）
    void busyChanged(bool busy);
    
    /// 日志消息
    void logMessage(const QString& message);
    
    /// 需要先保存项目
    void saveRequired();
    
    /// 需要校验配置
    void validationRequired(BuildType type, QStringList& errors, bool& valid);

private slots:
    // ===== 编译过程回调 =====
    void onDslCompilerFinished(int exitCode, bool normalExit, const QString& stdOut, const QString& stdErr);
    void onDslCompilerFailedToStart(const QString& errorString);

    // 兼容旧版 moc/增量构建生成的槽签名
    void onCompileProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onCompileProcessError(QProcess::ProcessError error);

private:
    // ===== 内部方法 =====
    
    /// 设置忙碌状态
    void setBusy(bool busy);
    
    /// 生成时间戳日志消息
    QString timestampedMessage(const QString& msg) const;

    /// 当前项目主脚本路径（优先配置主入口，回退旧约定 main.lm/main.dsl）
    QString currentDslScriptPath(const ProjectRuntimeConfig& config) const;

    /// 获取编译输出目录
    QString buildOutputDirectory(BuildType type) const;

private:
    // ===== 编译接口 =====
    DSLCompilerInterface* m_dslCompiler = nullptr;
    
    // ===== 状态 =====
    bool m_busy = false;
    BuildType m_currentBuildType = BuildType::Configuration;
    QString m_currentProjectPath;
    CompileResult m_lastCompileResult;
};

#endif // BUILDCONTROLLER_H
