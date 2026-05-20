// 文件：src/communication/EthernetInterface.cpp
// 以太网通信接口实现

#include "EthernetInterface.h"
#include <QHostAddress>
#include <QNetworkDatagram>
#include "Common.h"

EthernetInterface::EthernetInterface(QObject *parent)
    : ICommInterface(parent)
    , m_tcpSocket(nullptr)
    , m_tcpServer(nullptr)
    , m_udpSocket(nullptr)
    , m_keepAliveTimer(new QTimer(this))
    , m_bytesReceived(0)
    , m_bytesSent(0)
{
    m_protocolType = CommProtocolType::EthernetTCP;
    
    connect(m_keepAliveTimer, &QTimer::timeout, 
            this, &EthernetInterface::onKeepAliveTimeout);
}

EthernetInterface::~EthernetInterface()
{
    close();
}

bool EthernetInterface::open(const QVariantMap& config)
{
    m_config = EthernetConfig::fromMap(config);
    return open(m_config);
}

bool EthernetInterface::open(const EthernetConfig& config)
{
    close(); // 先关闭之前的连接
    
    m_config = config;
    m_parameters = config.toVariantMap();
    
    // 更新协议类型
    m_protocolType = (config.protocol == Protocol::UDP) ? 
                     CommProtocolType::EthernetUDP : CommProtocolType::EthernetTCP;
    
    bool success = false;
    
    if (config.protocol == Protocol::TCP) {
        if (config.role == Role::Server) {
            success = openTcpServer();
        } else {
            success = openTcpClient();
        }
    } else {
        success = openUdp();
    }
    
    if (success && config.keepAliveInterval > 0) {
        m_keepAliveTimer->start(config.keepAliveInterval);
    }
    
    return success;
}

void EthernetInterface::close()
{
    m_keepAliveTimer->stop();
    
    closeTcp();
    closeUdp();
    
    {
        QMutexLocker locker(&m_bufferMutex);
        m_receiveBuffer.clear();
        m_receiveWaitCondition.wakeAll();
    }
    
    emit connectionStateChanged(false);
}

bool EthernetInterface::openTcpClient()
{
    m_tcpSocket = new QTcpSocket(this);
    
    connect(m_tcpSocket, &QTcpSocket::readyRead, 
            this, &EthernetInterface::onTcpReadyRead);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred,
            this, &EthernetInterface::onTcpError);
    connect(m_tcpSocket, &QTcpSocket::connected, 
            this, &EthernetInterface::onTcpConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, 
            this, &EthernetInterface::onTcpDisconnected);
    
    // 设置接收缓冲区大小
    m_tcpSocket->setReadBufferSize(m_config.receiveBufferSize);
    
    LOG_INFO(QString("正在连接 TCP 服务器: %1:%2")
             .arg(m_config.host)
             .arg(m_config.port));
    
    m_tcpSocket->connectToHost(m_config.host, m_config.port);
    
    // 等待连接完成
    if (!m_tcpSocket->waitForConnected(m_config.connectTimeout)) {
        reportError(CommErrorCode::ConnectionTimeout, 
                    "TCP 连接超时",
                    m_tcpSocket->errorString());
        delete m_tcpSocket;
        m_tcpSocket = nullptr;
        return false;
    }
    
    return true;
}

bool EthernetInterface::openTcpServer()
{
    m_tcpServer = new QTcpServer(this);
    
    connect(m_tcpServer, &QTcpServer::newConnection, 
            this, &EthernetInterface::onNewConnection);
    
    QHostAddress address = m_config.host.isEmpty() ? 
                           QHostAddress::Any : QHostAddress(m_config.host);
    
    if (!m_tcpServer->listen(address, m_config.port)) {
        reportError(CommErrorCode::ConnectionFailed, 
                    "TCP 服务器启动失败",
                    m_tcpServer->errorString());
        delete m_tcpServer;
        m_tcpServer = nullptr;
        return false;
    }
    
    LOG_INFO(QString("TCP 服务器已启动，监听端口: %1").arg(m_config.port));
    emit connectionStateChanged(true);
    return true;
}

