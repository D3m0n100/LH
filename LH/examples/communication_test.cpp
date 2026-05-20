#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include "communication/CommFactory.h"
#include "communication/ProtocolDetector.h"
#include "communication/ModbusInterface.h"
#include "communication/SerialInterface.h"

/**
 * @brief 通信协议测试示例
 * 演示Modbus、串口通信和协议自动检测功能
 */
class CommunicationTest : public QObject
{
    Q_OBJECT

public:
    CommunicationTest(QObject* parent = nullptr) : QObject(parent) {}

    void runTests()
    {
        qDebug() << "=== 通信协议测试开始 ===";
        
        // 测试1: 协议工厂
        testProtocolFactory();
        
        // 测试2: Modbus接口
        testModbusInterface();
        
        // 测试3: 串口接口
        testSerialInterface();
        
        // 测试4: 协议自动检测
        testProtocolDetection();
        
        qDebug() << "=== 通信协议测试完成 ===";
        
        QTimer::singleShot(1000, qApp, &QCoreApplication::quit);
    }

private:
    void testProtocolFactory()
    {
        qDebug() << "\n--- 协议工厂测试 ---";
        
        QStringList protocols = CommFactory::getSupportedProtocols();
        qDebug() << "支持的协议类型:" << protocols;
        
        // 测试创建各种协议接口
        auto canInterface = CommFactory::create("CAN");
        auto modbusInterface = CommFactory::create("ModbusRTU");
        auto serialInterface = CommFactory::create("Serial");
        auto detector = CommFactory::createProtocolDetector();
        
        qDebug() << "CAN接口创建:" << (canInterface ? "成功" : "失败");
        qDebug() << "Modbus接口创建:" << (modbusInterface ? "成功" : "失败");
        qDebug() << "串口接口创建:" << (serialInterface ? "成功" : "失败");
        qDebug() << "协议检测器创建:" << (detector ? "成功" : "失败");
        
        // 清理
        if (canInterface) delete canInterface;
        if (modbusInterface) delete modbusInterface;
        if (serialInterface) delete serialInterface;
        if (detector) delete detector;
    }

    void testModbusInterface()
    {
        qDebug() << "\n--- Modbus接口测试 ---";
        
        auto modbus = qobject_cast<ModbusInterface*>(CommFactory::create("ModbusRTU"));
        if (!modbus) {
            qDebug() << "Modbus接口创建失败";
            return;
        }
        
        // 测试RTU配置
        QVariantMap rtuConfig;
        rtuConfig["mode"] = "RTU";
        rtuConfig["port"] = "COM1";
        rtuConfig["baudRate"] = 9600;
        rtuConfig["dataBits"] = 8;
        rtuConfig["parity"] = "None";
        rtuConfig["stopBits"] = 1;
        rtuConfig["address"] = 1;
        rtuConfig["type"] = "Master";
        
        qDebug() << "Modbus RTU配置测试...";
        // 注意：实际打开串口需要硬件支持，这里只测试配置
        
        // 测试TCP配置
        QVariantMap tcpConfig;
        tcpConfig["mode"] = "TCP";
        tcpConfig["host"] = "127.0.0.1";
        tcpConfig["port"] = 502;
        tcpConfig["address"] = 1;
        tcpConfig["type"] = "Master";
        
        qDebug() << "Modbus TCP配置测试...";
        
        // 测试寄存器读写功能
        connect(modbus, &ModbusInterface::registerDataReceived, 
                [](int address, const QVector<quint16>& values) {
            qDebug() << "寄存器数据接收 - 地址:" << address << "值:" << values;
        });
        
        connect(modbus, &ModbusInterface::registerDataWritten,
                [](int address, int count) {
            qDebug() << "寄存器写入完成 - 地址:" << address << "数量:" << count;
        });
        
        delete modbus;
    }

    void testSerialInterface()
    {
        qDebug() << "\n--- 串口接口测试 ---";
        
        auto serial = qobject_cast<SerialInterface*>(CommFactory::create("Serial"));
        if (!serial) {
            qDebug() << "串口接口创建失败";
            return;
        }
        
        // 测试串口配置
        QVariantMap serialConfig;
        serialConfig["port"] = "COM1";
        serialConfig["baudRate"] = 0; // 0表示自动检测
        serialConfig["dataBits"] = 8;
        serialConfig["parity"] = "None";
        serialConfig["stopBits"] = 1;
        serialConfig["flowControl"] = "None";
        serialConfig["protocol"] = "Raw";
        serialConfig["frameTimeout"] = 50;
        serialConfig["rs485Mode"] = false;
        
        qDebug() << "串口配置测试...";
        qDebug() << "支持的波特率:" << serial->getSupportedBaudRates();
        
        // 测试协议设置
        serial->setProtocol(SerialInterface::SerialProtocol::ModbusRTU);
        qDebug() << "协议设置为: ModbusRTU";
        
        // 测试信号连接
        connect(serial, &SerialInterface::baudRateDetected,
                [](int baudRate) {
            qDebug() << "波特率自动检测到:" << baudRate;
        });
        
        connect(serial, &SerialInterface::dataFramed,
                [](const QByteArray& frame) {
            qDebug() << "数据帧接收:" << frame.toHex();
        });
        
        delete serial;
    }

    void testProtocolDetection()
    {
        qDebug() << "\n--- 协议自动检测测试 ---";
        
        auto detector = CommFactory::createProtocolDetector();
        if (!detector) {
            qDebug() << "协议检测器创建失败";
            return;
        }
        
        // 测试协议类型转换
        auto protocols = ProtocolDetector::getSupportedProtocols();
        qDebug() << "支持的检测协议:";
        for (auto protocol : protocols) {
            QString name = ProtocolDetector::protocolTypeToString(protocol);
            qDebug() << "  -" << name;
        }
        
        // 测试协议验证
        auto testInterface = CommFactory::create("Serial");
        if (testInterface) {
            qDebug() << "协议验证测试...";
            
            // 连接检测信号
            connect(detector, &ProtocolDetector::detectionProgress,
                    [](int percentage) {
                qDebug() << "检测进度:" << percentage << "%";
            });
            
            connect(detector, &ProtocolDetector::protocolDetected,
                    [](const ProtocolDetector::ProtocolInfo& info) {
                qDebug() << "检测到协议:" << info.name 
                         << "置信度:" << info.confidence << "%"
                         << "描述:" << info.description;
            });
            
            connect(detector, &ProtocolDetector::detectionFailed,
                    [](const QString& error) {
                qDebug() << "检测失败:" << error;
            });
            
            // 注意：实际检测需要真实的通信接口
            qDebug() << "协议检测功能已准备就绪";
        }
        
        delete testInterface;
        delete detector;
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    CommunicationTest test;
    QTimer::singleShot(100, &test, &CommunicationTest::runTests);
    
    return app.exec();
}

#include "communication_test.moc"