"""
LH 解释器自验证测试套件
========================
通过 Python 直接构造合法 .code 二进制文件，
验证解释器各指令语义的正确性。
无需真实板卡，无需编译器输出文件即可独立运行。
"""

import struct
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lh_interpreter import LHInterpreter, MAGIC


def build_code_file(instructions_bytes: bytes, data_bytes: bytes = b'',
                    total_mem: int = 256) -> bytes:
    """构造合法的 .code 二进制文件"""
    code_offset = 32
    code_size   = len(instructions_bytes)
    data_offset = code_offset + code_size
    data_size   = len(data_bytes)
    entry_point = 0

    header = struct.pack('<4sHHIIIIIHH',
        MAGIC,          # magic
        0x0001,         # version
        0x0000,         # flags
        code_offset,    # code_offset
        code_size,      # code_size
        data_offset,    # data_offset
        data_size,      # data_size
        entry_point,    # entry_point
        total_mem,      # total_mem
        0x0000          # reserved
    )
    return header + instructions_bytes + data_bytes


def enc(*args) -> bytes:
    """将指令字节序列编码为bytes（每个参数为int）"""
    return bytes(args)


def enc16(opcode: int, val: int) -> bytes:
    return struct.pack('<Bh', opcode, val)


def enc16u(opcode: int, val: int) -> bytes:
    return struct.pack('<BH', opcode, val)


def enc32(opcode: int, val: int) -> bytes:
    return struct.pack('<Bi', opcode, val)


def enc_fb(type_id: int, base_addr: int) -> bytes:
    return struct.pack('<BHH', 0x50, type_id, base_addr)


HALT = bytes([0xFF])

# ─────────────────────────── 测试用例 ───────────────────────────

class TestResult:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.failures = []

    def check(self, name, got, expected):
        if got == expected:
            self.passed += 1
            print(f'  ✓  {name}')
        else:
            self.failed += 1
            self.failures.append(f'{name}: 期望={expected}, 实际={got}')
            print(f'  ✗  {name}  期望={expected}, 实际={got}')


def run_test(code_bytes: bytes, total_mem: int = 64) -> LHInterpreter:
    data = build_code_file(code_bytes, total_mem=total_mem)
    vm = LHInterpreter(verbose=False, trace=False)
    # 写入临时文件
    tmp = '/tmp/lm_test.code'
    with open(tmp, 'wb') as f:
        f.write(data)
    vm.load(tmp)
    vm.execute()
    return vm


def test_load_imm(r: TestResult):
    print('\n[1] LOAD_IMM 指令测试')
    # LOAD_IMM16 压入 42
    code = enc16(0x01, 42) + HALT
    vm = run_test(code)
    r.check('LOAD_IMM16 压栈值', vm.stack[-1], 42)

    # LOAD_IMM16 负数（-5 → 0xFFFB）
    code = enc16(0x01, -5) + HALT
    vm = run_test(code)
    r.check('LOAD_IMM16 负数（-5）', vm._as_signed16(vm.stack[-1]), -5)

    # LOAD_IMM32 压入 70000
    code = enc32(0x02, 70000) + HALT
    vm = run_test(code)
    r.check('LOAD_IMM32 压栈值', vm.stack[-1], 70000)


def test_store_load_addr(r: TestResult):
    print('\n[2] STORE/LOAD_ADDR 内存读写测试')
    # LOAD_IMM16 100 → STORE_ADDR[10] → LOAD_ADDR[10]
    code = (enc16(0x01, 100) +
            enc16u(0x05, 10) +
            enc16u(0x03, 10) +
            HALT)
    vm = run_test(code)
    r.check('写入addr=10后读回', vm.stack[-1], 100)
    r.check('内存[10]=100', vm._mem_read16(10), 100)


def test_arithmetic_int(r: TestResult):
    print('\n[3] 整数算术指令测试')
    def arith(a, b, op):
        code = enc16(0x01, a) + enc16(0x01, b) + bytes([op]) + HALT
        return run_test(code).stack[-1]

    r.check('ADD_I  10+7=17',  vm_top := arith(10, 7,  0x10), 17)
    r.check('SUB_I  10-7=3',   arith(10, 7,  0x11), 3)
    r.check('MUL_I  6×7=42',   arith(6,  7,  0x12), 42)
    r.check('DIV_I  20÷4=5',   arith(20, 4,  0x13), 5)
    r.check('MOD_I  17%5=2',   arith(17, 5,  0x14), 2)


