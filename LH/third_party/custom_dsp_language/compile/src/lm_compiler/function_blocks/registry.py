# -*- coding: utf-8 -*-
"""
功能块注册表
管理所有功能块的元数据：类型ID、内存占用、参数定义等
"""

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Any


@dataclass
class ParameterDef:
    """功能块参数定义"""
    name: str
    data_type: str          # BOOL, INT, REAL, DINT, ...
    direction: str = "IN"   # IN, OUT, IN_OUT
    offset: int = 0         # 在功能块内存中的偏移量
    default_value: Any = None
    description: str = ""

    def __repr__(self):
        return f"ParameterDef({self.name}: {self.data_type}, offset={self.offset})"


@dataclass
class FunctionBlockMeta:
    """功能块元数据"""
    name: str
    type_id: int            # .code 文件中的类型ID
    memory_size: int        # 占用的内存单元数量
    parameters: List[ParameterDef] = field(default_factory=list)
    description: str = ""
    category: str = ""      # 分类: system, math, logic, timer, counter, ...

    def get_parameter(self, name: str) -> Optional[ParameterDef]:
        """按名称查找参数"""
        for p in self.parameters:
            if p.name.upper() == name.upper():
                return p
        return None

    def get_parameter_offset(self, name: str) -> Optional[int]:
        """获取参数在功能块内存中的偏移量"""
        param = self.get_parameter(name)
        return param.offset if param else None

    def __repr__(self):
        return (f"FunctionBlockMeta({self.name}, "
                f"type_id={self.type_id}, "
                f"mem={self.memory_size}, "
                f"params={len(self.parameters)})")


class FunctionBlockRegistry:
    """
    功能块注册表
    
    管理所有可用功能块的元数据。
    代码生成器通过此注册表查询功能块的类型ID、内存占用和参数偏移。
    """

    def __init__(self):
        self._blocks: Dict[str, FunctionBlockMeta] = {}

    def register(self, meta: FunctionBlockMeta):
        """注册一个功能块"""
        self._blocks[meta.name] = meta

    def get(self, name: str) -> Optional[FunctionBlockMeta]:
        """按名称获取功能块元数据"""
        return self._blocks.get(name)

    def has(self, name: str) -> bool:
        """检查功能块是否已注册"""
        return name in self._blocks

    def all_blocks(self) -> Dict[str, FunctionBlockMeta]:
        """返回所有已注册的功能块"""
        return dict(self._blocks)

    def list_names(self) -> List[str]:
        """列出所有已注册的功能块名称"""
        return list(self._blocks.keys())

    def load_defaults(self):
        """加载内置的默认功能块定义"""
        from .definitions import get_all_definitions
        for meta in get_all_definitions():
            self.register(meta)

    def __repr__(self):
        return f"FunctionBlockRegistry({len(self._blocks)} blocks)"

    def __len__(self):
        return len(self._blocks)
