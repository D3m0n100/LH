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
    return FunctionBlockMeta(
        name="Counter",
        type_id=311,
        memory_size=24,
        category="counter",
        description="通用计数器",
        parameters=[]
    )


def _define_ctu() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="CTU",
        type_id=312,
        memory_size=24,
        category="counter",
        description="加计数器",
        parameters=[]
    )


def _define_ctd() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="CTD",
        type_id=313,
        memory_size=24,
        category="counter",
        description="减计数器",
        parameters=[]
    )


def _define_ctud() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="CTUD",
        type_id=314,
        memory_size=32,
        category="counter",
        description="加减计数器",
        parameters=[]
    )


def _define_counterreset() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="CounterReset",
        type_id=315,
        memory_size=16,
        category="counter",
        description="复位计数器",
        parameters=[]
    )