def test_arithmetic_real(r: TestResult):
    print('\n[4] 浮点算术指令测试')
    import struct as st

    def float32(f):
        return st.unpack('<I', st.pack('<f', f))[0]

    def arith_r(a, b, op):
        fa, fb = float32(a), float32(b)
        code = (enc32(0x02, fa) + enc32(0x02, fb) + bytes([op]) + HALT)
        vm = run_test(code)
        return vm._as_float(vm.stack[-1])

    r.check('ADD_R  1.5+2.5=4.0', abs(arith_r(1.5, 2.5, 0x18) - 4.0) < 1e-4, True)
    r.check('SUB_R  5.0-2.0=3.0', abs(arith_r(5.0, 2.0, 0x19) - 3.0) < 1e-4, True)
    r.check('MUL_R  3.0×2.0=6.0', abs(arith_r(3.0, 2.0, 0x1A) - 6.0) < 1e-4, True)
    r.check('DIV_R  9.0÷3.0=3.0', abs(arith_r(9.0, 3.0, 0x1B) - 3.0) < 1e-4, True)


def test_compare(r: TestResult):
    print('\n[5] 比较指令测试')
    def cmp(a, b, op):
        code = enc16(0x01, a) + enc16(0x01, b) + bytes([op]) + HALT
        return run_test(code).stack[-1]

    r.check('CMP_EQ  5==5 → 1', cmp(5, 5, 0x20), 1)
    r.check('CMP_EQ  5==6 → 0', cmp(5, 6, 0x20), 0)
    r.check('CMP_NE  5≠6 → 1', cmp(5, 6, 0x21), 1)
    r.check('CMP_LT  3<5 → 1', cmp(3, 5, 0x22), 1)
    r.check('CMP_LT  5<3 → 0', cmp(5, 3, 0x22), 0)
    r.check('CMP_GT  5>3 → 1', cmp(5, 3, 0x24), 1)
    r.check('CMP_GE  5≥5 → 1', cmp(5, 5, 0x25), 1)
    r.check('CMP_LE  3≤5 → 1', cmp(3, 5, 0x23), 1)


def test_logic(r: TestResult):
    print('\n[6] 逻辑指令测试')
    def logic2(a, b, op):
        code = enc16(0x01, a) + enc16(0x01, b) + bytes([op]) + HALT
        return run_test(code).stack[-1]

    def logic1(a, op):
        code = enc16(0x01, a) + bytes([op]) + HALT
        return run_test(code).stack[-1]

    r.check('AND  1 AND 1 → 1', logic2(1, 1, 0x30), 1)
    r.check('AND  1 AND 0 → 0', logic2(1, 0, 0x30), 0)
    r.check('OR   0 OR  1 → 1', logic2(0, 1, 0x31), 1)
    r.check('OR   0 OR  0 → 0', logic2(0, 0, 0x31), 0)
    r.check('NOT  NOT(0) → 1',  logic1(0, 0x32), 1)
    r.check('NOT  NOT(1) → 0',  logic1(1, 0x32), 0)
    r.check('XOR  1 XOR 0 → 1', logic2(1, 0, 0x33), 1)
    r.check('XOR  1 XOR 1 → 0', logic2(1, 1, 0x33), 0)


def test_jump(r: TestResult):
    print('\n[7] 跳转指令测试')

    # JMP_IF_FALSE：条件为假则跳转，跳过 LOAD_IMM16(99)，结果应为0
    # 布局：LOAD_IMM16(0) JMP_IF_FALSE(+3) LOAD_IMM16(99) HALT
    # JMP_IF_FALSE 本身占3字节(1+2)，跳过后续LOAD_IMM16(3字节)
    skip = enc16(0x01, 99)           # 3字节，要跳过的指令
    jmp  = struct.pack('<Bh', 0x41, len(skip))
    code = enc16(0x01, 0) + jmp + skip + HALT
    vm = run_test(code)
    r.check('JMP_IF_FALSE 条件假时跳转（跳过LOAD99）', len(vm.stack), 0)

    # JMP_IF_FALSE：条件为真，不跳转，正常执行 LOAD_IMM16(99)
    code2 = enc16(0x01, 1) + jmp + skip + HALT
    vm2 = run_test(code2)
    r.check('JMP_IF_FALSE 条件真时不跳转（执行LOAD99）', vm2.stack[-1] if vm2.stack else -1, 99)

    # JMP 无条件跳转，跳过 LOAD_IMM16(77)
    skip2 = enc16(0x01, 77)
    jmp2  = struct.pack('<Bh', 0x40, len(skip2))
    code3 = jmp2 + skip2 + HALT
    vm3 = run_test(code3)
    r.check('JMP 无条件跳转（跳过LOAD77）', len(vm3.stack), 0)


