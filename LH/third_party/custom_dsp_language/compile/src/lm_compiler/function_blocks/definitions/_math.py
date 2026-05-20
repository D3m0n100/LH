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
    """
    _Add - 加法
    """
    return FunctionBlockMeta(
        name="_Add",
        type_id=41300,
        memory_size=16,
        category="math",
        description="加法",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_sub() -> FunctionBlockMeta:
    """
    _Sub - 减法
    """
    return FunctionBlockMeta(
        name="_Sub",
        type_id=41301,
        memory_size=16,
        category="math",
        description="减法",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_mul() -> FunctionBlockMeta:
    """
    _Mul - 乘法
    """
    return FunctionBlockMeta(
        name="_Mul",
        type_id=41302,
        memory_size=16,
        category="math",
        description="乘法",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_div() -> FunctionBlockMeta:
    """
    _Div - 除法
    """
    return FunctionBlockMeta(
        name="_Div",
        type_id=41303,
        memory_size=16,
        category="math",
        description="除法",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_mod() -> FunctionBlockMeta:
    """
    _Mod - 取模
    """
    return FunctionBlockMeta(
        name="_Mod",
        type_id=41304,
        memory_size=16,
        category="math",
        description="取模",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_abs() -> FunctionBlockMeta:
    """
    _Abs - 绝对值
    """
    return FunctionBlockMeta(
        name="_Abs",
        type_id=41305,
        memory_size=16,
        category="math",
        description="绝对值",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_sqrt() -> FunctionBlockMeta:
    """
    _Sqrt - 平方根
    """
    return FunctionBlockMeta(
        name="_Sqrt",
        type_id=41306,
        memory_size=16,
        category="math",
        description="平方根",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_exp() -> FunctionBlockMeta:
    """
    _Exp - 指数
    """
    return FunctionBlockMeta(
        name="_Exp",
        type_id=41307,
        memory_size=16,
        category="math",
        description="指数",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_ln() -> FunctionBlockMeta:
    """
    _Ln - 自然对数
    """
    return FunctionBlockMeta(
        name="_Ln",
        type_id=41308,
        memory_size=16,
        category="math",
        description="自然对数",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_log() -> FunctionBlockMeta:
    """
    _Log - 常用对数
    """
    return FunctionBlockMeta(
        name="_Log",
        type_id=41309,
        memory_size=16,
        category="math",
        description="常用对数",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_sin() -> FunctionBlockMeta:
    """
    _Sin - 正弦
    """
    return FunctionBlockMeta(
        name="_Sin",
        type_id=41310,
        memory_size=16,
        category="math",
        description="正弦",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_cos() -> FunctionBlockMeta:
    """
    _Cos - 余弦
    """
    return FunctionBlockMeta(
        name="_Cos",
        type_id=41311,
        memory_size=16,
        category="math",
        description="余弦",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tan() -> FunctionBlockMeta:
    """
    _Tan - 正切
    """
    return FunctionBlockMeta(
        name="_Tan",
        type_id=41312,
        memory_size=16,
        category="math",
        description="正切",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_limit() -> FunctionBlockMeta:
    """
    _Limit - 限幅
    """
    return FunctionBlockMeta(
        name="_Limit",
        type_id=41313,
        memory_size=24,
        category="math",
        description="限幅",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_min() -> FunctionBlockMeta:
    """
    _Min - 最小值
    """
    return FunctionBlockMeta(
        name="_Min",
        type_id=41314,
        memory_size=24,
        category="math",
        description="最小值",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_max() -> FunctionBlockMeta:
    """
    _Max - 最大值
    """
    return FunctionBlockMeta(
        name="_Max",
        type_id=41315,
        memory_size=24,
        category="math",
        description="最大值",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


