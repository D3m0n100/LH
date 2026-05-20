"""
LM 字节码解释器 v2  —— 文本格式 FB 实例化列表
===============================================
解析并模拟执行 LM 编译器实际输出的 .code 文件。

实际文件格式（文本，每行一条 FB 实例记录）：
  type_id  type_id  base_addr  [init_val]

字段说明：
  col0  type_id    功能块类型编号（企业私有编号体系，0xA000+ 段）
  col1  type_id    同 col0（冗余校验字段）
  col2  base_addr  该 FB 实例在运行时内存中的起始字节地址
  col3  init_val   可选初始化参数，整数或 IEEE 754 float32 编码值

第一行为模块声明头：type_id=40961(0xA001)，base_addr=0
"""

import struct
import sys
import os
import argparse
from collections import defaultdict, OrderedDict
from typing import List, Optional, Dict, Tuple

# ─────────────────────────── 功能块类型注册表 ───────────────────────────
# 基于对话记录和论文第3.5节整理的企业私有编号段（0xA000+）

FB_REGISTRY: Dict[int, dict] = {
    # ── 模块/系统管理 ──────────────────────────────────────────────────
    40961: {'name': 'MODULE_HEADER',  'domain': '系统管理', 'mem_size': 0,
            'desc': '模块声明头'},
    41000: {'name': 'systemInit',     'domain': '系统管理', 'mem_size': 16,
            'desc': '系统初始化'},

    # ── 基础数据类型 ───────────────────────────────────────────────────
    41029: {'name': 'intConst',       'domain': '算术运算', 'mem_size': 32,
            'desc': '整型常量'},
    41030: {'name': 'floatConst',     'domain': '算术运算', 'mem_size': 16,
            'desc': '浮点常量'},
    41031: {'name': 'boolConst',      'domain': '逻辑控制', 'mem_size': 8,
            'desc': '布尔常量'},

    # ── I/O 驱动域 ─────────────────────────────────────────────────────
    41200: {'name': 'adcRead',        'domain': 'I/O驱动',  'mem_size': 32,
            'desc': 'ADC 模拟量输入'},
    41202: {'name': 'dacWrite',       'domain': 'I/O驱动',  'mem_size': 24,
            'desc': 'DAC 模拟量输出'},
    41203: {'name': 'digitalIn',      'domain': 'I/O驱动',  'mem_size': 16,
            'desc': '数字量输入'},
    41207: {'name': 'pwmOutput',      'domain': 'I/O驱动',  'mem_size': 24,
            'desc': 'PWM 输出'},
    41208: {'name': 'digitalOut',     'domain': 'I/O驱动',  'mem_size': 24,
            'desc': '数字量输出'},

    # ── 比较运算域 ─────────────────────────────────────────────────────
    41300: {'name': '_GT',            'domain': '比较运算', 'mem_size': 16,
            'desc': '大于比较'},
    41301: {'name': '_LT',            'domain': '比较运算', 'mem_size': 16,
            'desc': '小于比较'},
    41302: {'name': '_EQ',            'domain': '比较运算', 'mem_size': 16,
            'desc': '等于比较'},
    41304: {'name': '_NE',            'domain': '比较运算', 'mem_size': 16,
            'desc': '不等于比较'},

    # ── 算术运算域 ─────────────────────────────────────────────────────
    41400: {'name': 'intAdd',         'domain': '算术运算', 'mem_size': 16,
            'desc': '整型加法'},
    41401: {'name': 'intSub',         'domain': '算术运算', 'mem_size': 16,
            'desc': '整型减法'},
    41402: {'name': 'intMul',         'domain': '算术运算', 'mem_size': 16,
            'desc': '整型乘法'},

    # ── 逻辑控制域 ─────────────────────────────────────────────────────
    41600: {'name': 'ifCondition',    'domain': '逻辑控制', 'mem_size': 8,
            'desc': '条件选择'},

    # ── 定时计数域 ─────────────────────────────────────────────────────
    41700: {'name': 'timer100ms',     'domain': '定时计数', 'mem_size': 32,
            'desc': '100ms 定时器'},
    41900: {'name': 'counter',        'domain': '定时计数', 'mem_size': 16,
            'desc': '计数器'},

    # ── PID 控制域 ─────────────────────────────────────────────────────
    42100: {'name': 'pidInit',        'domain': 'PID控制',  'mem_size': 48,
            'desc': 'PID 初始化'},
    42101: {'name': 'pidController',  'domain': 'PID控制',  'mem_size': 64,
            'desc': 'PID 控制器'},
    42102: {'name': 'pidOutput',      'domain': 'PID控制',  'mem_size': 16,
            'desc': 'PID 输出'},

    # ── data_process 专用域 ────────────────────────────────────────────
    42200: {'name': 'intBuilder',     'domain': '数据处理', 'mem_size': 16,
            'desc': '整型构造'},
    42201: {'name': 'intDirect',      'domain': '数据处理', 'mem_size': 8,
            'desc': '整型直通'},
    42202: {'name': 'floatBuilder',   'domain': '数据处理', 'mem_size': 16,
            'desc': '浮点构造'},
    42203: {'name': 'floatConst25_5', 'domain': '数据处理', 'mem_size': 8,
            'desc': '浮点常量25.5'},
    42204: {'name': 'intCompare',     'domain': '数据处理', 'mem_size': 16,
            'desc': '整型比较'},
    42205: {'name': 'floatCompare',   'domain': '数据处理', 'mem_size': 16,
            'desc': '浮点比较'},
    42206: {'name': 'intMaxSel',      'domain': '数据处理', 'mem_size': 16,
            'desc': '整型最大值选择'},
    42207: {'name': 'intMinSel',      'domain': '数据处理', 'mem_size': 16,
            'desc': '整型最小值选择'},
    42208: {'name': 'floatMaxSel',    'domain': '数据处理', 'mem_size': 16,
            'desc': '浮点最大值选择'},
    42209: {'name': 'floatMinSel',    'domain': '数据处理', 'mem_size': 16,
            'desc': '浮点最小值选择'},
    42210: {'name': 'intLimiter',     'domain': '数据处理', 'mem_size': 24,
            'desc': '整型限幅'},
    42211: {'name': 'floatLimiter',   'domain': '数据处理', 'mem_size': 24,
            'desc': '浮点限幅'},
    42212: {'name': 'floatVariLim',   'domain': '数据处理', 'mem_size': 24,
            'desc': '浮点可变限幅'},
    42213: {'name': 'intBuffer',      'domain': '数据处理', 'mem_size': 16,
            'desc': '整型缓冲'},
    42214: {'name': 'floatBuffer',    'domain': '数据处理', 'mem_size': 16,
            'desc': '浮点缓冲'},
    42215: {'name': 'intLogicBuf',    'domain': '数据处理', 'mem_size': 16,
            'desc': '整型逻辑缓冲'},
    42216: {'name': 'intModOper',     'domain': '数据处理', 'mem_size': 16,
            'desc': '整型模运算'},
    42217: {'name': 'roughFilter',    'domain': '数据处理', 'mem_size': 32,
            'desc': '粗滤波'},
    42218: {'name': 'dataRedundancy', 'domain': '数据处理', 'mem_size': 32,
            'desc': '数据冗余'},
    42219: {'name': 'dealInUnify',    'domain': '数据处理', 'mem_size': 16,
            'desc': '输入统一处理'},
    42220: {'name': 'dealOutUnify',   'domain': '数据处理', 'mem_size': 16,
            'desc': '输出统一处理'},
    42221: {'name': 'dealToData',     'domain': '数据处理', 'mem_size': 16,
            'desc': '转换为数据'},
    42222: {'name': 'dealToSwit',     'domain': '数据处理', 'mem_size': 16,
            'desc': '转换为开关量'},
}

