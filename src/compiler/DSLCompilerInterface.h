#ifndef DSLCOMPILERINTERFACE_H
#define DSLCOMPILERINTERFACE_H

struct CompileArtifact;
struct CompileResult;

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>
#include <functional>
#include "common/ConfigTypes.h"
#include "common/RuntimePointTypes.h"
#include "Common.h"

// DSL 编译接口封装：调用 Python main.py compile
// 本类提供基于 QProcess 信号的异步接口。
struct CompileArtifact
{
    QString type;
    QString path;
    QString format;
    QString checksum;
    QVariantMap metadata;
};

struct CompileResult
{
    bool success = false;
    QString projectName;
    QString generationId;
    QVariantMap metadata;
    QList<CompileArtifact> artifacts;
    QString stdOut;
    QString stdErr;
    QStringList warnings;
    QStringList errors;
};

struct CompilerRuntimeSearchContext
{
    QString appDirPath;            // 为空时使用 QCoreApplication::applicationDirPath()
    QString sourceDirOverride;     // 为空时使用编译期 LX_PROJECT_SOURCE_DIR
    enum class ModePolicy {
        Auto,             // 自动检测安装标记及目录布局
        ForceInstalled,   // 强制安装模式：仅使用安装运行时，严禁回退到源码目录
        ForceDeveloper    // 强制开发模式：允许回退到源码目录
    } modePolicy = ModePolicy::Auto;
    bool checkMarkerFile = true;
};

struct CompilerRuntimeResolution
{
    bool success = false;
    bool isInstalledMode = false;
    QString runtimeWorkingDir;
    QString entryScript;
    QString candidateSearched;
    QString errorMessage;
};

namespace CompileArtifactType {
    inline constexpr const char* Download        = "download";
    inline constexpr const char* ParameterData   = "parameter_data";
    inline constexpr const char* CommunicationXml = "communication_xml";
    inline constexpr const char* CommunicationTags = "communication_tags";
    inline constexpr const char* CommunicationAct = "communication_act";
    inline constexpr const char* CommunicationTx = "communication_tx";
    inline constexpr const char* CommunicationRx = "communication_rx";
    inline constexpr const char* CommunicationRt = "communication_rt";
    inline constexpr const char* CommunicationEngineering = "communication_engineering";
    inline constexpr const char* CommunicationComm = "communication_comm";
    inline constexpr const char* CommunicationDebug = "communication_debug";
    inline constexpr const char* ParameterInitials = "parameter_initials";
    inline constexpr const char* ParameterLayout = "parameter_layout";
    inline constexpr const char* CompileReport   = "compile_report";
    inline constexpr const char* RuntimePoints   = "runtime_points";
    inline constexpr const char* RuntimeManifest = "runtime_manifest";
}

class DSLCompilerInterface : public QObject
{
    Q_OBJECT
public:
    explicit DSLCompilerInterface(QObject* parent = nullptr);

    CompileResult lastCompileResult() const { return m_lastCompileResult; }

    // ================= 异步编译接口 =================
    // 使用 QProcess 的异步模式进行编译，不阻塞调用线程（UI 线程）。
    // 结果通过 compileFinished / compileFailedToStart 信号返回。
    void compileDslFileAsync(const QString& sourceFile,
                             const QString& outputDir,
                             const QString& projectName,
                             quint64 operationGeneration = 0);
    void compileProjectAsync(const QString& projectPath,
                             const ProjectRuntimeConfig& config,
                             const QString& outputDir,
                             const QString& projectName,
                             quint64 operationGeneration = 0);

    // 取消当前异步编译
    void cancelCurrentCompile();

    // ================= 运行时搜索与解析接口 (T06) =================
    static CompilerRuntimeResolution resolveCompilerRuntime(
            const CompilerRuntimeSearchContext& context = CompilerRuntimeSearchContext());
    static bool isInstalledLayout(
            const QString& appDirPath = QString(),
            bool checkMarkerFile = true);

    void setRuntimeSearchContext(const CompilerRuntimeSearchContext& context);
    CompilerRuntimeSearchContext runtimeSearchContext() const;

    QString compilerWorkingDir() const;                // third_party/.../compile 目录
    QString compilerEntryScript() const;               // lmc.py 路径

    static QString cachedPythonInterpreter(const QString& workDir = QString());
    static void setCachedPythonInterpreter(const QString& workDir, const QString& python);
    static void clearPythonInterpreterCache();
    static void clearPythonInterpreterCache(const QString& workDir);

signals:
    // 异步编译结束（无论成功 / 失败都会触发）
    // exitCode   : 进程退出码
    // normalExit : true 表示正常退出，false 表示 CrashExit
    // stdOut     : 标准输出
    // stdErr     : 标准错误输出
    void compileFinished(int exitCode,
                         bool normalExit,
                         const QString& stdOut,
                         const QString& stdErr);

    // 进程无法启动等致命错误（在真正编译开始前触发）
    void compileFailedToStart(const QString& errorString);

