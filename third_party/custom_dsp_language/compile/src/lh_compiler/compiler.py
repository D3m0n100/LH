# -*- coding: utf-8 -*-
"""
LH 编译器主程序
整合词法分析、语法分析、AST构建、代码生成的完整编译流程
"""

import sys
import os
from pathlib import Path
from dataclasses import dataclass
from typing import List, Optional

# 添加路径以支持导入
current_dir = Path(__file__).parent
src_dir = current_dir.parent.parent

if str(src_dir) not in sys.path:
    sys.path.insert(0, str(src_dir))

try:
    from antlr4 import InputStream, CommonTokenStream
    from antlr4.error.ErrorListener import ErrorListener
    from grammar.LHLexer import LHLexer
    from grammar.LHParser import LHParser
except ImportError as e:
    print(f"错误: 无法导入ANTLR解析器: {e}")
    print("请确保 grammar 目录存在且包含生成的解析器文件")
    sys.exit(1)

from lh_compiler.frontend.ast_builder import ASTBuilder
from lh_compiler.frontend.ast_nodes import Program
from lh_compiler.function_blocks.registry import FunctionBlockRegistry
from lh_compiler.backend.codegen import CodeGenerator
from lh_compiler.backend.emitter import CodeEmitter, CompileSupportEmitter


class CollectingErrorListener(ErrorListener):
    """收集 ANTLR 词法和语法分析错误"""

    def __init__(self, stage_name: str = "解析"):
        super().__init__()
        self.stage_name = stage_name
        self.errors = []

    def syntaxError(self, recognizer, offendingSymbol, line, column, msg, e):
        self.errors.append(f"{self.stage_name}错误 (第 {line} 行, 第 {column} 列): {msg}")


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


