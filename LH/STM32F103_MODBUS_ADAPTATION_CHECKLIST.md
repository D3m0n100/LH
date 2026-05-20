# STM32F103 Modbus 寄存器 / 下载协议适配清单

## 1. 适配结论
当前软件**最适合**与 STM32F103 通过 **RS-485 + Modbus RTU** 对接。

现有代码中已经具备：
- Modbus RTU / TCP 主站接口（QtSerialBus）
- Fun03 / Fun04 / Fun01 / Fun02 读取
- Fun06 / Fun16 / Fun05 / Fun15 写入
- 响应超时、重试、错误映射
- 桥接层 handshake / target probe / 分包下载框架
- JSON 配置驱动的下载步骤框架

但仍**缺少 STM32F103 专用落地定义**，因此不能直接认定“已完整适配”。

---

## 2. PC 侧已具备能力（从代码看）
### 2.1 通信层
建议固定为：
- 物理层：RS-485 半双工
- 协议：Modbus RTU
- 角色：PC 上位机 = 主站；STM32F103 = 从站

建议默认串口参数：
- 波特率：115200（也可 9600 / 19200 / 57600）
- 数据位：8
- 校验：None 或 Even（二选一，必须统一）
- 停止位：1
- 从站地址：1~247

### 2.2 代码已支持的 Modbus 操作
- 读保持寄存器 Fun03
- 读输入寄存器 Fun04
- 读线圈 Fun01
- 读离散输入 Fun02
- 写单寄存器 Fun06
- 写多寄存器 Fun16
- 写单线圈 Fun05
- 写多线圈 Fun15

### 2.3 下载框架已具备但未板卡化
现有下载框架支持：
- enter：进入下载模式
- poll：轮询状态寄存器
- sendChunk：按寄存器分包写入
- finalize：触发收尾
- queryResult：读取结果寄存器

这说明软件侧**有下载流程框架**，但**没有 STM32F103 专用寄存器映射和 Bootloader 协议定义**。

---

## 3. 必须补齐的 STM32F103 寄存器表
以下寄存器建议全部明确下来，否则 PC 端无法稳定联调。

### 3.1 设备识别区（建议只读）
- 0x0000：设备类型 DeviceType
- 0x0001：协议版本 ProtocolVersion
- 0x0002：固件主版本 FirmwareMajor
- 0x0003：固件次版本 FirmwareMinor
- 0x0004：硬件版本 HardwareVersion
- 0x0005：设备地址 NodeId / SlaveId
- 0x0006：设备状态 DeviceState
- 0x0007：错误码 LastError

### 3.2 运行状态区（建议只读）
- 0x0010：运行使能 RunEnable
- 0x0011：故障状态 FaultState
- 0x0012：告警状态 WarningState
- 0x0013：控制模式 ControlMode
- 0x0014：输入命令值 CommandIn
- 0x0015：实际输出值 FeedbackOut
- 0x0016：电流/电压采样值
- 0x0017：温度或其他诊断值

### 3.3 参数区（建议读写，掉电保存策略要明确）
- 0x0100：KP
- 0x0101：KI
- 0x0102：KD
- 0x0103：零偏 Offset
- 0x0104：比例系数 Gain
- 0x0105：输出限幅 UpperLimit
- 0x0106：输出限幅 LowerLimit
- 0x0107：滤波参数 Filter
- 0x0108：采样周期 SamplePeriod
- 0x0109：控制周期 ControlPeriod

### 3.4 控制命令区（建议写后动作）
- 0x0200：启动命令 Start
- 0x0201：停止命令 Stop
- 0x0202：清故障 ClearFault
- 0x0203：恢复默认参数 RestoreDefault
- 0x0204：保存参数 SaveConfig
- 0x0205：软复位 SoftReset

### 3.5 监控采样区（建议连续映射，便于批量读取）
- 0x0300~0x031F：监控通道 1~32
- 明确每个通道的数据类型：
  - int16 / uint16 / int32 / uint32 / float
- 若是 32 位 / float，明确：
  - 高低字顺序
  - 字节序
  - 是否使用 IEEE754

---

## 4. 必须补齐的下载 / 升级协议定义
如果要让此软件给 STM32F103 下载程序，必须再定义下面内容。

