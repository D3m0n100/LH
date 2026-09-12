#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lmc - LH Compiler 命令行工具

用法:
    lmc program.lh              # 编译单个文件
    lmc program.lh -o out.code  # 指定输出文件
    lmc *.lh -d output/         # 批量编译到指定目录
    lmc program.lh -v           # 显示详细信息
    lmc program.lh -d           # 显示调试信息
"""

import sys
import argparse
from pathlib import Path
import glob

# 添加路径
compile_root = Path(__file__).parent
sys.path.insert(0, str(compile_root / "src"))
venv_site = compile_root / "venv" / "Lib" / "site-packages"
if venv_site.exists():
    sys.path.insert(1, str(venv_site))

from lh_compiler.compiler import LHCompiler
from lh_compiler.function_blocks.registry import FunctionBlockRegistry


def normalize_cli_args(argv=None):
    """规范化命令行参数，使 lmc.py 兼容 Click 风格的子命令结构"""
    if argv is None:
        argv = sys.argv[1:]
    else:
        argv = list(argv)

    subcommands = {"compile", "check", "list-blocks", "categories", "describe", "version"}
    skip_next = False
    for i, arg in enumerate(argv):
        if skip_next:
            skip_next = False
            continue
        if arg in ("-o", "--output", "-d", "--output-dir", "-g", "--grammar", "--describe"):
            skip_next = True
            continue
        if arg in subcommands:
            cmd = argv.pop(i)
            if cmd == "compile":
                pass
            elif cmd == "check":
                argv.insert(0, "--check")
            elif cmd == "list-blocks":
                argv.insert(0, "--list-blocks")
            elif cmd == "categories":
                argv.insert(0, "--categories")
            elif cmd == "describe":
                argv.insert(i, "--describe")
            elif cmd == "version":
                argv.insert(0, "--version")
            break
        elif not arg.startswith("-"):
            break

    return argv


def main(argv=None):
    if argv is None:
        argv = sys.argv[1:]
    argv = normalize_cli_args(argv)

    parser = argparse.ArgumentParser(
        prog='lmc',
        description='LH编译器 - 将LH源代码编译为.code字节码',
        epilog='示例: lmc program.lh -o output.code -v'
    )
    
    parser.add_argument(
        'input',
        nargs='*',
        help='输入的.lh源文件（支持通配符）'
    )
    
    parser.add_argument(
        '-o', '--output',
        help='输出文件路径（单文件模式）'
    )
    
    parser.add_argument(
        '-d', '--output-dir',
        help='输出目录（批量编译模式）'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='显示详细编译信息'
    )
    
    parser.add_argument(
        '--debug',
        action='store_true',
        help='显示调试信息（包括AST和指令）'
    )
    
    parser.add_argument(
        '-g', '--grammar',
        help='ANTLR语法文件目录'
    )
    
    parser.add_argument(
        '--check',
        action='store_true',
        help='仅检查语法和语义，不生成目标代码'
    )

    parser.add_argument(
        '--list-blocks',
        action='store_true',
        help='列出所有支持的功能块'
    )

    parser.add_argument(
        '--categories',
        action='store_true',
        help='列出所有功能块类别'
    )

    parser.add_argument(
        '--describe',
        metavar='BLOCK',
        help='显示指定功能块的详细信息'
    )

    parser.add_argument(
        '--version',
        action='version',
        version='lmc 1.0.0'
    )
    
    args = parser.parse_args(argv)

    # 处理功能块注册表查询
    if args.list_blocks or args.categories or args.describe:
        reg = FunctionBlockRegistry()
        reg.load_defaults()

        if args.categories:
            print("功能块类别列表:")
            for cat in sorted(reg.list_categories()):
                blocks = reg.get_category(cat)
                print(f"  - {cat} ({len(blocks)} 个功能块)")
            return 0

        if args.describe:
            block = reg.get(args.describe)
            if not block:
                print(f"错误: 未找到功能块 '{args.describe}'")
                return 1
            print(f"功能块: {block.name} (Type ID: {block.type_id})")
            status_info = f"支持 ({block.status})" if block.is_supported else f"未完善 ({block.status}) - {block.incomplete_reason}"
            print(f"状态: {status_info}")
            print(f"类别: {block.category or '未分类'}")
            print(f"描述: {block.description or '无描述'}")
            if block.parameters:
                print("参数列表:")
                for p in block.parameters:
                    def_str = f" (默认: {p.default_value})" if p.default_value is not None else ""
                    print(f"  - {p.name}: {p.data_type}{def_str}")
            return 0

        if args.list_blocks:
            blocks = reg.list_all()
            print(f"所有功能块 (共 {len(blocks)} 个):")
            for b in sorted(blocks, key=lambda x: x.name):
                status_str = f" [{b.status.upper()}]" if b.is_incomplete else ""
                print(f"  {b.name:<24} [Type {b.type_id:<5}] {b.category or ''}{status_str}")
            return 0

    # 展开通配符
    input_files = []
    for pattern in args.input:
        if '*' in pattern or '?' in pattern:
            input_files.extend(glob.glob(pattern))
        else:
            input_files.append(pattern)
    
    if not input_files:
        print("错误: 未指定输入的 .lh 源文件，使用 --help 查看用法")
        return 1
    
    # 创建编译器
    try:
        compiler = LHCompiler(
            verbose=args.verbose,
            debug=args.debug,
            grammar_path=args.grammar
        )
    except Exception as e:
        print(f"错误: 无法初始化编译器: {e}")
        return 1
    
    # 单文件模式
    if len(input_files) == 1 and not args.output_dir:
        source_file = input_files[0]
        output_file = args.output
        
        if args.check:
            try:
                with open(source_file, 'r', encoding='utf-8') as f:
                    src = f.read()
            except Exception as e:
                print(f"错误: 无法读取源文件: {e}")
                return 1
            result = compiler.compile_string(src, None, source_file)
            if result.success:
                print(f"✓ 语法与语义检查通过: {source_file}")
                return 0
            else:
                print(f"✗ 检查失败: {source_file}")
                for error in result.errors:
                    print(f"  {error}")
                return 1

        result = compiler.compile_file(source_file, output_file)
        
        if result.success:
            if not args.verbose:
                print(f"✓ 编译成功: {result.output_file}")
                
            return 0
        else:
            print(f"✗ 编译失败")
            for error in result.errors:
                print(f"  {error}")
            return 1
    
    # 批量编译模式
    else:
        if args.output:
            print("警告: 批量编译模式下忽略 -o 参数")
        
        if args.check:
            fail_count = 0
            for sf in input_files:
                try:
                    with open(sf, 'r', encoding='utf-8') as f:
                        src = f.read()
                except Exception as e:
                    print(f"错误: 无法读取源文件 {sf}: {e}")
                    fail_count += 1
                    continue
                r = compiler.compile_string(src, None, sf)
                if not r.success:
                    fail_count += 1
                    print(f"✗ 检查失败: {sf}")
                    for error in r.errors:
                        print(f"  {error}")
                else:
                    print(f"✓ 检查通过: {sf}")
            return 0 if fail_count == 0 else 1

        output_dir = args.output_dir
        
        print(f"批量编译 {len(input_files)} 个文件...")
        
        results = compiler.batch_compile(input_files, output_dir)
        
        success_count = sum(1 for r in results if r.success)
        fail_count = len(results) - success_count
        
        print(f"\n总计: {len(results)} 个文件")
        print(f"成功: {success_count} 个")
        print(f"失败: {fail_count} 个")
        
        if fail_count > 0:
            print("\n失败的文件:")
            for i, result in enumerate(results):
                if not result.success:
                    print(f"  {input_files[i]}")
                    for error in result.errors:
                        print(f"    - {error}")
        
        return 0 if fail_count == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
    
