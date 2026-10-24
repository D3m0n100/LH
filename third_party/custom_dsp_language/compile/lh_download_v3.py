"""
LH .code 文件下载脚本 v3
支持新 Bootloader 协议：下载完成后发送触发命令跳转执行

用法：
    python lh_download.py <code文件路径> [COM口] [设备地址]

示例：
    python lh_download.py lh_programs\\main.code COM5
    python lh_download.py lh_programs\\main.code COM5 65
"""

import sys
import os
import time
import struct
import serial
import serial.tools.list_ports

BOOT_TRIGGER_REG   = 40960   # 写 0xAA55 触发执行
BOOT_STATUS_REG    = 40959   # 状态寄存器（0=等待,1=加载中,15=运行中）
BOOT_TRIGGER_VALUE = 0xAA55


# ─────────────────────────────────────────────
# Modbus RTU
# ─────────────────────────────────────────────

def modbus_crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc


def build_write_single(device_addr, reg_0base, value):
    if not 1 <= device_addr <= 247 or not 0 <= reg_0base <= 65535 or not 0 <= value <= 65535:
        raise ValueError('Invalid Modbus address or register value')
    pdu = struct.pack('>BBHH', device_addr, 0x06, reg_0base, value)
    crc = modbus_crc16(pdu)
    return pdu + struct.pack('<H', crc)


def build_read_single(device_addr, reg_0base):
    if not 1 <= device_addr <= 247 or not 0 <= reg_0base <= 65535:
        raise ValueError('Invalid Modbus address')
    pdu = struct.pack('>BBHH', device_addr, 0x03, reg_0base, 1)
    crc = modbus_crc16(pdu)
    return pdu + struct.pack('<H', crc)


class ModbusRTU:
    def __init__(self, port, baudrate=19200, device_addr=65):
        self.device_addr = device_addr
        self.ser = serial.Serial(
            port=port, baudrate=baudrate,
            bytesize=8, parity=serial.PARITY_NONE,
            stopbits=1, timeout=2.0, write_timeout=2.0
        )
        time.sleep(0.2)
        self.ser.reset_input_buffer()
        print(f"[串口] 已打开 {port}，波特率 {baudrate}，设备地址 {device_addr}")

    def _exchange(self, request, response_size):
        # A single monotonic budget covers fragmented reads, including exceptions.
        self.last_error = ''
        timeout = self.ser.timeout
        deadline = time.monotonic() + (timeout if timeout is not None else 2.0)
        self.ser.reset_input_buffer()
        try:
            if self.ser.write(request) != len(request):
                return None
            response = bytearray()
            expected = response_size
            while len(response) < expected:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                self.ser.timeout = remaining
                chunk = self.ser.read(expected - len(response))
                if not chunk:
                    return None
                response.extend(chunk)
                if len(response) >= 2 and response[1] & 0x80:
                    expected = 5
            response = bytes(response)
            if (len(response) == 5 and response[:2] == bytes((request[0], request[1] | 0x80))
                    and modbus_crc16(response[:-2]) == int.from_bytes(response[-2:], 'little')):
                self.last_error = f'Modbus exception {response[2]}'
                return None
            if (time.monotonic() > deadline or len(response) != response_size or response[:2] != request[:2]
                    or modbus_crc16(response[:-2]) != int.from_bytes(response[-2:], 'little')):
                return None
            return response
        except (serial.SerialException, OSError):
            return None
        finally:
            self.ser.timeout = timeout

    def write_register(self, reg_4x, value):
        addr = reg_4x - 40001
        req  = build_write_single(self.device_addr, addr, value)
        resp = self._exchange(req, 8)
        if resp is not None and resp != req:
            self.last_error = 'Register address or value echo does not match request'
        return resp == req

    def read_register(self, reg_4x):
        addr = reg_4x - 40001
        req  = build_read_single(self.device_addr, addr)
        resp = self._exchange(req, 7)
        if resp is not None and resp[2] == 2:
            return struct.unpack('>H', resp[3:5])[0]
        return None

    def close(self):
        self.ser.close()


# ─────────────────────────────────────────────
# .code 文件解析
# ─────────────────────────────────────────────

def parse_code_file(code_path):
    with open(code_path, 'rb') as f:
        raw = f.read()
    if (b'// LH-EXECUTION-UNCONFIRMED: scalar-assignment-v1' in raw
            or b'// LH-OFFLINE-COMPATIBILITY: legacy-constants-v1' in raw):
        raise ValueError('赋值写回尚未通过目标固件验证，当前产物不可下载或运行。')
    print(f"[解析] 文件大小: {len(raw)} 字节")

    text = raw.decode('ascii')
    writes = []
    for line_number, original in enumerate(text.splitlines(), 1):
        line = original.split('//', 1)[0].strip()
        if not line:
            continue
        parts = line.split()
        try:
            if len(parts) == 3:
                reg, value = int(parts[0]), int(parts[2])
                offset = 0
            elif len(parts) == 4:
                reg, offset, value = int(parts[0]), int(parts[2]), int(parts[3])
            else:
                raise ValueError('expected three or four fields')
            if (not 0 <= int(parts[1]) <= 65535 or not 40001 <= reg <= 49999 or offset < 0
                    or not 40001 <= reg + offset <= 49999 or not 0 <= value <= 65535):
                raise ValueError('invalid format, address, offset or value')
            writes.append((reg + offset, value))
        except ValueError as exc:
            raise ValueError(f'{code_path}:{line_number}: {exc}') from exc

    print(f"[解析] {len(writes)} 条写操作")
    if writes:
        regs = [r for r, _ in writes]
        print(f"[解析] 寄存器范围: {min(regs)} ~ {max(regs)}")
        print(f"[解析] 前5条预览:")
        for r, v in writes[:5]:
            print(f"         写 寄存器{r} = {v}")
    return writes


