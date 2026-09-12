"""
LH 字节码批量验证脚本
=====================
对指定目录下所有 .code 文件执行解释器验证，
生成结构化测试报告，用于论文第五章实验验证。
"""

import os
import sys
import struct
import time
import argparse
from pathlib import Path

# 引入解释器模块
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lh_interpreter import LHInterpreter, parse_header, disassemble, FB_NAMES, OPCODES

MAGIC = b'LMC\x00'


def validate_file_structure(filepath: str) -> dict:
    """验证 .code 文件结构完整性"""
    result = {
        'file': os.path.basename(filepath),
        'size': 0,
        'structure_ok': False,
        'header': None,
        'errors': []
    }
    try:
        with open(filepath, 'rb') as f:
            data = f.read()
        result['size'] = len(data)

        # 1. 文件头完整性
        if len(data) < 32:
            result['errors'].append(f'文件头不完整: 仅 {len(data)} 字节')
            return result

        hdr = parse_header(data)
        result['header'] = hdr

        # 2. 魔数校验
        if hdr.magic != MAGIC:
            result['errors'].append(f'魔数错误: {hdr.magic.hex()}')
            return result

        # 3. 版本号校验
        if hdr.version != 0x0001:
            result['errors'].append(f'版本号异常: 0x{hdr.version:04X}')

        # 4. 代码段范围校验
        if hdr.code_offset + hdr.code_size > len(data):
            result['errors'].append('代码段超出文件范围')
        elif hdr.code_size == 0:
            result['errors'].append('代码段大小为0')

        # 5. 数据段范围校验
        if hdr.data_size > 0:
            if hdr.data_offset + hdr.data_size > len(data):
                result['errors'].append('数据段超出文件范围')

        # 6. 入口点校验
        if hdr.entry_point >= hdr.code_size:
            result['errors'].append(
                f'入口点 0x{hdr.entry_point:04X} 超出代码段 ({hdr.code_size} B)')

        # 7. 内存需求校验
        if hdr.total_mem == 0:
            result['errors'].append('total_mem 为0')

        if not result['errors']:
            result['structure_ok'] = True

    except Exception as e:
        result['errors'].append(str(e))

    return result


def analyze_instructions(filepath: str) -> dict:
    """分析指令集使用情况"""
    info = {
        'instr_count': 0,
        'opcode_stats': {},
        'fb_calls': [],
        'has_halt': False,
        'has_branch': False,
        'unique_opcodes': 0,
    }
    try:
        with open(filepath, 'rb') as f:
            data = f.read()
        hdr = parse_header(data)
        code = data[hdr.code_offset: hdr.code_offset + hdr.code_size]
        instructions = disassemble(code)

        info['instr_count'] = len(instructions)

        for instr in instructions:
            op = instr.opcode
            mne = instr.mnemonic
            info['opcode_stats'][mne] = info['opcode_stats'].get(mne, 0) + 1

            if op == 0xFF:
                info['has_halt'] = True
            if op in (0x40, 0x41, 0x42):
                info['has_branch'] = True
            if op == 0x50 and instr.operands:
                type_id = instr.operands[0]
                fb_name = FB_NAMES.get(type_id, f'FB_{type_id}')
                info['fb_calls'].append({'type_id': type_id, 'name': fb_name})

        info['unique_opcodes'] = len(info['opcode_stats'])

    except Exception as e:
        info['error'] = str(e)

    return info


def run_execution_test(filepath: str) -> dict:
    """运行执行测试"""
    result = {
        'success': False,
        'halt_reason': '',
        'steps': 0,
        'fb_call_count': 0,
        'stack_depth_final': 0,
        'elapsed_ms': 0,
        'error': None
    }
    try:
        vm = LHInterpreter(verbose=False, trace=False)
        vm.load(filepath)
        t0 = time.perf_counter()
        ok = vm.execute()
        elapsed = (time.perf_counter() - t0) * 1000

        result['success'] = ok
        result['halt_reason'] = vm.halt_reason
        result['steps'] = vm.step_count
        result['fb_call_count'] = len(vm.fb_call_log)
        result['stack_depth_final'] = len(vm.stack)
        result['elapsed_ms'] = round(elapsed, 3)

    except Exception as e:
        result['error'] = str(e)

    return result


