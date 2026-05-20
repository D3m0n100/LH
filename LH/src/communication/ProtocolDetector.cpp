#include "ProtocolDetector.h"
#include <QDebug>
#include <QThread>
#include <QEventLoop>
#include <QTimer>

namespace {
static void waitWithEventLoop(int ms)
{
    if (ms <= 0) {
        return;
    }
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(ms);
    loop.exec();
}
} // namespace

ProtocolDetector::ProtocolDetector(QObject* parent)
    : QObject(parent)
    , m_detectionTimer(new QTimer(this))
    , m_currentInterface(nullptr)
    , m_timeout(5000)
    , m_detectionActive(false)
    , m_totalTests(0)
    , m_successfulTests(0)
{
    m_detectionTimer->setSingleShot(true);
    connect(m_detectionTimer, &QTimer::timeout, this, &ProtocolDetector::onDetectionTimeout);
}

ProtocolDetector::~ProtocolDetector()
{
    if (m_detectionActive) {
        m_detectionTimer->stop();
    }
}

ProtocolDetector::ProtocolInfo ProtocolDetector::detectProtocol(ICommInterface* interface, int timeout_ms)
{
    if (!interface) {
        emit detectionFailed("Invalid interface");
        return ProtocolInfo{ProtocolType::Unknown, "Unknown", "Invalid interface", QVariantMap(), 0};
    }
    
    m_currentInterface = interface;
    m_timeout = timeout_ms;
    m_detectionActive = true;
    m_elapsedTimer.start();
    
    // 尝试检测各种协议
    QList<ProtocolInfo> results = detectAllProtocols(interface, timeout_ms);
    
    // 选择置信度最高的协议
    ProtocolInfo bestMatch{ProtocolType::Unknown, "Unknown", "No matching protocol found", QVariantMap(), 0};
    
    for (const auto& result : results) {
        if (result.confidence > bestMatch.confidence) {
            bestMatch = result;
        }
    }
    
    m_detectionActive = false;
    
    if (bestMatch.type != ProtocolType::Unknown) {
        emit protocolDetected(bestMatch);
        return bestMatch;
    } else {
        emit detectionFailed("No protocol detected");
        return bestMatch;
    }
}

QList<ProtocolDetector::ProtocolInfo> ProtocolDetector::detectAllProtocols(ICommInterface* interface, int timeout_ms)
{
    QList<ProtocolInfo> results;
    
    if (!interface) {
        return results;
    }
    
    m_currentInterface = interface;
    m_timeout = timeout_ms;
    m_totalTests = 0;
    m_successfulTests = 0;
    
    // 检测各种协议
    results.append(detectModbusRTU(interface));
    results.append(detectModbusTCP(interface));
    results.append(detectCANOpen(interface));
    results.append(detectJ1939(interface));
    results.append(detectRawCAN(interface));
    
    // 按置信度排序
    std::sort(results.begin(), results.end(), [](const ProtocolInfo& a, const ProtocolInfo& b) {
        return a.confidence > b.confidence;
    });
    
    return results;
}

bool ProtocolDetector::verifyProtocol(ICommInterface* interface, ProtocolType type, int timeout_ms)
{
    if (!interface) {
        return false;
    }
    
    m_currentInterface = interface;
    m_timeout = timeout_ms;
    m_elapsedTimer.start();
    
    ProtocolInfo info;
    
    switch (type) {
    case ProtocolType::ModbusRTU:
        info = detectModbusRTU(interface);
        break;
    case ProtocolType::ModbusTCP:
        info = detectModbusTCP(interface);
        break;
    case ProtocolType::CANOpen:
        info = detectCANOpen(interface);
        break;
    case ProtocolType::J1939:
        info = detectJ1939(interface);
        break;
    case ProtocolType::RawCAN:
        info = detectRawCAN(interface);
        break;
    default:
        return false;
    }
    
    return info.confidence > 50; // 置信度大于50%认为验证通过
}

QList<ProtocolDetector::ProtocolType> ProtocolDetector::getSupportedProtocols()
{
    return {
        ProtocolType::ModbusRTU,
        ProtocolType::ModbusTCP,
        ProtocolType::CANOpen,
        ProtocolType::J1939,
        ProtocolType::RawCAN
    };
}

