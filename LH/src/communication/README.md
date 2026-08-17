# ServoValve 通信模块

## 概述

统一的工业通信接口库，支持多种通信协议，提供一致的 API 设计。

本次改造重点：

- **Modbus RTU/TCP 主站（工业现场可用）**：基于 QtSerialBus，支持 Fun3/Fun15/Fun16，支持超时与重试，并映射错误码
- **ControllerBridge（三层定位）**：串口连通 / 控制器在线 / CAN 侧目标响应
- **下载协议框架预留**：`IDownloadTransport` + `DownloadProfile(JSON)`（配置驱动）

---

## Modbus RTU（工业可用实现）

### 关键点

- 基于 **QtSerialBus**
  - Qt5：`QModbusRtuSerialMaster`
  - Qt6：`QModbusRtuSerialClient`
- 支持功能码：
  - **Fun3**：读保持寄存器（Holding Registers）
  - **Fun15**：写多个线圈（Coils）
  - **Fun16**：写多个保持寄存器（Holding Registers）
- 支持：
  - `responseTimeout` + `retryCount`（来自 `CommTypes.h::ModbusConfig`）
  - 错误映射到 `CommErrorCode` 并通过 `ICommInterface::errorOccurred(CommError)` 上报

> 采用 QtSerialBus 后，Modbus RTU 的分帧与 CRC 校验由 QtSerialBus 内部处理；无需在 `SerialInterface` 中自写 CRC/分帧。

### 典型配置（115200 / 8N1）

```cpp
QVariantMap modbusCfg{
    {"protocol", "MODBUS"},   // 或 "SERIAL" + mode=RTU
    {"mode", "RTU"},
    {"type", "Master"},
    {"address", 1},

    // 串口参数（要求：115200/8N1）
    {"port", "COM7"},
    {"baudRate", 115200},
    {"dataBits", 8},
    {"parity", "None"},
    {"stopBits", 1},

    // 超时与重试
    {"responseTimeout", 500},  // ms
    {"retryCount", 3}
};
```

---

## ControllerBridge：三层定位（串口 / 控制器 / CAN 目标）

### 文件

- `ControllerBridge.h/.cpp`

### 能力说明

1) **串口是否连通**  
由 `ModbusInterface::open()` 完成：端口打不开、参数错误、连接超时等会报 `ConnectionFailed/ConnectionTimeout/InvalidConfig`。

2) **控制器是否在线（握手）**  
`ControllerBridge::handshake()`：open 后通过 Fun3 读取一个“控制器寄存器”判断控制器是否响应。  
当 **串口线插着但控制器断电** 时：open 成功、handshake 失败 → 上报 **“串口已打开，但控制器握手失败（控制器离线或无响应）”**。

3) **CAN 侧目标是否响应（目标探测）**  
`ControllerBridge::probeTarget()`：通过 Fun3 读取“目标探测寄存器/状态寄存器”。  
当 **控制器在线但 CAN 线拔掉/目标断电** 时：握手成功、probe 失败 → 上报 **“控制器在线，但 CAN 侧目标设备无响应（目标探测失败）”**。

### Bridge 配置

推荐把桥接相关配置放在 `bridge` 子节点（也支持将 `handshake/targetProbe` 直接放在顶层）。

```cpp
QVariantMap bridgeCfg{
    {"enableHandshake", true},
    {"enableTargetProbe", true},

    {"handshake", QVariantMap{
        {"slaveId", 1},
        {"address", 0},   // 建议：控制器固定可读寄存器（版本号/状态字等）
        {"count", 1}
    }},
    {"targetProbe", QVariantMap{
        {"address", 100}, // 建议：控制器映射的“目标在线状态/设备ID”等寄存器
        {"count", 1},
        // optional：若配置 expected，则返回值不匹配也视为“目标不可用”
        // {"expected", 1}
        // {"expected", QVariantList{1}}
    }}
};
```

---

## 上层集成：CommFactory / Communication.h 自动握手与探测

### 1) 使用 CommFactory（推荐）

`CommFactory::createAndOpen()` 会在 `open()` 成功后自动挂载 `ControllerBridge` 并执行 `handshake()` / `probeTarget()`：

- 若失败：返回 `nullptr`，同时通过 `errorOccurred(CommError)` 上报明确错误
- 若成功：返回已打开的接口；桥接层对象作为 `interface` 的 child，可通过 outBridge 或 `findChild` 获取

