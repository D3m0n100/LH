# -*- coding: utf-8 -*-
"""
功能块注册表
管理所有功能块的元数据：类型ID、内存占用、参数定义等
"""

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Any
import math


@dataclass
class ParameterDef:
    """功能块参数定义"""
    name: str
    data_type: str          # BOOL, INT, REAL, DINT, ...
    direction: str = "IN"   # IN, OUT, IN_OUT
    offset: Optional[int] = None  # 仅显式证据可指定；构造参数不推导运行时地址
    default_value: Any = None
    description: str = ""

    def __repr__(self):
        return f"ParameterDef({self.name}: {self.data_type}, offset={self.offset})"


@dataclass(frozen=True)
class RuntimeFieldDef:
    """Explicit local field layout, separate from encoded constructor arguments."""
    name: str
    data_type: str
    offset: int
    size: int
    direction: str = "IN"


def validate_runtime_layout(fields: List[RuntimeFieldDef], memory_size: int):
    names = set()
    occupied = []
    for field in fields:
        if field.name.upper() in names:
            raise ValueError(f"重复运行时字段: {field.name}")
        names.add(field.name.upper())
        if field.direction not in {"IN", "OUT", "IN_OUT"}:
            raise ValueError(f"字段方向非法: {field.name}")
        if (type(field.offset) is not int or type(field.size) is not int or
                field.offset < 0 or field.size <= 0 or field.offset + field.size > memory_size):
            raise ValueError(f"字段布局越界: {field.name}")
        for start, end in occupied:
            if field.offset < end and start < field.offset + field.size:
                raise ValueError(f"字段布局重叠: {field.name}")
        occupied.append((field.offset, field.offset + field.size))


@dataclass
class FunctionBlockMeta:
    """功能块元数据"""
    name: str
    type_id: int            # .code 文件中的类型ID
    memory_size: int        # 占用的内存单元数量
    parameters: List[ParameterDef] = field(default_factory=list)
    description: str = ""
    category: str = ""      # 分类: system, math, logic, timer, counter, ...
    status: str = "supported"              # "supported" | "incomplete"
    incomplete_reason: str = ""            # 缺失契约/参数定义的原因说明
    runtime_fields: List[RuntimeFieldDef] = field(default_factory=list)

    def definition_errors(self) -> List[str]:
        errors = []
        seen = set()
        ranges = {'SINT': (-128, 127), 'USINT': (0, 255), 'BYTE': (0, 255),
                  'INT': (-32768, 32767), 'UINT': (0, 65535), 'WORD': (0, 65535),
                  'DINT': (-2147483648, 2147483647), 'UDINT': (0, 4294967295),
                  'DWORD': (0, 4294967295)}
        if not self.parameters and self.name != 'Nop':
            errors.append('参数契约未定义；空参数表不表示完整功能')
        for p in self.parameters:
            key = p.name.upper()
            if not key or key in seen:
                errors.append(f'{p.name}: 参数名称为空或重复')
            seen.add(key)
            if p.direction not in ('IN', 'OUT', 'IN_OUT'):
                errors.append(f'{p.name}: 参数方向无效')
            dt, value = p.data_type.upper(), p.default_value
            if dt not in {*ranges, 'BOOL', 'REAL'}:
                errors.append(f'{p.name}: 未支持的参数类型 {dt}')
            if value is None:
                continue
            if dt in ranges and (type(value) is not int or not ranges[dt][0] <= value <= ranges[dt][1]):
                errors.append(f'{p.name}: 默认值 {value} 超出 {dt} 类型范围或类型错误')
            elif dt == 'BOOL' and not (type(value) is bool or type(value) is int and value in (0, 1)):
                errors.append(f'{p.name}: BOOL 默认值无效')
            elif dt == 'REAL' and (type(value) not in (int, float) or not math.isfinite(value) or abs(value) > 3.4028234663852886e38):
                errors.append(f'{p.name}: REAL 默认值无效')
        return errors

    def capability_state(self) -> Dict[str, Any]:
        issues = self.definition_errors()
        return {'name': self.name, 'status': self.status,
                'definitionValid': not issues, 'definitionErrors': issues,
                'compilerHandles': self.is_supported and not issues,
                'targetExecutionVerified': False}

    def runtime_field_layout(self):
        validate_runtime_layout(self.runtime_fields, self.memory_size)
        return tuple(self.runtime_fields)

    @property
    def is_supported(self) -> bool:
        return self.status == "supported"

    @property
    def is_incomplete(self) -> bool:
        return self.status == "incomplete"

    def get_parameter(self, name: str) -> Optional[ParameterDef]:
        """按名称查找参数"""
        for p in self.parameters:
            if p.name.upper() == name.upper():
                return p
        return None

    def get_parameter_offset(self, name: str) -> Optional[int]:
        """获取参数在功能块内存中的偏移量"""
        for runtime_field in self.runtime_field_layout():
            if runtime_field.name.upper() == name.upper():
                return runtime_field.offset
        return None

    def __repr__(self):
        status_suffix = f", status={self.status}" if self.status != "supported" else ""
        return (f"FunctionBlockMeta({self.name}, "
                f"type_id={self.type_id}, "
                f"mem={self.memory_size}, "
                f"params={len(self.parameters)}{status_suffix})")


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
            issues = meta.definition_errors()
            if issues and not meta.parameters and meta.status == 'supported':
                meta.status = 'incomplete'
                meta.incomplete_reason = '; '.join(issues)
            self.register(meta)

    def __repr__(self):
        return f"FunctionBlockRegistry({len(self._blocks)} blocks)"

    def __len__(self):
        return len(self._blocks)

    def __contains__(self, name: str) -> bool:
        return self.has(name)

    def get_by_id(self, type_id: int) -> Optional[FunctionBlockMeta]:
        """按类型ID获取功能块元数据"""
        for meta in self._blocks.values():
            if meta.type_id == type_id:
                return meta
        return None

    def list_all(self) -> List[FunctionBlockMeta]:
        """返回所有功能块列表"""
        return list(self._blocks.values())

    def list_incomplete(self) -> List[FunctionBlockMeta]:
        """返回所有未完善契约的功能块列表"""
        return [b for b in self._blocks.values() if b.status == "incomplete"]

    def list_supported(self) -> List[FunctionBlockMeta]:
        """返回所有支持的功能块列表"""
        return [b for b in self._blocks.values() if b.status == "supported"]

    def list_categories(self) -> List[str]:
        """列出所有分类"""
        cats = {b.category for b in self._blocks.values() if b.category}
        return sorted(cats)

    def get_category(self, category: str) -> List[FunctionBlockMeta]:
        """获取指定分类下的所有功能块"""
        return [b for b in self._blocks.values() if b.category.lower() == category.lower()]

    def search(self, query: str) -> List[FunctionBlockMeta]:
        """按名称或描述搜索功能块"""
        q = query.lower()
        return [
            b for b in self._blocks.values()
            if q in b.name.lower() or (b.description and q in b.description.lower())
        ]
