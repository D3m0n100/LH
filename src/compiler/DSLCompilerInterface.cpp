#include "DSLCompilerInterface.h"
#include "DSLCompilerInternal.h"
#include "TextEncoding.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

QString DSLCompilerInterface::s_cachedPythonInterpreter;
QHash<QString, QString> DSLCompilerInterface::s_cachedPythonInterpretersByWorkDir;


DSLCompilerInterface::DSLCompilerInterface(QObject* parent)
    : QObject(parent)
{
}

bool DSLCompilerInterface::isInstalledLayout(const QString& appDirPath, bool checkMarkerFile)
{
    QString dirPath = appDirPath;
    if (dirPath.isEmpty()) {
        dirPath = QCoreApplication::applicationDirPath();
    }
    if (dirPath.isEmpty()) {
        return false;
    }
    const QDir dir(dirPath);

    if (checkMarkerFile) {
        // 检查当前可执行文件目录下的安装标记
        if (QFileInfo::exists(dir.absoluteFilePath(QStringLiteral(".lh_install_marker")))) {
            return true;
        }
        // 检查上级目录（如 bin/../.lh_install_marker）
        if (QFileInfo::exists(dir.absoluteFilePath(QStringLiteral("../.lh_install_marker")))) {
            return true;
        }
    }

    return false;
}

CompilerRuntimeResolution DSLCompilerInterface::resolveCompilerRuntime(
        const CompilerRuntimeSearchContext& context)
{
    CompilerRuntimeResolution result;
    const QString relativePath = QStringLiteral("third_party/custom_dsp_language/compile");

    // 1. 确定应用执行目录
    QString appDirPath = context.appDirPath;
    if (appDirPath.isEmpty()) {
        appDirPath = QCoreApplication::applicationDirPath();
    }
    appDirPath = QDir::cleanPath(appDirPath);

    // 2. 检查环境变量覆盖（LH_RUNTIME_MODE: installed | developer）
    CompilerRuntimeSearchContext::ModePolicy effectivePolicy = context.modePolicy;
    if (effectivePolicy == CompilerRuntimeSearchContext::ModePolicy::Auto) {
        const QString envMode = qEnvironmentVariable("LH_RUNTIME_MODE").trimmed().toLower();
        if (envMode == QStringLiteral("installed")) {
            effectivePolicy = CompilerRuntimeSearchContext::ModePolicy::ForceInstalled;
        } else if (envMode == QStringLiteral("developer") || envMode == QStringLiteral("dev")) {
            effectivePolicy = CompilerRuntimeSearchContext::ModePolicy::ForceDeveloper;
        }
    }

    // 3. 判断是否为安装模式
    bool isInstalled = false;
    if (effectivePolicy == CompilerRuntimeSearchContext::ModePolicy::ForceInstalled) {
        isInstalled = true;
    } else if (effectivePolicy == CompilerRuntimeSearchContext::ModePolicy::ForceDeveloper) {
        isInstalled = false;
    } else {
        isInstalled = isInstalledLayout(appDirPath, context.checkMarkerFile);
    }
    result.isInstalledMode = isInstalled;

    // 4. 构建安装候选路径（基于 appDirPath 与 parent/prefix）
    QStringList installedCandidates;
    if (!appDirPath.isEmpty()) {
        QDir dir(appDirPath);
        // 候选 A: appDirPath/third_party/... (直接位于 prefix 根目录)
        installedCandidates << QDir::cleanPath(dir.absoluteFilePath(relativePath));
        // 候选 B: appDirPath/../third_party/... (位于 bin/ 目录下)
        if (dir.cdUp()) {
            installedCandidates << QDir::cleanPath(dir.absoluteFilePath(relativePath));
        }
    }

    // 5. 构建开发模式候选路径（基于 sourceDirOverride 或编译期宏 LX_PROJECT_SOURCE_DIR）
    QStringList devCandidates;
    QString sourceDir = context.sourceDirOverride;
#ifdef LX_PROJECT_SOURCE_DIR
    if (sourceDir.isEmpty()) {
        sourceDir = QStringLiteral(LX_PROJECT_SOURCE_DIR);
    }
#endif
    if (!sourceDir.isEmpty()) {
        devCandidates << QDir::cleanPath(QDir(sourceDir).absoluteFilePath(relativePath));
    }

    // 6. 按照模式执行隔离搜索
    if (isInstalled) {
        // 安装模式：严禁回退到源码目录！
        QStringList searched;
        for (const QString& candidate : installedCandidates) {
            searched << candidate;
            const QString entry = QDir(candidate).absoluteFilePath(QStringLiteral("lmc.py"));
            if (QFileInfo::exists(entry)) {
                result.success = true;
                result.runtimeWorkingDir = candidate;
                result.entryScript = entry;
                result.candidateSearched = candidate;
                return result;
            }
        }

        result.success = false;
        result.runtimeWorkingDir = installedCandidates.isEmpty() ? QString() : installedCandidates.constFirst();
        result.entryScript = result.runtimeWorkingDir.isEmpty()
                ? QString()
                : QDir(result.runtimeWorkingDir).absoluteFilePath(QStringLiteral("lmc.py"));
        result.candidateSearched = searched.join(QStringLiteral("; "));
        result.errorMessage = QStringLiteral(
                "Running in installed layout, but compiler runtime was not found. "
                "Searched installed paths: [%1]. Missing entry script lmc.py. "
                "Source tree fallback is strictly prohibited in installed mode.")
                .arg(result.candidateSearched);
        return result;
    }

    // 开发模式：
    // 优先尝试本地可能已安装的运行时路径（如开发人员测试前缀）
    for (const QString& candidate : installedCandidates) {
        const QString entry = QDir(candidate).absoluteFilePath(QStringLiteral("lmc.py"));
        if (QFileInfo::exists(entry)) {
            result.success = true;
            result.runtimeWorkingDir = candidate;
            result.entryScript = entry;
            result.candidateSearched = candidate;
            return result;
        }
    }

    // 回退到开发源码树
    QStringList searched = installedCandidates;
    for (const QString& candidate : devCandidates) {
        searched << candidate;
        const QString entry = QDir(candidate).absoluteFilePath(QStringLiteral("lmc.py"));
        if (QFileInfo::exists(entry)) {
            result.success = true;
            result.runtimeWorkingDir = candidate;
            result.entryScript = entry;
            result.candidateSearched = candidate;
            return result;
        }
    }

    result.success = false;
    result.runtimeWorkingDir = devCandidates.isEmpty()
            ? (installedCandidates.isEmpty() ? QString() : installedCandidates.constFirst())
            : devCandidates.constFirst();
    result.entryScript = result.runtimeWorkingDir.isEmpty()
            ? QString()
            : QDir(result.runtimeWorkingDir).absoluteFilePath(QStringLiteral("lmc.py"));
    result.candidateSearched = searched.join(QStringLiteral("; "));
    result.errorMessage = QStringLiteral(
            "Compiler runtime not found in developer mode. "
            "Searched paths: [%1]. Missing entry script lmc.py.")
            .arg(result.candidateSearched);
    return result;
}

