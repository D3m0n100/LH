# -*- coding: utf-8 -*-
"""
代码生成?遍历 AST 生成 .code 文件所需的指令序
编译流程:
    源代?-> ANTLR解析 -> AST -> CodeGenerator -> 指令列表 -> CodeEmitter -> .code文件

.code 文件格式:
    每行一条指? 类型ID 类型ID 内存地址 参数1 参数2 ...
    
    示例:
        40961 40961 0 1 100 2601    (_System, 地址0, Author=1, Config=100, Date=2601)
        41029 41029 38 0 25         (_IntConstBuild, 地址38, Value=25)

用法:
    from lm_compiler.backend import CodeGenerator
    from lm_compiler.function_blocks import FunctionBlockRegistry
    
    registry = FunctionBlockRegistry()
    registry.load_defaults()
    
    generator = CodeGenerator(registry)
    instructions = generator.generate(ast)
"""

import struct
import sys
from dataclasses import dataclass, field
from typing import List, Optional, Any, Dict

# 使用相对导入时需要确保包结构正确
# 如果作为独立脚本运行, 需要调整导入路?import sys
import os

# 确保可以导入同级和上级包
current_dir = os.path.dirname(os.path.abspath(__file__))
src_dir = os.path.dirname(os.path.dirname(current_dir))
if src_dir not in sys.path:
    sys.path.insert(0, src_dir)

try:
    from lm_compiler.frontend.ast_nodes import (
        Program, Variable, Assignment, FunctionBlockCall, Parameter,
        BinaryOp, UnaryOp, Literal, Identifier,
        ASTNode, Expression, Statement
    )
    from lm_compiler.function_blocks.registry import FunctionBlockRegistry
    from src.lm_compiler.backend.memory import MemoryAllocator
except ImportError:
    # 在包内使用相对导
    from src.lm_compiler.frontend.ast_nodes import (
        Program, Variable, Assignment, FunctionBlockCall, Parameter,
        BinaryOp, UnaryOp, Literal, Identifier,
        ASTNode, Expression, Statement
    )
    from src.lm_compiler.function_blocks.registry import FunctionBlockRegistry
    from src.lm_compiler.backend.memory import MemoryAllocator

# 兼容性导入：ArrayIndex / MemberAccess / FuncCallExpr
try:
    from lm_compiler.frontend.ast_nodes import ArrayIndex, MemberAccess, FuncCallExpr
except ImportError:
    try:
        from src.lm_compiler.frontend.ast_nodes import ArrayIndex, MemberAccess, FuncCallExpr
    except ImportError:
        class ArrayIndex:
            pass
        class MemberAccess:
            pass
        class FuncCallExpr:
            pass


@dataclass
class Instruction:
    """
    一?.code 指令
    
    格式: type_id type_id address param1 param2 ...
    注意: type_id 出现两次�?.code 文件的固定格
    """
    type_id: int
    address: int
    params: List[int] = field(default_factory=list)
    comment: str = ""       # 调试注释, 不写入文
    def to_code_line(self) -> str:
        """生成 .code 文件中的一"""
        parts = [str(self.type_id), str(self.type_id), str(self.address)]
        parts.extend(str(p) for p in self.params)
        return " ".join(parts)

    def __repr__(self):
        line = self.to_code_line()
        if self.comment:
            return f"{line}  // {self.comment}"
        return line


class CompileError(Exception):
    """编译错误"""
    def __init__(self, message: str, node: Optional[ASTNode] = None):
        self.node = node
        if node and hasattr(node, 'line'):
            message = f"第{node.line}? {message}"
        super().__init__(message)