QString ProtocolDetector::protocolTypeToString(ProtocolType type)
{
    switch (type) {
    case ProtocolType::ModbusRTU: return "ModbusRTU";
    case ProtocolType::ModbusTCP: return "ModbusTCP";
    case ProtocolType::CANOpen: return "CANOpen";
    case ProtocolType::J1939: return "J1939";
    case ProtocolType::RawCAN: return "RawCAN";
    default: return "Unknown";
    }
}

ProtocolDetector::ProtocolType ProtocolDetector::stringToProtocolType(const QString& str)
{
    QString upperStr = str.toUpper();
    if (upperStr == "MODBUSRTU") return ProtocolType::ModbusRTU;
    if (upperStr == "MODBUSTCP") return ProtocolType::ModbusTCP;
    if (upperStr == "CANOPEN") return ProtocolType::CANOpen;
    if (upperStr == "J1939") return ProtocolType::J1939;
    if (upperStr == "RAWCAN") return ProtocolType::RawCAN;
    return ProtocolType::Unknown;
}

ProtocolDetector::ProtocolInfo ProtocolDetector::detectModbusRTU(ICommInterface* interface)
{
    ProtocolInfo info{ProtocolType::ModbusRTU, "ModbusRTU", "Modbus RTU protocol", QVariantMap(), 0};
    
    // 发送Modbus RTU探测帧
    // 读取从站1的保持寄存器0，1个寄存器
    QByteArray probeFrame;
    probeFrame.append(char(0x01)); // 从站地址
    probeFrame.append(char(0x03)); // 功能码：读保持寄存器
    probeFrame.append(char(0x00)); // 起始地址高字节
    probeFrame.append(char(0x00)); // 起始地址低字节
    probeFrame.append(char(0x00)); // 寄存器数量高字节
    probeFrame.append(char(0x01)); // 寄存器数量低字节
    
    // 计算CRC
    quint16 crc = calculateCRC16(probeFrame);
    probeFrame.append(char(crc & 0xFF));        // CRC低字节
    probeFrame.append(char((crc >> 8) & 0xFF)); // CRC高字节
    
    // 发送探测帧
    if (interface->send(probeFrame) != probeFrame.size()) {
        return info;
    }
    
    // 等待响应
    waitWithEventLoop(100);
    
    QByteArray response = interface->receive(500);
    if (response.size() >= 5 && analyzeModbusRTUFrame(response)) {
        info.confidence = 90;
        info.description = "Modbus RTU protocol detected";
        
        // 解析检测到的参数
        info.detectedParams["slaveAddress"] = static_cast<int>(response[0]);
        info.detectedParams["functionCode"] = static_cast<int>(response[1]);
        info.detectedParams["frameLength"] = response.size();
    }
    
    m_totalTests++;
    if (info.confidence > 0) m_successfulTests++;
    
    return info;
}

ProtocolDetector::ProtocolInfo ProtocolDetector::detectModbusTCP(ICommInterface* interface)
{
    ProtocolInfo info{ProtocolType::ModbusTCP, "ModbusTCP", "Modbus TCP protocol", QVariantMap(), 0};
    
    // Modbus TCP探测帧
    QByteArray probeFrame;
    // MBAP头部
    probeFrame.append(char(0x00)); // 事务标识符高字节
    probeFrame.append(char(0x01)); // 事务标识符低字节
    probeFrame.append(char(0x00)); // 协议标识符高字节
    probeFrame.append(char(0x00)); // 协议标识符低字节
    probeFrame.append(char(0x00)); // 长度高字节
    probeFrame.append(char(0x06)); // 长度低字节 (6字节)
    probeFrame.append(char(0x01)); // 单元标识符 (从站地址)
    // PDU
    probeFrame.append(char(0x03)); // 功能码：读保持寄存器
    probeFrame.append(char(0x00)); // 起始地址高字节
    probeFrame.append(char(0x00)); // 起始地址低字节
    probeFrame.append(char(0x00)); // 寄存器数量高字节
    probeFrame.append(char(0x01)); // 寄存器数量低字节
    
    // 发送探测帧
    if (interface->send(probeFrame) != probeFrame.size()) {
        return info;
    }
    
    // 等待响应
    waitWithEventLoop(100);
    
    QByteArray response = interface->receive(500);
    if (response.size() >= 12 && analyzeModbusTCPFrame(response)) {
        info.confidence = 90;
        info.description = "Modbus TCP protocol detected";
        
        info.detectedParams["transactionId"] = (response[0] << 8) | response[1];
        info.detectedParams["protocolId"] = (response[2] << 8) | response[3];
        info.detectedParams["length"] = (response[4] << 8) | response[5];
        info.detectedParams["unitId"] = static_cast<int>(response[6]);
    }
    
    m_totalTests++;
    if (info.confidence > 0) m_successfulTests++;
    
    return info;
}

