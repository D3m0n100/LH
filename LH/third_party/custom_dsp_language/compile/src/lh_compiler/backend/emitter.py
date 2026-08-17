# -*- coding: utf-8 -*-
"""
.code 文件输出器
将指令列表写入 .code 文件

.code 文件格式:
    - 纯文本文件
    - 每行一条指令
    - 格式: 类型ID 类型ID 内存地址 参数1 参数2 ...
    - 类型ID 出现两次是该格式的固定规则

用法:
    emitter = CodeEmitter()
    emitter.emit(instructions, "output.code")
    
    # 或者获取字符串内容
    content = emitter.to_string(instructions)
"""

import os
from datetime import datetime
from typing import Any, Dict, List, Optional

# 同级导入
try:
    from lh_compiler.backend.codegen import Instruction
except ImportError:
    from .codegen import Instruction


class CodeEmitter:
    """
    .code 文件输出器
    
    负责将 CodeGenerator 生成的指令列表写入最终的 .code 文件。
    """

    def __init__(self, add_comments: bool = False):
        """
        Args:
            add_comments: 是否在 .code 文件中添加注释（调试用）
        """
        self.add_comments = add_comments

    def to_string(self, instructions: List[Instruction]) -> str:
        """
        将指令列表转换为 .code 文件内容字符串
        
        Args:
            instructions: 指令列表
        
        Returns:
            .code 文件的文本内容
        """
        lines = []
        for inst in instructions:
            line = inst.to_code_line()
            if self.add_comments and inst.comment:
                line += f"  // {inst.comment}"
            lines.append(line)
        return "\n".join(lines)

    def emit(self, instructions: List[Instruction], output_path: str):
        """
        将指令列表写入 .code 文件
        
        Args:
            instructions: 指令列表
            output_path: 输出文件路径
        """
        # 确保输出目录存在
        output_dir = os.path.dirname(output_path)
        if output_dir and not os.path.exists(output_dir):
            os.makedirs(output_dir, exist_ok=True)

        content = self.to_string(instructions)

        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(content)
            f.write("\n")  # 文件末尾换行

    def emit_with_header(
        self,
        instructions: List[Instruction],
        output_path: str,
        program_name: str = "",
        metadata: Optional[dict] = None
    ):
        """
        写入带头部信息的 .code 文件（调试模式）
        
        生成的文件会在开头包含注释形式的元数据,
        实际运行时应使用 emit() 生成干净的文件。
        """
        lines = []

        if self.add_comments:
            lines.append(f"// Program: {program_name}")
            if metadata:
                for key, val in metadata.items():
                    lines.append(f"// {key}: {val}")
            lines.append(f"// Instructions: {len(instructions)}")
            lines.append("//")

        for inst in instructions:
            line = inst.to_code_line()
            if self.add_comments and inst.comment:
                line += f"  // {inst.comment}"
            lines.append(line)

        content = "\n".join(lines)

        output_dir = os.path.dirname(output_path)
        if output_dir and not os.path.exists(output_dir):
            os.makedirs(output_dir, exist_ok=True)

        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(content)
            f.write("\n")

    @staticmethod
    def print_instructions(instructions: List[Instruction]):
        """在终端打印指令列表（调试用）"""
        print("=== .code 指令 ===")
        for i, inst in enumerate(instructions):
            comment = f"  // {inst.comment}" if inst.comment else ""
            print(f"  [{i:3d}] {inst.to_code_line()}{comment}")
        print(f"=== 共 {len(instructions)} 条指令 ===")


