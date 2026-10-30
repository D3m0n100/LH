/**
 * @file MainWindowRunFlow.cpp
 * @brief 第二阶段：构建输入、构建快照、F8/F9 运行编排与下载确认。
 */

#include "MainWindow.h"

#include "BuildController.h"
#include "DownloadDockWidget.h"
#include "DownloadConfirmation.h"
#include "ProjectController.h"
#include "RuntimeSessionController.h"
#include "ui/GlobalStatusBar.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QUuid>

QString MainWindow::runFlowStageName() const
{
    switch (m_runFlowStage) {
    case RunFlowStage::Idle: return QStringLiteral("空闲");
    case RunFlowStage::Saving: return QStringLiteral("保存输入");
    case RunFlowStage::Compiling: return QStringLiteral("编译");
    case RunFlowStage::Connecting: return QStringLiteral("连接准备");
    case RunFlowStage::AwaitingConfirmation: return QStringLiteral("待确认");
    case RunFlowStage::Downloading: return QStringLiteral("下载");
    case RunFlowStage::RequestingRun: return QStringLiteral("请求运行");
    case RunFlowStage::Completed: return QStringLiteral("完成");
    case RunFlowStage::Failed: return QStringLiteral("失败");
    case RunFlowStage::Cancelled: return QStringLiteral("已取消");
    }
    return QStringLiteral("未知");
}

bool MainWindow::isRunFlowActive() const
{
    switch (m_runFlowStage) {
    case RunFlowStage::Idle:
    case RunFlowStage::Completed:
    case RunFlowStage::Failed:
    case RunFlowStage::Cancelled:
        return false;
    default:
        return true;
    }
}

bool MainWindow::canStartCompileAction() const
{
    if (!m_projectController || !m_buildController || isRunFlowActive()
            || m_buildController->isBusy()) {
        return false;
    }
    if (m_downloadWidget && m_downloadWidget->isBusy()) return false;
    if (!m_sessionController) return true;
    if (m_sessionController->hasWorkerOperationInFlight()) return false;
    const RuntimeSessionState state = m_sessionController->state();
    if (state == RuntimeSessionState::Connecting || state == RuntimeSessionState::Downloading)
        return false;
    switch (m_sessionController->downloadState()) {
    case DownloadState::Precheck:
    case DownloadState::Downloading:
    case DownloadState::Retrying:
    case DownloadState::Verifying:
        return false;
    default:
        return true;
    }
}

void MainWindow::setRunFlowStage(RunFlowStage stage, const QString& detail)
{
    m_runFlowStage = stage;
    const QString text = detail.isEmpty()
            ? QStringLiteral("运行流程：%1").arg(runFlowStageName())
            : QStringLiteral("运行流程：%1（%2）").arg(runFlowStageName(), detail);
    updateStatusBar(text);
    if (m_globalStatusBar) {
        m_globalStatusBar->setBuildState(runFlowStageName());
    }
    setCompileActionsEnabled(true);
}

void MainWindow::beginCompileAndRunFlow()
{
    m_runFlowOperationId = QUuid::createUuid().toString();
    m_buildSnapshotStale = false;
    setRunFlowStage(RunFlowStage::Saving, QStringLiteral("仅保存本次参与构建的输入"));
}

QString MainWindow::resolveProjectFilePath(const QString& path) const
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || !m_projectController) {
        return QString();
    }
    const QFileInfo info(trimmed);
    if (info.isAbsolute()) {
        return normalizeDocumentIdentity(trimmed);
    }
    return normalizeDocumentIdentity(
            QDir(m_projectController->currentProjectPath()).absoluteFilePath(trimmed));
}

QStringList MainWindow::buildInputFiles() const
{
    QStringList inputs;
    if (!m_projectController || !m_projectController->hasOpenProject()) {
        return inputs;
    }
    auto appendUnique = [&inputs](const QString& path) {
        const QString norm = normalizeDocumentIdentity(path);
        if (norm.isEmpty()) {
            return;
        }
        for (const QString& existing : inputs) {
            if (existing.compare(norm, Qt::CaseInsensitive) == 0) {
                return;
            }
        }
        inputs.append(norm);
    };

    const ProjectRuntimeConfig& cfg = m_projectController->runtimeConfig();
    appendUnique(resolveProjectFilePath(cfg.mainScriptPath));
    for (const QString& script : cfg.scriptFiles) {
        appendUnique(resolveProjectFilePath(script));
    }
    appendUnique(QDir(m_projectController->currentProjectPath())
                 .filePath(QStringLiteral("project_config.json")));
    return inputs;
}