```cpp
ControllerBridge* bridge = nullptr;
ICommInterface* iface = CommFactory::createAndOpen(modbusCfg.unite(QVariantMap{{"bridge", bridgeCfg}}),
                                                   nullptr, &bridge);
if (!iface) {
    // 创建或 open 失败
    return;
}

if (iface->lastError().code != CommErrorCode::NoError) {
    // open 成功但握手/探测失败：lastError 已包含语义化错误
    // 同时 errorOccurred 也会发出（若已连接）。UI 可据此展示：
    // - 串口问题
    // - 控制器离线
    // - CAN 侧目标无响应
    return;
}

// 可选：查询桥接层状态（也会写入 interface parameters）
const bool controllerOnline = iface->getParameter("controllerOnline").toBool();
const bool targetOnline = iface->getParameter("targetOnline").toBool();
```

### 2) 使用 Communication.h

同样支持 `Communication::createAndOpen(config)` 自动桥接；并提供辅助函数获取桥接层：

```cpp
ICommInterface* iface = Communication::createAndOpen(config);
ControllerBridge* bridge = Communication::getControllerBridge(iface);
```

---

## 下载 Profile 契约

### 文件

- `DownloadProfile.h/.cpp`：下载步骤解析、兼容字段归一化和结构校验
- `ControllerBridge.h/.cpp`：下载面板使用的 Bridge 执行入口
- `ControllerDeviceBackendDownload.cpp`：运行会话使用的控制器后端执行入口

两个执行入口使用同一组规范字段。下载产物必须配置 Profile；缺少、不可读或结构无效时，下载会在写设备前失败。寄存器地址、命令值和结果状态仍须以实际硬件协议为准。

运行会话的控制器后端目前只接受需要响应的 `writeRegs`，并仅支持设备站号直达；`writeCoils`、`needResponse=false` 或目标选择寄存器寻址会明确报配置错误，需改用 Bridge 下载入口。

### DownloadProfile JSON 示例

```json
{
  "name": "example_download_v1",
  "slaveId": 1,
  "steps": [
    { "type": "enter", "params": { "layer": "target", "address": 2000, "values": [1] } },
    { "type": "poll", "params": { "layer": "target", "address": 2002, "count": 1, "expected": [1], "timeoutMs": 2000, "pollIntervalMs": 100 } },
    { "type": "sendChunk", "params": { "layer": "target", "dataAddress": 2100, "chunkWords": 60, "byteOrder": "BigEndian", "needResponse": true } },
    { "type": "finalize", "params": { "layer": "target", "address": 2001, "values": [1] } },
    { "type": "queryResult", "params": { "layer": "target", "address": 2300, "count": 2, "expected": [0, 0] } }
  ]
}
```

规范字段为 `pollIntervalMs`、`dataAddress`、`chunkWords`、`byteOrder` 以及可选的 `packetIndexAddress`、`packetLengthAddress`、`packetCrcAddress`、`packetOffsetAddress`。旧字段 `intervalMs`、`maxRegisters`、`maxRegistersPerChunk`、`chunkHoldingAddress` 仅作为兼容别名保留。

---

## 最小测试步骤（三层定位验收）

1) **控制器断电，但串口线仍插着**  
- `open()` 成功  
- `handshake()` 失败  
- 期望报错：`DeviceNotFound` + “串口已打开，但控制器握手失败/控制器离线”

2) **控制器上电，但 CAN 线拔掉或目标设备断电**  
- `open()` 成功  
- `handshake()` 成功  
- `probeTarget()` 失败  
- 期望报错：`DeviceNotFound` 或 `ReceiveTimeout/InvalidResponse`（视控制器实现） + “控制器在线，但 CAN 侧目标设备无响应”

3) **正常情况下**  
- `open + handshake + probeTarget` 全部成功  
- 连续执行 Fun3/Fun15/Fun16：应稳定；超时会按 `retryCount` 重试，最终错误码清晰
---

## Classic OPC / Modbus 点位地址兼容说明

`ClassicOpcServer` 会优先读取 `RuntimePointDefinition.addressing`，并兼容一组常见别名字段，用于适配 LH 历史点表或导入数据。

### 支持的字段

- 区域 / 寄存器类型
  - `area`
  - `registerType`
  - `tagArea`
  - `memoryArea`
  - `region`
- 地址
  - `address`
  - `regAddress`
  - `register`
  - `pointAddress`
  - `offset`
- 从站 / 站号
  - `unitId`
  - `slaveId`
  - `stationAddress`
  - `serverAddress`
- 位偏移
  - `bitOffset`
  - `bit`
  - `bitIndex`
- 元素个数
  - `elementCount`
  - `count`
  - `length`
  - `quantity`

### 默认规则

- `area` 默认按 `holding` 处理
- `address` 缺失时会继续从 `point.metadata.address`、`point.bindingPath` 中推断
- `readOnly` 点在未显式指定区域时，默认更偏向 `input`
- `bitOffset` 会叠加到基础地址上，用于位偏移兼容

### 约定

- `coil/coils` 走线圈读写
- `input/input-register/input-registers` 走输入寄存器读
- 其余情况默认走保持寄存器读写