bool EthernetInterface::openUdp()
{
    m_udpSocket = new QUdpSocket(this);
    
    connect(m_udpSocket, &QUdpSocket::readyRead, 
            this, &EthernetInterface::onUdpReadyRead);
    
    QHostAddress bindAddress = m_config.host.isEmpty() ? 
                               QHostAddress::Any : QHostAddress(m_config.host);
    
    if (!m_udpSocket->bind(bindAddress, m_config.port)) {
        reportError(CommErrorCode::ConnectionFailed, 
                    "UDP 绑定失败",
                    m_udpSocket->errorString());
        delete m_udpSocket;
        m_udpSocket = nullptr;
        return false;
    }
    
    LOG_INFO(QString("UDP 套接字已绑定，端口: %1").arg(m_config.port));
    emit connectionStateChanged(true);
    return true;
}

void EthernetInterface::closeTcp()
{
    // 关闭客户端连接
    for (QTcpSocket* client : m_clientSockets) {
        client->disconnectFromHost();
        client->deleteLater();
    }
    m_clientSockets.clear();
    
    // 关闭服务器
    if (m_tcpServer) {
        m_tcpServer->close();
        delete m_tcpServer;
        m_tcpServer = nullptr;
    }
    
    // 关闭客户端套接字
    if (m_tcpSocket) {
        m_tcpSocket->disconnectFromHost();
        delete m_tcpSocket;
        m_tcpSocket = nullptr;
    }
}

void EthernetInterface::closeUdp()
{
    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

int EthernetInterface::send(const QByteArray& data)
{
    if (!isConnected()) {
        reportError(CommErrorCode::SendFailed, "发送失败：未连接");
        return -1;
    }
    
    qint64 written = 0;
    
    if (m_config.protocol == Protocol::TCP) {
        if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
            written = m_tcpSocket->write(data);
        } else if (!m_clientSockets.isEmpty()) {
            // 服务器模式：发送给所有客户端
            for (QTcpSocket* client : m_clientSockets) {
                if (client->state() == QAbstractSocket::ConnectedState) {
                    written = client->write(data);
                }
            }
        }
    } else if (m_udpSocket) {
        // UDP 发送到配置的目标地址
        written = m_udpSocket->writeDatagram(data, 
                                              QHostAddress(m_config.host), 
                                              m_config.port);
    }
    
    if (written < 0) {
        reportError(CommErrorCode::SendFailed, "发送数据失败");
        return -1;
    }
    
    m_bytesSent += written;
    return static_cast<int>(written);
}

int EthernetInterface::sendTo(const QByteArray& data, const QString& host, quint16 port)
{
    if (!m_udpSocket) {
        reportError(CommErrorCode::SendFailed, "发送失败：UDP 套接字未初始化");
        return -1;
    }
    
    qint64 written = m_udpSocket->writeDatagram(data, QHostAddress(host), port);
    
    if (written < 0) {
        reportError(CommErrorCode::SendFailed, 
                    "UDP 发送失败",
                    m_udpSocket->errorString());
        return -1;
    }
    
    m_bytesSent += written;
    return static_cast<int>(written);
}

QByteArray EthernetInterface::receive(int timeout_ms)
{
    QMutexLocker locker(&m_bufferMutex);
    
    if (m_receiveBuffer.isEmpty() && timeout_ms > 0) {
        m_receiveWaitCondition.wait(&m_bufferMutex, static_cast<unsigned long>(timeout_ms));
    }
    
    if (m_receiveBuffer.isEmpty()) {
        return QByteArray();
    }
    
    QByteArray data = m_receiveBuffer;
    m_receiveBuffer.clear();
    return data;
}

bool EthernetInterface::isConnected() const
{
    if (m_config.protocol == Protocol::TCP) {
        if (m_tcpSocket) {
            return m_tcpSocket->state() == QAbstractSocket::ConnectedState;
        }
        if (m_tcpServer) {
            return m_tcpServer->isListening();
        }
        return false;
    } else {
        return m_udpSocket && m_udpSocket->state() == QAbstractSocket::BoundState;
    }
}

quint16 EthernetInterface::localPort() const
{
    if (m_tcpSocket) {
        return m_tcpSocket->localPort();
    }
    if (m_tcpServer) {
        return m_tcpServer->serverPort();
    }
    if (m_udpSocket) {
        return m_udpSocket->localPort();
    }
    return 0;
}

QString EthernetInterface::peerAddress() const
{
    if (m_tcpSocket) {
        return m_tcpSocket->peerAddress().toString();
    }
    return QString();
}

