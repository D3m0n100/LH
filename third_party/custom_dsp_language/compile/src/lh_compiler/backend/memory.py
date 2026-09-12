# -*- coding: utf-8 -*-
"""
内存分配器
实现线性内存地址分配机制

分配规律:
    下一地址 = 当前地址 + 当前功能块占用空间
    
    示例:
        _System:        地址=0,  占用=38
        _IntConstBuild: 地址=38, 占用=16
        下一个块:       地址=54, ...

用法:
    allocator = MemoryAllocator()
    addr = allocator.allocate("_System", 38)    # 返回 0
    addr = allocator.allocate("myConst", 16)    # 返回 38
"""

from dataclasses import dataclass, field
from typing import Dict, List, Optional


@dataclass
class MemoryBlock:
    """一个已分配的内存块"""
    name: str           # 实例名称 (如 "system", "myConst")
    block_type: str     # 功能块类型 (如 "_System", "_IntConstBuild")
    address: int        # 起始地址
    size: int           # 占用大小

    @property
    def end_address(self) -> int:
        """结束地址（不含）"""
        return self.address + self.size

    def __repr__(self):
        return (f"MemoryBlock({self.name}: {self.block_type}, "
                f"addr={self.address}, size={self.size})")


class MemoryAllocator:
    """
    线性内存分配器
    
    按照 .code 文件的内存布局规则，从地址 0 开始线性分配。
    每个功能块实例占用固定大小的连续内存区域。
    """

    def __init__(self, start_address: int = 0):
        self._next_address: int = start_address
        self._blocks: List[MemoryBlock] = []
        self._name_map: Dict[str, MemoryBlock] = {}
        self._variable_addresses: Dict[str, int] = {}

    @property
    def next_address(self) -> int:
        """下一个可用地址"""
        return self._next_address

    @property
    def total_allocated(self) -> int:
        """已分配的总内存大小"""
        return self._next_address

    def allocate(self, instance_name: str, block_type: str, size: int) -> int:
        """
        分配一块内存
        
        Args:
            instance_name: 实例名称
            block_type: 功能块类型名
            size: 需要的内存大小
        
        Returns:
            分配的起始地址
        """
        address = self._next_address

        block = MemoryBlock(
            name=instance_name,
            block_type=block_type,
            address=address,
            size=size
        )

        self._blocks.append(block)
        self._name_map[instance_name] = block
        self._next_address += size

        return address

    def allocate_variable(self, var_name: str, data_type: str) -> int:
        """
        为简单变量分配内存地址
        
        Args:
            var_name: 变量名
            data_type: 数据类型 (BOOL, INT, REAL, DINT, ...)
        
        Returns:
            分配的地址
        """
        size = self._type_size(data_type)
        address = self._next_address
        self._variable_addresses[var_name] = address
        self._next_address += size
        return address

    def get_block(self, instance_name: str) -> Optional[MemoryBlock]:
        """按实例名获取内存块"""
        return self._name_map.get(instance_name)

    def get_address(self, instance_name: str) -> Optional[int]:
        """获取实例的起始地址"""
        block = self._name_map.get(instance_name)
        if block:
            return block.address
        return self._variable_addresses.get(instance_name)

    def get_all_blocks(self) -> List[MemoryBlock]:
        """获取所有已分配的内存块"""
        return list(self._blocks)

    def dump(self) -> str:
        """打印内存分配表（调试用）"""
        lines = ["=== 内存分配表 ==="]
        lines.append(f"{'名称':<20} {'类型':<20} {'地址':>6} {'大小':>6} {'结束':>6}")
        lines.append("-" * 80)

        for block in self._blocks:
            lines.append(
                f"{block.name:<20} {block.block_type:<20} "
                f"{block.address:>6} {block.size:>6} {block.end_address:>6}"
            )

        if self._variable_addresses:
            lines.append("")
            lines.append("--- 简单变量 ---")
            for name, addr in self._variable_addresses.items():
                lines.append(f"  {name}: 地址={addr}")

        lines.append(f"\n总计已分配: {self._next_address} 个单元")
        return "\n".join(lines)

    def reset(self):
        """重置分配器"""
        self._next_address = 0
        self._blocks.clear()
        self._name_map.clear()
        self._variable_addresses.clear()

    @staticmethod
    def _type_size(data_type: str) -> int:
        """获取数据类型的内存大小"""
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
            "STRING": 256,  # 默认字符串长度
        }
        return sizes.get(data_type.upper(), 2)  # 默认 2 字节
