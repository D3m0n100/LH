// 文件：src/communication/CommTypes.h
// 通信模块通用类型定义 - 错误枚举、配置结构体等

#ifndef COMMTYPES_H
#define COMMTYPES_H

#include <QString>
#include <QVariantMap>
#include <QMetaType>
#include <QDateTime>
#include <QtGlobal>

// ============================================================================
// 错误码枚举
// ============================================================================

enum class CommErrorCode
{
    NoError = 0,
    
    // 连接相关 (1xx)
    ConnectionFailed = 100,
    ConnectionLost = 101,
    ConnectionTimeout = 102,
    DeviceNotFound = 103,
    DeviceBusy = 104,
    PermissionDenied = 105,
    
    // 配置相关 (2xx)
    InvalidConfig = 200,
    UnsupportedBaudRate = 201,
    UnsupportedProtocol = 202,
    InvalidAddress = 203,
    InvalidParameter = 204,
    
    // 传输相关 (3xx)
    SendFailed = 300,
    ReceiveTimeout = 301,
    ReceiveFailed = 302,
    BufferOverflow = 303,
    DataTooLarge = 304,
    
    // 协议相关 (4xx)
    CrcError = 400,
    FrameError = 401,
    ProtocolError = 402,
    InvalidResponse = 403,
    AddressMismatch = 404,
    
    // 系统相关 (5xx)
    ResourceError = 500,
    InternalError = 501,
    NotImplemented = 502,
    OperationCancelled = 503,
    
    // 未知错误
    UnknownError = 999
};

inline QString commErrorCodeToString(CommErrorCode code)
{
    switch (code) {
    case CommErrorCode::NoError: return QStringLiteral("NoError");
    case CommErrorCode::ConnectionFailed: return QStringLiteral("ConnectionFailed");
    case CommErrorCode::ConnectionLost: return QStringLiteral("ConnectionLost");
    case CommErrorCode::ConnectionTimeout: return QStringLiteral("ConnectionTimeout");
    case CommErrorCode::DeviceNotFound: return QStringLiteral("DeviceNotFound");
    case CommErrorCode::DeviceBusy: return QStringLiteral("DeviceBusy");
    case CommErrorCode::PermissionDenied: return QStringLiteral("PermissionDenied");
    case CommErrorCode::InvalidConfig: return QStringLiteral("InvalidConfig");
    case CommErrorCode::UnsupportedBaudRate: return QStringLiteral("UnsupportedBaudRate");
    case CommErrorCode::UnsupportedProtocol: return QStringLiteral("UnsupportedProtocol");
    case CommErrorCode::InvalidAddress: return QStringLiteral("InvalidAddress");
    case CommErrorCode::InvalidParameter: return QStringLiteral("InvalidParameter");
    case CommErrorCode::SendFailed: return QStringLiteral("SendFailed");
    case CommErrorCode::ReceiveTimeout: return QStringLiteral("ReceiveTimeout");
    case CommErrorCode::ReceiveFailed: return QStringLiteral("ReceiveFailed");
    case CommErrorCode::BufferOverflow: return QStringLiteral("BufferOverflow");
    case CommErrorCode::DataTooLarge: return QStringLiteral("DataTooLarge");
    case CommErrorCode::CrcError: return QStringLiteral("CrcError");
    case CommErrorCode::FrameError: return QStringLiteral("FrameError");
    case CommErrorCode::ProtocolError: return QStringLiteral("ProtocolError");
    case CommErrorCode::InvalidResponse: return QStringLiteral("InvalidResponse");
    case CommErrorCode::AddressMismatch: return QStringLiteral("AddressMismatch");
    case CommErrorCode::ResourceError: return QStringLiteral("ResourceError");
    case CommErrorCode::InternalError: return QStringLiteral("InternalError");
    case CommErrorCode::NotImplemented: return QStringLiteral("NotImplemented");
    case CommErrorCode::OperationCancelled: return QStringLiteral("OperationCancelled");
    case CommErrorCode::UnknownError:
    default:
        return QStringLiteral("UnknownError");
    }
}

inline bool commErrorCodeIsTransportFailure(CommErrorCode code)
{
    return code == CommErrorCode::ConnectionLost
            || code == CommErrorCode::ConnectionFailed
            || code == CommErrorCode::ConnectionTimeout
            || code == CommErrorCode::ReceiveTimeout
            || code == CommErrorCode::SendFailed
            || code == CommErrorCode::ReceiveFailed;
}

inline bool commErrorCodeIsVerificationFailure(CommErrorCode code)
{
    return code == CommErrorCode::InvalidResponse
            || code == CommErrorCode::ProtocolError
            || code == CommErrorCode::CrcError
            || code == CommErrorCode::FrameError
            || code == CommErrorCode::AddressMismatch;
}

