/**
 * @file MainWindowInspector.cpp
 * @brief MainWindow inspector and diagnostics panel helpers.
 */

#include "MainWindow.h"

#include "BuildController.h"
#include "MonitorManager.h"
#include "MonitorWidget.h"
#include "ParameterController.h"
#include "ParameterTuningWindow.h"
#include "ProjectController.h"
#include "RuntimeSessionController.h"
#include "../communication/IOpcServer.h"
#include "ui/InspectorPanel.h"
#include "ui/ProblemsPanel.h"
#include "ui/StatusTextHelper.h"

#include <QMap>
#include <QStringList>
#include <cmath>

namespace {

bool containsSeparatedToken(const QString& text, const QString& token)
{
    if (text.isEmpty() || token.isEmpty()) {
        return false;
    }

    const QString lower = text.toLower();
    int index = lower.indexOf(token);
    while (index >= 0) {
        const int beforeIndex = index - 1;
        const int afterIndex = index + token.size();
        const bool beforeOk = beforeIndex < 0 || !lower.at(beforeIndex).isLetterOrNumber();
        const bool afterOk = afterIndex >= lower.size() || !lower.at(afterIndex).isLetterOrNumber();
        if (beforeOk && afterOk) {
            return true;
        }
        index = lower.indexOf(token, index + 1);
    }
    return false;
}

bool isPidParameter(const ParameterDefinition& parameter)
{
    const QString name = parameter.name.trimmed().toLower();
    const QString dataType = parameter.dataType.trimmed().toLower();
    const QString unit = parameter.unit.trimmed().toLower();
    const QString kind = parameter.metadata.value("kind").toString().trimmed().toLower();
    const QString role = parameter.metadata.value("role").toString().trimmed().toLower();
    const QString category = parameter.metadata.value("category").toString().trimmed().toLower();

    const QString combined = QStringList{ name, dataType, unit, kind, role, category }.join(' ');
    if (combined.contains("pid")) {
        return true;
    }

    if (containsSeparatedToken(combined, "kp") ||
        containsSeparatedToken(combined, "ki") ||
        containsSeparatedToken(combined, "kd")) {
        return true;
    }

    return false;
}

QList<ParameterDefinition> filterPidParameters(const QList<ParameterDefinition>& parameters)
{
    QList<ParameterDefinition> pidParameters;
    for (const auto& parameter : parameters) {
        if (parameter.onlineEditable && isPidParameter(parameter)) {
            pidParameters.append(parameter);
        }
    }
    return pidParameters;
}

} // namespace