MODULE_HEADER_ID = 40961  # 0xA001


# ─────────────────────────── 数据结构 ───────────────────────────

class FBInstance:
    """一个功能块实例记录"""
    __slots__ = ['line_no', 'type_id', 'base_addr', 'init_val',
                 'has_init', 'name', 'domain']

    def __init__(self, line_no: int, type_id: int,
                 base_addr: int, init_val=None):
        self.line_no   = line_no
        self.type_id   = type_id
        self.base_addr = base_addr
        self.init_val  = init_val
        self.has_init  = init_val is not None
        info = FB_REGISTRY.get(type_id, {})
        self.name   = info.get('name',   f'FB_{type_id}')
        self.domain = info.get('domain', '未知')

    def init_as_float(self) -> Optional[float]:
        if self.init_val is None:
            return None
        try:
            return struct.unpack('<f', struct.pack('<I', self.init_val))[0]
        except Exception:
            return None

    def is_float_init(self) -> bool:
        """判断初始值是否为 float32 编码（值 > 0x7FFFF 且非布尔）"""
        if self.init_val is None or self.init_val <= 1:
            return False
        return self.init_val > 65535


# ─────────────────────────── 解析器 ───────────────────────────

def parse_code_file(filepath: str) -> Tuple[Optional[FBInstance], List[FBInstance], List[str]]:
    """
    解析 .code 文件，返回 (header, fb_list, warnings)
    """
    header = None
    fb_list = []
    warnings = []

    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        lines = f.readlines()

    for line_no, raw_line in enumerate(lines, 1):
        line = raw_line.strip()
        if not line:
            continue

        parts = line.split()
        if len(parts) < 3:
            warnings.append(f'第{line_no}行字段不足（期望≥3，实际{len(parts)}）: {line!r}')
            continue

        try:
            type_id   = int(parts[0])
            type_id2  = int(parts[1])
            base_addr = int(parts[2])
        except ValueError as e:
            warnings.append(f'第{line_no}行解析失败: {e}')
            continue

        # 冗余字段校验
        if type_id != type_id2:
            warnings.append(
                f'第{line_no}行 type_id 冗余字段不一致: '
                f'{type_id} ≠ {type_id2}')

        # 初始值解析
        init_val = None
        if len(parts) >= 4:
            try:
                init_val = int(parts[3])
            except ValueError:
                warnings.append(f'第{line_no}行 init_val 解析失败: {parts[3]!r}')

        fb = FBInstance(line_no, type_id, base_addr, init_val)

        if type_id == MODULE_HEADER_ID and line_no == 1:
            header = fb
        else:
            fb_list.append(fb)

    return header, fb_list, warnings


