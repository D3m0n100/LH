# -*- coding: utf-8 -*-
"""
SYSTEM 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_system_blocks() -> List[FunctionBlockMeta]:
    """返回system类别的功能块定义"""
    return [
        _define_system(),
        _define_systemend(),
        _define_exception(),
        _define_nop(),
    ]


def _define_system() -> FunctionBlockMeta:
    """
    _System - 系统初始化
    """
    return FunctionBlockMeta(
        name="_System",
        type_id=40961,
        memory_size=38,
        category="system",
        description="系统初始化",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_systemend() -> FunctionBlockMeta:
    """
    _SystemEnd - 系统结束标志
    """
    return FunctionBlockMeta(
        name="_SystemEnd",
        type_id=40962,
        memory_size=8,
        category="system",
        description="系统结束标志",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_exception() -> FunctionBlockMeta:
    """
    _Exception - 异常处理
    """
    return FunctionBlockMeta(
        name="_Exception",
        type_id=40963,
        memory_size=16,
        category="system",
        description="异常处理",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_nop() -> FunctionBlockMeta:
    """
    _Nop - 空操作
    """
    return FunctionBlockMeta(
        name="_Nop",
        type_id=40964,
        memory_size=8,
        category="system",
        description="空操作",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