class CodeGenerator:
    """
    代码生成
    ?AST 转换?.code 指令序列
    编译策略:
        1. 首先生成 _System 指令（每个程序必须有
        2. 为每个功能块实例分配内存并生成指
        3. 为简单变量赋值生成常量构建指
        4. 为表达式生成运算指令
    """

    def __init__(self, registry: FunctionBlockRegistry):
        self.registry = registry
        self.memory = MemoryAllocator()
        self.instructions: List[Instruction] = []
        self.errors: List[str] = []
        
        # 符号? 变量?-> {type, address, fb_type}
        self._symbols: Dict[str, Dict[str, Any]] = {}

    def generate(self, program: Program) -> List[Instruction]:
        """
        编译整个程序
        
        Args:
            program: AST 根节?(Program)
        
        Returns:
            指令列表
        """
        self.instructions = []
        self.errors = []
        self.memory.reset()
        self._symbols.clear()

        # ?? 生成 _System 指令 (必须是第一
        self._emit_system(program)

        # ?? 处理变量声明, 为功能块实例分配内存
        self._process_variables(program.variables)

        # ?? 处理语句, 生成指令
        for stmt in program.statements:
            self._process_statement(stmt)

        return self.instructions

    def _emit_system(self, program: Program):
        """生成 _System 功能块指"""
        system_meta = self.registry.get("_System")
        if not system_meta:
            self.errors.append("未找?_System 功能块定")
            return

        # _System 总是在地址 0
        address = self.memory.allocate("_system_", "_System", system_meta.memory_size)

        # 查找用户是否显式声明?_System 并传递了参数
        # 如果没有, 使用默认参数
        system_params = self._find_system_params(program)

        # 构建参数列表 (使用默认值填
        param_values = []
        for p_def in system_meta.parameters:
            value = system_params.get(p_def.name, p_def.default_value)
            if value is None:
                value = 0
            param_values.append(self._to_int(value))

        self.instructions.append(Instruction(
            type_id=system_meta.type_id,
            address=address,
            params=param_values,
            comment=f"_System(Author={param_values[0] if param_values else '?'})"
        ))

    def _find_system_params(self, program: Program) -> Dict[str, Any]:
        """?AST 中查?_System 的参"""
        params = {}
        
        # 查找名为 system �?_System 类型的变
        system_instance = None
        for var in program.variables:
            if var.data_type == "_System":
                system_instance = var.name
                break

        if not system_instance:
            return params

        # 查找对应的功能块调用语句
        for stmt in program.statements:
            if isinstance(stmt, FunctionBlockCall):
                if stmt.instance_name == system_instance:
                    for p in stmt.parameters:
                        params[p.name] = self._eval_literal(p.value)
                    break

        return params

    def _process_variables(self, variables: List[Variable]):
        """处理变量声明"""
        for var in variables:
            if self.registry.has(var.data_type):
                meta = self.registry.get(var.data_type)

                if meta.name == "_System":
                    # _System 已在 _emit_system 中分? 复用地址 0
                    address = 0
                else:
                    # 其他功能块类�?-> 分配功能块内
                    address = self.memory.allocate(
                        var.name, var.data_type, meta.memory_size
                    )

                self._symbols[var.name] = {
                    "type": var.data_type,
                    "address": address,
                    "is_fb": True,
                    "meta": meta
                }
            else:
                # 简单变量类型 -> 分配变量内存
                address = self.memory.allocate_variable(
                    var.name, var.data_type
                )
                sym_entry = {
                    "type": var.data_type,
                    "address": address,
                    "is_fb": False,
                    "meta": None
                }
                # 若为数组类型，解析并存储边界，用于编译时越界检查
                dt_str = str(var.data_type.value).upper() if hasattr(var.data_type, 'value') else str(var.data_type).upper()
                if dt_str.startswith("ARRAY"):
                    import re
                    m = re.search(r'\[(\d+)\.\.(\d+)\]', dt_str)
                    if m:
                        sym_entry["array_bounds"] = (int(m.group(1)), int(m.group(2)))
                self._symbols[var.name] = sym_entry

    def _process_statement(self, stmt: Statement):
        """处理一条语"""
        if isinstance(stmt, FunctionBlockCall):
            self._process_fb_call(stmt)
        elif isinstance(stmt, Assignment):
            self._process_assignment(stmt)
        else:
            # 尝试处理 IF / WHILE / FOR 等控制流语句（递归处理内部语句
            self._process_control_flow(stmt)

    def _process_control_flow(self, stmt):
        """递归处理控制流语句（IF/WHILE/FOR 等）中的子语"""
        # 常见的含有子语句列表的属性名
        body_attrs = [
            'then_statements', 'else_statements', 'elsif_clauses',
            'body', 'statements', 'true_branch', 'false_branch'
        ]
        found = False
        for attr in body_attrs:
            child = getattr(stmt, attr, None)
            if child is None:
                continue
            found = True
            if isinstance(child, list):
                for s in child:
                    if s is None:
                        continue
                    # elsif_clause 本身可能也有 then_statements
                    if isinstance(s, (FunctionBlockCall, Assignment)):
                        self._process_statement(s)
                    else:
                        self._process_control_flow(s)
            elif isinstance(child, (FunctionBlockCall, Assignment)):
                self._process_statement(child)
            else:
                self._process_control_flow(child)

        if not found:
            stmt_type = type(stmt).__name__
            if stmt_type not in ('NoneType',):
                self.errors.append(f"未知语句类型: {stmt_type}")

    def _process_fb_call(self, call: FunctionBlockCall):
        """
        处理功能块调
        ?system(Author := 1, Config := 100) 转换
            type_id type_id address param1 param2 ...
        """
        # call.instance_name 可能?str ?Identifier 对象
        instance_name = call.instance_name
        if hasattr(call.instance_name, 'name'):
            instance_name = call.instance_name.name
        elif not isinstance(call.instance_name, str):
            instance_name = str(call.instance_name)

        sym = self._symbols.get(instance_name)
        if not sym:
            self.errors.append(
                f"未声明的功能块实例: {instance_name}"
            )
            return

        if not sym.get("is_fb"):
            self.errors.append(
                f"{instance_name} 不是功能块类型"
            )
            return

        meta = sym["meta"]
        address = sym["address"]

        # _System 已经在 _emit_system 中处理过，跳过
        if meta.name == "_System":
            return

        # 验证调用中所有参数名在功能块定义中存在
        # 仅当功能块定义了非空参数列表时才做校验，
        # 若 meta.parameters 为空（部分功能块采用通用参数接口），则跳过
        if meta.parameters:
            valid_param_names = {p_def.name.upper() for p_def in meta.parameters}
            for p in call.parameters:
                if p.name.upper() not in valid_param_names:
                    self.errors.append(
                        f"功能块 '{meta.name}' 不接受参数 '{p.name}'，"
                        f"有效参数为: {', '.join(p_def.name for p_def in meta.parameters)}"
                    )
                    return

        # 构建参数值列表
        param_values = []
        for p_def in meta.parameters:
            # 查找调用中是否提供了这个参数
            provided = None
            for p in call.parameters:
                if p.name.upper() == p_def.name.upper():
                    provided = self._eval_expression(p.value)
                    break

            if provided is not None:
                param_values.append(self._to_int(provided))
            elif p_def.default_value is not None:
                param_values.append(self._to_int(p_def.default_value))
            else:
                param_values.append(0)

        self.instructions.append(Instruction(
            type_id=meta.type_id,
            address=address,
            params=param_values,
            comment=f"{instance_name}({meta.name})"
        ))

    def _process_assignment(self, assign: Assignment):
        """
        处理赋值语
        ?temp := 25.5 转换为常量构建指
        策略:
            - 字面量赋?-> 生成对应类型?ConstBuild 指令
            - 表达式赋?-> 需要生成运算链（后续扩展）
        """
        # assign.target 可能是 str、Identifier、ArrayIndex、MemberAccess 对象
        target = assign.target

        # 成员访问 instance.member -> 跳过（运行时赋值，暂不生成指令）
        if isinstance(target, MemberAccess):
            return
        if isinstance(target, str) and '.' in target:
            return

        # 数组下标 arr[i] -> 提取 base 名称用于符号表查找
        if isinstance(target, ArrayIndex):
            base = target.base if hasattr(target, 'base') else None
            if base is not None:
                target_name = base.name if hasattr(base, 'name') else str(base)
            else:
                target_name = str(target)
            # 常量索引越界检查
            index_expr = target.index if hasattr(target, 'index') else None
            if index_expr is not None and isinstance(index_expr, Literal):
                try:
                    idx = int(index_expr.value)
                    sym_check = self._symbols.get(target_name)
                    if sym_check and "array_bounds" in sym_check:
                        lo, hi = sym_check["array_bounds"]
                        if idx < lo or idx > hi:
                            self.errors.append(
                                f"数组 '{target_name}' 索引 {idx} 越界"
                                f"（声明范围 [{lo}..{hi}]）"
                            )
                            return
                except (ValueError, TypeError):
                    pass
        elif hasattr(target, 'name'):
            target_name = target.name
        elif isinstance(target, str):
            # 处理字符串形式的数组下标，如 "txBuffer[0]" -> "txBuffer"
            if '[' in target:
                import re
                target_name = target.split('[')[0]
                # 常量索引越界检查（字符串形式）
                m = re.search(r'\[(\d+)\]', target)
                if m:
                    try:
                        idx = int(m.group(1))
                        sym_check = self._symbols.get(target_name)
                        if sym_check and "array_bounds" in sym_check:
                            lo, hi = sym_check["array_bounds"]
                            if idx < lo or idx > hi:
                                self.errors.append(
                                    f"数组 '{target_name}' 索引 {idx} 越界"
                                    f"（声明范围 [{lo}..{hi}]）"
                                )
                                return
                    except (ValueError, TypeError):
                        pass
            else:
                target_name = target
        else:
            target_name = str(target)

        sym = self._symbols.get(target_name)
        if not sym:
            self.errors.append(f"未声明的变量: {target_name}")
            return

        value = self._eval_expression(assign.value)

        if isinstance(assign.value, Literal):
            # 简单字面量赋?-> 生成常量构建指令
            self._emit_const_build(target_name, sym["type"], value)
        elif isinstance(assign.value, Identifier):
            # 变量引用 -> 需要生成拷贝指令（后续扩展
            self._emit_const_build(target_name, sym["type"], value)
        elif isinstance(assign.value, (BinaryOp, UnaryOp)):
            # 表达式 -> 尝试常量折叠，折叠成功则生成指令，否则跳过（运行时求值）
            if value is not None:
                self._emit_const_build(target_name, sym["type"], value)
            # 无法常量折叠时不报错，运行时处理
        elif isinstance(assign.value, (ArrayIndex, MemberAccess)):
            # 数组下标或成员访问作为右值 -> 运行时求值，跳过常量生成
            pass
        elif isinstance(assign.value, FuncCallExpr):
            # 函数调用表达式作为右值（如 INT_TO_REAL、REAL_TO_INT 等转换函数）
            # 运行时求值，编译阶段跳过常量生成
            pass
        else:
            # 未知类型：只警告，不中断编译
            self.errors.append(
                f"未知的表达式类型: {type(assign.value).__name__}"
            )

    def _emit_const_build(self, var_name: str, data_type: str, value: Any):
        """
        生成常量构建指令
        
        根据数据类型选择对应�?ConstBuild 功能
        """
        dt_upper = str(data_type.value).upper() if hasattr(data_type, 'value') else str(data_type).upper()

        # ARRAY 类型无法用 ConstBuild 初始化，直接跳过
        if dt_upper.startswith("ARRAY") or dt_upper.startswith("STRING"):
            return

        if dt_upper in ("INT", "DINT", "UINT", "WORD", "DWORD", "BYTE"):
            fb_name = "_IntConstBuild"
        elif dt_upper in ("REAL", "LREAL"):
            fb_name = "_RealConstBuild"
        elif dt_upper == "BOOL":
            fb_name = "_BoolConstBuild"
        else:
            self.errors.append(f"不支持的数据类型: {data_type}")
            return

        meta = self.registry.get(fb_name)
        if not meta:
            self.errors.append(f"未找到功能块: {fb_name}")
            return

        # 为这个常量构建器分配内存
        alloc_name = f"_const_{var_name}"
        address = self.memory.allocate(alloc_name, fb_name, meta.memory_size)

        self.instructions.append(Instruction(
            type_id=meta.type_id,
            address=address,
            params=[self._to_int(value)],
            comment=f"{var_name} := {value} ({fb_name})"
        ))

    def _eval_expression(self, expr: Expression) -> Any:
        """
        求值表达式（编译时常量折叠
        对于可以在编译时计算的表达式, 直接求值
        对于运行时表达式, 返回 None
        """
        if expr is None:
            return None

        if isinstance(expr, Literal):
            return self._eval_literal(expr)

        if isinstance(expr, Identifier):
            # 如果变量有已知的编译时�? 返回
            # 目前简单处? 返回 0
            return 0

        if isinstance(expr, BinaryOp):
            left = self._eval_expression(expr.left)
            right = self._eval_expression(expr.right)
            if left is not None and right is not None:
                return self._eval_binary_op(expr.operator, left, right)
            return None

        if isinstance(expr, UnaryOp):
            operand = self._eval_expression(expr.operand)
            if operand is not None:
                return self._eval_unary_op(expr.operator, operand)
            return None

        # 数组下标 arr[i] -> 返回 base 变量地址（简化处理）
        if isinstance(expr, ArrayIndex):
            base = expr.base if hasattr(expr, 'base') else None
            if base is not None:
                base_name = base.name if hasattr(base, 'name') else str(base)
                sym = self._symbols.get(base_name)
                if sym:
                    return sym['address']
            return 0

        # 成员访问 instance.member -> 运行时值，编译期返回 0
        if isinstance(expr, MemberAccess):
            return 0

        # 函数调用表达式（如 INT_TO_REAL、REAL_TO_INT）-> 运行时求值，返回 None
        if isinstance(expr, FuncCallExpr):
            return None

        return None

    def _eval_literal(self, lit) -> Any:
        """求值字面量"""
        if isinstance(lit, Literal):
            val = lit.value
            dt = str(lit.data_type.value).upper() if lit.data_type else ""

            if dt == "BOOL":
                if isinstance(val, str):
                    return 1 if val.upper() in ("TRUE", "1") else 0
                return 1 if val else 0
            elif dt in ("INT", "DINT", "UINT"):
                return int(val)
            elif dt in ("REAL", "LREAL"):
                return float(val)
            else:
                # 尝试自动推断
                try:
                    if "." in str(val):
                        return float(val)
                    return int(val)
                except (ValueError, TypeError):
                    return 0
        elif isinstance(lit, (int, float)):
            return lit
        return 0

    @staticmethod
    def _eval_binary_op(operator, left, right):
        """计算二元运算"""
        op = str(operator).upper()
        # 处理枚举类型
        if hasattr(operator, 'value'):
            op = str(operator.value).upper()
        if hasattr(operator, 'name'):
            op = operator.name.upper()

        try:
            if op in ("ADD", "+"):
                return left + right
            elif op in ("SUB", "-"):
                return left - right
            elif op in ("MUL", "*"):
                return left * right
            elif op in ("DIV", "/"):
                return left / right if right != 0 else 0
            elif op in ("MOD", "%"):
                return left % right if right != 0 else 0
            elif op in ("AND", "&"):
                return int(bool(left) and bool(right))
            elif op in ("OR", "|"):
                return int(bool(left) or bool(right))
            elif op in ("XOR", "^"):
                return int(bool(left) ^ bool(right))
            elif op in ("EQ", "=", "=="):
                return int(left == right)
            elif op in ("NE", "<>", "!="):
                return int(left != right)
            elif op in ("LT", "<"):
                return int(left < right)
            elif op in ("GT", ">"):
                return int(left > right)
            elif op in ("LE", "<="):
                return int(left <= right)
            elif op in ("GE", ">="):
                return int(left >= right)
        except (TypeError, ZeroDivisionError):
            pass
        return None

    @staticmethod
    def _eval_unary_op(operator, operand):
        """计算一元运"""
        op = str(operator).upper()
        if hasattr(operator, 'value'):
            op = str(operator.value).upper()
        if hasattr(operator, 'name'):
            op = operator.name.upper()

        if op in ("NOT", "!"):
            return int(not bool(operand))
        elif op in ("NEG", "-"):
            return -operand
        return None

    @staticmethod
    def _to_int(value: Any) -> int:
        """将值转换为整数（用于写?.code 文件"""
        if value is None:
            return 0
        if isinstance(value, bool):
            return 1 if value else 0
        if isinstance(value, float):
            # 对于浮点? 使用 IEEE 754 编码
            # �?float 转为�?32 位整数表
            return struct.unpack('!I', struct.pack('!f', value))[0]
        try:
            return int(value)
        except (ValueError, TypeError):
            return 0