# ─────────────────────────────────────────────
# 主下载流程
# ─────────────────────────────────────────────

def download(code_path, port='COM5', device_addr=65):
    print("=" * 60)
    print("LH 程序下载工具 v3（配合 F2812 Bootloader）")
    print("=" * 60)

    if not os.path.exists(code_path):
        print(f"[错误] 找不到文件: {code_path}")
        sys.exit(1)

    # 1. 解析
    print(f"\n[步骤1] 解析 {code_path}")
    try:
        writes = parse_code_file(code_path)
    except ValueError as exc:
        print(f"[错误] {exc}")
        sys.exit(1)
    if not writes:
        print("[错误] 文件解析结果为空")
        sys.exit(1)

    # 2. 连接串口
    print(f"\n[步骤2] 连接串口 {port}")
    try:
        mb = ModbusRTU(port=port, baudrate=19200, device_addr=device_addr)
    except serial.SerialException as e:
        print(f"[错误] 串口打开失败: {e}")
        sys.exit(1)

    # 3. 握手：读状态寄存器
    print("\n[步骤3] 握手（读 Bootloader 状态寄存器 40959）")
    status = mb.read_register(BOOT_STATUS_REG)
    if status is not None:
        print(f"[握手] 成功！Bootloader 状态 = {status}（0=等待下载）")
    else:
        print("[握手] 无响应。")
        print("  请确认：Bootloader 已通过 JTAG 烧入 Flash，控制板已上电")
        ans = input("  是否仍继续下载？(y/n): ").strip().lower()
        if ans != 'y':
            mb.close()
            sys.exit(0)

    # 4. 写入程序数据
    print(f"\n[步骤4] 写入程序，共 {len(writes)} 条指令")
    ok_count   = 0
    fail_count = 0

    for i, (reg, val) in enumerate(writes):
        ok = mb.write_register(reg, val)
        if ok:
            ok_count += 1
        else:
            fail_count += 1
            print(f"  [警告] 第{i+1}条失败：寄存器{reg} = {val}")

        if (i + 1) % 50 == 0 or (i + 1) == len(writes):
            pct = (i + 1) / len(writes) * 100
            print(f"  进度: {i+1}/{len(writes)} ({pct:.1f}%)  成功:{ok_count} 失败:{fail_count}")

    if fail_count > 0:
        print(f"\n[警告] 有 {fail_count} 条写入失败，建议重试")
        ans = input("  是否仍发送执行触发命令？(y/n): ").strip().lower()
        if ans != 'y':
            mb.close()
            sys.exit(0)

    # 5. 发送执行触发命令
    print(f"\n[步骤5] 发送执行触发命令（写 40960 = 0xAA55）")
    ok = mb.write_register(BOOT_TRIGGER_REG, BOOT_TRIGGER_VALUE)
    if ok:
        print("[触发] 命令已发送，F2812 正在跳转执行 LH 程序...")
    else:
        print("[触发] 无响应（F2812 可能已跳转执行，属正常现象）")

    # 6. 验证状态
    time.sleep(0.5)
    print("\n[步骤6] 读取最终状态")
    status = mb.read_register(BOOT_STATUS_REG)
    if status == 15:
        print("  状态 = 15：LH 程序正在运行！下载成功。")
    elif status is None:
        print("  无响应（F2812 已切换到 LH 程序执行，Bootloader 已退出，属正常）")
    else:
        print(f"  状态 = {status}")

    mb.close()

    print("\n" + "=" * 60)
    if fail_count == 0:
        print(f"下载完成！{ok_count} 条指令全部写入成功。")
    else:
        print(f"下载完成，{fail_count} 条失败，请检查后重试。")
    print("=" * 60)


def list_ports():
    print("可用串口：")
    for p in serial.tools.list_ports.comports():
        print(f"  {p.device}: {p.description}")


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("用法: python lh_download.py <code文件> [COM口] [设备地址]")
        print("      python lh_download.py --list-ports")
        list_ports()
        sys.exit(0)

    if sys.argv[1] == '--list-ports':
        list_ports()
        sys.exit(0)

    code_file = sys.argv[1]
    com_port  = sys.argv[2] if len(sys.argv) > 2 else 'COM5'
    dev_addr  = int(sys.argv[3]) if len(sys.argv) > 3 else 65

    download(code_file, com_port, dev_addr)
