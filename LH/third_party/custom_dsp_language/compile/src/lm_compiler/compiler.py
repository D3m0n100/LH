# -*- coding: utf-8 -*-
"""
LM 编译器主程序
整合词法分析、语法分析、AST构建、代码生成的完整编译流程
"""

import sys
import os
from pathlib import Path
from dataclasses import dataclass
from datetime import datetime
from typing import List, Optional

# 添加路径以支持导入
current_dir = Path(__file__).parent
src_dir = current_dir.parent.parent
if str(src_dir) not in sys.path:
    sys.path.insert(0, str(src_dir))

try:
    from antlr4 import InputStream, CommonTokenStream
    from grammar.LMLexer import LMLexer
    from grammar.LMParser import LMParser
except ImportError as e:
    print(f"错误: 无法导入ANTLR解析器: {e}")
    print("请确保 grammar 目录存在且包含生成的解析器文件")
    sys.exit(1)

from lm_compiler.frontend.ast_builder import ASTBuilder
from lm_compiler.frontend.ast_nodes import Program
from lm_compiler.function_blocks.registry import FunctionBlockRegistry
from lm_compiler.backend.codegen import CodeGenerator
from lm_compiler.backend.emitter import CodeEmitter
from lm_compiler import __version__ as compiler_version


@dataclass
class CompileResult:
    """编译结果"""
    success: bool
    ast: Optional[Program] = None
    instructions: List = None
    errors: List[str] = None
    output_file: Optional[str] = None
    
    def __post_init__(self):
        if self.instructions is None:
            self.instructions = []
        if self.errors is None:
            self.errors = []


