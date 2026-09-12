# -*- coding: utf-8 -*-
"""
比较功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta


def get_compare_blocks() -> List[FunctionBlockMeta]:
    """返回比较类别的功能块定义"""
    return [
        _define_gt(),
        _define_lt(),
        _define_le(),
        _define_ge(),
        _define_eq(),
        _define_ne(),
    ]


def _define_gt() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="GT",
        type_id=241,
        memory_size=16,
        category="compare",
        description="大于比较：Input1 > Input2",
        parameters=[]
    )


def _define_lt() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="LT",
        type_id=242,
        memory_size=16,
        category="compare",
        description="小于比较：Input1 < Input2",
        parameters=[]
    )


def _define_le() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="LE",
        type_id=243,
        memory_size=16,
        category="compare",
        description="小于等于比较：Input1 <= Input2",
        parameters=[]
    )


def _define_ge() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="GE",
        type_id=244,
        memory_size=16,
        category="compare",
        description="大于等于比较：Input1 >= Input2",
        parameters=[]
    )


def _define_eq() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="EQ",
        type_id=245,
        memory_size=16,
        category="compare",
        description="等于比较：Input1 = Input2",
        parameters=[]
    )


def _define_ne() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="NE",
        type_id=246,
        memory_size=16,
        category="compare",
        description="不等于比较：Input1 <> Input2",
        parameters=[]
    )
