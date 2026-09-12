# -*- coding: utf-8 -*-
"""
代码生成器：遍历 AST 生成 .code 文件所需的指令序列
编译流程:
    源代码 -> ANTLR解析 -> AST -> CodeGenerator -> 指令列表 -> CodeEmitter -> .code文件
"""

import struct
import math
import sys
import os
from dataclasses import dataclass, field
from typing import List, Optional, Any, Dict, Tuple

FLOAT32_MAX = 3.4028234663852886e+38

def encode_float32(val: Any) -> Tuple[Optional[int], Optional[str]]:
    """
    严格校验并将浮点数或整型数值编码为 IEEE-754 32位单精度整型表示。
    要求:
    - 拒绝布尔值
    - 必须为有限数值 (math.isfinite)，严禁 Infinity、NaN
    - 绝对值不得超出 float32 表示范围 [-FLOAT32_MAX, FLOAT32_MAX]
    - 返回 (encoded_uint32, error_msg)
    """
    if isinstance(val, bool):
        return None, "浮点类型无法接受布尔值"
    if not isinstance(val, (int, float)):
        try:
            val = float(val)
        except (ValueError, TypeError) as e:
            return None, f"无法转为浮点数: {e}"

    try:
        f_val = float(val)
    except (ValueError, TypeError, OverflowError) as e:
        return None, f"无法转为浮点数: {e}"

    if not math.isfinite(f_val):
        return None, f"非有限浮点数 (Infinity/NaN): {f_val}"

    if abs(f_val) > FLOAT32_MAX:
        return None, f"浮点数值 {f_val} 超出 32 位单精度浮点数表示范围 [-{FLOAT32_MAX}, {FLOAT32_MAX}]"

    try:
        packed = struct.pack('!f', f_val)
        encoded = struct.unpack('!I', packed)[0]
        return encoded, None
    except struct.error as e:
        return None, f"单精度浮点数编码失败: {e}"


# 确保可以导入同级和上级包
current_dir = os.path.dirname(os.path.abspath(__file__))
src_dir = os.path.dirname(os.path.dirname(current_dir))
if src_dir not in sys.path:
    sys.path.insert(0, src_dir)

try:
    from lh_compiler.frontend.ast_nodes import (
        Program, Variable, Assignment, FunctionBlockCall, Parameter,
        BinaryOp, UnaryOp, Literal, Identifier,
        ASTNode, Expression, Statement,
        IfStatement, ElseIfClause, CaseStatement, CaseClause,
        ForStatement, WhileStatement, RepeatStatement,
        ExitStatement, ReturnStatement, ContinueStatement,
        MemberAccess, ArrayIndex, FuncCallExpr
    )
    from lh_compiler.function_blocks.registry import FunctionBlockRegistry
    from lh_compiler.backend.memory import MemoryAllocator
except ImportError:
    from src.lh_compiler.frontend.ast_nodes import (
        Program, Variable, Assignment, FunctionBlockCall, Parameter,
        BinaryOp, UnaryOp, Literal, Identifier,
        ASTNode, Expression, Statement,
        IfStatement, ElseIfClause, CaseStatement, CaseClause,
        ForStatement, WhileStatement, RepeatStatement,
        ExitStatement, ReturnStatement, ContinueStatement,
        MemberAccess, ArrayIndex, FuncCallExpr
    )
    from src.lh_compiler.function_blocks.registry import FunctionBlockRegistry
    from src.lh_compiler.backend.memory import MemoryAllocator


@dataclass
class Instruction:
    """
    一条 .code 指令
    格式: type_id type_id address param1 param2 ...
    注意: type_id 出现两次是 .code 文件的固定格式
    """
    type_id: int
    address: int
    params: List[int] = field(default_factory=list)
    comment: str = ""       # 调试注释, 不写入文件

    def to_code_line(self) -> str:
        """生成 .code 文件中的一行"""
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
            message = f"第{node.line}行: {message}"
        super().__init__(message)