void DSLCompilerInterface::setRuntimeSearchContext(const CompilerRuntimeSearchContext& context)
{
    m_runtimeContext = context;
}

CompilerRuntimeSearchContext DSLCompilerInterface::runtimeSearchContext() const
{
    return m_runtimeContext;
}

QString DSLCompilerInterface::compilerWorkingDir() const
{
    const CompilerRuntimeResolution resolution = resolveCompilerRuntime(m_runtimeContext);
    if (resolution.success) {
        static QString s_lastLoggedDir;
        if (s_lastLoggedDir != resolution.runtimeWorkingDir) {
            s_lastLoggedDir = resolution.runtimeWorkingDir;
            qInfo() << "[DSLCompilerInterface] Selected compiler runtime:"
                    << resolution.runtimeWorkingDir
                    << QStringLiteral("(mode: %1, entry: %2)")
                       .arg(resolution.isInstalledMode ? QStringLiteral("installed") : QStringLiteral("developer"),
                            resolution.entryScript);
        }
        return resolution.runtimeWorkingDir;
    }

    qWarning() << "[DSLCompilerInterface]" << resolution.errorMessage;
    return resolution.runtimeWorkingDir;
}

QString DSLCompilerInterface::compilerEntryScript() const
{
    const CompilerRuntimeResolution resolution = resolveCompilerRuntime(m_runtimeContext);
    if (resolution.success) {
        return resolution.entryScript;
    }
    return QDir(compilerWorkingDir()).absoluteFilePath(QStringLiteral("lmc.py"));
}

QString DSLCompilerInterface::cachedPythonInterpreter(const QString& workDir)
{
    const QString cleanDir = QDir::cleanPath(workDir);
    if (!cleanDir.isEmpty()) {
        return s_cachedPythonInterpretersByWorkDir.value(cleanDir);
    }
    return s_cachedPythonInterpreter;
}

void DSLCompilerInterface::setCachedPythonInterpreter(const QString& workDir, const QString& python)
{
    const QString cleanDir = QDir::cleanPath(workDir);
    if (!cleanDir.isEmpty()) {
        s_cachedPythonInterpretersByWorkDir.insert(cleanDir, python);
    }
    s_cachedPythonInterpreter = python;
}

void DSLCompilerInterface::clearPythonInterpreterCache()
{
    s_cachedPythonInterpretersByWorkDir.clear();
    s_cachedPythonInterpreter.clear();
}

void DSLCompilerInterface::clearPythonInterpreterCache(const QString& workDir)
{
    const QString cleanDir = QDir::cleanPath(workDir);
    if (!cleanDir.isEmpty()) {
        s_cachedPythonInterpretersByWorkDir.remove(cleanDir);
    } else {
        s_cachedPythonInterpretersByWorkDir.clear();
    }
    if (s_cachedPythonInterpretersByWorkDir.isEmpty()) {
        s_cachedPythonInterpreter.clear();
    }
}
