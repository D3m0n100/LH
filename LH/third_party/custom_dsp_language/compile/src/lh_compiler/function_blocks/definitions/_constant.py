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
    return FunctionBlockMeta(
        name="IntConstBuild",
        type_id=121,
        memory_size=16,
        category="constant",
        description="整数常量",
        parameters=[]
    )


def _define_realconstbuild() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="RealConstBuild",
        type_id=122,
        memory_size=16,
        category="constant",
        description="浮点常量",
        parameters=[]
    )


def _define_boolconstbuild() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="BoolConstBuild",
        type_id=123,
        memory_size=8,
        category="constant",
        description="布尔常量",
        parameters=[]
    )
