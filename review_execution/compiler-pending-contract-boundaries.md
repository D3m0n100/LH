# 编译器未确认契约与实施边界

这是当前实施状态，C01 的 v1 冻结记录保持原样。没有运行编译器、测试或控制器下载。

## 已改变的错误路径

T03：初始值不再被成功校验后忽略；简单变量和功能块实例初始化明确失败。

T04：常量赋值不再分配独立 `_const_var` 块冒充变量写入；保留范围/类型/常量折叠错误，缺写回契约时明确失败。

C02：新增 Immediate/VariableRef/MemberRef/Init/Write/NoInstanceOperation 符号模型；常量编码入口只接收 Immediate；只读变量、OUT 字段写入、不同类型写回失败。构造参数不等于可读写运行时字段，未证明的参数地址留空。显式 RuntimeFieldDef 的方向、大小、偏移、唯一名与重叠由共享 metadata 验证，list/typ 使用同一布局。`Parameter.is_output` 不再被忽略而当输入常量编码，输出绑定明确失败。

这些模型还没有经 LH 固件版本认证的引用或写回序列化器。不得把整数恰好等于地址作为引用，也不得将引用送入 REAL 浮点常量编码。

## 仍缺生产指令契约

| 任务 | 当前安全行为 | 需要确认的外部事实 |
|---|---|---|
| C04 | 运行时拷贝、成员访问、不可折叠运算继续明确失败 | LH 运算/转换/写回操作数、类型与除零规则 |
| C05 | IF/CASE/FOR/WHILE/REPEAT/EXIT/CONTINUE/RETURN 不生成伪控制代码 | 分支目标表示、跳转宽度/重定位、循环与任务执行边界 |
| C06 | 普通参数重复继续拒绝，旧样例的重复 Input/Item 保留原顺序到证据记录 | 哪些块允许重复组、结束标记/组长度/编码及每组最大项数 |
| C07 | ARRAY/ArrayIndex 的声明和读写继续拒绝 | 地址单位、元素宽度/对齐、静态与动态下标寻址、运行时边界错误语义 |

## 21 个功能块

每项有独立 JSON：`function-contract-evidence/FBxx-Name.json`。归档条目、源行、SHA256、有序调用参数和语法类别、typ 字段以及候选 code 行均来自本轮读取。候选旧名称来自原审查记录，保存 mappingStatus，未认证为 LH 别名。PDF 采用从 1 开始的物理页号并记录完整文件 SHA256。

无调用不等于没有功能。TaskWake/TaskDataWake/TaskSem* 主要依赖手册，不能根据当前占位 ID 启用；TaskPeriodic 只关联旧 Task 的时间消息候选，而非独立旧操作码。TSOAutoTune 的候选 `_TSORelayTuneCtrl` 有五处调用，成功 SEHC 的 FCrtTSO 有 37 个字段。typ 的旧地址间距不能直接解释为当前 INT=2/REAL=4 的字节宽度。

手册物理第 169 页和第 183 页明确说明浮点/IQ 类型取决于软件版本；其版本、IQ 缩放与限幅规则不能由函数名称确定。TaskLock/TaskUnlock/TaskEnd 的源格式没有实例地址，当前固定 `id id address params` 格式不能直接套用。M600 显示组的 ItemEnd/重复 Input 及 SCIDisp 缓冲区引用不能恢复为普通立即数列表。

21 个注册项仍为 incomplete，UI 仍禁止插入，错误原因由泛泛 TODO 改为具体契约缺口。完成取证并不代表完成 LH 编译或设备实现。

下一步必须先提供与 LH 控制器/固件版本绑定的指令表、引用/字段布局、初始化/写回和无实例操作形式，再据每项独立 fixture 实现和由总控验收；不能机械复制 LM 的指令号、枚举或绝对地址。
