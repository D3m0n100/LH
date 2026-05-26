#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lmc - LM Compiler 命令行工具

用法:
    lmc program.lm              # 编译单个文件
    lmc program.lm -o out.code  # 指定输出文件
    lmc *.lm -d output/         # 批量编译到指定目录
    lmc program.lm -v           # 显示详细信息
    lmc program.lm -d           # 显示调试信息
"""

import sys
import argparse
from pathlib import Path
import glob

# 添加路径
sys.path.insert(0, str(Path(__file__).parent / "src"))

from lm_compiler.compiler import LMCompiler


def main():
    parser = argparse.ArgumentParser(
        prog='lmc',
        description='LM编译器 - 将LM源代码编译为.code字节码',
        epilog='示例: lmc program.lm -o output.code -v'
    )
    
    parser.add_argument(
        'input',
        nargs='*',
        help='输入的.lm源文件（支持通配符）'
    )

    parser.add_argument(
        '--describe-blocks',
        action='store_true',
        help='输出所有功能块的元数据（JSON格式）'
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
        '--version',
        action='version',
        version='lmc 1.0.0'
    )
    
    args = parser.parse_args()

    # --describe-blocks: 输出所有功能块元数据
    if args.describe_blocks:
        import json
        from lm_compiler.function_blocks.registry import FunctionBlockRegistry
        registry = FunctionBlockRegistry()
        registry.load_defaults()
        blocks = []
        for name, meta in sorted(registry.all_blocks().items()):
            params = []
            for p in meta.parameters:
                params.append({
                    "name": p.name,
                    "data_type": p.data_type,
                    "direction": p.direction,
                    "offset": p.offset,
                    "default_value": p.default_value,
                    "description": p.description,
                })
            blocks.append({
                "name": meta.name,
                "type_id": meta.type_id,
                "memory_size": meta.memory_size,
                "category": meta.category,
                "description": meta.description,
                "parameters": params,
            })
        print(json.dumps(blocks, ensure_ascii=False))
        return 0

    if not args.input:
        print("错误: 未指定输入文件")
        return 1

    # 展开通配符
    input_files = []
    for pattern in args.input:
        if '*' in pattern or '?' in pattern:
            input_files.extend(glob.glob(pattern))
        else:
            input_files.append(pattern)
    
    if not input_files:
        print("错误: 未找到匹配的输入文件")
        return 1
    
    # 创建编译器
    try:
        compiler = LMCompiler(
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