    void compileFinishedForGeneration(quint64 operationGeneration,
                                      int exitCode,
                                      bool normalExit,
                                      const QString& stdOut,
                                      const QString& stdErr);
    void compileFailedToStartForGeneration(quint64 operationGeneration,
                                            const QString& errorString);

private slots:
    // QProcess 信号中转槽（仅在异步模式下使用）
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessErrorOccurred(QProcess::ProcessError error);
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onCompileTimeout();
    void onPythonProbeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onPythonProbeErrorOccurred(QProcess::ProcessError error);
    void onPythonProbeTimeout();

private:
    quint64 allocateOperationGeneration(quint64 requestedGeneration);
    void emitCompileFailedToStart(quint64 operationGeneration, const QString& errorString);
    void resolvePythonInterpreterAsync(quint64 operationGeneration,
                                       std::function<void(const QString&)> onResolved);
    void startNextPythonProbe();
    void finishPythonProbe(const QString& python = QString());
    void clearPythonProbe();
    void ensurePythonProbeTimeoutTimer();
    QString prepareCompilerInput(const QString& sourceFile,
                                 const QString& outputDir,
                                 QString* errorMessage) const;
    void resetFinishedProcess();
    void ensureCompileTimeoutTimer();
    void startAsyncCompilerProcess(const QString& logPrefix,
                                   const QString& python,
                                   const QStringList& args,
                                   const QString& workDir,
                                   const QString& sourceFile,
                                   const QString& mainScriptFile,
                                   const QStringList& scriptFiles,
                                   const QString& outputDir,
                                   const QString& projectName,
                                   const QString& outputFile,
                                   const QString& compilerInputFile,
                                   const QString& projectPath = QString(),
                                   const QString& generationId = QString(),
                                   quint64 operationGeneration = 0);
    CompileResult buildCompileResult(const QString& sourceFile,
                                     const QString& outputDir,
                                     const QString& projectName,
                                     const QString& mainScriptFile,
                                     const QStringList& scriptFiles,
                                     bool success,
                                     const QString& stdOut,
                                     const QString& stdErr) const;

    CompileResult buildCompileResult(const QString& sourceFile,
                                     const QString& outputDir,
                                     const QString& projectName,
                                     const QString& mainScriptFile,
                                     const QStringList& scriptFiles,
                                     bool success,
                                     const QString& stdOut,
                                     const QString& stdErr,
                                     const ProjectRuntimeConfig& config) const;

    CompileResult buildCompileResult(const QString& sourceFile,
                                     const QString& outputDir,
                                     const QString& projectName,
                                     const QString& mainScriptFile,
                                     const QStringList& scriptFiles,
                                     bool success,
                                     const QString& stdOut,
                                     const QString& stdErr,
                                     const ProjectRuntimeConfig& config,
                                     const QString& projectPath,
                                     const QString& generationId) const;

    CompileArtifact generateRuntimePointsJson(const QString& outputDir,
                                              const ProjectRuntimeConfig& config) const;

    CompileArtifact generateRuntimeManifestJson(const QString& outputDir,
                                                const QString& projectPath,
                                                const QString& projectName,
                                                const QString& mainScriptFile,
                                                const QStringList& scriptFiles,
                                                int pointCount,
                                                int parameterCount) const;

    // 异步编译相关成员
    // 注意：本类预期在 UI 线程中创建并使用，QProcess 也运行在同一线程，
    // 通过 Qt 的信号/槽机制回调到主线程，不涉及显式多线程。
    QProcess* m_process = nullptr;    // 当前正在运行的编译进程（仅供异步接口使用）
    QByteArray m_asyncStdOutBytes;    // 有界标准输出尾部
    QByteArray m_asyncStdErrBytes;    // 有界标准错误尾部
    bool m_asyncStdOutTruncated = false;
    bool m_asyncStdErrTruncated = false;
    QString m_asyncStdOut;
    QString m_asyncStdErr;
    QTimer*   m_compileTimeoutTimer = nullptr;
    CompileResult m_lastCompileResult;
    ProjectRuntimeConfig m_asyncProjectConfig;  // 异步项目编译时暂存配置
    quint64 m_nextOperationGeneration = 0;
    quint64 m_asyncOperationGeneration = 0;

    // 异步 Python/ANTLR 探测与待启动的编译请求均在本对象线程运行。
    QProcess* m_pythonProbeProcess = nullptr;
    QTimer* m_pythonProbeTimeoutTimer = nullptr;
    QStringList m_pythonProbeCandidates;
    QString m_pythonProbeWorkDir;
    QString m_pythonProbeCandidate;
    int m_pythonProbeCandidateIndex = 0;
    bool m_pythonProbeChecksRuntime = false;
    quint64 m_pythonProbeGeneration = 0;
    std::function<void(const QString&)> m_pendingAsyncCompilerStart;

    // 运行时搜索上下文（测试或特殊部署配置）
    CompilerRuntimeSearchContext m_runtimeContext;

    // Python 解释器路径缓存（按工作目录隔离，避免切换运行时后沿用失效缓存）
    static QHash<QString, QString> s_cachedPythonInterpretersByWorkDir;
    static QString s_cachedPythonInterpreter;
};

#endif // DSLCOMPILERINTERFACE_H
