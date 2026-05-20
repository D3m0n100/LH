// 文件：src/communication/ICommInterface.h
// 通信接口基类 - 定义统一的接口规范

#ifndef ICOMMINTERFACE_H
#define ICOMMINTERFACE_H

#include <QObject>
#include <QByteArray>
#include <QVariantMap>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>
#include <QElapsedTimer>
#include <QTimer>
#include <QEventLoop>
#include "CommTypes.h"

/**
 * @brief 通信接口抽象基类
 * 
 * 设计原则：
 * 1. 异步优先：数据接收通过信号通知，避免阻塞 GUI 线程
 * 2. 统一错误模型：所有错误通过 CommError 结构和信号传播
 * 3. 配置类型化：支持强类型配置结构，同时兼容 QVariantMap
 * 4. 线程安全：内部使用互斥锁保护共享资源
 */
class ICommInterface : public QObject
{
    Q_OBJECT
    
public:
    explicit ICommInterface(QObject *parent = nullptr) 
        : QObject(parent)
        , m_protocolType(CommProtocolType::Unknown)
    {}
    
    virtual ~ICommInterface() = default;
    
    // ========================================================================
    // 核心接口 - 必须实现
    // ========================================================================
    
    /**
     * @brief 打开通信连接
     * @param config 配置参数（QVariantMap 格式，兼容旧调用）
     * @return 成功返回 true
     */
    virtual bool open(const QVariantMap& config) = 0;
    
    /**
     * @brief 关闭通信连接
     */
    virtual void close() = 0;
    
    /**
     * @brief 发送数据
     * @param data 要发送的数据
     * @return 成功发送的字节数，失败返回 -1
     */
    virtual int send(const QByteArray& data) = 0;
    
    /**
     * @brief 同步接收数据（带超时）
     * @param timeout_ms 超时时间（毫秒）
     * @return 接收到的数据，超时返回空
     * 
     * @note 此方法会阻塞当前线程，不建议在 GUI 线程中使用。
     *       推荐使用 dataReceived 信号进行异步接收。
     */
    virtual QByteArray receive(int timeout_ms = 1000) = 0;
    
    /**
     * @brief 检查连接状态
     * @return 已连接返回 true
     */
    virtual bool isConnected() const = 0;
    
    // ========================================================================
    // 配置接口 - 提供默认实现
    // ========================================================================
    
    /**
     * @brief 设置单个参数
     */
    virtual void setParameter(const QString& key, const QVariant& value) {
        QMutexLocker locker(&m_paramMutex);
        m_parameters[key] = value;
    }
    
    /**
     * @brief 获取单个参数
     */
    virtual QVariant getParameter(const QString& key) const {
        QMutexLocker locker(&m_paramMutex);
        return m_parameters.value(key);
    }
    
    /**
     * @brief 获取所有参数
     */
    virtual QVariantMap getParameters() const {
        QMutexLocker locker(&m_paramMutex);
        return m_parameters;
    }
    
    /**
     * @brief 获取协议类型
     */
    CommProtocolType protocolType() const { return m_protocolType; }
    
    // ========================================================================
    // 错误处理辅助方法
    // ========================================================================
    
    /**
     * @brief 获取最后一个错误
     */
    CommError lastError() const {
        QMutexLocker locker(&m_errorMutex);
        return m_lastError;
    }
    
    /**
     * @brief 清除错误状态
     */
    void clearError() {
        QMutexLocker locker(&m_errorMutex);
        m_lastError = CommError();
    }
    
signals:
    /**
     * @brief 数据接收信号
     * @param data 接收到的原始数据
     */
    void dataReceived(const QByteArray& data);
    
    /**
     * @brief 错误发生信号
     * @param error 错误信息结构
     */
    void errorOccurred(const CommError& error);
    
    /**
     * @brief 连接状态改变信号
     * @param connected 是否已连接
     */
    void connectionStateChanged(bool connected);
    
    /**
     * @brief 旧式错误信号（兼容性保留）
     * @param errorMsg 错误消息字符串
     * @deprecated 请使用 errorOccurred(const CommError&) 代替
     */
    void errorOccurred(const QString& errorMsg);
    
protected:
    // ========================================================================
    // 子类辅助方法
    // ========================================================================
    
    /**
     * @brief 报告错误（统一错误处理入口）
     */
    void reportError(CommErrorCode code, const QString& message, const QString& details = QString()) {
        CommError error(m_protocolType, code, message, details);
        
        {
            QMutexLocker locker(&m_errorMutex);
            m_lastError = error;
        }
        
        emit errorOccurred(error);
        emit errorOccurred(error.message); // 兼容旧接口
    }
    
    /**
     * @brief 非阻塞等待（使用事件循环替代 sleep）
     * @param ms 等待时间（毫秒）
     * 
     * 此方法不会阻塞事件循环，适合在需要短暂等待时使用
     */
    static void waitNonBlocking(int ms) {
        if (ms <= 0) return;
        
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(ms);
        loop.exec();
    }
    
    /**
     * @brief 带条件的等待（使用事件循环）
     * @param condition 等待条件（返回 true 时退出等待）
     * @param timeout_ms 超时时间
     * @param checkInterval_ms 检查间隔
     * @return 条件满足返回 true，超时返回 false
     */
    template<typename Func>
    static bool waitForCondition(Func condition, int timeout_ms, int checkInterval_ms = 10) {
        QElapsedTimer timer;
        timer.start();
        
        while (!condition()) {
            if (timer.elapsed() >= timeout_ms) {
                return false;
            }
            
            QEventLoop loop;
            QTimer checkTimer;
            checkTimer.setSingleShot(true);
            QObject::connect(&checkTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
            checkTimer.start(qMin(checkInterval_ms, timeout_ms - static_cast<int>(timer.elapsed())));
            loop.exec();
        }
        
        return true;
    }
    
protected:
    QVariantMap m_parameters;
    mutable QMutex m_paramMutex;
    
    CommProtocolType m_protocolType;
    
    CommError m_lastError;
    mutable QMutex m_errorMutex;
};

#endif // ICOMMINTERFACE_H
