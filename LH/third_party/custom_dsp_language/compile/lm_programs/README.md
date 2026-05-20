# LM工业控制系统 - 完整源代码

## 📋 项目概述

这是一套完整的工业控制系统模板，使用CODESYS风格的IEC 61131-3结构化文本编写，涵盖全部70个LM功能块。

**版本**: v1.0  
**日期**: 2026-02-06  
**语言**: LM (CODESYS风格)  
**目标平台**: F2812开发板 + LM运行时

---

## 📁 文件结构

```
lm_programs/
├── main.lm              主程序（整合所有模块）
├── system_init.lm       系统初始化模块
├── io_module.lm         I/O驱动模块
├── data_process.lm      数据处理模块
├── control_module.lm    控制逻辑模块
├── safety_module.lm     安全监控模块
├── comm_module.lm       通信模块
├── display_module.lm    显示交互模块
├── exca_module.lm       EXCA专用模块
└── README.md            本文档
```

---

## 🎯 功能块分布

### 1. system_init.lm (3个功能块)
- `_System` (40961) - 系统配置 **[必需]**
- `_Task` (40979) - 任务管理
- `_ParamtEngBuild` (41054) - 参数引擎

### 2. io_module.lm (5个功能块)
- `_DrvAI` (41062) - 模拟量输入
- `_DrvDI` (41150) - 数字量输入
- `_DrvT2PeriodIn` (40994) - 周期量输入
- `_DrvT2PulseIn` (40999) - 脉冲输入
- `_OutRealTime3` (41056) - 实时输出

### 3. data_process.lm (24个功能块)
**数据构建:**
- `_IntBuild` (41038), `_IntConstBuild` (41039), `_IntDirectBuild` (41040)
- `_FloatBuild` (41054), `_FloatConstBuild` (41055)

**比较运算:**
- `_IntComp` (41077), `_FloatComp` (40985)

**最值选择:**
- `_IntMax` (41089), `_IntMin` (40992)
- `_FloatMax` (41052), `_FloatMin` (40985)

**限幅:**
- `_IntLimt` (40992), `_FloatLimt` (40985), `_FloatVariLimt` (40992)

**缓冲区操作:**
- `_IntBuffOper` (40981), `_FloatBuffOper` (41043), `_IntBuffLogicOper` (41042)

**运算:**
- `_IntModOper` (40981)

**滤波:**
- `_FilterBW` (41150), `_FloatRoughFilter` (40985)

**数据处理:**
- `_DataRedundancy` (41052)
- `_DealInUnify` (40992), `_DealOutUnify` (41097)
- `_DealToData` (40992), `_DealToSwit` (41020)

### 4. control_module.lm (16个功能块)
**条件判断:**
- `_If` (41052), `_If2` (40992)

**开关选择:**
- `_Switch` (41047), `_Switch2` (40979)

**循环:**
- `_For` (40984)

**定时:**
- `_Timer` (40991), `_Timing` (40992), `_TimeDly` (41097), `_TimeInteg2` (41100)

**数据转换:**
- `_FuncIntFloat` (41077), `_FuncIntInt` (40985)
- `_FuncIntInSelct` (40979), `_FuncFloatInSelct` (40992)
- `_FuncMultLine` (41047), `_FuncReversIntFloat` (41080)

**控制:**
- `_TSORelayTuneCtrl` (40992), `_LockCmdGenery` (40985)

### 5. safety_module.lm (5个功能块)
- `_FloatBelowWatch` (41002) - 下限监视
- `_FloatOverWatch` (41058) - 上限监视
- `_PasswordVerify2` (40985) - 密码验证
- `_EXCASFC` (40985) - 安全功能控制
- `_EXCAIntCheck` (41148) - 完整性检查

### 6. comm_module.lm (5个功能块)
- `_CommCANInit` (41008) - CAN总线初始化
- `_CommGPRS` (41056) - GPRS通信
- `_CommJ1939Trans` (40999) - J1939协议
- `_CommTrans` (41077) - 通用传输
- `_CommWatch` (41150) - 通信监视

### 7. display_module.lm (5个功能块)
- `_KeyScan` (41194) - 键盘扫描
- `_M600TextDisp` (40981) - 文本显示
- `_M600ProgressBar` (40992) - 进度条
- `_SCIDispInit` (41047) - 串口显示初始化
- `_EXCACycDisp` (41077) - 循环显示

### 8. exca_module.lm (5个功能块)
- `_EXCAKeyParamt` (40992) - 关键参数
- `_EXCAModifyParamt` (41062) - 参数修改
- `_EXCAOperComp` (40992) - 操作补偿
- `_EXCASpdUnify` (40992) - 速度统一
- `_EXCATMaintain` (41092) - 定时维护

