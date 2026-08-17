// File: src/communication/ControllerDeviceBackendDiagnostics.cpp

#include "ControllerDeviceBackend.h"

#include "ControllerDebugProtocol.h"

#include <QMutexLocker>
#include <QSettings>
#include <QSet>
#include <QtGlobal>
#include <QtSerialPort/QSerialPortInfo>

namespace {
struct SerialModeParts
{
    bool valid = false;
    int baudRate = 0;
    QString parity;
    int dataBits = 0;
    int stopBits = 0;
};

QVariantList stringListToVariantList(const QStringList& values)
{
    QVariantList list;
    list.reserve(values.size());
    for (const QString& value : values) {
        list.append(value);
    }
    return list;
}

QString normalizedParity(const QString& parity)
{
    QString value = parity.trimmed().toLower();
    if (value == QStringLiteral("n")) return QStringLiteral("none");
    if (value == QStringLiteral("e")) return QStringLiteral("even");
    if (value == QStringLiteral("o")) return QStringLiteral("odd");
    return value;
}

SerialModeParts parseSerialMode(const QString& text)
{
    SerialModeParts parts;
    const QStringList tokens = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (tokens.size() != 4) {
        return parts;
    }

    bool ok = false;
    parts.baudRate = tokens.at(0).trimmed().toInt(&ok);
    if (!ok || parts.baudRate <= 0) {
        return SerialModeParts();
    }

    parts.parity = normalizedParity(tokens.at(1));
    const QSet<QString> validParity = {
        QStringLiteral("none"),
        QStringLiteral("even"),
        QStringLiteral("odd"),
        QStringLiteral("space"),
        QStringLiteral("mark")
    };
    if (!validParity.contains(parts.parity)) {
        return SerialModeParts();
    }

    parts.dataBits = tokens.at(2).trimmed().toInt(&ok);
    if (!ok || parts.dataBits < 5 || parts.dataBits > 8) {
        return SerialModeParts();
    }

    parts.stopBits = tokens.at(3).trimmed().toInt(&ok);
    if (!ok || parts.stopBits < 1 || parts.stopBits > 2) {
        return SerialModeParts();
    }

    parts.valid = true;
    return parts;
}

QStringList availableSerialPortNames()
{
    QStringList ports;
    const QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
    ports.reserve(infos.size());
    for (const QSerialPortInfo& info : infos) {
        ports << info.portName();
    }
    ports.removeDuplicates();
    ports.sort(Qt::CaseInsensitive);
    return ports;
}

bool serialPortNameMatches(const QStringList& availablePorts, const QString& port)
{
    for (const QString& available : availablePorts) {
        if (available.compare(port, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool opcProgIdRegistered(const QString& progId)
{
    if (progId.trimmed().isEmpty()) {
        return false;
    }
#ifdef Q_OS_WIN
    const QString key = QStringLiteral("HKEY_CLASSES_ROOT\\%1\\CLSID").arg(progId.trimmed());
    const QSettings settings(key, QSettings::NativeFormat);
    return settings.contains(QStringLiteral("."))
            || settings.contains(QStringLiteral("Default"))
            || !settings.allKeys().isEmpty();
#else
    Q_UNUSED(progId)
    return false;
#endif
}
} // namespace

bool ControllerDeviceBackend::preflight(QString* errorMessage, QVariantMap* report) const
{
    QMutexLocker lock(&m_mutex);
    QStringList errors;
    QStringList warnings;
    QVariantMap localReport = buildPreflightReport(&errors, &warnings);
    if (report) {
        *report = localReport;
    }
    if (!errors.isEmpty()) {
        if (errorMessage) {
            *errorMessage = errors.join(QStringLiteral("; "));
        }
        return false;
    }
    if (errorMessage) {
        *errorMessage = warnings.join(QStringLiteral("; "));
    }
    return true;
}

bool ControllerDeviceBackend::testConnection(ControllerConnectionDiagnostic* diagnostic,
                                             QString* errorMessage)
{
    if (!ensureOnline(errorMessage)) {
        return false;
    }

    ControllerDebugStatus status;
    if (!m_client->readStatus(&status)) {
        const CommError err = currentDebugError(QStringLiteral("控制器连接诊断失败：状态读取失败。"));
        if (errorMessage) {
            *errorMessage = err.message;
        }
        setFailure(err.code, err.message, err.details);
        return false;
    }

    QString targetError;
    const bool targetOk = probeTarget(&targetError);
    QVariantMap extras;

    {
        QMutexLocker lock(&m_mutex);
        updateStatusCache(status);
        m_targetOnline = targetOk;
        if (!targetOk) {
            m_lastDownloadError = targetError;
        }
        extras.insert(QStringLiteral("backend"), QStringLiteral("controller"));
        extras.insert(QStringLiteral("online"), true);
        extras.insert(QStringLiteral("deviceId"), deviceId());
        extras.insert(QStringLiteral("port"), portName());
        extras.insert(QStringLiteral("targetOnline"), m_targetOnline);
        extras.insert(QStringLiteral("targetDeviceId"), m_targetDeviceId);
        extras.insert(QStringLiteral("state"), status.state);
        extras.insert(QStringLiteral("workMode"), status.workMode);
        extras.insert(QStringLiteral("componentLine"), status.componentLine);
        extras.insert(QStringLiteral("reset"), status.reset);
        extras.insert(QStringLiteral("version"), status.version);
        extras.insert(QStringLiteral("lastTargetProbe"), m_lastTargetProbe);
        extras.insert(QStringLiteral("preflight"), buildPreflightReport(nullptr, nullptr));
    }

    if (diagnostic) {
        diagnostic->controllerOnline = true;
        diagnostic->targetOnline = targetOk;
        diagnostic->deviceId = deviceId();
        diagnostic->targetDeviceId = m_targetDeviceId;
        diagnostic->state = status.state;
        diagnostic->workMode = status.workMode;
        diagnostic->componentLine = status.componentLine;
        diagnostic->reset = status.reset;
        diagnostic->version = status.version;
        diagnostic->portName = portName();
        diagnostic->message = targetOk
                ? QStringLiteral("控制器连接正常，目标侧探测正常。")
                : QStringLiteral("控制器连接正常，目标侧探测未通过：%1").arg(targetError);
        diagnostic->extras = extras;
    }
    clearFailure();
    return true;
}

QVariantMap ControllerDeviceBackend::buildPreflightReport(QStringList* errors, QStringList* warnings) const
{
    QStringList localErrors;
    QStringList localWarnings;

    const QVariantMap cfg = buildRtuConfig();
    const QString port = cfg.value(QStringLiteral("port")).toString().trimmed();
    const int id = deviceId();
    const int baud = cfg.value(QStringLiteral("baudRate")).toInt();
    const int dataBits = cfg.value(QStringLiteral("dataBits")).toInt();
    const int stopBits = cfg.value(QStringLiteral("stopBits")).toInt();
    const QString parity = normalizedParity(cfg.value(QStringLiteral("parity")).toString());
    const int timeoutMs = cfg.value(QStringLiteral("responseTimeout")).toInt();
    const int retries = cfg.value(QStringLiteral("retryCount")).toInt();
    const QStringList availablePorts = availableSerialPortNames();

    if (port.isEmpty()) {
        localErrors << QStringLiteral("未指定串口端口");
    } else if (!availablePorts.isEmpty() && !serialPortNameMatches(availablePorts, port)) {
        localWarnings << QStringLiteral("当前系统未检测到串口 %1，可用端口：%2")
                         .arg(port, availablePorts.join(QStringLiteral(", ")));
    }
    if (!ControllerDebugProtocol::canReadDeviceId(id)) {
        localErrors << QStringLiteral("控制器装置号必须为 1..63，当前为 %1").arg(id);
    }
    if (baud <= 0) {
        localErrors << QStringLiteral("波特率无效：%1").arg(baud);
    }
    if (dataBits < 5 || dataBits > 8) {
        localErrors << QStringLiteral("数据位无效：%1").arg(dataBits);
    }
    if (stopBits < 1 || stopBits > 2) {
        localErrors << QStringLiteral("停止位无效：%1").arg(stopBits);
    }
    const QSet<QString> validParity = {
        QStringLiteral("none"),
        QStringLiteral("even"),
        QStringLiteral("odd"),
        QStringLiteral("space"),
        QStringLiteral("mark")
    };
    if (!parity.isEmpty() && !validParity.contains(parity)) {
        localErrors << QStringLiteral("校验位无效：%1").arg(cfg.value(QStringLiteral("parity")).toString());
    }
    if (timeoutMs <= 0) {
        localWarnings << QStringLiteral("响应超时未配置，将使用通信层默认值");
    }
    if (retries < 0) {
        localWarnings << QStringLiteral("重试次数为负数，将使用通信层默认值");
    }

    const QVariantMap probe = targetProbeConfig();
    const int probeSlaveId = probe.value(QStringLiteral("slaveId"), targetDeviceId()).toInt();
    const int probeAddress = probe.value(QStringLiteral("address"), -1).toInt();
    const int probeCount = probe.value(QStringLiteral("count"), 1).toInt();
    if (!ControllerDebugProtocol::canReadDeviceId(probeSlaveId)) {
        localErrors << QStringLiteral("目标探测装置号必须为 1..63，当前为 %1").arg(probeSlaveId);
    }
    if (probeAddress < 0) {
        localErrors << QStringLiteral("目标探测寄存器地址无效：%1").arg(probeAddress);
    }
    if (probeCount <= 0 || probeCount > 125) {
        localErrors << QStringLiteral("目标探测寄存器数量必须为 1..125，当前为 %1").arg(probeCount);
    }
    if (m_pointDefinitions.size() > 0 && m_pointMappings.isEmpty()) {
        localWarnings << QStringLiteral("当前项目没有可直接映射到 holding register 的运行点位");
    } else if (!m_unmappedPointReasons.isEmpty()) {
        localWarnings << QStringLiteral("当前项目存在 %1 个未映射运行点位")
                         .arg(m_unmappedPointReasons.size());
    }

    QVariantMap serial;
    serial.insert(QStringLiteral("port"), port);
    serial.insert(QStringLiteral("availablePorts"), stringListToVariantList(availablePorts));
    serial.insert(QStringLiteral("baudRate"), baud);
    serial.insert(QStringLiteral("parity"), cfg.value(QStringLiteral("parity")).toString());
    serial.insert(QStringLiteral("dataBits"), dataBits);
    serial.insert(QStringLiteral("stopBits"), stopBits);
    serial.insert(QStringLiteral("responseTimeout"), timeoutMs);
    serial.insert(QStringLiteral("retryCount"), retries);

    const OpcServerConfig opcConfig = m_config.opcServer;
    const SerialModeParts opcSerialMode = parseSerialMode(opcConfig.serialMode);
    QVariantMap opc;
    opc.insert(QStringLiteral("enabled"), opcConfig.enabled);
    opc.insert(QStringLiteral("progId"), opcConfig.opcProgId);
    opc.insert(QStringLiteral("progIdRegistered"), opcProgIdRegistered(opcConfig.opcProgId));
    opc.insert(QStringLiteral("channelName"), opcConfig.channelName);
    opc.insert(QStringLiteral("deviceName"), opcConfig.deviceName);
    opc.insert(QStringLiteral("serialMode"), opcConfig.serialMode);
    opc.insert(QStringLiteral("serialModeValid"), opcSerialMode.valid);
    opc.insert(QStringLiteral("timeoutMs"), opcConfig.timeoutMs);
    opc.insert(QStringLiteral("reconnectDelayMs"), opcConfig.reconnectDelayMs);
    opc.insert(QStringLiteral("retries"), opcConfig.retries);
    if (opcSerialMode.valid) {
        opc.insert(QStringLiteral("baudRate"), opcSerialMode.baudRate);
        opc.insert(QStringLiteral("parity"), opcSerialMode.parity);
        opc.insert(QStringLiteral("dataBits"), opcSerialMode.dataBits);
        opc.insert(QStringLiteral("stopBits"), opcSerialMode.stopBits);
    }

    if (opcConfig.enabled) {
        if (opcConfig.opcProgId.trimmed().isEmpty()) {
            localErrors << QStringLiteral("OPC ProgID 为空");
        } else if (!opc.value(QStringLiteral("progIdRegistered")).toBool()) {
            localWarnings << QStringLiteral("当前系统未确认 OPC ProgID 已注册：%1")
                             .arg(opcConfig.opcProgId);
        }

        if (opcConfig.deviceName.trimmed().isEmpty()) {
            localWarnings << QStringLiteral("OPC 设备端口为空");
        } else if (!port.isEmpty()
                   && opcConfig.deviceName.compare(port, Qt::CaseInsensitive) != 0) {
            localWarnings << QStringLiteral("控制器串口 %1 与 OPC 设备端口 %2 不一致")
                             .arg(port, opcConfig.deviceName);
        }

        if (!opcSerialMode.valid) {
            localErrors << QStringLiteral("OPC 串口模式格式无效：%1，应为 19200,N,8,1 这类格式")
                           .arg(opcConfig.serialMode);
        } else {
            if (opcSerialMode.baudRate != baud) {
                localWarnings << QStringLiteral("控制器波特率 %1 与 OPC 波特率 %2 不一致")
                                 .arg(baud)
                                 .arg(opcSerialMode.baudRate);
            }
            if (opcSerialMode.parity != parity) {
                localWarnings << QStringLiteral("控制器校验位 %1 与 OPC 校验位 %2 不一致")
                                 .arg(parity, opcSerialMode.parity);
            }
            if (opcSerialMode.dataBits != dataBits) {
                localWarnings << QStringLiteral("控制器数据位 %1 与 OPC 数据位 %2 不一致")
                                 .arg(dataBits)
                                 .arg(opcSerialMode.dataBits);
            }
            if (opcSerialMode.stopBits != stopBits) {
                localWarnings << QStringLiteral("控制器停止位 %1 与 OPC 停止位 %2 不一致")
                                 .arg(stopBits)
                                 .arg(opcSerialMode.stopBits);
            }
        }
    }

    QVariantMap report;
    report.insert(QStringLiteral("valid"), localErrors.isEmpty());
    report.insert(QStringLiteral("serial"), serial);
    report.insert(QStringLiteral("opc"), opc);
    report.insert(QStringLiteral("deviceId"), id);
    report.insert(QStringLiteral("targetDeviceId"), targetDeviceId());
    report.insert(QStringLiteral("targetProbe"), probe);
    report.insert(QStringLiteral("pointMappings"), pointMappingSummary());
    report.insert(QStringLiteral("errors"), stringListToVariantList(localErrors));
    report.insert(QStringLiteral("warnings"), stringListToVariantList(localWarnings));

    if (errors) {
        *errors = localErrors;
    }
    if (warnings) {
        *warnings = localWarnings;
    }
    return report;
}

QVariantMap ControllerDeviceBackend::targetProbeConfig() const
{
    QVariantMap probe = m_config.bridge.parameters.value(QStringLiteral("targetProbe")).toMap();
    if (probe.isEmpty()) {
        probe = m_config.transport.parameters.value(QStringLiteral("targetProbe")).toMap();
    }
    if (probe.isEmpty()) {
        probe.insert(QStringLiteral("slaveId"), targetDeviceId());
        probe.insert(QStringLiteral("address"), ControllerDebugProtocol::address(ControllerSystemRegister::Version));
        probe.insert(QStringLiteral("count"), 1);
    }
    return probe;
}

bool ControllerDeviceBackend::probeTarget(QString* errorMessage)
{
    if (!m_client || !m_client->isConnected()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("控制器调试客户端未连接。");
        }
        return false;
    }

    const QVariantMap probe = targetProbeConfig();
    const int slaveId = probe.value(QStringLiteral("slaveId"), targetDeviceId()).toInt();
    const int address = probe.value(QStringLiteral("address"),
                                    ControllerDebugProtocol::address(ControllerSystemRegister::Version)).toInt();
    const int count = qMax(1, probe.value(QStringLiteral("count"), 1).toInt());

    QVector<quint16> values;
    const bool ok = m_client->readHoldingRegisters(slaveId, address, count, &values);
    QMutexLocker lock(&m_mutex);
    m_targetDeviceId = slaveId;
    m_targetOnline = ok;
    m_lastTargetProbe = probe;
    m_lastTargetProbe.insert(QStringLiteral("ok"), ok);
    if (ok) {
        QVariantList list;
        for (quint16 value : values) {
            list.append(value);
        }
        m_lastTargetProbe.insert(QStringLiteral("values"), list);
        return true;
    }

    const CommError err = currentDebugError(QStringLiteral("目标侧探测失败。"));
    m_lastTargetProbe.insert(QStringLiteral("error"), err.message);
    if (errorMessage) {
        *errorMessage = err.message;
    }
    return false;
}