class LHCompiler:
    """
    LH编译器

    功能：
    - 读取 .lh 源文件
    - ANTLR词法/语法分析
    - 构建AST
    - 生成 .code 字节码

    用法:
        compiler = LHCompiler()
        result = compiler.compile_file("program.lh", "output.code")
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
        self.support_emitter = CompileSupportEmitter()

    @staticmethod
    def _cleanup_output_on_failure(output_path: Optional[str], source_path: Optional[str] = None) -> List[str]:
        """
        编译失败或异常时，清理本次编译对应的目标可运行文件与旧符号表辅助产物，防止下游误用。
        返回遇到的清理错误列表（空列表表示清理成功或文件本就不存在）。
        """
        cleanup_errors: List[str] = []
        if not output_path:
            return cleanup_errors

        out_path = Path(output_path)

        src_canon = None
        if source_path:
            try:
                src_canon = Path(source_path).resolve()
            except Exception:
                src_canon = Path(os.path.abspath(str(source_path)))

        # 确定需要清理的文件集合（只清理目标产物及以其为基准的相关辅助文件，绝不递归删除目录）
        targets_to_check: List[Path] = [out_path]
        if out_path.suffix.lower() == '.code':
            targets_to_check.append(out_path.with_suffix('.list'))
            targets_to_check.append(out_path.with_suffix('.typ'))
        elif out_path.suffix == '':
            targets_to_check.extend([
                out_path.with_suffix('.code'),
                out_path.with_suffix('.list'),
                out_path.with_suffix('.typ')
            ])

        for target in targets_to_check:
            try:
                target_canon = target.resolve()
            except Exception:
                target_canon = Path(os.path.abspath(str(target)))

            # 安全防护：绝不删除源文件
            if src_canon and target_canon == src_canon:
                continue

            if target.is_dir():
                cleanup_errors.append(f"清理产物失败: 路径 '{target}' 是目录，禁止递归删除")
                continue

            if target.exists():
                try:
                    target.unlink()
                except OSError as e:
                    cleanup_errors.append(f"无法清理残留产物文件 '{target}': {e}")
                except Exception as e:
                    cleanup_errors.append(f"清理产物文件遇到未知错误 '{target}': {e}")

        return cleanup_errors

    def compile_file(self, source_path: str, output_path: str = None) -> CompileResult:
        """
        编译 LH 源文件

        Args:
            source_path: 源文件路径 (.lh)
            output_path: 输出文件路径 (.code), 如果为None则自动生成

        Returns:
            CompileResult: 编译结果
        """
        source_p = Path(source_path)

        # 确定输出路径
        if output_path is None:
            resolved_output = source_p.with_suffix('.code')
        else:
            resolved_output = Path(output_path)

        # 检查源文件与输出路径是否相同或指向同一文件别名
        try:
            is_same_file = source_p.resolve() == resolved_output.resolve()
        except Exception:
            is_same_file = os.path.abspath(str(source_p)).lower() == os.path.abspath(str(resolved_output)).lower()

        if is_same_file:
            return CompileResult(
                success=False,
                errors=[f"输入源文件路径与输出路径不能相同: {source_path}"]
            )

        # 检查源文件
        if not source_p.exists():
            cleanup_errors = self._cleanup_output_on_failure(str(resolved_output), str(source_p))
            return CompileResult(
                success=False,
                errors=[f"源文件不存在: {source_p}"] + cleanup_errors
            )

        if self.verbose:
            print(f"\n{'='*60}")
            print(f"编译: {source_p.name}")
            print(f"输出: {resolved_output.name}")
            print(f"{'='*60}")

        # 读取源代码
        try:
            with open(source_p, 'r', encoding='utf-8') as f:
                source_code = f.read()
        except Exception as e:
            cleanup_errors = self._cleanup_output_on_failure(str(resolved_output), str(source_p))
            return CompileResult(
                success=False,
                errors=[f"无法读取源文件: {e}"] + cleanup_errors
            )

        # 编译
        return self.compile_string(source_code, str(resolved_output), str(source_p))

    def compile_string(self, source_code: str, output_path: str = None, source_path: str = "") -> CompileResult:
        """
        编译字符串形式的源代码

        Args:
            source_code: LH源代码
            output_path: 输出文件路径 (.code), 如果为None则不写入文件
            source_path: 源文件路径，用于生成 LH 附属产物头部

        Returns:
            CompileResult: 编译结果
        """
        errors = []
        ast = None
        instructions = []

        if source_path and output_path:
            try:
                is_same_file = Path(source_path).resolve() == Path(output_path).resolve()
            except Exception:
                is_same_file = os.path.abspath(str(source_path)).lower() == os.path.abspath(str(output_path)).lower()
            if is_same_file:
                return CompileResult(
                    success=False,
                    errors=[f"输入源文件路径与输出路径不能相同: {source_path}"]
                )

        def fail_result(step_errors: List[str], current_ast=None) -> CompileResult:
            target_ast = current_ast if current_ast is not None else ast
            all_errors = list(step_errors)
            cleanup_errs = self._cleanup_output_on_failure(output_path, source_path)
            if cleanup_errs:
                all_errors.extend(cleanup_errs)
            if output_path:
                try:
                    self.support_emitter.emit(
                        source_path=source_path,
                        output_path=str(output_path),
                        program=target_ast,
                        instructions=instructions,
                        code_generator=self.code_generator,
                        errors=all_errors
                    )
                except Exception as exc:
                    all_errors.append(f"无法写入编译失败诊断产物: {exc}")
            return CompileResult(
                success=False,
                ast=target_ast,
                instructions=instructions,
                errors=all_errors,
                output_file=None
            )

        # 步骤1: 词法分析
        if self.verbose:
            print("\n[1/4] 词法分析...")

        try:
            input_stream = InputStream(source_code)
            lexer = LHLexer(input_stream)
            lexer_listener = CollectingErrorListener("词法分析")
            lexer.removeErrorListeners()
            lexer.addErrorListener(lexer_listener)

            token_stream = CommonTokenStream(lexer)

            if self.debug:
                token_stream.fill()
                tokens = token_stream.tokens
                print(f"  生成 {len(tokens)} 个词法单元")
        except Exception as e:
            return fail_result([f"词法分析失败: {e}"])

        # 步骤2: 语法分析
        if self.verbose:
            print("[2/4] 语法分析...")

        try:
            parser = LHParser(token_stream)
            parser_listener = CollectingErrorListener("语法分析")
            parser.removeErrorListeners()
            parser.addErrorListener(parser_listener)

            parse_tree = parser.program()

            if lexer_listener.errors:
                errors.extend(lexer_listener.errors)
            if parser_listener.errors:
                errors.extend(parser_listener.errors)
            if parser.getNumberOfSyntaxErrors() > 0 and not parser_listener.errors:
                errors.append(f"发现 {parser.getNumberOfSyntaxErrors()} 个语法错误")

            if errors:
                return fail_result(errors)
        except Exception as e:
            return fail_result([f"语法分析失败: {e}"])

        # 步骤3: 构建AST
        if self.verbose:
            print("[3/4] 构建AST...")

        try:
            ast_builder = ASTBuilder()
            ast = ast_builder.visit(parse_tree)

            if ast is None and not ast_builder.errors:
                ast_builder.errors.append("AST 构建返回空节点")

            if ast_builder.errors:
                errors.extend(ast_builder.errors)
                return fail_result(errors, current_ast=ast)

            if self.debug:
                from lh_compiler.frontend.ast_nodes import ASTPrinter
                print("\n--- AST 结构 ---")
                printer = ASTPrinter()
                printer.visit(ast)
                print()
        except Exception as e:
            return fail_result([f"AST构建失败: {e}"])

        # 步骤4: 代码生成
        if self.verbose:
            print("[4/4] 生成代码...")

        try:
            instructions = self.code_generator.generate(ast)

            if self.code_generator.errors:
                errors.extend(self.code_generator.errors)
                return fail_result(errors, current_ast=ast)

            if self.verbose:
                print(f"  生成 {len(instructions)} 条指令")
                print(f"  内存占用: {self.code_generator.memory.next_address} 个单元")

            if self.debug:
                print("\n--- 内存分配表 ---")
                print(self.code_generator.memory.dump())
                print("\n--- 生成的指令 ---")
                self.emitter.print_instructions(instructions)
        except Exception as e:
            return fail_result([f"代码生成失败: {e}"], current_ast=ast)

        # 步骤5: 只有无语义错误时才输出可下载代码和 LH 附属产物
        if output_path:
            out_p = Path(output_path)
            tmp_code_path = out_p.with_name(f"{out_p.name}.tmp.{os.getpid()}")
            try:
                out_p.parent.mkdir(parents=True, exist_ok=True)

                # 采用临时文件写入 + 原子重命名发布，避免半写或竞争
                self.emitter.emit(instructions, str(tmp_code_path))
                os.replace(str(tmp_code_path), str(out_p))

                self.support_emitter.emit(
                    source_path=source_path,
                    output_path=str(out_p),
                    program=ast,
                    instructions=instructions,
                    code_generator=self.code_generator,
                    errors=[]
                )

                if self.verbose:
                    print(f"\n✓ 编译成功: {out_p}")
                    print(f"  文件大小: {out_p.stat().st_size} 字节")
            except Exception as e:
                # 异常时严格清理临时文件与目标文件，防止损坏或过时产物残留
                if tmp_code_path.exists():
                    try:
                        tmp_code_path.unlink()
                    except Exception:
                        pass
                return fail_result([f"写入输出文件失败: {e}"], current_ast=ast)

        # 返回结果
        return CompileResult(
            success=True,
            ast=ast,
            instructions=instructions,
            errors=[],
            output_file=str(output_path) if output_path else None
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
        description='LH编译器 - 将.lh源文件编译为.code字节码'
    )
    parser.add_argument('input', help='输入的.lh源文件')
    parser.add_argument('-o', '--output', help='输出的.code文件（默认：与源文件同名）')
    parser.add_argument('-v', '--verbose', action='store_true', help='显示详细信息')
    parser.add_argument('-d', '--debug', action='store_true', help='显示调试信息')

    args = parser.parse_args()

    # 创建编译器
    compiler = LHCompiler(verbose=args.verbose, debug=args.debug)

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