namespace MainWindowStatusFormatting {

QString formatOpcStatusDetails(const BackendStatusSnapshot& status)
{
    if (status.backendType == QStringLiteral("matrikon-opc-da")) {
        QStringList parts;
        parts << QStringLiteral("backend=%1").arg(status.backendType);
        parts << QStringLiteral("progId=%1").arg(status.extras.value(QStringLiteral("opcProgId")).toString());
        parts << QStringLiteral("items=%1").arg(status.extras.value(QStringLiteral("itemCount")).toInt());
        parts << QStringLiteral("active=%1").arg(status.extras.value(QStringLiteral("successfulItemCount")).toInt());
        parts << QStringLiteral("matched=%1").arg(status.extras.value(QStringLiteral("matchedItemCount")).toInt());
        parts << QStringLiteral("unmatched=%1").arg(status.extras.value(QStringLiteral("unmatchedItemCount")).toInt());
        parts << QStringLiteral("read ok=%1").arg(status.extras.value(QStringLiteral("successfulReadCount")).toInt());
        parts << QStringLiteral("read fail=%1").arg(status.extras.value(QStringLiteral("failedReadCount")).toInt());
        parts << QStringLiteral("write ok=%1").arg(status.extras.value(QStringLiteral("successfulWriteCount")).toInt());
        parts << QStringLiteral("write fail=%1").arg(status.extras.value(QStringLiteral("failedWriteCount")).toInt());
        parts << QStringLiteral("quality=%1").arg(status.extras.value(QStringLiteral("lastQualityText")).toString());

        const QString lastReadItem = status.extras.value(QStringLiteral("lastReadItemId")).toString();
        const QString lastWriteItem = status.extras.value(QStringLiteral("lastWriteItemId")).toString();
        const QString lastUnmatched = status.extras.value(QStringLiteral("lastUnmatchedItemId")).toString();
        const QString probe = status.extras.value(QStringLiteral("configurationProbeMessage")).toString();
        if (!lastReadItem.isEmpty()) {
            parts << QStringLiteral("lastReadItem=%1").arg(lastReadItem);
        }
        if (!lastWriteItem.isEmpty()) {
            parts << QStringLiteral("lastWriteItem=%1").arg(lastWriteItem);
        }
        if (!lastUnmatched.isEmpty()) {
            parts << QStringLiteral("lastUnmatched=%1").arg(lastUnmatched);
        }
        if (!probe.isEmpty()) {
            parts << QStringLiteral("config=%1").arg(probe);
        }
        if (!status.lastErrorMessage.trimmed().isEmpty()) {
            parts << QStringLiteral("lastErr=%1").arg(status.lastErrorMessage.trimmed());
        }
        return parts.join(QStringLiteral(" | "));
    }

    const QString modbusOnline = status.extras.value(QStringLiteral("modbusConnected")).toBool()
            ? QStringLiteral("online")
            : QStringLiteral("offline");
    const QString polling = status.extras.value(QStringLiteral("polling")).toBool()
            ? QStringLiteral("on")
            : QStringLiteral("off");
    QStringList parts;
    parts << QStringLiteral("backend=%1").arg(status.backendType);
    parts << QStringLiteral("modbus=%1").arg(modbusOnline);
    parts << QStringLiteral("polling=%1").arg(polling);
    parts << QStringLiteral("poll ok=%1").arg(status.extras.value(QStringLiteral("successfulPollCount")).toInt());
    parts << QStringLiteral("poll fail=%1").arg(status.extras.value(QStringLiteral("failedPollCount")).toInt());
    parts << QStringLiteral("write ok=%1").arg(status.extras.value(QStringLiteral("successfulWriteCount")).toInt());
    parts << QStringLiteral("write fail=%1").arg(status.extras.value(QStringLiteral("failedWriteCount")).toInt());
    parts << QStringLiteral("points=%1/%2")
                   .arg(status.extras.value(QStringLiteral("addressedPointCount")).toInt())
                   .arg(status.extras.value(QStringLiteral("unresolvedPointCount")).toInt());

    const QString lastOkWrite = status.extras.value(QStringLiteral("lastSuccessfulWriteTime")).toString();
    const QString lastFailWrite = status.extras.value(QStringLiteral("lastFailedWriteTime")).toString();
    const QString lastOkPoll = status.extras.value(QStringLiteral("lastSuccessfulPollTime")).toString();
    const QString lastErr = status.lastErrorMessage.trimmed();
    const QString lastWritePoint = status.extras.value(QStringLiteral("lastWritePointId")).toString();
    const QString lastWriteMsg = status.extras.value(QStringLiteral("lastWriteMessage")).toString();

    if (!lastWritePoint.isEmpty()) {
        parts << QStringLiteral("lastWrite=%1").arg(lastWritePoint);
    }
    if (!lastWriteMsg.isEmpty()) {
        parts << QStringLiteral("lastWriteMsg=%1").arg(lastWriteMsg);
    }
    if (!lastOkWrite.isEmpty()) {
        parts << QStringLiteral("lastOkWrite=%1").arg(lastOkWrite);
    }
    if (!lastFailWrite.isEmpty()) {
        parts << QStringLiteral("lastFailWrite=%1").arg(lastFailWrite);
    }
    if (!lastOkPoll.isEmpty()) {
        parts << QStringLiteral("lastOkPoll=%1").arg(lastOkPoll);
    }
    if (!lastErr.isEmpty()) {
        parts << QStringLiteral("lastErr=%1").arg(lastErr);
    }

    return parts.join(QStringLiteral(" | "));
}

} // namespace MainWindowStatusFormatting

namespace {

QString downloadStateText(DownloadState state)
{
    switch (state) {
    case DownloadState::Idle:
        return QStringLiteral("空闲");
    case DownloadState::Precheck:
        return QStringLiteral("前置校验");
    case DownloadState::PrecheckFailed:
        return QStringLiteral("前置校验失败");
    case DownloadState::Downloading:
        return QStringLiteral("下载中");
    case DownloadState::Retrying:
        return QStringLiteral("重试中");
    case DownloadState::Verifying:
        return QStringLiteral("校验中");
    case DownloadState::Succeeded:
        return QStringLiteral("成功");
    case DownloadState::TransportFailed:
        return QStringLiteral("传输失败");
    case DownloadState::DeviceRejected:
        return QStringLiteral("设备拒绝");
    case DownloadState::VerifyFailed:
        return QStringLiteral("校验失败");
    case DownloadState::Failed:
    default:
        return QStringLiteral("失败");
    }
}

} // namespace

void MainWindow::refreshInspectorPanel()
{
    if (!m_projectController || m_refreshingInspector) {
        return;
    }

    m_refreshingInspector = true;
    refreshInspectorPanel(m_inspectorPanel);
    refreshInspectorPanel(m_parameterTuningWindow);
    m_refreshingInspector = false;
}

