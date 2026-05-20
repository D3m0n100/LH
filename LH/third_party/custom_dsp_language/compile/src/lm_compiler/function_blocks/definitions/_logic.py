# -*- coding: utf-8 -*-
"""
LOGIC 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_logic_blocks() -> List[FunctionBlockMeta]:
    """返回logic类别的功能块定义"""
    return [
        _define_and(),
        _define_or(),
        _define_not(),
        _define_xor(),
        _define_shl(),
        _define_shr(),
        _define_rol(),
        _define_ror(),
    ]


def _define_and() -> FunctionBlockMeta:
    """
    _And - 逻辑与
    """
    return FunctionBlockMeta(
        name="_And",
        type_id=41400,
        memory_size=16,
        category="logic",
        description="逻辑与",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_or() -> FunctionBlockMeta:
    """
    _Or - 逻辑或
    """
    return FunctionBlockMeta(
        name="_Or",
        type_id=41401,
        memory_size=16,
        category="logic",
        description="逻辑或",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_not() -> FunctionBlockMeta:
    """
    _Not - 逻辑非
    """
    return FunctionBlockMeta(
        name="_Not",
        type_id=41402,
        memory_size=16,
        category="logic",
        description="逻辑非",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_xor() -> FunctionBlockMeta:
    """
    _Xor - 异或
    """
    return FunctionBlockMeta(
        name="_Xor",
        type_id=41403,
        memory_size=16,
        category="logic",
        description="异或",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_shl() -> FunctionBlockMeta:
    """
    _Shl - 左移
    """
    return FunctionBlockMeta(
        name="_Shl",
        type_id=41404,
        memory_size=16,
        category="logic",
        description="左移",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_shr() -> FunctionBlockMeta:
    """
    _Shr - 右移
    """
    return FunctionBlockMeta(
        name="_Shr",
        type_id=41405,
        memory_size=16,
        category="logic",
        description="右移",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_rol() -> FunctionBlockMeta:
    """
    _Rol - 循环左移
    """
    return FunctionBlockMeta(
        name="_Rol",
        type_id=41406,
        memory_size=16,
        category="logic",
        description="循环左移",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_ror() -> FunctionBlockMeta:
    """
    _Ror - 循环右移
    """
    return FunctionBlockMeta(
        name="_Ror",
        type_id=41407,
        memory_size=16,
        category="logic",
        description="循环右移",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


