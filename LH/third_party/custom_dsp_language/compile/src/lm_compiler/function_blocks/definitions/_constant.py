# -*- coding: utf-8 -*-
"""
CONSTANT 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_constant_blocks() -> List[FunctionBlockMeta]:
    """返回constant类别的功能块定义"""
    return [
        _define_intconstbuild(),
        _define_realconstbuild(),
        _define_boolconstbuild(),
    ]


def _define_intconstbuild() -> FunctionBlockMeta:
    """
    _IntConstBuild - 整数常量
    """
    return FunctionBlockMeta(
        name="_IntConstBuild",
        type_id=41029,
        memory_size=16,
        category="constant",
        description="整数常量",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_realconstbuild() -> FunctionBlockMeta:
    """
    _RealConstBuild - 浮点常量
    """
    return FunctionBlockMeta(
        name="_RealConstBuild",
        type_id=41030,
        memory_size=16,
        category="constant",
        description="浮点常量",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_boolconstbuild() -> FunctionBlockMeta:
    """
    _BoolConstBuild - 布尔常量
    """
    return FunctionBlockMeta(
        name="_BoolConstBuild",
        type_id=41031,
        memory_size=8,
        category="constant",
        description="布尔常量",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


