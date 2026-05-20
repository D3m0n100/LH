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
    """
    _Timer - 通用定时器
    """
    return FunctionBlockMeta(
        name="_Timer",
        type_id=41600,
        memory_size=24,
        category="timer",
        description="通用定时器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_ton() -> FunctionBlockMeta:
    """
    _TON - 接通延时
    """
    return FunctionBlockMeta(
        name="_TON",
        type_id=41601,
        memory_size=24,
        category="timer",
        description="接通延时",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tof() -> FunctionBlockMeta:
    """
    _TOF - 断开延时
    """
    return FunctionBlockMeta(
        name="_TOF",
        type_id=41602,
        memory_size=24,
        category="timer",
        description="断开延时",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tp() -> FunctionBlockMeta:
    """
    _TP - 脉冲定时器
    """
    return FunctionBlockMeta(
        name="_TP",
        type_id=41603,
        memory_size=24,
        category="timer",
        description="脉冲定时器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_timerstop() -> FunctionBlockMeta:
    """
    _TimerStop - 停止定时器
    """
    return FunctionBlockMeta(
        name="_TimerStop",
        type_id=41604,
        memory_size=16,
        category="timer",
        description="停止定时器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