### 4.1 进入升级模式
至少明确：
- 进入升级命令寄存器地址
- 命令值
- 返回状态值
- 超时时间
- 是否需要重启到 Bootloader

建议示例：
- 0x0400：BootCmd
  - 写 0x5A5A = 请求进入 Bootloader
- 0x0401：BootState
  - 0 = Idle
  - 1 = Ready
  - 2 = Busy
  - 3 = Done
  - 4 = Error

### 4.2 数据分包写入
必须明确：
- 数据写入起始地址
- 单包最大寄存器数
- 单包最大字节数
- 偏移量是否显式传输
- 是否需要包序号
- 是否需要每包 CRC
- 超时重发策略

建议示例：
- 0x0410：PacketIndex
- 0x0411：PacketLength
- 0x0412：PacketCRC16
- 0x0420~0x045B：DataBuffer（60 words = 120 bytes）
- 每包写完后轮询 0x0401 BootState == Ready 再发下一包

### 4.3 收尾与校验
必须明确：
- 完成下载命令地址
- 是否整包 CRC32
- 是否需要写入总长度
- 是否需要写入版本号
- 成功后是否自动复位

建议示例：
- 0x0402：TotalLength
- 0x0404~0x0405：ImageCRC32
- 0x0406：FinalizeCmd = 0xA5A5
- 0x0401：BootState == Done 表示成功
- 0x0407：BootErrorCode 表示失败原因

---

## 5. STM32F103 固件侧必须实现的能力
### 5.1 Modbus 从站
必须实现：
- 从站地址
- Fun03 / Fun06 / Fun16（最低必需）
- 建议再实现 Fun04 / Fun01 / Fun05 / Fun15
- CRC16 Modbus
- 超时处理
- 非法地址异常码
- 非法功能码异常码
- 设备忙异常码（若升级期间不可读写）

### 5.2 485 收发控制
STM32F103 常见问题：
- 需要 DE/RE 引脚方向控制
- 发送完成后再切回接收
- 波特率较高时要注意最后一个字节发送完成标志
- 避免因方向切换过早导致丢尾字节

### 5.3 Flash 下载写入
必须明确：
- Bootloader 区和应用区划分
- 页擦除策略
- 写入粒度
- 断电保护策略
- 校验失败回滚策略
- 升级中禁止控制输出的安全策略

---

## 6. 当前软件与 STM32F103 对接时的真实缺口
1. **没有 STM32F103 专用寄存器表**
2. **没有 STM32F103 专用 Bootloader/下载协议**
3. **没有板级联调记录**
4. **没有明确 32 位/浮点寄存器编码规范**
5. **没有掉电保存和参数生效时序定义**
6. **没有异常码与错误寄存器定义**
7. **没有监控变量统一映射表**
8. **没有升级成功/失败状态机定义**
9. **没有总线冲突、超时、重试验收标准**
10. **没有针对 F103 Flash 空间的镜像分块约束说明**

---

## 7. 推荐最小落地方案
如果要最快打通，建议第一版只做以下最小闭环：

### 7.1 运行期通信最小闭环
- STM32F103 做 Modbus RTU 从站
- 先只实现：Fun03 / Fun06 / Fun16
- 固定从站地址 = 1
- 固定 115200, 8N1
- 先定义 32~64 个保持寄存器
- 先不做 float，先全部用 uint16 / int16

### 7.2 下载最小闭环
- 进入升级模式寄存器 1 个
- 状态寄存器 1 个
- 数据缓冲区 60 words
- 完成寄存器 1 个
- 错误码寄存器 1 个
- 镜像 CRC32 寄存器 2 个

这样最容易套进当前软件已有的 DownloadProfile 框架。

---

## 8. 建议直接给固件/AI的输出物
你应该让 AI 或固件工程师一次性交付以下文件：
1. `stm32f103_modbus_register_map.md`
2. `stm32f103_bootloader_protocol.md`
3. `stm32f103_modbus_slave.c/.h`
4. `stm32f103_bootloader.c/.h`
5. `download_profile_stm32f103.json`
6. `pc_stm32联调测试用例.md`

---

## 9. 一句结论
**能对接，但当前软件还缺 STM32F103 专用寄存器表与下载协议定义。**

只要你把上面第 3、4、5 部分补齐，这个软件就可以按现有 Modbus/Download 框架去配合 STM32F103。