bool MainWindow::saveDirtyBuildInputs(QStringList* errors)
{
    if (!m_projectController || !m_projectController->hasOpenProject()) {
        if (errors) errors->append(QStringLiteral("没有打开的工程。"));
        return false;
    }

    const QStringList inputs = buildInputFiles();
    auto isParticipating = [&inputs](const QString& path) {
        for (const QString& input : inputs) {
            if (input.compare(path, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
        return false;
    };

    if (m_mdiArea) {
        for (QMdiSubWindow* sub : m_mdiArea->subWindowList()) {
            if (!sub || sub == m_editorSubWindow) {
                continue;
            }
            const QString path = normalizeDocumentIdentity(
                    sub->property("filePath").toString());
            if (!isParticipating(path)) {
                continue;
            }
            auto* editor = qobject_cast<QPlainTextEdit*>(sub->widget());
            const bool dirty = sub->property("modified").toBool()
                    || (editor && editor->document()->isModified());
            if (dirty && !saveAuxiliarySubWindow(sub)) {
                if (errors) {
                    errors->append(QStringLiteral("无法保存参与构建的文档：%1")
                                   .arg(QDir::toNativeSeparators(path)));
                }
                return false;
            }
        }
    }

    QString saveFailure;
    if (!m_projectController->saveProjectForBuildInputs(&saveFailure)) {
        if (errors) {
            errors->append(saveFailure.isEmpty()
                           ? QStringLiteral("无法保存主脚本或工程配置。")
                           : saveFailure);
        }
        return false;
    }

    for (const QString& input : inputs) {
        const QFileInfo info(input);
        if (!info.exists() || !info.isFile()) {
            if (errors) {
                errors->append(QStringLiteral("参与构建的输入不存在：%1")
                               .arg(QDir::toNativeSeparators(input)));
            }
            return false;
        }
        QFile file(input);
        if (!file.open(QIODevice::ReadOnly)) {
            if (errors) {
                errors->append(QStringLiteral("参与构建的输入不可读：%1（%2）")
                               .arg(QDir::toNativeSeparators(input), file.errorString()));
            }
            return false;
        }
    }
    return true;
}

void MainWindow::captureBuildSnapshot()
{
    m_activeBuildSnapshot = BuildSnapshot();
    m_buildSnapshotStale = false;
    if (!m_projectController || !m_projectController->hasOpenProject() || !m_buildController) {
        return;
    }

    BuildSnapshot snapshot;
    snapshot.valid = true;
    snapshot.sessionId = m_projectSessionId;
    snapshot.projectPath = QDir(m_projectController->currentProjectPath()).absolutePath();
    snapshot.entryPath = resolveProjectFilePath(
            m_projectController->runtimeConfig().mainScriptPath);
    snapshot.configurationFingerprint = QString::fromLatin1(QCryptographicHash::hash(
                QJsonDocument(m_projectController->runtimeConfig().toJson())
                .toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    snapshot.buildGeneration = m_buildController->activeOperationGeneration();
    snapshot.operationId = m_runFlowOperationId.isEmpty()
            ? QUuid::createUuid().toString()
            : m_runFlowOperationId;

    const QStringList inputs = buildInputFiles();
    for (const QString& input : inputs) {
        QFile file(input);
        if (!file.open(QIODevice::ReadOnly)) {
            snapshot.valid = false;
            m_activeBuildSnapshot = snapshot;
            return;
        }
        snapshot.inputChecksums.insert(input,
                QString::fromLatin1(QCryptographicHash::hash(file.readAll(),
                    QCryptographicHash::Sha256).toHex()));
        if (m_documentVersions.contains(input)) {
            snapshot.documentVersions.insert(input, m_documentVersions.value(input));
        }
    }
    m_activeBuildSnapshot = snapshot;
}

bool MainWindow::isBuildSnapshotCurrent() const
{
    if (!m_activeBuildSnapshot.valid || m_buildSnapshotStale) {
        return false;
    }
    if (!m_projectController || !m_projectController->hasOpenProject()) {
        return false;
    }
    if (m_projectSessionId != m_activeBuildSnapshot.sessionId) {
        return false;
    }
    const QString projectPath =
            QDir(m_projectController->currentProjectPath()).absolutePath();
    if (projectPath.compare(m_activeBuildSnapshot.projectPath, Qt::CaseInsensitive) != 0) {
        return false;
    }
    const QString entry = resolveProjectFilePath(
            m_projectController->runtimeConfig().mainScriptPath);
    if (entry.compare(m_activeBuildSnapshot.entryPath, Qt::CaseInsensitive) != 0) {
        return false;
    }
    const QString configFingerprint = QString::fromLatin1(QCryptographicHash::hash(
                QJsonDocument(m_projectController->runtimeConfig().toJson())
                .toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    if (configFingerprint != m_activeBuildSnapshot.configurationFingerprint) {
        return false;
    }

    for (auto it = m_activeBuildSnapshot.inputChecksums.constBegin();
         it != m_activeBuildSnapshot.inputChecksums.constEnd(); ++it) {
        const QFileInfo info(it.key());
        if (!info.exists() || !info.isFile()) {
            return false;
        }
        QFile file(it.key());
        if (!file.open(QIODevice::ReadOnly)
                || QString::fromLatin1(QCryptographicHash::hash(file.readAll(),
                    QCryptographicHash::Sha256).toHex()) != it.value()) {
            return false;
        }
    }
    for (auto it = m_activeBuildSnapshot.documentVersions.constBegin();
         it != m_activeBuildSnapshot.documentVersions.constEnd(); ++it) {
        if (m_documentVersions.value(it.key(), it.value()) != it.value()) {
            return false;
        }
    }
    return true;
}

bool MainWindow::refreshBuildSnapshotPublicationIdentity()
{
    if (!m_activeBuildSnapshot.valid || m_buildSnapshotStale || !m_projectController
            || !m_projectController->hasOpenProject()
            || m_activeBuildSnapshot.operationId != m_runFlowOperationId
            || m_activeBuildSnapshot.sessionId != m_projectSessionId) {
        return false;
    }
    const QString projectPath = QDir(m_projectController->currentProjectPath()).absolutePath();
    if (projectPath.compare(m_activeBuildSnapshot.projectPath, Qt::CaseInsensitive) != 0
            || resolveProjectFilePath(m_projectController->runtimeConfig().mainScriptPath)
                .compare(m_activeBuildSnapshot.entryPath, Qt::CaseInsensitive) != 0) {
        return false;
    }

    const QString configPath = normalizeDocumentIdentity(
                QDir(projectPath).filePath(QStringLiteral("project_config.json")));
    bool sawConfig = false;
    for (auto it = m_activeBuildSnapshot.inputChecksums.begin();
         it != m_activeBuildSnapshot.inputChecksums.end(); ++it) {
        QFile file(it.key());
        if (!file.open(QIODevice::ReadOnly)) return false;
        const QString currentHash = QString::fromLatin1(QCryptographicHash::hash(
                    file.readAll(), QCryptographicHash::Sha256).toHex());
        if (it.key().compare(configPath, Qt::CaseInsensitive) == 0) {
            it.value() = currentHash;
            sawConfig = true;
        } else if (currentHash != it.value()) {
            return false;
        }
    }
    if (!sawConfig) return false;
    for (auto it = m_activeBuildSnapshot.documentVersions.constBegin();
         it != m_activeBuildSnapshot.documentVersions.constEnd(); ++it) {
        if (m_documentVersions.value(it.key(), it.value()) != it.value()) return false;
    }
    m_activeBuildSnapshot.configurationFingerprint = QString::fromLatin1(
                QCryptographicHash::hash(QJsonDocument(m_projectController->runtimeConfig().toJson())
                                         .toJson(QJsonDocument::Compact),
                                         QCryptographicHash::Sha256).toHex());
    return true;
}

void MainWindow::invalidateBuildSnapshot(const QString& reason)
{
    if (!m_activeBuildSnapshot.valid && m_buildSnapshotStale) {
        return;
    }
    m_buildSnapshotStale = true;
    m_sessionController->setPendingRunAfterCompile(false);
    m_sessionController->invalidateCompiledArtifact();
    const QString message = QStringLiteral(
            "本次构建结果已失效（%1）：未发布产物，也不会自动下载或运行。请重新构建。")
            .arg(reason);
    appendOutput(QStringLiteral("[%1] %2")
                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")),
                      message));
    addProblem(QStringLiteral("warning"), QStringLiteral("构建"), message);
}

void MainWindow::invalidateActiveBuildForEntryChange()
{
    if (isRunFlowActive() || (m_buildController && m_buildController->isBusy())) {
        invalidateBuildSnapshot(QStringLiteral("构建入口或工程已改变"));
        setRunFlowStage(RunFlowStage::Cancelled, QStringLiteral("构建入口改变"));
    }
}

bool MainWindow::confirmDownloadSnapshot(const DownloadConfirmationSnapshot& snapshot)
{
    const bool belongsToRunFlow = !m_runFlowOperationId.isEmpty()
            && snapshot.operationId == m_runFlowOperationId;
    const QString operationId = m_runFlowOperationId;
    const auto stillCurrent = [this, belongsToRunFlow, operationId]() {
        return !belongsToRunFlow
                || (operationId == m_runFlowOperationId
                    && isRunFlowActive()
                    && isBuildSnapshotCurrent());
    };
    if (!stillCurrent()) return false;
    const bool approved = showMessageBox(QStringLiteral("下载前确认"),
                                         snapshot.toDisplayText(),
                                         QMessageBox::Yes | QMessageBox::No,
                                         QMessageBox::No) == QMessageBox::Yes;
    return approved && stillCurrent();
}