def test_neg(r: TestResult):
    print('\n[8] 取负指令测试')
    code = enc16(0x01, 10) + bytes([0x60]) + HALT
    vm = run_test(code)
    r.check('NEG_I -10', vm._as_signed16(vm.stack[-1]), -10)

    code2 = enc16(0x01, -10) + bytes([0x60]) + HALT
    vm2 = run_test(code2)
    r.check('NEG_I -(-10)=10', vm2._as_signed16(vm2.stack[-1]), 10)


def test_fb_call(r: TestResult):
    print('\n[9] FB_CALL 功能块调用测试')
    # 调用 type_id=10000 (systemInit)，base_addr=0x0020
    code = enc_fb(10000, 0x0020) + HALT
    vm = run_test(code)
    r.check('FB_CALL 执行不崩溃', vm.halt_reason, 'HALT 指令')
    r.check('FB_CALL 记录日志', len(vm.fb_call_log), 1)
    r.check('FB_CALL type_id记录正确', vm.fb_call_log[0]['type_id'], 10000)
    r.check('FB_CALL 名称解析正确', vm.fb_call_log[0]['name'], 'systemInit')


def test_file_structure(r: TestResult):
    print('\n[10] 文件结构验证测试')
    # 合法文件：只含 HALT
    code_bytes = HALT
    data = build_code_file(code_bytes, total_mem=64)

    r.check('文件大小 = 32 + 1', len(data), 33)

    # 文件头魔数
    r.check('魔数正确', data[:4], MAGIC)

    # 版本号（小端序 0x0001）
    version = struct.unpack_from('<H', data, 4)[0]
    r.check('版本号 0x0001', version, 0x0001)

    # 代码段偏移 = 32
    code_off = struct.unpack_from('<I', data, 8)[0]
    r.check('代码段偏移 = 32', code_off, 32)

    # 代码段大小 = 1（HALT）
    code_sz = struct.unpack_from('<I', data, 12)[0]
    r.check('代码段大小 = 1', code_sz, 1)

    # 数据段大小 = 0
    data_sz = struct.unpack_from('<I', data, 20)[0]
    r.check('数据段大小 = 0', data_sz, 0)

    # total_mem = 64
    total_mem = struct.unpack_from('<H', data, 28)[0]
    r.check('total_mem = 64', total_mem, 64)


def test_complex_program(r: TestResult):
    """综合测试：模拟简单控制逻辑
       计算 result = (a + b) * 2，其中 a=10, b=5
       期望结果 = 30，存入内存地址 5
    """
    print('\n[11] 综合测试：(10 + 5) × 2 → mem[5]')
    code = (
        enc16(0x01, 10) +    # LOAD_IMM16 10
        enc16(0x01, 5)  +    # LOAD_IMM16 5
        bytes([0x10])   +    # ADD_I  → 15
        enc16(0x01, 2)  +    # LOAD_IMM16 2
        bytes([0x12])   +    # MUL_I  → 30
        enc16u(0x05, 5) +    # STORE_ADDR [5]
        HALT
    )
    vm = run_test(code)
    r.check('综合运算结果 mem[5]=30', vm._mem_read16(5), 30)
    r.check('执行正常终止', vm.halt_reason, 'HALT 指令')


# ─────────────────────────── 主函数 ───────────────────────────

def main():
    print('=' * 60)
    print('  LH 字节码解释器自验证测试套件')
    print('=' * 60)

    r = TestResult()

    test_file_structure(r)
    test_load_imm(r)
    test_store_load_addr(r)
    test_arithmetic_int(r)
    test_arithmetic_real(r)
    test_compare(r)
    test_logic(r)
    test_jump(r)
    test_neg(r)
    test_fb_call(r)
    test_complex_program(r)

    print('\n' + '=' * 60)
    print(f'  测试结果: {r.passed} 通过 / {r.passed + r.failed} 总计')
    if r.failures:
        print(f'  失败项:')
        for f in r.failures:
            print(f'    - {f}')
    else:
        print('  全部通过 ✓')
    print('=' * 60)

    sys.exit(0 if r.failed == 0 else 1)


if __name__ == '__main__':
    main()
