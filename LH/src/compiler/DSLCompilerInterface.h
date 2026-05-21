#ifndef DSLCOMPILERINTERFACE_H
#define DSLCOMPILERINTERFACE_H

struct CompileArtifact;
struct CompileResult;

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>
#include "common/ConfigTypes.h"
#include "Common.h"

// DSL 编译接口封装：调用 Python main.py compile
// 本类既提供同步接口（阻塞式），也提供异步接口（基于 QProcess 信号）。
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
    QList<CompileArtifact> artifacts;
    QString stdOut;
    QString stdErr;
    QStringList warnings;
    QStringList errors;
};

class DSLCompilerInterface : public QObject
{
    Q_OBJECT
public:
    explicit DSLCompilerInterface(QObject* parent = nullptr);

    // ================= 同步编译接口 =================
    // 编译一个 DSL 文件（同步阻塞版本）
    // sourceFile: 绝对路径或相对当前工作目录的 .dsl 文件
    // outputDir : 输出目录（不存在则自动创建）
    // projectName: 项目名称，可选，将作为编译器参数传入
    // compilerStdout / compilerStderr: 如非空，则返回编译器的标准输出 / 错误输出
    bool compileDslFile(const QString& sourceFile,
                        const QString& outputDir,
                        const QString& projectName,
                        QString* compilerStdout = nullptr,
                        QString* compilerStderr = nullptr);

    CompileResult compileDslFileWithResult(const QString& sourceFile,
                                           const QString& outputDir,
                                           const QString& projectName);
    CompileResult compileProjectWithResult(const QString& projectPath,
                                           const ProjectRuntimeConfig& config,
                                           const QString& outputDir,
                                           const QString& projectName);

    CompileResult lastCompileResult() const { return m_lastCompileResult; }

    // ================= 异步编译接口 =================
    // 使用 QProcess 的异步模式进行编译，不阻塞调用线程（UI 线程）。
    // 结果通过 compileFinished / compileFailedToStart 信号返回。
    void compileDslFileAsync(const QString& sourceFile,
                             const QString& outputDir,
                             const QString& projectName);
    void compileProjectAsync(const QString& projectPath,
                             const ProjectRuntimeConfig& config,
                             const QString& outputDir,
                             const QString& projectName);

    // 只列出组件信息（可选，用于以后扩展函数列表自动生成）
    bool listComponents(QString* compilerStdout,
                        QString* compilerStderr = nullptr);

    // 取消当前异步编译
    void cancelCurrentCompile();

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

private slots:
    // QProcess 信号中转槽（仅在异步模式下使用）
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessErrorOccurred(QProcess::ProcessError error);
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onCompileTimeout();

private:
    QString resolvePythonInterpreter() const;          // 找到 python 可执行文件
    QString compilerWorkingDir() const;                // third_party/.../compile 目录
    QString compilerEntryScript() const;               // lmc.py 路径
    QString prepareCompilerInput(const QString& sourceFile,
                                 const QString& outputDir,
                                 QString* errorMessage) const;
    CompileResult buildCompileResult(const QString& sourceFile,
                                     const QString& outputDir,
                                     const QString& projectName,
                                     const QString& mainScriptFile,
                                     const QStringList& scriptFiles,
                                     bool success,
                                     const QString& stdOut,
                                     const QString& stdErr) const;

    // 异步编译相关成员
    // 注意：本类预期在 UI 线程中创建并使用，QProcess 也运行在同一线程，
    // 通过 Qt 的信号/槽机制回调到主线程，不涉及显式多线程。
    QProcess* m_process = nullptr;    // 当前正在运行的编译进程（仅供异步接口使用）
    QString   m_asyncStdOut;          // 异步编译累计标准输出
    QString   m_asyncStdErr;          // 异步编译累计标准错误
    QTimer*   m_compileTimeoutTimer = nullptr;
    CompileResult m_lastCompileResult;
};

#endif // DSLCOMPILERINTERFACE_H
