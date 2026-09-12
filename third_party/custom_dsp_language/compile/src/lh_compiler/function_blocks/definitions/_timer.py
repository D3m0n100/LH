# -*- coding: utf-8 -*-
"""
TIMER 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_timer_blocks() -> List[FunctionBlockMeta]:
    """返回timer类别的功能块定义"""
    return [
        _define_timer(),
        _define_ton(),
        _define_tof(),
        _define_tp(),
        _define_timerstop(),
    ]


def _define_timer() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Timer",
        type_id=301,
        memory_size=24,
        category="timer",
        description="通用定时器",
        parameters=[]
    )


def _define_ton() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="TON",
        type_id=302,
        memory_size=24,
        category="timer",
        description="接通延时",
        parameters=[]
    )


def _define_tof() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="TOF",
        type_id=303,
        memory_size=24,
        category="timer",
        description="断开延时",
        parameters=[]
    )


def _define_tp() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="TP",
        type_id=304,
        memory_size=24,
        category="timer",
        description="脉冲定时器",
        parameters=[]
    )


def _define_timerstop() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="TimerStop",
        type_id=305,
        memory_size=16,
        category="timer",
        description="停止定时器",
        parameters=[]
    )