# ─────────────────────────── 验证器 ───────────────────────────

class ValidationResult:
    def __init__(self, filepath: str):
        self.filepath   = filepath
        self.filename   = os.path.basename(filepath)
        self.file_size  = os.path.getsize(filepath)
        self.header     = None
        self.fb_list    = []
        self.warnings   = []
        self.errors     = []
        # 统计
        self.fb_count         = 0
        self.unique_type_ids  = 0
        self.max_addr         = 0
        self.estimated_mem    = 0
        self.domain_stats     = {}
        self.addr_conflicts   = []
        self.unknown_type_ids = []
        self.float_inits      = []   # (name, addr, float_val)
        self.ok               = False


def validate(filepath: str) -> ValidationResult:
    vr = ValidationResult(filepath)

    try:
        header, fb_list, warnings = parse_code_file(filepath)
    except Exception as e:
        vr.errors.append(f'文件读取失败: {e}')
        return vr

    vr.header   = header
    vr.fb_list  = fb_list
    vr.warnings = warnings

    # ── 1. 模块头检查 ───────────────────────────────────────────
    if header is None:
        vr.errors.append('缺少模块声明头（第一行应为 type_id=40961）')
    else:
        if header.base_addr != 0:
            vr.warnings.append(f'模块头 base_addr={header.base_addr}，期望 0')

    if not fb_list:
        vr.errors.append('功能块实例列表为空')
        return vr

    # ── 2. 基础统计 ─────────────────────────────────────────────
    vr.fb_count = len(fb_list)
    vr.unique_type_ids = len(set(fb.type_id for fb in fb_list))
    vr.max_addr = max(fb.base_addr for fb in fb_list)

    # 域分布统计
    for fb in fb_list:
        vr.domain_stats[fb.domain] = vr.domain_stats.get(fb.domain, 0) + 1

    # ── 3. 未知 type_id 检查 ────────────────────────────────────
    unknown = set()
    for fb in fb_list:
        if fb.type_id not in FB_REGISTRY:
            unknown.add(fb.type_id)
    vr.unknown_type_ids = sorted(unknown)

    # ── 4. 地址合法性检查 ───────────────────────────────────────
    if vr.max_addr < 0:
        vr.errors.append(f'存在负地址')

    # 检查地址是否按序递增（编译器通常顺序分配）
    addrs = [fb.base_addr for fb in fb_list if fb.base_addr > 0]
    for i in range(1, len(addrs)):
        if addrs[i] < addrs[i - 1]:
            vr.warnings.append(
                f'地址乱序：第{i}个地址 {addrs[i]} < 前一个 {addrs[i-1]}')

    # 估算内存占用（最大地址 + 估算末尾FB大小）
    if fb_list:
        last_fb = max(fb_list, key=lambda x: x.base_addr)
        last_info = FB_REGISTRY.get(last_fb.type_id, {})
        tail_size = last_info.get('mem_size', 16)
        vr.estimated_mem = last_fb.base_addr + tail_size

    # ── 5. 浮点初始值解码 ───────────────────────────────────────
    for fb in fb_list:
        if fb.is_float_init():
            fval = fb.init_as_float()
            if fval is not None:
                vr.float_inits.append((fb.name, fb.base_addr, fval))

    # ── 6. 综合判定 ─────────────────────────────────────────────
    vr.ok = len(vr.errors) == 0

    return vr