class LMCompiler:
    """
    LM编译器
    
    功能：
    - 读取 .lm 源文件
    - ANTLR词法/语法分析
    - 构建AST
    - 生成 .code 字节码
    
    用法:
        compiler = LMCompiler()
        result = compiler.compile_file("program.lm", "output.code")
        if result.success:
            print("编译成功!")
    """
    
    def __init__(self, verbose=False, debug=False, grammar_path=None):
        """
        初始化编译器
        
        Args:
            verbose: 是否显示详细信息
            debug: 是否显示调试信息
            grammar_path: ANTLR语法文件路径（可选）
        """
        self.verbose = verbose
        self.debug = debug
        
        # 初始化功能块注册表
        self.registry = FunctionBlockRegistry()
        self.registry.load_defaults()
        
        if self.verbose:
            print(f"✓ 已加载 {len(self.registry)} 个功能块定义")
        
        # 初始化代码生成器
        self.code_generator = CodeGenerator(self.registry)
        
        # 初始化代码输出器
        self.emitter = CodeEmitter(add_comments=False)
    
    def compile_file(self, source_path: str, output_path: str = None) -> CompileResult:
        """
        编译LM源文件
        
        Args:
            source_path: 源文件路径 (.lm)
            output_path: 输出文件路径 (.code), 如果为None则自动生成
            
        Returns:
            CompileResult: 编译结果
        """
        source_path = Path(source_path)
        
        # 检查源文件
        if not source_path.exists():
            return CompileResult(
                success=False,
                errors=[f"源文件不存在: {source_path}"]
            )
        
        # 确定输出路径
        if output_path is None:
            output_path = source_path.with_suffix('.code')
        else:
            output_path = Path(output_path)
        
        if self.verbose:
            print(f"\n{'='*60}")
            print(f"编译: {source_path.name}")
            print(f"输出: {output_path.name}")
            print(f"{'='*60}")
        
        # 读取源代码
        try:
            with open(source_path, 'r', encoding='utf-8') as f:
                source_code = f.read()
        except Exception as e:
            return CompileResult(
                success=False,
                errors=[f"无法读取源文件: {e}"]
            )
        
        # 编译
        return self.compile_string(source_code, str(output_path))
    
    def compile_string(self, source_code: str, output_path: str = None) -> CompileResult:
        """
        编译字符串形式的源代码
        
        Args:
            source_code: LM源代码
            output_path: 输出文件路径 (.code), 如果为None则不写入文件
            
        Returns:
            CompileResult: 编译结果
        """
        errors = []
        
        # 步骤1: 词法分析
        if self.verbose:
            print("\n[1/4] 词法分析...")
        
        try:
            input_stream = InputStream(source_code)
            lexer = LMLexer(input_stream)
            token_stream = CommonTokenStream(lexer)
            
            if self.debug:
                token_stream.fill()
                tokens = token_stream.tokens
                print(f"  生成 {len(tokens)} 个词法单元")
        except Exception as e:
            return CompileResult(
                success=False,
                errors=[f"词法分析失败: {e}"]
            )
        
        # 步骤2: 语法分析
        if self.verbose:
            print("[2/4] 语法分析...")
        
        try:
            parser = LMParser(token_stream)
            parse_tree = parser.program()
            
            if parser.getNumberOfSyntaxErrors() > 0:
                errors.append(f"发现 {parser.getNumberOfSyntaxErrors()} 个语法错误")
        except Exception as e:
            return CompileResult(
                success=False,
                errors=[f"语法分析失败: {e}"]
            )
        
        # 步骤3: 构建AST
        if self.verbose:
            print("[3/4] 构建AST...")
        
        try:
            ast_builder = ASTBuilder()
            ast = ast_builder.visit(parse_tree)
            
            if self.debug:
                from lm_compiler.frontend.ast_nodes import ASTPrinter
                print("\n--- AST 结构 ---")
                printer = ASTPrinter()
                printer.visit(ast)
                print()
        except Exception as e:
            return CompileResult(
                success=False,
                errors=[f"AST构建失败: {e}"]
            )
        
        # 步骤4: 代码生成
        if self.verbose:
            print("[4/4] 生成代码...")
        
        try:
            instructions = self.code_generator.generate(ast)
            
            if self.code_generator.errors:
                errors.extend(self.code_generator.errors)
            
            if self.verbose:
                print(f"  生成 {len(instructions)} 条指令")
                print(f"  内存占用: {self.code_generator.memory.next_address} 个单元")
            
            if self.debug:
                print("\n--- 内存分配表 ---")
                print(self.code_generator.memory.dump())
                print("\n--- 生成的指令 ---")
                self.emitter.print_instructions(instructions)
        except Exception as e:
            return CompileResult(
                success=False,
                ast=ast,
                errors=[f"代码生成失败: {e}"]
            )
        
        # 步骤5: 只有无语义错误时才输出文件
        if output_path and len(errors) == 0:
            try:
                output_path = Path(output_path)
                output_path.parent.mkdir(parents=True, exist_ok=True)

                self.emitter.emit_with_header(
                    instructions,
                    str(output_path),
                    program_name=source_path.stem,
                    metadata={
                        "版本号": compiler_version,
                        "文件名": str(source_path),
                        "日期_时间": datetime.now().strftime("%a %b %d %H:%M:%S %Y"),
                        "说明": "构件码构件数据项",
                    },
                )

                if self.verbose:
                    print(f"\n✓ 编译成功: {output_path}")
                    print(f"  文件大小: {output_path.stat().st_size} 字节")
            except Exception as e:
                errors.append(f"写入输出文件失败: {e}")

        # 返回结果
        return CompileResult(
            success=len(errors) == 0,
            ast=ast,
            instructions=instructions,
            errors=errors,
            output_file=str(output_path) if output_path and len(errors) == 0 else None
        )
    
    def batch_compile(self, source_files: List[str], output_dir: str = None) -> List[CompileResult]:
        """
        批量编译多个文件
        
        Args:
            source_files: 源文件路径列表
            output_dir: 输出目录，如果为None则输出到源文件同目录
            
        Returns:
            List[CompileResult]: 编译结果列表
        """
        if output_dir:
            output_dir = Path(output_dir)
            output_dir.mkdir(parents=True, exist_ok=True)
        
        results = []
        
        for source_file in source_files:
            source_path = Path(source_file)
            
            if output_dir:
                output_path = output_dir / source_path.with_suffix('.code').name
            else:
                output_path = source_path.with_suffix('.code')
            
            result = self.compile_file(str(source_path), str(output_path))
            results.append(result)
        
        return results


def main():
    """命令行入口"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='LM编译器 - 将.lm源文件编译为.code字节码'
    )
    parser.add_argument('input', help='输入的.lm源文件')
    parser.add_argument('-o', '--output', help='输出的.code文件（默认：与源文件同名）')
    parser.add_argument('-v', '--verbose', action='store_true', help='显示详细信息')
    parser.add_argument('-d', '--debug', action='store_true', help='显示调试信息')
    
    args = parser.parse_args()
    
    # 创建编译器
    compiler = LMCompiler(verbose=args.verbose, debug=args.debug)
    
    # 编译
    result = compiler.compile_file(args.input, args.output)
    
    # 显示结果
    if result.success:
        print(f"✓ 编译成功")
        sys.exit(0)
    else:
        print(f"✗ 编译失败")
        for error in result.errors:
            print(f"  {error}")
        sys.exit(1)


if __name__ == '__main__':
    main()
