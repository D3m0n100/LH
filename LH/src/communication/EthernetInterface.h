// 文件：src/communication/EthernetInterface.h
// 以太网通信接口

#ifndef ETHERNETINTERFACE_H
#define ETHERNETINTERFACE_H

#include "ICommInterface.h"
#include "CommTypes.h"
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTcpServer>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>

/**
 * @brief 以太网通信接口
 * 
 * 支持 TCP 和 UDP 协议，客户端和服务器模式。
 */
class EthernetInterface : public ICommInterface
{
    Q_OBJECT
    
public:
    using Protocol = EthernetConfig::Protocol;
    using Role = EthernetConfig::Role;

    explicit EthernetInterface(QObject *parent = nullptr);
    ~EthernetInterface() override;
    
    // ========================================================================
    // ICommInterface 实现
    // ========================================================================
    
    bool open(const QVariantMap& config) override;
    void close() override;
    int send(const QByteArray& data) override;
    QByteArray receive(int timeout_ms) override;
    bool isConnected() const override;
    
    // ========================================================================
    // 强类型配置接口
    // ========================================================================
    
    bool open(const EthernetConfig& config);
    EthernetConfig currentConfig() const { return m_config; }

    // ========================================================================
    // 以太网特有接口
    // ========================================================================
    
    int sendTo(const QByteArray& data, const QString& host, quint16 port);
    
    quint16 localPort() const;
    QString peerAddress() const;
    quint16 peerPort() const;
    
    qint64 bytesReceived() const { return m_bytesReceived; }
    qint64 bytesSent() const { return m_bytesSent; }

signals:
    void clientConnected(const QString& address, quint16 port);
    void clientDisconnected(const QString& address, quint16 port);
    
private slots:
    void onTcpReadyRead();
    void onTcpError(QAbstractSocket::SocketError socketError);
    void onTcpConnected();
    void onTcpDisconnected();
    
    void onUdpReadyRead();
    
    void onNewConnection();
    void onKeepAliveTimeout();
    
private:
    bool openTcpClient();
    bool openTcpServer();
    bool openUdp();
    
    void closeTcp();
    void closeUdp();
    
    EthernetConfig m_config;
    
    QTcpSocket* m_tcpSocket;
    QTcpServer* m_tcpServer;
    QList<QTcpSocket*> m_clientSockets;
    
    QUdpSocket* m_udpSocket;
    
    QByteArray m_receiveBuffer;
    mutable QMutex m_bufferMutex;
    QWaitCondition m_receiveWaitCondition;
    
    QTimer* m_keepAliveTimer;
    
    qint64 m_bytesReceived;
    qint64 m_bytesSent;
    
    static constexpr int MAX_BUFFER_SIZE = 1048576;
};

#endif // ETHERNETINTERFACE_H