class CodeGenerator:
    """
    代码生成器
    将 AST 转换为 .code 指令列表
    """

    def __init__(self, registry: FunctionBlockRegistry = None):
        if registry is None:
            self.registry = FunctionBlockRegistry()
            self.registry.load_defaults()
        else:
            self.registry = registry

        self.memory = MemoryAllocator()
        self.instructions: List[Instruction] = []
        self._symbols: Dict[str, dict] = {}
        self.errors: List[str] = []

    def generate(self, program: Program) -> List[Instruction]:
        """
        主生成函数: 遍历 AST 生成指令序列
        """
        self.instructions = []
        self.memory.reset()
        self._symbols = {}
        self.errors = []

        # 1. 首先生成 _System 功能块指令 (总是在地址 0)
        self._emit_system(program)

        # 2. 处理变量声明, 为功能块实例分配内存
        self._process_variables(program.variables)

        # 3. 处理语句, 生成指令
        for stmt in program.statements:
            self._process_statement(stmt)

        return self.instructions

    def _validate_and_encode_param(
        self, fb_name: str, param_name: str, data_type: Any, val: Any, line_str: str = ""
    ) -> Optional[int]:
        """
        严格校验功能块参数契约并编码为 32 位整型表示:
        - REAL/LREAL: IEEE-754 32位单精度浮点数
        - BOOL: 严格只接受布尔值或 0/1, 拒绝其他整数或浮点数
        - INT/整型: 严格拒绝浮点数 (如 2.5), 严格校验范围 [-32768, 32767]
        """
        dt_upper = str(data_type.value if hasattr(data_type, 'value') else data_type).upper().strip()

        if dt_upper in ("REAL", "LREAL"):
            encoded, err = encode_float32(val)
            if err:
                self.errors.append(
                    f"{line_str}功能块 '{fb_name}' 参数 '{param_name}': {err}"
                )
                return None
            return encoded

        elif dt_upper == "BOOL":
            if isinstance(val, bool):
                return 1 if val else 0
            if isinstance(val, int):
                if val in (0, 1):
                    return val
                self.errors.append(
                    f"{line_str}功能块 '{fb_name}' 参数 '{param_name}' 类型为 BOOL，只接受 TRUE/FALSE 或 0/1，传入了非法值: {val}"
                )
                return None
            if isinstance(val, float):
                self.errors.append(
                    f"{line_str}功能块 '{fb_name}' 参数 '{param_name}' 类型为 BOOL，无法接受浮点数: {val}"
                )
                return None
            if isinstance(val, str):
                s = val.strip().upper()
                if s in ("TRUE", "1"):
                    return 1
                if s in ("FALSE", "0"):
                    return 0
                self.errors.append(
                    f"{line_str}功能块 '{fb_name}' 参数 '{param_name}' 类型为 BOOL，只接受 TRUE/FALSE 或 0/1，传入了非法值: '{val}'"
                )
                return None
            self.errors.append(
                f"{line_str}功能块 '{fb_name}' 参数 '{param_name}' 类型为 BOOL，只接受 TRUE/FALSE 或 0/1"
            )
            return None

        elif dt_upper in ("INT", "DINT", "SINT", "UINT", "USINT", "UDINT", "WORD", "BYTE", "DWORD"):
            if isinstance(val, bool):
                self.errors.append(
                    f"{line_str}功能块 '{fb_name}' 参数 '{param_name}' 类型为 {dt_upper}，无法接受布尔值"
                )
                return None
            if isinstance(val, float):
                self.errors.append(
                    f"{line_str}功能块 '{fb_name}' 参数 '{param_name}' 类型不匹配: 浮点数无法隐式转换为整型"
                )
                return None
            if not isinstance(val, int):
                try:
                    val = int(val)
                except (ValueError, TypeError):
                    self.errors.append(
                        f"{line_str}功能块 '{fb_name}' 参数 '{param_name}' 无法转换为整数: {val}"
                    )
                    return None

            type_ranges = {
                "SINT": (-128, 127),
                "USINT": (0, 255),
                "BYTE": (0, 255),
                "INT": (-32768, 32767),
                "UINT": (0, 65535),
                "WORD": (0, 65535),
                "DINT": (-2147483648, 2147483647),
                "UDINT": (0, 4294967295),
                "DWORD": (0, 4294967295),
            }
            if dt_upper in type_ranges:
                min_val, max_val = type_ranges[dt_upper]
                if val < min_val or val > max_val:
                    self.errors.append(
                        f"{line_str}功能块 '{fb_name}' 参数 '{param_name}' 的值 {val} 超出 {dt_upper} 有效范围 [{min_val}, {max_val}]"
                    )
                    return None

            return val

        else:
            self.errors.append(
                f"{line_str}功能块 '{fb_name}' 参数 '{param_name}' 具有不支持的数据类型: '{data_type}'"
            )
            return None

    def _emit_system(self, program: Program):
        """生成 _System 功能块指令并严格校验参数契约"""
        system_meta = self.registry.get("System")
        if not system_meta:
            self.errors.append("未找到 System 功能块定义")
            return

        # _System 总是在地址 0
        address = self.memory.allocate("_system_", "System", system_meta.memory_size)

        valid_params = {p.name.upper(): p for p in system_meta.parameters}

        # 查找所有声明为 System 类型的变量名（大小写不敏感）
        system_var_names = set()
        for var in program.variables:
            dt = str(var.data_type.value if hasattr(var.data_type, 'value') else var.data_type).strip()
            if dt.upper() == "SYSTEM":
                system_var_names.add(var.name.lower())

        # 查找对应的 System 功能块调用语句
        system_calls = []
        for stmt in program.statements:
            if isinstance(stmt, FunctionBlockCall):
                inst_lower = stmt.instance_name.lower()
                if inst_lower in system_var_names or inst_lower in ("system", "_system_", "sys"):
                    system_calls.append(stmt)

        if len(system_calls) > 1:
            self.errors.append("功能块 'System' 只能被调用一次")

        param_values_map = {}
        if system_calls:
            call = system_calls[0]
            line = getattr(call, 'line', 0)
            line_str = f"第 {line} 行: " if line else ""

            # 检查重复参数
            has_duplicate = False
            seen_params = set()
            for p in call.parameters:
                p_name = p.name if hasattr(p, 'name') else str(p)
                p_upper = p_name.upper()
                if p_upper in seen_params:
                    self.errors.append(f"{line_str}功能块 'System' 存在重复参数 '{p_name}'")
                    has_duplicate = True
                seen_params.add(p_upper)

            if has_duplicate:
                return

            for p in call.parameters:
                p_name = p.name if hasattr(p, 'name') else str(p)
                p_upper = p_name.upper()
                if p_upper not in valid_params:
                    self.errors.append(
                        f"{line_str}功能块 'System' 不接受参数 '{p_name}'，"
                        f"有效参数为: {', '.join(p_def.name for p_def in system_meta.parameters)}"
                    )
                    continue

                val = self._eval_expression(p.value)
                if val is None:
                    self.errors.append(
                        f"{line_str}功能块 'System' 参数 '{p_name}' 的值无法在编译期求值为有效常量"
                    )
                    continue

                p_def = valid_params[p_upper]
                encoded = self._validate_and_encode_param(
                    "System", p_def.name, p_def.data_type, val, line_str
                )
                if encoded is not None:
                    param_values_map[p_upper] = encoded

        # 构建参数列表（按 system_meta.parameters 顺序填入）
        param_values = []
        for p_def in system_meta.parameters:
            if p_def.name.upper() in param_values_map:
                val = param_values_map[p_def.name.upper()]
            elif p_def.default_value is not None:
                encoded = self._validate_and_encode_param(
                    "System", p_def.name, p_def.data_type, p_def.default_value, ""
                )
                val = encoded if encoded is not None else 0
            else:
                val = 0
            param_values.append(int(val))

        self.instructions.append(Instruction(
            type_id=system_meta.type_id,
            address=address,
            params=param_values,
            comment=f"System(Author={param_values[0] if param_values else '?'})"
        ))

    def _process_variables(self, variables: List[Variable]):
        """处理变量声明"""
        declared_names = set()
        for var in variables:
            if var.name in declared_names:
                self.errors.append(f"第 {var.line} 行: 重复声明变量 '{var.name}'")
                continue
            declared_names.add(var.name)
            if self.registry.has(var.data_type):
                meta = self.registry.get(var.data_type)
                line = getattr(var, 'line', 0)
                line_str = f"第 {line} 行: " if line else ""

                if meta.status == "incomplete":
                    reason = meta.incomplete_reason or "缺少参数契约与协议定义 (TODO)"
                    self.errors.append(
                        f"{line_str}功能块 '{meta.name}' 尚未完善契约定义 ({reason})，禁止生成控制代码"
                    )
                    continue

                if meta.name == "System":
                    # _System 已在 _emit_system 中分配, 复用地址 0
                    address = 0
                else:
                    # 其他功能块类型 -> 分配功能块内存
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
                dt_str = str(var.data_type.value).upper() if hasattr(var.data_type, 'value') else str(var.data_type).upper()
                line = getattr(var, 'line', 0)
                line_str = f"第 {line} 行: " if line else ""

                if dt_str.startswith("ARRAY"):
                    self.errors.append(f"{line_str}暂不支持数组类型 '{var.data_type}'，禁止生成控制代码")
                    continue
                if dt_str.startswith("STRING"):
                    self.errors.append(f"{line_str}暂不支持字符串类型 '{var.data_type}'，禁止生成控制代码")
                    continue
                if dt_str in ("TIME", "DATE", "DT", "TOD", "POINTER"):
                    self.errors.append(f"{line_str}暂不支持数据类型 '{var.data_type}'，禁止生成控制代码")
                    continue
                if dt_str not in ("BOOL", "BYTE", "INT", "UINT", "SINT", "USINT", "DINT", "UDINT", "WORD", "DWORD", "REAL", "LREAL"):
                    self.errors.append(f"{line_str}未支持的数据类型: '{var.data_type}'")
                    continue

                # 简单变量类型 -> 分配变量内存
                address = self.memory.allocate_variable(
                    var.name, var.data_type
                )
                self._symbols[var.name] = {
                    "type": var.data_type,
                    "address": address,
                    "is_fb": False,
                    "meta": None
                }
                if var.initial_value is not None:
                    init_val = self._eval_expression(var.initial_value)
                    if init_val is None:
                        self.errors.append(f"{line_str}变量 '{var.name}' 初始值无法在编译期求值")
                    else:
                        self._validate_and_encode_param(
                            "变量初始化", var.name, dt_str, init_val, line_str)

    def _process_statement(self, stmt: Statement):
        """处理一条语句"""
        if isinstance(stmt, FunctionBlockCall):
            self._process_fb_call(stmt)
        elif isinstance(stmt, Assignment):
            self._process_assignment(stmt)
        elif isinstance(stmt, (IfStatement, ElseIfClause, CaseStatement, CaseClause,
                               ForStatement, WhileStatement, RepeatStatement,
                               ExitStatement, ReturnStatement, ContinueStatement)):
            stmt_type = type(stmt).__name__
            line = getattr(stmt, 'line', 0)
            line_str = f"第 {line} 行: " if line else ""
            self.errors.append(
                f"{line_str}暂不支持控制流语句 {stmt_type}，指令生成拒绝静默跳过，禁止生成控制代码"
            )
        else:
            stmt_type = type(stmt).__name__
            if stmt_type not in ('NoneType',):
                self.errors.append(f"未知或不支持的语句类型: {stmt_type}")

    def _process_fb_call(self, call: FunctionBlockCall):
        """处理功能块调用"""
        instance_name = call.instance_name
        if hasattr(call.instance_name, 'name'):
            instance_name = call.instance_name.name
        elif not isinstance(call.instance_name, str):
            instance_name = str(call.instance_name)

        line = getattr(call, 'line', 0)
        line_str = f"第 {line} 行: " if line else ""

        # 如果直接以未完善契约的功能块名称进行调用，例如 FilterBW()
        direct_meta = self.registry.get(instance_name)
        if direct_meta and direct_meta.status == "incomplete":
            reason = direct_meta.incomplete_reason or "缺少参数契约与协议定义 (TODO)"
            self.errors.append(
                f"{line_str}功能块 '{direct_meta.name}' 尚未完善契约定义 ({reason})，禁止调用"
            )
            return

        sym = self._symbols.get(instance_name)
        if not sym:
            self.errors.append(f"{line_str}未声明的功能块实例: {instance_name}")
            return

        if not sym.get("is_fb"):
            self.errors.append(f"{line_str}{instance_name} 不是功能块类型")
            return

        meta = sym["meta"]
        address = sym["address"]

        if meta.status == "incomplete":
            reason = meta.incomplete_reason or "缺少参数契约与协议定义 (TODO)"
            self.errors.append(
                f"{line_str}功能块 '{meta.name}' 尚未完善契约定义 ({reason})，禁止调用"
            )
            return

        # _System 已经在 _emit_system 中处理过，跳过
        if meta.name == "System":
            return

        # 检查重复参数
        has_duplicate = False
        seen_params = set()
        for p in call.parameters:
            p_name = p.name if hasattr(p, 'name') else str(p)
            p_upper = p_name.upper()
            if p_upper in seen_params:
                self.errors.append(f"{line_str}功能块 '{meta.name}' 存在重复参数 '{p_name}'")
                has_duplicate = True
            seen_params.add(p_upper)

        if has_duplicate:
            return

        # 验证调用中所有参数名在功能块定义中存在
        # 若功能块未定义参数列表（meta.parameters 为空），但调用时传入了参数，严格报错拒绝
        if not meta.parameters:
            if call.parameters:
                param_names = [p.name for p in call.parameters if hasattr(p, 'name')]
                self.errors.append(
                    f"{line_str}功能块 '{meta.name}' 未定义参数（不接受参数调用），"
                    f"但调用时传入了参数: {', '.join(param_names)}"
                )
                return
        else:
            valid_param_names = {p_def.name.upper() for p_def in meta.parameters}
            for p in call.parameters:
                p_name = p.name if hasattr(p, 'name') else str(p)
                if p_name.upper() not in valid_param_names:
                    self.errors.append(
                        f"{line_str}功能块 '{meta.name}' 不接受参数 '{p_name}'，"
                        f"有效参数为: {', '.join(p_def.name for p_def in meta.parameters)}"
                    )
                    return

        # 构建参数值列表
        param_values = []
        for p_def in meta.parameters:
            # 查找调用中是否提供了这个参数
            provided = None
            found = False
            for p in call.parameters:
                p_name = p.name if hasattr(p, 'name') else str(p)
                if p_name.upper() == p_def.name.upper():
                    found = True
                    provided = self._eval_expression(p.value)
                    break

            if provided is not None:
                val = provided
            elif found:
                self.errors.append(
                    f"{line_str}功能块 '{meta.name}' 参数 '{p_def.name}' 的值无法在编译期求值为有效常量"
                )
                return
            elif p_def.default_value is not None:
                val = p_def.default_value
            else:
                self.errors.append(
                    f"{line_str}功能块 '{meta.name}' 缺少必需参数 '{p_def.name}' 且无默认值"
                )
                return

            encoded = self._validate_and_encode_param(
                meta.name, p_def.name, p_def.data_type, val, line_str
            )
            if encoded is None:
                return
            param_values.append(encoded)

        self.instructions.append(Instruction(
            type_id=meta.type_id,
            address=address,
            params=param_values,
            comment=f"{instance_name}({meta.name})"
        ))

    def _process_assignment(self, assign: Assignment):
        """处理赋值语句"""
        target = assign.target
        line = getattr(assign, 'line', 0)
        line_str = f"第 {line} 行: " if line else ""

        # 成员访问 instance.member 赋值 -> 暂不支持运行时成员赋值，禁止静默跳过
        if isinstance(target, MemberAccess) or (isinstance(target, str) and '.' in target):
            self.errors.append(f"{line_str}暂不支持成员访问赋值: '{target}'")
            return

        # 数组下标 arr[i] -> 暂不支持数组元素访问或赋值，禁止静默跳过或篡改基地址
        if isinstance(target, ArrayIndex) or (isinstance(target, str) and '[' in target):
            self.errors.append(f"{line_str}暂不支持数组元素访问或赋值: '{target}'")
            return

        if hasattr(target, 'name'):
            target_name = target.name
        else:
            target_name = str(target)

        sym = self._symbols.get(target_name)
        if not sym:
            self.errors.append(f"{line_str}未声明的变量: {target_name}")
            return

        initial_errors_count = len(self.errors)
        value = self._eval_expression(assign.value)

        if value is not None:
            # 简单字面量或可常量折叠表达式 -> 生成常量构建指令
            self._emit_const_build(target_name, sym["type"], value, line=line)
        elif len(self.errors) > initial_errors_count:
            # 表达式求值过程中已产生具体错误（例如除零），直接返回
            return
        elif isinstance(assign.value, Literal):
            self.errors.append(
                f"{line_str}暂不支持字面量赋值: '{target_name}'（值: {assign.value.value}）"
            )
        elif isinstance(assign.value, Identifier):
            # 变量引用 -> 暂无变量拷贝指令契约，报错拒绝
            self.errors.append(
                f"{line_str}暂不支持变量赋值/拷贝指令: '{target_name} := {assign.value.name}'"
            )
        elif isinstance(assign.value, (BinaryOp, UnaryOp)):
            # 无法常量折叠的表达式
            self.errors.append(
                f"{line_str}无法常量折叠的表达式赋值，暂不支持运行时求值指令: '{target_name}'"
            )
        elif isinstance(assign.value, ArrayIndex):
            self.errors.append(
                f"{line_str}暂不支持数组元素作为右值赋值: '{target_name}'"
            )
        elif isinstance(assign.value, MemberAccess):
            self.errors.append(
                f"{line_str}暂不支持成员访问作为右值赋值: '{target_name}'"
            )
        elif isinstance(assign.value, FuncCallExpr):
            self.errors.append(
                f"{line_str}暂不支持函数调用表达式作为右值: '{assign.value.name}'"
            )
        else:
            self.errors.append(
                f"{line_str}未知的表达式类型: {type(assign.value).__name__}"
            )

    def _emit_const_build(self, var_name: str, data_type: str, value: Any, line: int = 0):
        """
        生成常量构建指令

        根据数据类型选择对应的 ConstBuild 功能
        """
        line_str = f"第 {line} 行: " if line else ""
        dt_upper = str(data_type.value).upper() if hasattr(data_type, 'value') else str(data_type).upper()

        if dt_upper in ("REAL", "LREAL"):
            fb_name = "RealConstBuild"
            encoded, err = encode_float32(value)
            if err:
                self.errors.append(f"{line_str}变量 '{var_name}' 浮点数编码失败: {err}")
                return
            param_val = encoded

        elif dt_upper == "BOOL":
            fb_name = "BoolConstBuild"
            if isinstance(value, bool):
                param_val = 1 if value else 0
            elif isinstance(value, (int, float)) and value in (0, 1):
                param_val = int(value)
            else:
                self.errors.append(f"{line_str}类型不匹配: 无法将 '{value}' 赋值给布尔类型 '{dt_upper}'")
                return

        elif dt_upper in ("INT", "DINT", "UINT", "WORD", "DWORD", "BYTE", "SINT", "USINT", "UDINT"):
            fb_name = "IntConstBuild"
            # 强类型检查：禁止非整数浮点数隐式赋值给整数
            if isinstance(value, float):
                if not value.is_integer():
                    self.errors.append(f"{line_str}类型不匹配: 浮点数值 {value} 无法隐式赋值给整数类型 '{dt_upper}'")
                    return
                value = int(value)
            elif isinstance(value, bool):
                self.errors.append(f"{line_str}类型不匹配: 布尔值无法隐式赋值给整数类型 '{dt_upper}'")
                return
            elif not isinstance(value, int):
                try:
                    value = int(value)
                except (ValueError, TypeError):
                    self.errors.append(f"{line_str}类型不匹配: 无法将 '{value}' 赋值给整数类型 '{dt_upper}'")
                    return

            # 严格范围检查
            type_ranges = {
                "SINT": (-128, 127),
                "USINT": (0, 255),
                "BYTE": (0, 255),
                "INT": (-32768, 32767),
                "UINT": (0, 65535),
                "WORD": (0, 65535),
                "DINT": (-2147483648, 2147483647),
                "UDINT": (0, 4294967295),
                "DWORD": (0, 4294967295),
            }
            if dt_upper in type_ranges:
                min_val, max_val = type_ranges[dt_upper]
                if value < min_val or value > max_val:
                    self.errors.append(f"{line_str}值 {value} 超出类型 '{dt_upper}' 的有效范围 [{min_val}, {max_val}]")
                    return

            param_val = value
        else:
            self.errors.append(f"{line_str}不支持用于常量构建的数据类型: {data_type}")
            return

        meta = self.registry.get(fb_name)
        if not meta:
            self.errors.append(f"{line_str}未找到功能块: {fb_name}")
            return

        alloc_name = f"_const_{var_name}"
        address = self.memory.allocate(alloc_name, fb_name, meta.memory_size)

        self.instructions.append(Instruction(
            type_id=meta.type_id,
            address=address,
            params=[param_val],
            comment=f"{var_name} := {value} ({fb_name})"
        ))

    def _eval_expression(self, expr: Expression) -> Any:
        """
        求值表达式（编译时常量折叠）
        对于可以在编译时计算的常量表达式, 直接求值
        对于运行时变量或表达式, 返回 None（禁止伪造为 0）
        """
        if expr is None:
            return None

        if isinstance(expr, Literal):
            return self._eval_literal(expr)

        if isinstance(expr, Identifier):
            # 运行时变量无法在编译期求值为常数
            return None

        if isinstance(expr, BinaryOp):
            left = self._eval_expression(expr.left)
            right = self._eval_expression(expr.right)
            if left is not None and right is not None:
                try:
                    res = self._eval_binary_op(expr.operator, left, right)
                    if isinstance(res, float):
                        if not math.isfinite(res):
                            raise ValueError(f"非有限浮点数 (Infinity/NaN): {res}")
                        if abs(res) > FLOAT32_MAX:
                            raise OverflowError(f"浮点数值 {res} 超出 32 位单精度浮点数表示范围")
                    return res
                except (ZeroDivisionError, ValueError, OverflowError) as e:
                    line = getattr(expr, 'line', 0)
                    line_str = f"第 {line} 行: " if line else ""
                    self.errors.append(f"{line_str}常量表达式计算错误: {e}")
                    return None
            return None

        if isinstance(expr, UnaryOp):
            operand = self._eval_expression(expr.operand)
            if operand is not None:
                try:
                    res = self._eval_unary_op(expr.operator, operand)
                    if isinstance(res, float):
                        if not math.isfinite(res):
                            raise ValueError(f"非有限浮点数 (Infinity/NaN): {res}")
                        if abs(res) > FLOAT32_MAX:
                            raise OverflowError(f"浮点数值 {res} 超出 32 位单精度浮点数表示范围")
                    return res
                except (ValueError, OverflowError) as e:
                    line = getattr(expr, 'line', 0)
                    line_str = f"第 {line} 行: " if line else ""
                    self.errors.append(f"{line_str}常量表达式计算错误: {e}")
                    return None
            return None

        # 数组下标 arr[i] -> 运行时值，返回 None
        if isinstance(expr, ArrayIndex):
            return None

        # 成员访问 instance.member -> 运行时值，返回 None
        if isinstance(expr, MemberAccess):
            return None

        # 函数调用表达式 -> 运行时求值，返回 None
        if isinstance(expr, FuncCallExpr):
            return None

        return None

    def _eval_literal(self, lit) -> Any:
        """求值字面量"""
        if isinstance(lit, Literal):
            val = lit.value
            dt = str(lit.data_type.value).upper() if hasattr(lit.data_type, 'value') else str(lit.data_type).upper() if lit.data_type else ""

            if dt == "STRING":
                return None
            if dt == "BOOL":
                if isinstance(val, str):
                    return val.upper() in ("TRUE", "1")
                return bool(val)
            elif dt in ("INT", "DINT", "UINT", "SINT", "USINT", "UDINT", "BYTE", "WORD", "DWORD"):
                try:
                    return int(val)
                except (ValueError, TypeError):
                    return None
            elif dt in ("REAL", "LREAL"):
                encoded, err = encode_float32(val)
                if err:
                    line = getattr(lit, 'line', 0)
                    line_str = f"第 {line} 行: " if line else ""
                    self.errors.append(f"{line_str}浮点数字面量 '{val}': {err}")
                    return None
                return float(val)
            elif dt in ("TIME", "DATE", "DT", "TOD"):
                return None
            else:
                if isinstance(val, bool):
                    return val
                if isinstance(val, str):
                    if val.upper() == "TRUE":
                        return True
                    if val.upper() == "FALSE":
                        return False
                    return None
                try:
                    if "." in str(val):
                        return float(val)
                    return int(val)
                except (ValueError, TypeError):
                    return None
        elif isinstance(lit, bool):
            return lit
        elif isinstance(lit, (int, float)):
            return lit
        return None

    @staticmethod
    def _eval_binary_op(operator, left, right):
        """计算二元运算"""
        op = str(operator).upper()
        # 处理枚举类型
        if hasattr(operator, 'value'):
            op = str(operator.value).upper()
        if hasattr(operator, 'name'):
            op = operator.name.upper()

        if op in ("ADD", "+"):
            return left + right
        elif op in ("SUB", "-"):
            return left - right
        elif op in ("MUL", "*"):
            return left * right
        elif op in ("DIV", "/"):
            if right == 0:
                raise ZeroDivisionError("除数不能为0")
            if isinstance(left, int) and isinstance(right, int):
                return left // right
            return left / right
        elif op in ("MOD", "%"):
            if right == 0:
                raise ZeroDivisionError("模运算除数不能为0")
            return left % right
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
        return None

    @staticmethod
    def _eval_unary_op(operator, operand):
        """计算一元运算"""
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
        """将值转换为整数（用于写入 .code 文件）"""
        if value is None:
            return 0
        if isinstance(value, bool):
            return 1 if value else 0
        if isinstance(value, float):
            return struct.unpack('!I', struct.pack('!f', value))[0]
        try:
            return int(value)
        except (ValueError, TypeError):
            return 0
