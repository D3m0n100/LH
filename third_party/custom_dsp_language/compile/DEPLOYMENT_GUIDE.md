# 端到端编译器 - 文件部署指南

## 新增文件清单（共4个文件）

### 1. compiler.py - 主编译器
**下载的文件**: `compiler.py`  
**放置位置**: `E:\lh-compiler\src\lh_compiler\compiler.py`  
**功能**: 整合词法分析、语法分析、AST构建、代码生成的完整编译流程

### 2. test_e2e_compile.py - 端到端测试
**下载的文件**: `test_e2e_compile.py`  
**放置位置**: `E:\lh-compiler\test_e2e_compile.py` (项目根目录)  
**功能**: 测试完整编译流程，自动创建测试程序并编译

### 3. lmc.py - 命令行工具
**下载的文件**: `lmc.py`  
**放置位置**: `E:\lh-compiler\lmc.py` (项目根目录)  
**功能**: 方便的命令行编译工具，支持单文件和批量编译

### 4. COMPILER_GUIDE.md - 使用指南
**下载的文件**: `COMPILER_GUIDE.md`  
**放置位置**: `E:\lh-compiler\COMPILER_GUIDE.md` (项目根目录)  
**功能**: 详细的使用文档和示例

## 快速开始

### 步骤1: 部署文件

将下载的4个文件按照上面的位置放好。

### 步骤2: 运行端到端测试

打开虚拟环境，然后运行:

```bash
cd E:\lh-compiler
python test_e2e_compile.py
```

这会:
1. 自动创建 `test_programs/` 目录和测试程序
2. 编译所有测试程序
3. 输出到 `compiled_output/` 目录
4. 显示详细的编译过程和结果

### 步骤3: 使用命令行工具编译你自己的程序

创建一个 `.lh` 文件，例如 `my_program.lh`:

```pascal
PROGRAM MyFirst
VAR
    system : _System;
    temp : REAL;
END_VAR

system(Author := 1, Config := 100, Date := 2601);
temp := 25.5;

END_PROGRAM
```

然后编译:

```bash
python lmc.py my_program.lh
```

会生成 `my_program.code` 文件。

## 完整的项目结构（更新后）

```
E:\lh-compiler\
├── src\lh_compiler\
│   ├── __init__.py
│   ├── compiler.py                 ← 新增：主编译器
│   ├── frontend\
│   │   ├── __init__.py
│   │   ├── ast_nodes.py
│   │   └── ast_builder.py
│   ├── backend\
│   │   ├── __init__.py
│   │   ├── memory.py
│   │   ├── codegen.py
│   │   └── emitter.py
│   └── function_blocks\
│       ├── __init__.py
│       ├── registry.py
│       └── definitions\
│           ├── __init__.py
│           └── _system.py
├── grammar\
│   ├── LH.g4
│   ├── LHLexer.py
│   ├── LHParser.py
│   └── LHVisitor.py
├── lmc.py                          ← 新增：命令行工具
├── test_e2e_compile.py             ← 新增：端到端测试
├── test_codegen.py
├── test_parser.py
└── COMPILER_GUIDE.md               ← 新增：使用指南
```

## 使用示例

### 示例1: 编译单个文件（详细模式）

```bash
python lmc.py my_program.lh -v
```

输出:
```
======================================================================
编译: my_program.lh -> my_program.code
======================================================================

步骤 1/5: 读取源文件...
步骤 2/5: 词法和语法分析...
步骤 3/5: 构建 AST...
步骤 4/5: 生成代码...
步骤 5/5: 写入输出文件...

======================================================================
编译成功! 输出文件: my_program.code
指令数: 2
======================================================================
```

### 示例2: 批量编译

```bash
python lmc.py test_programs/*.lh -o compiled_output/
```

### 示例3: 调试模式（查看AST和指令）

```bash
python lmc.py my_program.lh -d
```

会显示:
```
--- AST 结构 ---
Program: MyFirst
  Variables:
    Variable(system: _System)
    Variable(temp: REAL)
  Statements:
    FunctionBlockCall(system)
    Assignment(temp := ...)

--- 生成的指令 ---
  [  0] 40961 40961 0 1 100 2601 0  // _System(Author=1)
  [  1] 41030 41030 38 1103888384  // temp := 25.5 (_RealConstBuild)

--- .code 文件内容 ---
40961 40961 0 1 100 2601 0
41030 41030 38 1103888384
--- 结束 ---
```

## 在 Python 代码中使用

```python
from lh_compiler import LHCompiler

# 创建编译器
compiler = LHCompiler(verbose=True)

# 编译文件
result = compiler.compile_file("my_program.lh")

# 检查结果
if result.success:
    print(f"成功! 生成 {len(result.instructions)} 条指令")
else:
    for err in result.errors:
        print(f"错误: {err}")
```

## 完成度

当前编译器已实现:
- ✅ 完整的编译流程（源码→字节码）
- ✅ CODESYS 风格语法支持
- ✅ 变量声明和赋值
- ✅ 功能块调用
- ✅ 常量折叠（编译时计算）
- ✅ 内存线性分配
- ✅ .code 文件生成
- ✅ 命令行工具
- ✅ 详细错误报告

待扩展功能:
- ⏳ 更多功能块（数学、定时器、计数器等）
- ⏳ 复杂表达式代码生成
- ⏳ 语义分析和类型检查
- ⏳ 优化器

## 测试验证

运行以下命令验证编译器是否正常工作:

```bash
# 1. 端到端测试
python test_e2e_compile.py

# 2. 代码生成测试
python test_codegen.py

# 3. 解析器测试
python test_parser.py
```

所有测试都应该通过。

## 故障排除

### 问题1: ImportError: No module named 'antlr4'
**解决**: 安装 ANTLR 运行时
```bash
pip install antlr4-python3-runtime
```

### 问题2: 无法导入解析器
**解决**: 确保 `grammar` 目录存在且包含生成的 Python 文件

### 问题3: 提示找不到 grammar 目录
**解决**: 使用 `-g` 参数指定 grammar 目录
```bash
python lmc.py program.lh -g E:\lh-compiler\grammar
```

## 下一步

现在你已经有了完整的编译器，可以:

1. **编译你的程序**: 创建 .lh 文件并编译
2. **扩展功能块**: 在 `function_blocks/definitions/` 添加新的功能块定义
3. **添加语言特性**: 扩展语法以支持更多 CODESYS 特性
4. **优化代码生成**: 改进生成的字节码质量

查看 `COMPILER_GUIDE.md` 获取更详细的使用说明和示例。
