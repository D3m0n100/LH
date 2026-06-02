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
    return FunctionBlockMeta(
        name="And",
        type_id=271,
        memory_size=16,
        category="logic",
        description="逻辑与",
        parameters=[]
    )


def _define_or() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Or",
        type_id=272,
        memory_size=16,
        category="logic",
        description="逻辑或",
        parameters=[]
    )


def _define_not() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Not",
        type_id=273,
        memory_size=16,
        category="logic",
        description="逻辑非",
        parameters=[]
    )


def _define_xor() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Xor",
        type_id=274,
        memory_size=16,
        category="logic",
        description="异或",
        parameters=[]
    )


def _define_shl() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Shl",
        type_id=275,
        memory_size=16,
        category="logic",
        description="左移",
        parameters=[]
    )


def _define_shr() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Shr",
        type_id=276,
        memory_size=16,
        category="logic",
        description="右移",
        parameters=[]
    )


def _define_rol() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Rol",
        type_id=277,
        memory_size=16,
        category="logic",
        description="循环左移",
        parameters=[]
    )


def _define_ror() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Ror",
        type_id=278,
        memory_size=16,
        category="logic",
        description="循环右移",
        parameters=[]
    )