class CompileSupportEmitter:
    """
    Emits LH sidecar files that mirror the useful parts of the legacy compile
    output while staying based on the current LH compiler data model.
    """

    def emit(
        self,
        source_path: str,
        output_path: str,
        program: Any,
        instructions: List[Instruction],
        code_generator: Any,
        errors: Optional[List[str]] = None
    ) -> Dict[str, str]:
        base_path, _ = os.path.splitext(output_path)
        paths = {
            "list": base_path + ".list",
            "typ": base_path + ".typ",
            "rep": base_path + ".rep",
        }

        output_dir = os.path.dirname(output_path)
        if output_dir and not os.path.exists(output_dir):
            os.makedirs(output_dir, exist_ok=True)

        self._write(paths["list"], self._build_list(source_path, program, code_generator))
        self._write(paths["typ"], self._build_typ(source_path, code_generator))
        self._write(paths["rep"], self._build_rep(source_path, output_path, instructions, code_generator, errors or []))
        return paths

    def _header(self, source_path: str, columns: str) -> List[str]:
        return [
            "# LH compiler generated file",
            f"# Source\t{source_path}",
            f"# GeneratedAt\t{datetime.now().isoformat(timespec='seconds')}",
            f"# Columns\t{columns}",
        ]

    def _build_list(self, source_path: str, program: Any, code_generator: Any) -> str:
        lines = self._header(source_path, "instance\tparameter\tdata_type\taddress\tdefault_value\tcurrent_value")
        memory = getattr(code_generator, "memory", None)
        registry = getattr(code_generator, "registry", None)
        calls = self._function_calls_by_instance(program)
        blocks = memory.get_all_blocks() if memory else []

        for block in blocks:
            meta = registry.get(block.block_type) if registry else None
            if not meta:
                continue
            call_values = calls.get(block.name, {})
            param_address = int(block.address)
            for param in getattr(meta, "parameters", []):
                value = call_values.get(param.name.upper(), param.default_value)
                lines.append("\t".join([
                    block.name,
                    param.name,
                    str(param.data_type),
                    str(param_address),
                    self._format_value(param.default_value),
                    self._format_value(value),
                ]))
                param_address += self._type_size(param.data_type)

        return "\n".join(lines) + "\n"

    def _build_typ(self, source_path: str, code_generator: Any) -> str:
        lines = self._header(source_path, "address\tkind\tsymbol\tdata_type\tsize\tparameter")
        memory = getattr(code_generator, "memory", None)
        symbols = getattr(code_generator, "_symbols", {})
        registry = getattr(code_generator, "registry", None)
        blocks = memory.get_all_blocks() if memory else []

        for block in blocks:
            symbol = symbols.get(block.name, {})
            meta = symbol.get("meta") or (registry.get(block.block_type) if registry else None)
            lines.append("\t".join([
                str(block.address),
                "function_block",
                block.name,
                block.block_type,
                str(block.size),
                "",
            ]))
            if meta:
                param_address = int(block.address)
                for param in getattr(meta, "parameters", []):
                    lines.append("\t".join([
                        str(param_address),
                        "parameter",
                        block.name,
                        str(param.data_type),
                        str(self._type_size(param.data_type)),
                        param.name,
                    ]))
                    param_address += self._type_size(param.data_type)

        variable_addresses = getattr(memory, "_variable_addresses", {}) if memory else {}
        for name in sorted(variable_addresses.keys()):
            symbol = symbols.get(name, {})
            data_type = str(symbol.get("type", "UNKNOWN"))
            lines.append("\t".join([
                str(variable_addresses[name]),
                "variable",
                name,
                data_type,
                str(self._type_size(data_type)),
                "",
            ]))

        return "\n".join(lines) + "\n"

    def _build_rep(
        self,
        source_path: str,
        output_path: str,
        instructions: List[Instruction],
        code_generator: Any,
        errors: List[str]
    ) -> str:
        memory = getattr(code_generator, "memory", None)
        lines = self._header(source_path, "key\tvalue")
        lines.extend([
            f"output\t{output_path}",
            f"instruction_count\t{len(instructions)}",
            f"memory_units\t{memory.total_allocated if memory else 0}",
            f"status\t{'failed' if errors else 'success'}",
        ])
        if errors:
            for error in errors:
                lines.append(f"error\t{error}")
        else:
            lines.append("error_count\t0")
        return "\n".join(lines) + "\n"

    def _function_calls_by_instance(self, program: Any) -> Dict[str, Dict[str, Any]]:
        calls: Dict[str, Dict[str, Any]] = {}

        def visit_statement(stmt: Any):
            if stmt is None:
                return
            if hasattr(stmt, "instance_name") and hasattr(stmt, "parameters"):
                instance_name = self._identifier_text(stmt.instance_name)
                values: Dict[str, Any] = {}
                for param in getattr(stmt, "parameters", []):
                    values[str(param.name).upper()] = getattr(param, "value", None)
                calls[instance_name] = values
                return
            for attr in ("then_statements", "else_statements", "elseif_clauses", "body", "statements", "clauses"):
                child = getattr(stmt, attr, None)
                if isinstance(child, list):
                    for item in child:
                        visit_statement(item)
                elif child is not None:
                    visit_statement(child)

        for statement in getattr(program, "statements", []):
            visit_statement(statement)
        return calls

    def _identifier_text(self, value: Any) -> str:
        if hasattr(value, "name"):
            return str(value.name)
        return str(value)

    def _format_value(self, value: Any) -> str:
        if value is None:
            return ""
        if hasattr(value, "value") and value.__class__.__name__ == "Literal":
            return str(value.value)
        if hasattr(value, "name") and value.__class__.__name__ == "Identifier":
            return str(value.name)
        if hasattr(value, "member") and hasattr(value, "base"):
            return f"{self._format_value(value.base)}.{value.member}"
        if hasattr(value, "operator") and hasattr(value, "left") and hasattr(value, "right"):
            operator = getattr(value.operator, "value", str(value.operator))
            return f"({self._format_value(value.left)} {operator} {self._format_value(value.right)})"
        if hasattr(value, "operator") and hasattr(value, "operand"):
            operator = getattr(value.operator, "value", str(value.operator))
            return f"{operator}{self._format_value(value.operand)}"
        return str(value)

    def _type_size(self, data_type: str) -> int:
        sizes = {
            "BOOL": 1,
            "BYTE": 1,
            "INT": 2,
            "UINT": 2,
            "WORD": 2,
            "DINT": 4,
            "UDINT": 4,
            "DWORD": 4,
            "REAL": 4,
            "LREAL": 8,
            "STRING": 256,
        }
        return sizes.get(str(data_type).upper(), 2)

    def _write(self, path: str, content: str):
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