# ─────────────────────────── 模拟执行器 ───────────────────────────

class LMSimulator:
    """
    模拟 .code 文件的运行时执行过程：
    按顺序实例化各 FB，将初始值写入对应内存地址，
    模拟一次完整的调度周期。
    """

    def __init__(self):
        self.memory: Dict[int, int] = {}   # addr -> value（字节地址）
        self.exec_log: List[str] = []
        self.step_count = 0

    def _write(self, addr: int, val: int):
        self.memory[addr] = val & 0xFFFFFFFF

    def _read(self, addr: int) -> int:
        return self.memory.get(addr, 0)

    def run(self, vr: ValidationResult) -> bool:
        """模拟执行，返回是否成功"""
        if not vr.ok:
            return False

        self.exec_log.append(f'开始执行: {vr.filename}')
        self.exec_log.append(f'共 {vr.fb_count} 个功能块实例')

        for fb in vr.fb_list:
            self.step_count += 1
            info = FB_REGISTRY.get(fb.type_id, {})
            desc = info.get('desc', '未知')

            # 写入初始值
            if fb.has_init:
                self._write(fb.base_addr, fb.init_val)
                if fb.is_float_init():
                    fval = fb.init_as_float()
                    log = (f'  [{self.step_count:03d}] {fb.name:<16} '
                           f'addr=0x{fb.base_addr:04X}  '
                           f'init={fval:.4f}(float)')
                else:
                    log = (f'  [{self.step_count:03d}] {fb.name:<16} '
                           f'addr=0x{fb.base_addr:04X}  '
                           f'init={fb.init_val}')
            else:
                log = (f'  [{self.step_count:03d}] {fb.name:<16} '
                       f'addr=0x{fb.base_addr:04X}  (无初始值)')

            self.exec_log.append(log)

        self.exec_log.append(f'执行完成，共处理 {self.step_count} 个实例')
        return True


# ─────────────────────────── 报告输出 ───────────────────────────