// ============================================================================
// 协议类型枚举
// ============================================================================

enum class CommProtocolType
{
    Unknown = 0,
    Serial,
    ModbusRTU,
    ModbusTCP,
    CAN,
    CANOpen,
    J1939,
    EthernetTCP,
    EthernetUDP,
    Custom
};

// ============================================================================
// 统一错误结构体
// ============================================================================

struct CommError
{
    CommProtocolType protocol = CommProtocolType::Unknown;
    CommErrorCode code = CommErrorCode::NoError;
    QString message;
    QString details;        // 额外的调试信息
    qint64 timestamp = 0;   // 错误发生时间戳 (ms since epoch)
    
    CommError() = default;
    
    CommError(CommProtocolType proto, CommErrorCode c, const QString& msg, const QString& det = QString())
        : protocol(proto)
        , code(c)
        , message(msg)
        , details(det)
        , timestamp(QDateTime::currentMSecsSinceEpoch())
    {}
    
    bool isError() const { return code != CommErrorCode::NoError; }
    bool isTimeout() const { 
        return code == CommErrorCode::ConnectionTimeout || 
               code == CommErrorCode::ReceiveTimeout; 
    }
    
    QString toString() const {
        return QString("[%1] %2: %3%4")
            .arg(protocolToString(protocol))
            .arg(static_cast<int>(code))
            .arg(message)
            .arg(details.isEmpty() ? "" : (" - " + details));
    }
    
    static QString protocolToString(CommProtocolType type) {
        switch (type) {
            case CommProtocolType::Serial:      return "Serial";
            case CommProtocolType::ModbusRTU:   return "ModbusRTU";
            case CommProtocolType::ModbusTCP:   return "ModbusTCP";
            case CommProtocolType::CAN:         return "CAN";
            case CommProtocolType::CANOpen:     return "CANOpen";
            case CommProtocolType::J1939:       return "J1939";
            case CommProtocolType::EthernetTCP: return "TCP";
            case CommProtocolType::EthernetUDP: return "UDP";
            case CommProtocolType::Custom:      return "Custom";
            default:                            return "Unknown";
        }
    }
};

// ============================================================================
// 设备后端状态快照
// ============================================================================

struct BackendStatusSnapshot
{
    bool online = false;
    QString backendType;
    bool downloading = false;
    int downloadPercent = 0;
    CommErrorCode lastErrorCode = CommErrorCode::NoError;
    QString lastErrorMessage;
    QString lastErrorDetails;
    bool partialSuccess = false;
    QDateTime timestamp = QDateTime::currentDateTimeUtc();
    QVariantMap extras;
};

// ============================================================================
// 配置基类
// ============================================================================

struct CommConfigBase
{
    virtual ~CommConfigBase() = default;
    virtual QVariantMap toVariantMap() const = 0;
    virtual bool fromVariantMap(const QVariantMap& map) = 0;
    virtual bool isValid() const = 0;
};

// ============================================================================
// 串口配置
// ============================================================================

struct SerialConfig : public CommConfigBase
{
    QString portName;
    int baudRate = 9600;
    int dataBits = 8;
    int stopBits = 1;           // 1 or 2
    QString parity = "None";    // "None", "Even", "Odd"
    QString flowControl = "None"; // "None", "Hardware", "Software"
    bool rs485Mode = false;
    bool rs485DirectionControl = false;
    int frameTimeout = 50;      // ms, 用于帧分割
    
    QVariantMap toVariantMap() const override {
        return {
            {"port", portName},
            {"baudRate", baudRate},
            {"dataBits", dataBits},
            {"stopBits", stopBits},
            {"parity", parity},
            {"flowControl", flowControl},
            {"rs485Mode", rs485Mode},
            {"rs485DirectionControl", rs485DirectionControl},
            {"frameTimeout", frameTimeout}
        };
    }
    
    bool fromVariantMap(const QVariantMap& map) override {
        portName = map.value("port").toString();
        baudRate = map.value("baudRate", 9600).toInt();
        dataBits = map.value("dataBits", 8).toInt();
        stopBits = map.value("stopBits", 1).toInt();
        parity = map.value("parity", "None").toString();
        flowControl = map.value("flowControl", "None").toString();
        rs485Mode = map.value("rs485Mode", false).toBool();
        rs485DirectionControl = map.value("rs485DirectionControl", false).toBool();
        frameTimeout = map.value("frameTimeout", 50).toInt();
        return true;
    }
    
    bool isValid() const override {
        return !portName.isEmpty() && baudRate > 0 && dataBits >= 5 && dataBits <= 8;
    }
    