ProtocolDetector::ProtocolInfo ProtocolDetector::detectCANOpen(ICommInterface* interface)
{
    ProtocolInfo info{ProtocolType::CANOpen, "CANOpen", "CANopen protocol", QVariantMap(), 0};
    
    // CANopen通常使用特定的COB-ID范围
    // 发送一个SDO读取请求到节点1的对象字典
    
    QByteArray probeFrame;
    probeFrame.append(char(0x60)); // COB-ID: SDO请求 (0x600 + 节点ID)
    probeFrame.append(char(0x40)); // 客户端命令符: 读取请求
    probeFrame.append(char(0x00)); // 索引低字节 (设备类型)
    probeFrame.append(char(0x10)); // 索引高字节
    probeFrame.append(char(0x00)); // 子索引
    probeFrame.append(char(0x00)); // 数据0
    probeFrame.append(char(0x00)); // 数据1
    probeFrame.append(char(0x00)); // 数据2
    probeFrame.append(char(0x00)); // 数据3
    
    if (interface->send(probeFrame) != probeFrame.size()) {
        return info;
    }
    
    waitWithEventLoop(100);
    
    QByteArray response = interface->receive(500);
    if (response.size() >= 9 && analyzeCANOpenFrame(response)) {
        info.confidence = 85;
        info.description = "CANopen protocol detected";
        
        info.detectedParams["nodeId"] = static_cast<int>(response[0] & 0x7F);
        info.detectedParams["cobId"] = static_cast<int>(response[0]);
    }
    
    m_totalTests++;
    if (info.confidence > 0) m_successfulTests++;
    
    return info;
}

ProtocolDetector::ProtocolInfo ProtocolDetector::detectJ1939(ICommInterface* interface)
{
    ProtocolInfo info{ProtocolType::J1939, "J1939", "SAE J1939 protocol", QVariantMap(), 0};
    
    // J1939使用29位标识符
    // 发送一个请求PGN 0xEA00 (请求地址)的消息
    
    QByteArray probeFrame;
    // 简化的J1939帧结构
    probeFrame.append(char(0x18)); // 优先级3 + 数据页0
    probeFrame.append(char(0xEA)); // PDU格式 (请求)
    probeFrame.append(char(0x00)); // PDU特定 (PGN低字节)
    probeFrame.append(char(0xFF)); // 源地址 (广播)
    probeFrame.append(char(0xFF)); // 目标地址 (全局)
    probeFrame.append(char(0x00)); // 数据长度
    probeFrame.append(char(0x00)); // 数据: 请求的PGN
    probeFrame.append(char(0x00));
    probeFrame.append(char(0x00));
    
    if (interface->send(probeFrame) != probeFrame.size()) {
        return info;
    }
    
    waitWithEventLoop(100);
    
    QByteArray response = interface->receive(500);
    if (response.size() >= 9 && analyzeJ1939Frame(response)) {
        info.confidence = 80;
        info.description = "J1939 protocol detected";
        
        info.detectedParams["priority"] = static_cast<int>((response[0] >> 2) & 0x07);
        info.detectedParams["dataPage"] = static_cast<int>((response[0] >> 1) & 0x01);
        info.detectedParams["pduFormat"] = static_cast<int>(response[1]);
        info.detectedParams["sourceAddress"] = static_cast<int>(response[4]);
    }
    
    m_totalTests++;
    if (info.confidence > 0) m_successfulTests++;
    
    return info;
}