def print_single_report(vr: ValidationResult, sim: LMSimulator,
                        verbose: bool = False):
    sep = '─' * 68
    print(sep)
    status = '✓ 通过' if vr.ok else '✗ 失败'
    print(f'  文件: {vr.filename}   [{status}]')
    print(f'  大小: {vr.file_size} 字节')
    print()

    # 文件头
    if vr.header:
        print(f'  模块头:    type_id={vr.header.type_id}'
              f'(0x{vr.header.type_id:05X}), base_addr=0')
    else:
        print('  模块头:    缺失')

    # 统计
    print(f'  FB实例数:  {vr.fb_count}')
    print(f'  唯一类型:  {vr.unique_type_ids} 种')
    print(f'  地址范围:  0x0000 ~ 0x{vr.max_addr:04X}  '
          f'(≈{vr.estimated_mem} 字节)')

    # 域分布
    domain_str = '  '.join(f'{d}:{c}' for d, c in sorted(vr.domain_stats.items()))
    print(f'  域分布:    {domain_str}')

    # 浮点初始值
    if vr.float_inits:
        vals_str = '  '.join(
            f'{name}={fval:.3f}' for name, _, fval in vr.float_inits)
        print(f'  浮点初值:  {vals_str}')

    # 未知类型
    if vr.unknown_type_ids:
        print(f'  未知类型:  {[hex(x) for x in vr.unknown_type_ids]}')

    # 错误与警告
    if vr.errors:
        for e in vr.errors:
            print(f'  [错误] {e}')
    if vr.warnings:
        for w in vr.warnings:
            print(f'  [警告] {w}')

    # 执行模拟结果
    if sim.step_count > 0:
        print(f'  执行模拟:  完成 {sim.step_count} 步')

    # 详细模式：打印所有FB实例
    if verbose and vr.fb_list:
        print()
        print('  功能块实例列表:')
        for fb in vr.fb_list:
            iv = ''
            if fb.has_init:
                if fb.is_float_init():
                    iv = f' = {fb.init_as_float():.4f}(f)'
                else:
                    iv = f' = {fb.init_val}'
            print(f'    [{fb.line_no:03d}] {fb.name:<18} '
                  f'type={fb.type_id}  '
                  f'addr=0x{fb.base_addr:04X}{iv}')
    print()


def print_summary(results: List[ValidationResult]):
    total  = len(results)
    passed = sum(1 for r in results if r.ok)
    sep    = '=' * 68

    print(sep)
    print('  LM .code 文件批量验证汇总报告')
    print(sep)
    print(f'  文件总数:     {total}')
    print(f'  验证通过:     {passed} / {total}')
    print()

    total_fb   = sum(r.fb_count for r in results)
    total_mem  = sum(r.estimated_mem for r in results)
    all_types  = set()
    for r in results:
        for fb in r.fb_list:
            all_types.add(fb.type_id)

    print(f'  FB实例总计:   {total_fb}')
    print(f'  涉及类型总计: {len(all_types)} 种')
    print(f'  估算总内存:   {total_mem} 字节')
    print()

    # 各文件一行摘要
    print(f'  {"文件名":<25} {"FB数":>5} {"类型数":>6} {"内存/B":>7} {"状态":>6}')
    print('  ' + '─' * 55)
    for r in results:
        status = '通过' if r.ok else '失败'
        print(f'  {r.filename:<25} {r.fb_count:>5} '
              f'{r.unique_type_ids:>6} {r.estimated_mem:>7} {status:>6}')

    print(sep)


# ─────────────────────────── 主函数 ───────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='LM .code 文件解释器与验证工具（文本 FB 序列格式）'
    )
    parser.add_argument('path', help='.code 文件或包含 .code 文件的目录')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='详细输出每个 FB 实例信息')
    parser.add_argument('--sim', action='store_true',
                        help='执行内存初始化模拟')
    parser.add_argument('-o', '--output', default=None,
                        help='报告输出文件（可选）')
    args = parser.parse_args()

    from pathlib import Path
    target = Path(args.path)
    if target.is_file():
        files = [str(target)]
    elif target.is_dir():
        files = sorted(str(p) for p in target.glob('*.code'))
    else:
        print(f'路径不存在: {args.path}', file=sys.stderr)
        sys.exit(1)

    if not files:
        print('未找到 .code 文件', file=sys.stderr)
        sys.exit(1)

    print(f'\n{"="*68}')
    print(f'  LM 字节码解释器  —  共发现 {len(files)} 个 .code 文件')
    print(f'{"="*68}\n')

    results = []
    for fp in files:
        vr = validate(fp)
        sim = LMSimulator()
        if args.sim:
            sim.run(vr)
        results.append(vr)
        print_single_report(vr, sim, verbose=args.verbose)

    print_summary(results)

    if args.output:
        # 将标准输出重定向到文件
        import io
        buf = io.StringIO()
        old_stdout = sys.stdout
        sys.stdout = buf
        for vr in results:
            sim = LMSimulator()
            print_single_report(vr, sim, verbose=True)
        print_summary(results)
        sys.stdout = old_stdout
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(buf.getvalue())
        print(f'\n报告已保存: {args.output}')


if __name__ == '__main__':
    main()