### 9. main.lm (主程序)
整合所有模块，实现完整的状态机控制

---

## 🚀 编译方法

### 方法1: 使用命令行工具

```bash
# 编译单个模块
python lmc.py lm_programs/system_init.lm -v

# 编译所有模块
python lmc.py lm_programs/*.lm -o compiled/
```

### 方法2: 批量编译脚本

```bash
# Windows
for %f in (lm_programs\*.lm) do python lmc.py %f -o compiled\

# Linux/Mac
for f in lm_programs/*.lm; do python lmc.py "$f" -o compiled/; done
```

---

## 📊 功能块统计

| 类别 | 数量 | 主要功能 |
|------|------|----------|
| 系统基础 | 3 | 初始化、任务、参数 |
| I/O驱动 | 5 | 模拟量、数字量、脉冲 |
| 数据处理 | 24 | 构建、比较、运算、滤波 |
| 控制逻辑 | 16 | 条件、循环、定时、转换 |
| 安全监控 | 5 | 越限、验证、检查 |
| 通信 | 5 | CAN、GPRS、J1939 |
| 显示交互 | 5 | 键盘、显示屏 |
| EXCA专用 | 5 | 工程机械控制 |
| **总计** | **68** | **(实际70次调用)** |

---

## 💡 应用场景

### 1. 泵站控制系统
- I/O: 液位传感器、压力传感器、流量计
- 控制: PID控制、阀门开度调节
- 通信: GPRS远程监控
- 显示: 本地LCD显示

### 2. 工程机械控制
- EXCA: 挖掘机专用功能
- 安全: 负载监控、温度保护
- I/O: 编码器、压力传感器
- 通信: J1939 CAN总线

### 3. 通用自动化系统
- 数据: 采集、处理、滤波
- 控制: 逻辑控制、顺序控制
- 通信: 多协议支持
- 安全: 多重保护

---

## 🛠️ 二次开发指南

### 修改I/O配置
编辑 `io_module.lm`，根据实际硬件修改：
- AI通道数量和量程
- DI通道数量和逻辑
- DO输出配置

### 添加控制算法
在 `control_module.lm` 中添加：
- PID控制器
- 模糊控制
- 自定义算法

### 扩展通信协议
在 `comm_module.lm` 中添加：
- Modbus RTU/TCP
- Profibus
- EtherCAT

### 定制显示界面
在 `display_module.lm` 中修改：
- 显示内容和布局
- 菜单结构
- 用户交互逻辑

---

## 📝 注意事项

### 1. 功能块参数
由于LM手册中某些功能块的参数定义不完整，部分参数值为推测值。
实际使用时需要参考：
- LM2007用户手册
- 功能块装配格式手册
- 实际硬件测试结果

### 2. 类型ID冲突
某些功能块共享相同的类型ID（如41056对应_CommGPRS和_OutRealTime3）。
编译器通过上下文区分，但需注意避免在同一程序中混用。

### 3. 硬件依赖
- AI/DI通道配置依赖F2812的实际外设
- 通信功能需要相应硬件支持（CAN收发器、GPRS模块等）
- 显示功能需要M600显示屏或兼容设备

### 4. 运行时支持
生成的.code文件需要LM运行时才能执行。
如果没有运行时，需要：
- 移植LM运行时到F2812
- 或采用方案B（编译到C代码）

---

## 🔍 调试建议

### 1. 分模块测试
先单独编译和测试每个模块，确认无误后再整合。

### 2. 使用详细模式
```bash
python lmc.py program.lm -v -d
```
查看AST树和生成的指令。

### 3. 内存分配检查
编译后查看内存分配表，确认：
- _System在地址0
- 后续功能块线性分配
- 无地址冲突

### 4. 参数验证
对照手册验证每个功能块的参数：
- 参数数量正确
- 参数类型匹配
- 参数值合理

---

## 📞 技术支持

如有问题，请检查：
1. `COMPILER_GUIDE.md` - 编译器使用指南
2. `DEPLOYMENT_GUIDE.md` - 部署指南
3. LM用户手册

---

## 🎓 学习资源

### 推荐阅读顺序
1. 先看 `system_init.lm` - 了解基本结构
2. 再看 `io_module.lm` - 学习I/O处理
3. 然后看 `data_process.lm` - 掌握数据处理
4. 最后看 `main.lm` - 理解整体架构

### CODESYS学习
- IEC 61131-3标准
- 结构化文本(ST)语法
- 功能块编程

---

## 📈 版本历史

**v1.0** (2026-02-06)
- 初始版本
- 涵盖70个功能块
- 完整的9个模块

---

**祝编程愉快！** 🚀