def print_report(all_results: list, output_file: str = None):
    lines = []
    sep = '=' * 72

    lines.append(sep)
    lines.append('  LH 字节码解释器批量验证报告')
    lines.append(sep)
    lines.append('')

    pass_count = sum(1 for r in all_results if r['exec']['success'])
    struct_ok  = sum(1 for r in all_results if r['struct']['structure_ok'])
    total      = len(all_results)

    lines.append(f'  文件总数:     {total}')
    lines.append(f'  结构验证通过: {struct_ok} / {total}')
    lines.append(f'  执行测试通过: {pass_count} / {total}')
    lines.append('')

    for r in all_results:
        fname   = r['struct']['file']
        struct_ = r['struct']
        instr_  = r['instr']
        exec_   = r['exec']

        lines.append('─' * 72)
        lines.append(f'  文件: {fname}')
        lines.append(f'  大小: {struct_["size"]} 字节')

        # 文件头信息
        if struct_['header']:
            hdr = struct_['header']
            lines.append(f'  文件头:  版本=0x{hdr.version:04X}  '
                         f'代码段={hdr.code_size}B  '
                         f'数据段={hdr.data_size}B  '
                         f'总内存={hdr.total_mem}字')

        # 结构验证
        if struct_['structure_ok']:
            lines.append('  结构验证: ✓ 通过')
        else:
            lines.append('  结构验证: ✗ 失败')
            for err in struct_['errors']:
                lines.append(f'            - {err}')

        # 指令统计
        if 'error' not in instr_:
            lines.append(f'  指令分析: 共 {instr_["instr_count"]} 条，'
                         f'涉及 {instr_["unique_opcodes"]} 种操作码，'
                         f'功能块调用 {len(instr_["fb_calls"])} 次')
            if instr_['fb_calls']:
                fb_set = {}
                for fb in instr_['fb_calls']:
                    fb_set[fb['name']] = fb_set.get(fb['name'], 0) + 1
                fb_str = ', '.join(f'{k}(×{v})' for k, v in fb_set.items())
                lines.append(f'  功能块:   {fb_str}')
            # 操作码分布
            top_ops = sorted(instr_['opcode_stats'].items(),
                             key=lambda x: -x[1])[:6]
            ops_str = '  '.join(f'{m}={c}' for m, c in top_ops)
            lines.append(f'  高频指令: {ops_str}')
        else:
            lines.append(f'  指令分析: 错误 - {instr_["error"]}')

        # 执行结果
        if exec_['error']:
            lines.append(f'  执行测试: ✗ 异常 - {exec_["error"]}')
        elif exec_['success']:
            lines.append(f'  执行测试: ✓ 通过  '
                         f'步数={exec_["steps"]}  '
                         f'FB调用={exec_["fb_call_count"]}  '
                         f'耗时={exec_["elapsed_ms"]}ms')
        else:
            lines.append(f'  执行测试: ✗ 失败  原因={exec_["halt_reason"]}')

        lines.append('')

    lines.append(sep)
    lines.append(f'  汇总: 结构通过率 {struct_ok}/{total}  执行通过率 {pass_count}/{total}')
    lines.append(sep)

    report_str = '\n'.join(lines)
    print(report_str)

    if output_file:
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(report_str)
        print(f'\n报告已保存: {output_file}')


def main():
    parser = argparse.ArgumentParser(
        description='LH .code 文件批量验证工具'
    )
    parser.add_argument('path', help='.code 文件或包含 .code 文件的目录')
    parser.add_argument('-o', '--output', default=None,
                        help='报告输出文件路径（可选）')
    args = parser.parse_args()

    # 收集文件列表
    target = Path(args.path)
    if target.is_file():
        files = [str(target)]
    elif target.is_dir():
        files = sorted(str(p) for p in target.glob('*.code'))
    else:
        print(f'路径不存在: {args.path}', file=sys.stderr)
        sys.exit(1)

    if not files:
        print('未找到任何 .code 文件', file=sys.stderr)
        sys.exit(1)

    print(f'\n发现 {len(files)} 个 .code 文件，开始验证...\n')

    all_results = []
    for fp in files:
        print(f'  验证: {os.path.basename(fp)} ...', end=' ', flush=True)
        struct_r = validate_file_structure(fp)
        instr_r  = analyze_instructions(fp)
        exec_r   = run_execution_test(fp)
        all_results.append({
            'struct': struct_r,
            'instr':  instr_r,
            'exec':   exec_r
        })
        status = '✓' if exec_r['success'] else '✗'
        print(status)

    print()
    print_report(all_results, output_file=args.output)


if __name__ == '__main__':
    main()