    static SerialConfig fromMap(const QVariantMap& map) {
        SerialConfig cfg;
        cfg.fromVariantMap(map);
        return cfg;
    }
};

// ============================================================================
// Modbus 配置
// ============================================================================

struct ModbusConfig : public CommConfigBase
{
    enum class Mode { RTU, TCP };
    enum class StationType { Master, Slave };
    
    Mode mode = Mode::RTU;
    StationType stationType = StationType::Master;
    int stationAddress = 1;
    int pollInterval = 1000;    // ms
    int responseTimeout = 1000; // ms
    int retryCount = 3;
    
    // RTU 特有配置
    QString portName;
    int baudRate = 9600;
    int dataBits = 8;
    int stopBits = 1;
    QString parity = "None";
    
    // TCP 特有配置
    QString host = "127.0.0.1";
    int port = 502;
    
    QVariantMap toVariantMap() const override {
        QVariantMap map = {
            {"mode", mode == Mode::TCP ? "TCP" : "RTU"},
            {"type", stationType == StationType::Slave ? "Slave" : "Master"},
            {"address", stationAddress},
            {"pollInterval", pollInterval},
            {"responseTimeout", responseTimeout},
            {"retryCount", retryCount}
        };
        if (mode == Mode::RTU) {
            map["port"] = portName;
            map["baudRate"] = baudRate;
            map["dataBits"] = dataBits;
            map["stopBits"] = stopBits;
            map["parity"] = parity;
        } else {
            map["host"] = host;
            map["tcpPort"] = port;
        }
        return map;
    }
    
    bool fromVariantMap(const QVariantMap& map) override {
        QString modeStr = map.value("mode", "RTU").toString().toUpper();
        mode = (modeStr == "TCP") ? Mode::TCP : Mode::RTU;
        
        QString typeStr = map.value("type", "Master").toString().toUpper();
        stationType = (typeStr == "SLAVE") ? StationType::Slave : StationType::Master;
        
        stationAddress = map.value("address", 1).toInt();
        pollInterval = map.value("pollInterval", 1000).toInt();
        responseTimeout = map.value("responseTimeout", 1000).toInt();
        retryCount = map.value("retryCount", 3).toInt();
        
        if (mode == Mode::RTU) {
            portName = map.value("port").toString();
            baudRate = map.value("baudRate", 9600).toInt();
            dataBits = map.value("dataBits", 8).toInt();
            stopBits = map.value("stopBits", 1).toInt();
            parity = map.value("parity", "None").toString();
        } else {
            host = map.value("host", "127.0.0.1").toString();
            port = map.value("tcpPort", map.value("port", 502)).toInt();
        }
        return true;
    }
    
    bool isValid() const override {
        if (mode == Mode::RTU) {
            return !portName.isEmpty() && baudRate > 0;
        } else {
            return !host.isEmpty() && port > 0 && port < 65536;
        }
    }
    
    static ModbusConfig fromMap(const QVariantMap& map) {
        ModbusConfig cfg;
        cfg.fromVariantMap(map);
        return cfg;
    }
};

// ============================================================================
// CAN 配置
// ============================================================================

struct CanConfig : public CommConfigBase
{
    QString plugin = "socketcan";       // Qt CAN 插件名
    QString interface = "can0";         // 接口名
    int bitrate = 500000;
    bool extendedFrame = false;
    quint32 defaultFrameId = 0x100;
    
    // 过滤器配置
    bool useFilter = false;
    quint32 filterMask = 0;
    quint32 filterId = 0;
    
    QVariantMap toVariantMap() const override {
        return {
            {"plugin", plugin},
            {"interface", interface},
            {"bitrate", bitrate},
            {"extended", extendedFrame},
            {"frameId", defaultFrameId},
            {"useFilter", useFilter},
            {"filterMask", filterMask},
            {"filterId", filterId}
        };
    }
    
    bool fromVariantMap(const QVariantMap& map) override {
        plugin = map.value("plugin", "socketcan").toString();
        interface = map.value("interface", "can0").toString();
        bitrate = map.value("bitrate", 500000).toInt();
        extendedFrame = map.value("extended", false).toBool();
        defaultFrameId = map.value("frameId", 0x100).toUInt();
        useFilter = map.value("useFilter", false).toBool();
        filterMask = map.value("filterMask", 0u).toUInt();
        filterId = map.value("filterId", 0u).toUInt();
        return true;
    }
    
    bool isValid() const override {
        return !plugin.isEmpty() && !interface.isEmpty() && bitrate > 0;
    }
    
    static CanConfig fromMap(const QVariantMap& map) {
        CanConfig cfg;
        cfg.fromVariantMap(map);
        return cfg;
    }
};

