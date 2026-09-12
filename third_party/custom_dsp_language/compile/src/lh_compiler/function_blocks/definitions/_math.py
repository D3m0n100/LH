# -*- coding: utf-8 -*-
"""
MATH 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_math_blocks() -> List[FunctionBlockMeta]:
    """返回math类别的功能块定义"""
    return [
        _define_add(),
        _define_sub(),
        _define_mul(),
        _define_div(),
        _define_mod(),
        _define_abs(),
        _define_sqrt(),
        _define_exp(),
        _define_ln(),
        _define_log(),
        _define_sin(),
        _define_cos(),
        _define_tan(),
        _define_limit(),
        _define_min(),
        _define_max(),
    ]


def _define_add() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Add",
        type_id=201,
        memory_size=16,
        category="math",
        description="加法",
        parameters=[]
    )


def _define_sub() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Sub",
        type_id=202,
        memory_size=16,
        category="math",
        description="减法",
        parameters=[]
    )


def _define_mul() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Mul",
        type_id=203,
        memory_size=16,
        category="math",
        description="乘法",
        parameters=[]
    )


def _define_div() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Div",
        type_id=204,
        memory_size=16,
        category="math",
        description="除法",
        parameters=[]
    )


def _define_mod() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Mod",
        type_id=205,
        memory_size=16,
        category="math",
        description="取模",
        parameters=[]
    )


def _define_abs() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Abs",
        type_id=206,
        memory_size=16,
        category="math",
        description="绝对值",
        parameters=[]
    )


def _define_sqrt() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Sqrt",
        type_id=207,
        memory_size=16,
        category="math",
        description="平方根",
        parameters=[]
    )


def _define_exp() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Exp",
        type_id=208,
        memory_size=16,
        category="math",
        description="指数",
        parameters=[]
    )


def _define_ln() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Ln",
        type_id=209,
        memory_size=16,
        category="math",
        description="自然对数",
        parameters=[]
    )


def _define_log() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Log",
        type_id=210,
        memory_size=16,
        category="math",
        description="常用对数",
        parameters=[]
    )


def _define_sin() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Sin",
        type_id=211,
        memory_size=16,
        category="math",
        description="正弦",
        parameters=[]
    )


def _define_cos() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Cos",
        type_id=212,
        memory_size=16,
        category="math",
        description="余弦",
        parameters=[]
    )


def _define_tan() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Tan",
        type_id=213,
        memory_size=16,
        category="math",
        description="正切",
        parameters=[]
    )


def _define_limit() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Limit",
        type_id=214,
        memory_size=24,
        category="math",
        description="限幅",
        parameters=[]
    )


def _define_min() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Min",
        type_id=215,
        memory_size=24,
        category="math",
        description="最小值",
        parameters=[]
    )


def _define_max() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Max",
        type_id=216,
        memory_size=24,
        category="math",
        description="最大值",
        parameters=[]
    )