ProtocolDetector::ProtocolInfo ProtocolDetector::detectRawCAN(ICommInterface* interface)
{
    ProtocolInfo info{ProtocolType::RawCAN, "RawCAN", "Raw CAN protocol", QVariantMap(), 0};
    
    // 发送一个标准的CAN帧
    QByteArray probeFrame;
    probeFrame.append(char(0x12)); // 仲裁ID
    probeFrame.append(char(0x34));
    probeFrame.append(char(0x56)); // 数据长度码
    probeFrame.append(char(0x78));
    probeFrame.append(char(0x01)); // 数据
    probeFrame.append(char(0x02));
    probeFrame.append(char(0x03));
    probeFrame.append(char(0x04));
    
    if (interface->send(probeFrame) != probeFrame.size()) {
        return info;
    }
    
    waitWithEventLoop(100);
    
    QByteArray response = interface->receive(500);
    if (response.size() >= 8 && validateCANChecksum(response)) {
        info.confidence = 70;
        info.description = "Raw CAN protocol detected";
        
        info.detectedParams["frameSize"] = response.size();
        info.detectedParams["hasChecksum"] = true;
    }
    
    m_totalTests++;
    if (info.confidence > 0) m_successfulTests++;
    
    return info;
}

bool ProtocolDetector::analyzeModbusRTUFrame(const QByteArray& data)
{
    if (data.size() < 5) return false;
    
    // 检查基本结构：地址 + 功能码 + 数据 + CRC
    int expectedLength = 5; // 最小长度
    
    // 根据功能码判断数据长度
    switch (data[1]) {
    case 0x01: case 0x02: case 0x03: case 0x04: // 读操作
        if (data.size() >= 5) {
            expectedLength = 5 + (data[2] * 256 + data[3]) * 2; // 寄存器数量 * 2
        }
        break;
    case 0x05: case 0x06: // 写单个
        expectedLength = 8;
        break;
    case 0x0F: case 0x10: // 写多个
        if (data.size() >= 6) {
            expectedLength = 6 + data[6]; // 数据长度
        }
        break;
    }
    
    // 验证CRC
    if (data.size() >= expectedLength) {
        QByteArray frameWithoutCRC = data.left(expectedLength - 2);
        quint16 calculatedCRC = calculateCRC16(frameWithoutCRC);
        quint16 receivedCRC = (data[expectedLength - 1] << 8) | data[expectedLength - 2];
        
        return calculatedCRC == receivedCRC;
    }
    
    return false;
}

bool ProtocolDetector::analyzeModbusTCPFrame(const QByteArray& data)
{
    if (data.size() < 12) return false; // 最小MBAP + PDU长度
    
    // 验证MBAP头部
    quint16 protocolId = (data[2] << 8) | data[3];
    if (protocolId != 0) return false; // Modbus协议标识符必须为0
    
    quint16 length = (data[4] << 8) | data[5];
    if (length + 6 != data.size()) return false; // 长度验证
    
    return true;
}

bool ProtocolDetector::analyzeCANOpenFrame(const QByteArray& data)
{
    if (data.size() < 9) return false;
    
    // 检查COB-ID范围
    quint8 cobId = data[0];
    if ((cobId & 0x80) == 0) return false; // 必须是COB-ID
    
    // 检查功能码
    quint8 functionCode = (cobId >> 7) & 0x0F;
    if (functionCode > 0xF) return false;
    
    return true;
}

bool ProtocolDetector::analyzeJ1939Frame(const QByteArray& data)
{
    if (data.size() < 9) return false;
    
    // 检查优先级范围 (0-7)
    quint8 priority = (data[0] >> 2) & 0x07;
    if (priority > 7) return false;
    
    // 检查数据页 (0-1)
    quint8 dataPage = (data[0] >> 1) & 0x01;
    if (dataPage > 1) return false;
    
    return true;
}

quint16 ProtocolDetector::calculateCRC16(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    
    for (char byte : data) {
        crc ^= static_cast<quint8>(byte);
        
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

bool ProtocolDetector::validateCANChecksum(const QByteArray& data)
{
    // 简化的CAN校验和验证
    // 实际的CAN协议有更复杂的CRC校验
    if (data.size() < 8) return false;
    
    // 这里可以实现更复杂的CAN帧验证逻辑
    return true;
}

void ProtocolDetector::onDetectionTimeout()
{
    if (m_detectionActive) {
        m_detectionActive = false;
        emit detectionFailed("Protocol detection timeout");
    }
}