// ============================================================================
// CANOpen 配置
// ============================================================================

struct CANOpenConfig : public CanConfig
{
    quint8 nodeId = 1;
    quint8 functionCode = 0x3;  // 默认 PDO1 Tx
    quint16 cobId = 0;          // 如果为0，则自动计算
    
    QVariantMap toVariantMap() const override {
        QVariantMap map = CanConfig::toVariantMap();
        map["nodeId"] = nodeId;
        map["functionCode"] = functionCode;
        map["cobId"] = cobId;
        return map;
    }
    
    bool fromVariantMap(const QVariantMap& map) override {
        CanConfig::fromVariantMap(map);
        nodeId = static_cast<quint8>(map.value("nodeId", 1).toUInt());
        functionCode = static_cast<quint8>(map.value("functionCode", 0x3).toUInt());
        cobId = static_cast<quint16>(map.value("cobId", 0).toUInt());
        return true;
    }
    
    static CANOpenConfig fromMap(const QVariantMap& map) {
        CANOpenConfig cfg;
        cfg.fromVariantMap(map);
        return cfg;
    }
};

// ============================================================================
// J1939 配置
// ============================================================================

struct J1939Config : public CanConfig
{
    quint8 priority = 6;
    quint32 pgn = 0;
    quint8 sourceAddress = 0x80;
    quint8 destinationAddress = 0xFF;
    bool usePDU1 = false;
    
    QVariantMap toVariantMap() const override {
        QVariantMap map = CanConfig::toVariantMap();
        map["priority"] = priority;
        map["pgn"] = pgn;
        map["sourceAddress"] = sourceAddress;
        map["destinationAddress"] = destinationAddress;
        return map;
    }
    
    bool fromVariantMap(const QVariantMap& map) override {
        CanConfig::fromVariantMap(map);
        priority = static_cast<quint8>(map.value("priority", 6).toUInt());
        pgn = map.value("pgn", 0u).toUInt();
        sourceAddress = static_cast<quint8>(map.value("sourceAddress", 0x80).toUInt());
        destinationAddress = static_cast<quint8>(map.value("destinationAddress", 0xFF).toUInt());
        extendedFrame = true; // J1939 always uses extended frames
        return true;
    }
    
    static J1939Config fromMap(const QVariantMap& map) {
        J1939Config cfg;
        cfg.fromVariantMap(map);
        return cfg;
    }
};

// ============================================================================
// 以太网配置
// ============================================================================

struct EthernetConfig : public CommConfigBase
{
    enum class Protocol { TCP, UDP };
    enum class Role { Client, Server };
    
    Protocol protocol = Protocol::TCP;
    Role role = Role::Client;
    QString host = "127.0.0.1";
    quint16 port = 8080;
    int connectTimeout = 3000;      // ms
    int keepAliveInterval = 30000;  // ms, 0 = disabled
    int receiveBufferSize = 65536;
    
    QVariantMap toVariantMap() const override {
        return {
            {"protocol", protocol == Protocol::UDP ? "UDP" : "TCP"},
            {"role", role == Role::Server ? "Server" : "Client"},
            {"host", host},
            {"port", port},
            {"connectTimeout", connectTimeout},
            {"keepAliveInterval", keepAliveInterval},
            {"receiveBufferSize", receiveBufferSize}
        };
    }
    
    bool fromVariantMap(const QVariantMap& map) override {
        QString protoStr = map.value("protocol", "TCP").toString().toUpper();
        protocol = (protoStr == "UDP") ? Protocol::UDP : Protocol::TCP;
        
        QString roleStr = map.value("role", "Client").toString().toUpper();
        role = (roleStr == "SERVER") ? Role::Server : Role::Client;
        
        host = map.value("host", "127.0.0.1").toString();
        port = static_cast<quint16>(map.value("port", 8080).toUInt());
        connectTimeout = map.value("connectTimeout", 3000).toInt();
        keepAliveInterval = map.value("keepAliveInterval", 30000).toInt();
        receiveBufferSize = map.value("receiveBufferSize", 65536).toInt();
        return true;
    }
    
    bool isValid() const override {
        return !host.isEmpty() && port > 0;
    }
    
    static EthernetConfig fromMap(const QVariantMap& map) {
        EthernetConfig cfg;
        cfg.fromVariantMap(map);
        return cfg;
    }
};

// ============================================================================
// Qt 元类型注册
// ============================================================================

Q_DECLARE_METATYPE(CommError)
Q_DECLARE_METATYPE(CommErrorCode)
Q_DECLARE_METATYPE(CommProtocolType)
Q_DECLARE_METATYPE(BackendStatusSnapshot)

#endif // COMMTYPES_H