quint16 EthernetInterface::peerPort() const
{
    if (m_tcpSocket) {
        return m_tcpSocket->peerPort();
    }
    return 0;
}

void EthernetInterface::onTcpReadyRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        socket = m_tcpSocket;
    }
    
    if (!socket) {
        return;
    }
    
    QByteArray data = socket->readAll();
    if (data.isEmpty()) {
        return;
    }
    
    m_bytesReceived += data.size();
    
    {
        QMutexLocker locker(&m_bufferMutex);
        
        // 防止缓冲区溢出
        if (m_receiveBuffer.size() + data.size() > MAX_BUFFER_SIZE) {
            int overflow = m_receiveBuffer.size() + data.size() - MAX_BUFFER_SIZE;
            m_receiveBuffer.remove(0, overflow);
            LOG_WARN("以太网接收缓冲区溢出，丢弃旧数据");
        }
        
        m_receiveBuffer.append(data);
        m_receiveWaitCondition.wakeAll();
    }
    
    emit dataReceived(data);
}

void EthernetInterface::onTcpError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError)
    
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        socket = m_tcpSocket;
    }
    
    CommErrorCode code = CommErrorCode::UnknownError;
    switch (socketError) {
        case QAbstractSocket::ConnectionRefusedError:
            code = CommErrorCode::ConnectionFailed;
            break;
        case QAbstractSocket::RemoteHostClosedError:
            code = CommErrorCode::ConnectionLost;
            break;
        case QAbstractSocket::HostNotFoundError:
            code = CommErrorCode::DeviceNotFound;
            break;
        case QAbstractSocket::SocketTimeoutError:
            code = CommErrorCode::ConnectionTimeout;
            break;
        case QAbstractSocket::NetworkError:
            code = CommErrorCode::ConnectionLost;
            break;
        default:
            break;
    }
    
    reportError(code, "TCP 错误", socket ? socket->errorString() : "");
}

void EthernetInterface::onTcpConnected()
{
    LOG_INFO(QString("TCP 已连接: %1:%2")
             .arg(m_tcpSocket->peerAddress().toString())
             .arg(m_tcpSocket->peerPort()));
    emit connectionStateChanged(true);
}

void EthernetInterface::onTcpDisconnected()
{
    LOG_INFO("TCP 连接已断开");
    emit connectionStateChanged(false);
}

void EthernetInterface::onUdpReadyRead()
{
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();
        
        if (data.isEmpty()) {
            continue;
        }
        
        m_bytesReceived += data.size();
        
        {
            QMutexLocker locker(&m_bufferMutex);
            
            if (m_receiveBuffer.size() + data.size() > MAX_BUFFER_SIZE) {
                int overflow = m_receiveBuffer.size() + data.size() - MAX_BUFFER_SIZE;
                m_receiveBuffer.remove(0, overflow);
            }
            
            m_receiveBuffer.append(data);
            m_receiveWaitCondition.wakeAll();
        }
        
        emit dataReceived(data);
    }
}

void EthernetInterface::onNewConnection()
{
    while (m_tcpServer && m_tcpServer->hasPendingConnections()) {
        QTcpSocket* clientSocket = m_tcpServer->nextPendingConnection();
        
        connect(clientSocket, &QTcpSocket::readyRead, 
                this, &EthernetInterface::onTcpReadyRead);
        connect(clientSocket, &QTcpSocket::disconnected, 
                this, [this, clientSocket]() {
            QString addr = clientSocket->peerAddress().toString();
            quint16 port = clientSocket->peerPort();
            
            m_clientSockets.removeOne(clientSocket);
            clientSocket->deleteLater();
            
            LOG_INFO(QString("客户端断开: %1:%2").arg(addr).arg(port));
            emit clientDisconnected(addr, port);
        });
        
        m_clientSockets.append(clientSocket);
        
        QString addr = clientSocket->peerAddress().toString();
        quint16 port = clientSocket->peerPort();
        
        LOG_INFO(QString("新客户端连接: %1:%2").arg(addr).arg(port));
        emit clientConnected(addr, port);
    }
}

void EthernetInterface::onKeepAliveTimeout()
{
    // 发送心跳包
    if (isConnected()) {
        // 可以发送一个空的心跳包或自定义心跳数据
        // send(QByteArray("\x00", 1));
    }
}
