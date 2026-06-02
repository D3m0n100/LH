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
from typing import List, Optional

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
