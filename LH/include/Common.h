/**
 * @file Common.h
 * @brief 通用宏与类型定义
 *
 * 本文件包含整个项目通用的宏定义、类型别名和枚举。
 * 
 * ============== 变更历史 ==============
 * 2025-01: 移除图形化逻辑组态相关类型（ComponentPtr, ConnectionPtr, 
 *          ComponentType, ConnectionType），这些功能已从项目中清除。
 */

#ifndef COMMON_H
#define COMMON_H

#include <QtCore>
#include <QDebug>
#include <QDateTime>
#include <memory>
#include <functional>

// ============ 版本信息 ============
#define PLATFORM_VERSION_MAJOR 1
#define PLATFORM_VERSION_MINOR 0
#define PLATFORM_VERSION_PATCH 0

// ============ 日志宏 ============
#define LOG_INFO(msg)    qInfo().noquote()    << "[INFO]"  << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << msg
#define LOG_WARN(msg)    qWarning().noquote() << "[WARN]"  << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << msg
#define LOG_ERROR(msg)   qCritical().noquote()<< "[ERROR]" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << msg
#define LOG_DEBUG(msg)   qDebug().noquote()   << "[DEBUG]" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << msg

// ============ 单例宏 ============
/**
 * @brief SINGLETON 宏用于快速实现单例模式
 *
 * 使用方式：
 * @code
 * class MyClass : public QObject
 * {
 *     Q_OBJECT
 *     SINGLETON(MyClass)
 * public:
 *     void doSomething();
 * };
 * @endcode
 *
 * @note 重要使用说明：
 *
 * 1. **资源管理责任**：
 *    - 使用 SINGLETON 宏的类，其析构函数是私有的，由静态局部变量机制自动调用。
 *    - 静态析构的顺序在 C++ 中是未定义的，因此**不要依赖静态析构顺序**来释放资源。
 *
 * 2. **显式清理接口要求**：
 *    - 对于持有外部资源（如数据库连接、文件句柄、定时器、线程等）的单例类，
 *      **必须**提供显式的 `shutdown()` 或 `close()` 方法来释放资源。
 *    - 应用程序退出前应主动调用这些清理方法，而不是依赖析构函数。
 *
 * 3. **推荐的生命周期模式**：
 *    @code
 *    // 应用启动时
 *    DataManager::instance().initialize("path/to/db");
 *    TaskScheduler::instance().start();
 *
 *    // 应用运行中...
 *
 *    // 应用退出前（在 main() 结束前或 QApplication::aboutToQuit 信号中）
 *    TaskScheduler::instance().shutdown();
 *    DataManager::instance().shutdown();
 *    @endcode
 *
 * 4. **线程安全说明**：
 *    - C++11 及以上标准保证静态局部变量的初始化是线程安全的。
 *    - 但单例内部的方法是否线程安全，取决于具体实现。
 */
#define SINGLETON(Class) \
public: \
    static Class& instance() { \
        static Class s_instance; \
        return s_instance; \
    } \
private: \
    Class(); \
    ~Class(); \
    Class(const Class&) = delete; \
    Class& operator=(const Class&) = delete;

// ============ 通信协议枚举 ============
namespace Platform {
    /**
     * @brief 通信协议枚举
     * 
     * 用于标识系统支持的通信协议类型。
     */
    enum class CommProtocol {
        CAN,         ///< CAN 总线
        Ethernet,    ///< 以太网 TCP/UDP
        SerialPort,  ///< 串口通信
        Modbus       ///< Modbus 协议
    };
}

// ============ 已移除的类型（图形化逻辑组态）============
//
// 以下类型已在 2025-01 版本中移除，因为图形化逻辑组态功能已从项目中清除：
//
// - using ComponentPtr = std::shared_ptr<class ComponentBase>;
// - using ConnectionPtr = std::shared_ptr<class LogicConnection>;
// - enum class ComponentType { PIDController, Filter, ... };
// - enum class ConnectionType { DataFlow, EventTrigger, ... };
//
// 如需在遗留代码中引用这些类型，请联系架构师评估迁移方案。

#endif // COMMON_H
