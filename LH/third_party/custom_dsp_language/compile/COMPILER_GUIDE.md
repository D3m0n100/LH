# LM 编译器使用指南

## 快速开始

### 1. 编译单个文件

```bash
# 方法1: 使用命令行工具（推荐）
python lmc.py program.lm

# 方法2: 直接使用 compiler.py
python -m lm_compiler.compiler program.lm

# 方法3: Python 代码中使用
from lm_compiler import LMCompiler
compiler = LMCompiler()
result = compiler.compile_file("program.lm", "output.code")
```

### 2. 指定输出文件

```bash
python lmc.py program.lm -o myoutput.code
```

### 3. 批量编译

```bash
# 编译 test_programs 目录下所有 .lm 文件到 compiled_output 目录
python lmc.py test_programs/*.lm -o compiled_output/
```

### 4. 详细模式和调试

```bash
# 显示详细编译过程
python lmc.py program.lm -v

# 显示调试信息（AST树、指令列表）
python lmc.py program.lm -d

# 组合使用
python lmc.py program.lm -vd
```

## 完整编译流程

```
源代码 (.lm)
    ↓
[词法分析] LMLexer
    ↓
[语法分析] LMParser
    ↓
[AST构建] ASTBuilder
    ↓
[代码生成] CodeGenerator
    ↓
[文件输出] CodeEmitter
    ↓
字节码文件 (.code)
```

## 示例程序

### 示例1: 最简单的程序

```pascal
PROGRAM Simple
VAR
    system : _System;
END_VAR

system(Author := 1, Config := 100, Date := 2601);

END_PROGRAM
```

编译后生成 `.code` 文件:
```
40961 40961 0 1 100 2601 0
```

### 示例2: 带变量赋值

```pascal
PROGRAM WithVars
VAR
    system : _System;
    temp : REAL;
    count : INT;
    flag : BOOL;
END_VAR

system(Author := 1, Config := 100, Date := 2601);
temp := 25.5;
count := 42;
flag := TRUE;

END_PROGRAM
```

### 示例3: 带表达式（常量折叠）

```pascal
PROGRAM WithExpr
VAR
    system : _System;
    result : INT;
END_VAR

system(Author := 1);
result := 10;

END_PROGRAM
```

## 运行测试

### 端到端测试

```bash
cd E:\lm-compiler
python test_e2e_compile.py
```

这会:
1. 自动创建测试程序（在 `test_programs/` 目录）
2. 编译所有测试程序
3. 输出到 `compiled_output/` 目录
4. 显示详细结果

### 代码生成器测试（无需源文件）

```bash
python test_codegen.py
```

这会测试直接从 AST 生成代码的功能。

## 文件位置说明

```
E:\lm-compiler\                     项目根目录
├── src\lm_compiler\                源代码包
│   ├── __init__.py
│   ├── compiler.py                 ← 主编译器（新增）
│   ├── frontend\                   前端（词法/语法/AST）
│   │   ├── __init__.py
│   │   ├── ast_nodes.py
│   │   └── ast_builder.py
│   ├── backend\                    后端（代码生成）
│   │   ├── __init__.py
│   │   ├── memory.py
│   │   ├── codegen.py
│   │   └── emitter.py
│   └── function_blocks\            功能块系统
│       ├── __init__.py
│       ├── registry.py
│       └── definitions\
│           ├── __init__.py
│           └── _system.py
├── grammar\                        ANTLR 生成的解析器
│   ├── LM.g4
│   ├── LMLexer.py
│   ├── LMParser.py
│   └── LMVisitor.py
├── lmc.py                          ← 命令行工具（新增）
├── test_e2e_compile.py             ← 端到端测试（新增）
├── test_codegen.py                 代码生成测试
├── test_parser.py                  解析器测试
├── test_programs\                  ← 测试源文件（自动生成）
│   ├── test1_simple.lm
│   ├── test2_vars.lm
│   └── ...
└── compiled_output\                ← 编译输出（自动生成）
    ├── test1_simple.code
    ├── test2_vars.code
    └── ...
```

## 在 Python 代码中使用

```python
from lm_compiler import LMCompiler

# 创建编译器
compiler = LMCompiler(verbose=True, debug=False)

# 编译文件
result = compiler.compile_file("program.lm", "output.code")

# 检查结果
if result.success:
    print(f"编译成功! 生成了 {len(result.instructions)} 条指令")
    print(f"输出文件: {result.output_file}")
else:
    print("编译失败:")
    for err in result.errors:
        print(f"  - {err}")

# 查看生成的 AST
if result.ast:
    from lm_compiler.frontend.ast_nodes import ASTPrinter
    printer = ASTPrinter()
    printer.visit(result.ast)

# 查看生成的指令
for inst in result.instructions:
    print(inst)
```

## 编译字符串形式的代码

```python
source_code = """
PROGRAM Test
VAR
    system : _System;
END_VAR
system(Author := 1);
END_PROGRAM
"""

result = compiler.compile_string(source_code, "test.code")
```

## 常见问题

### Q: 提示 "无法导入解析器"
**A**: 确保 `grammar` 目录存在且包含 ANTLR 生成的文件。运行:
```bash
cd grammar
antlr4 -Dlanguage=Python3 LM.g4 -visitor
```

### Q: 提示 "未安装 antlr4-python3-runtime"
**A**: 安装依赖:
```bash
pip install antlr4-python3-runtime
```

### Q: 编译后的 .code 文件格式是什么？
**A**: 每行一条指令，格式为:
```
类型ID 类型ID 内存地址 参数1 参数2 ...
```
类型ID 重复两次是 .code 文件的固定格式。

### Q: 如何查看编译过程的详细信息？
**A**: 使用 `-v` 参数:
```bash
python lmc.py program.lm -v
```

### Q: 如何查看生成的 AST 树？
**A**: 使用 `-d` 参数:
```bash
python lmc.py program.lm -d
```

## 下一步计划

- [ ] 添加更多功能块（数学运算、定时器、计数器等）
- [ ] 实现复杂表达式的代码生成
- [ ] 添加语义分析和类型检查
- [ ] 添加错误恢复机制
- [ ] 优化生成的代码

## 技术细节

### 内存分配

编译器使用线性内存分配：
```
_System:        地址=0,  占用=38 个单元
变量/常量块:    地址=38, 依次线性分配
```

### 常量折叠

编译器会在编译时计算常量表达式:
```pascal
result := 10 + 20;  // 编译时计算为 30
```

### 功能块定义

当前已定义的功能块:
- `_System` (type_id=40961, 38单元)
- `_IntConstBuild` (type_id=41029, 16单元)
- `_RealConstBuild` (type_id=41030, 16单元)
- `_BoolConstBuild` (type_id=41031, 8单元)

添加新功能块: 在 `src/lm_compiler/function_blocks/definitions/` 下创建新的定义文件。
