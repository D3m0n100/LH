# -*- coding: utf-8 -*-
"""
比较功能块定义
包含：_GT / _LT / _LE / _GE / _EQ / _NE 等比较运算功能块
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
    """
    _GT - 大于比较 (Greater Than)
    Input1 > Input2 -> Output = TRUE
    """
    return FunctionBlockMeta(
        name="_GT",
        type_id=41300,
        memory_size=16,
        category="compare",
        description="大于比较：Input1 > Input2",
        parameters=[]
    )


def _define_lt() -> FunctionBlockMeta:
    """
    _LT - 小于比较 (Less Than)
    Input1 < Input2 -> Output = TRUE
    """
    return FunctionBlockMeta(
        name="_LT",
        type_id=41301,
        memory_size=16,
        category="compare",
        description="小于比较：Input1 < Input2",
        parameters=[]
    )


def _define_le() -> FunctionBlockMeta:
    """
    _LE - 小于等于比较 (Less or Equal)
    Input1 <= Input2 -> Output = TRUE
    """
    return FunctionBlockMeta(
        name="_LE",
        type_id=41302,
        memory_size=16,
        category="compare",
        description="小于等于比较：Input1 <= Input2",
        parameters=[]
    )


def _define_ge() -> FunctionBlockMeta:
    """
    _GE - 大于等于比较 (Greater or Equal)
    Input1 >= Input2 -> Output = TRUE
    """
    return FunctionBlockMeta(
        name="_GE",
        type_id=41303,
        memory_size=16,
        category="compare",
        description="大于等于比较：Input1 >= Input2",
        parameters=[]
    )


def _define_eq() -> FunctionBlockMeta:
    """
    _EQ - 等于比较 (Equal)
    Input1 = Input2 -> Output = TRUE
    """
    return FunctionBlockMeta(
        name="_EQ",
        type_id=41304,
        memory_size=16,
        category="compare",
        description="等于比较：Input1 = Input2",
        parameters=[]
    )


def _define_ne() -> FunctionBlockMeta:
    """
    _NE - 不等于比较 (Not Equal)
    Input1 <> Input2 -> Output = TRUE
    """
    return FunctionBlockMeta(
        name="_NE",
        type_id=41305,
        memory_size=16,
        category="compare",
        description="不等于比较：Input1 <> Input2",
        parameters=[]
    )