void MainWindow::refreshInspectorPanel(InspectorPanel* panel)
{
    if (!panel || !m_projectController) {
        return;
    }

    panel->setProjectPath(m_projectController->currentProjectPath());
    panel->setCurrentFile(m_projectController->currentScriptFile());
    panel->setRuntimeState(runtimeStateText(m_sessionController && m_sessionController->isRunning()));
    panel->setBuildState(m_buildController && m_buildController->isBusy()
                                 ? QStringLiteral("忙碌")
                                 : QStringLiteral("空闲"));
    panel->setMonitoringState(monitoringStateText(m_monitorWidget && m_monitorWidget->isMonitoring()));
    QString downloadText = m_sessionController
            ? downloadStateText(m_sessionController->downloadState())
            : QStringLiteral("空闲");
    const QString downloadMessage = m_lastDownloadDiagnostic.value(QStringLiteral("message")).toString().trimmed();
    if (!downloadMessage.isEmpty()) {
        downloadText += QStringLiteral(" | %1").arg(downloadMessage);
    }
    panel->setDownloadState(downloadText);
    QString opcStateText = m_opcRunning
            ? QStringLiteral("运行中")
            : (m_lastOpcError.isEmpty() ? QStringLiteral("关闭") : QStringLiteral("错误"));
    if (m_sessionController && m_sessionController->opcServer()) {
        const auto status = m_sessionController->opcServer()->statusSnapshot();
        opcStateText += QStringLiteral(" | %1").arg(MainWindowStatusFormatting::formatOpcStatusDetails(status));
    }
    panel->setOpcState(opcStateText);
    const auto& cfg = m_projectController->runtimeConfig();

    m_parameterController->loadDefinitions(cfg.parameters);

    int editableParameters = 0;
    for (const auto& p : cfg.parameters) {
        if (p.onlineEditable) {
            ++editableParameters;
        }
    }
    panel->setVariableSummary(QStringLiteral("%1 个，已挂载监控").arg(cfg.variables.size()));
    panel->setParameterSummary(QStringLiteral("%1 个，在线可改 %2 个")
                                            .arg(cfg.parameters.size())
                                            .arg(editableParameters));
    panel->setResourceSummary(QStringLiteral("%1 个，已挂载监控").arg(cfg.resources.size()));
    panel->setParameterDetails(cfg.parameters);

    QMap<QString, ParameterStateInfo> stateMap;
    for (const auto& si : m_parameterController->parameterStates())
        stateMap.insert(si.name, si);
    panel->setParameterStateMap(stateMap);

    QStringList readbackReady;
    QMap<QString, double> deviationMap;
    for (const auto& p : cfg.parameters) {
        const QString channelName = QStringLiteral("param::%1").arg(p.name);
        const auto samples = Monitor::MonitorManager::instance().history(channelName, 1);
        const bool hasReadback = !samples.isEmpty();
        if (hasReadback && !p.currentValue.isEmpty()) {
            readbackReady.append(p.name);
            bool okCurrent = false;
            const double currentValue = p.currentValue.toDouble(&okCurrent);
            const double sampleValue = samples.last().value;
            if (okCurrent && std::isfinite(sampleValue)) {
                deviationMap.insert(p.name, sampleValue - currentValue);
                continue;
            }
        }
    }
    panel->setParameterReadbackReady(readbackReady);
    panel->setParameterDeviationMap(deviationMap);
    if (m_workspaceTabs) {
        panel->setWorkspaceName(m_workspaceTabs->tabText(m_workspaceTabs->currentIndex()));
    }
}

void MainWindow::refreshInspectorPanel(ParameterTuningWindow* window)
{
    if (!window || !m_projectController) {
        return;
    }

    const auto& cfg = m_projectController->runtimeConfig();
    const QList<ParameterDefinition> pidParameters = filterPidParameters(cfg.parameters);
    window->setPidParameterDetails(pidParameters);
    QStringList readbackReady;
    QMap<QString, double> deviationMap;
    for (const auto& p : pidParameters) {
        const QString channelName = QStringLiteral("param::%1").arg(p.name);
        const auto samples = Monitor::MonitorManager::instance().history(channelName, 1);
        const bool hasReadback = !samples.isEmpty();
        if (hasReadback && !p.currentValue.isEmpty()) {
            readbackReady.append(p.name);
            bool okCurrent = false;
            const double currentValue = p.currentValue.toDouble(&okCurrent);
            const double sampleValue = samples.last().value;
            if (okCurrent && std::isfinite(sampleValue)) {
                deviationMap.insert(p.name, sampleValue - currentValue);
                continue;
            }
        }
    }
    window->setParameterReadbackReady(readbackReady);
    window->setParameterDeviationMap(deviationMap);

    QMap<QString, ParameterStateInfo> stateMap;
    for (const auto& si : m_parameterController->parameterStates())
        stateMap.insert(si.name, si);
    window->setParameterStateMap(stateMap);
}

void MainWindow::addProblem(const QString& severity, const QString& source, const QString& message)
{
    if (m_problemsPanel) {
        m_problemsPanel->addProblem(severity, source, message);
    }
}
