# -*- coding: utf-8 -*-
"""
COUNTER 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_counter_blocks() -> List[FunctionBlockMeta]:
    """返回counter类别的功能块定义"""
    return [
        _define_counter(),
        _define_ctu(),
        _define_ctd(),
        _define_ctud(),
        _define_counterreset(),
    ]


def _define_counter() -> FunctionBlockMeta:
    """
    _Counter - 通用计数器
    """
    return FunctionBlockMeta(
        name="_Counter",
        type_id=41700,
        memory_size=24,
        category="counter",
        description="通用计数器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_ctu() -> FunctionBlockMeta:
    """
    _CTU - 加计数器
    """
    return FunctionBlockMeta(
        name="_CTU",
        type_id=41701,
        memory_size=24,
        category="counter",
        description="加计数器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_ctd() -> FunctionBlockMeta:
    """
    _CTD - 减计数器
    """
    return FunctionBlockMeta(
        name="_CTD",
        type_id=41702,
        memory_size=24,
        category="counter",
        description="减计数器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_ctud() -> FunctionBlockMeta:
    """
    _CTUD - 加减计数器
    """
    return FunctionBlockMeta(
        name="_CTUD",
        type_id=41703,
        memory_size=32,
        category="counter",
        description="加减计数器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_counterreset() -> FunctionBlockMeta:
    """
    _CounterReset - 复位计数器
    """
    return FunctionBlockMeta(
        name="_CounterReset",
        type_id=41704,
        memory_size=16,
        category="counter",
        description="复位计数器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


