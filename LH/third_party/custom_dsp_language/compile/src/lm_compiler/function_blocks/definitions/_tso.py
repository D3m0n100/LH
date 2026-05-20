# -*- coding: utf-8 -*-
"""
TSO 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_tso_blocks() -> List[FunctionBlockMeta]:
    """返回tso类别的功能块定义"""
    return [
        _define_tso(),
        _define_tsoautotune(),
        _define_twoposition(),
        _define_relayctrl2(),
    ]


def _define_tso() -> FunctionBlockMeta:
    """
    _TSO - TSO控制器
    """
    return FunctionBlockMeta(
        name="_TSO",
        type_id=41850,
        memory_size=64,
        category="tso",
        description="TSO控制器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tsoautotune() -> FunctionBlockMeta:
    """
    _TSOAutoTune - 自整定TSO
    """
    return FunctionBlockMeta(
        name="_TSOAutoTune",
        type_id=41851,
        memory_size=64,
        category="tso",
        description="自整定TSO",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_twoposition() -> FunctionBlockMeta:
    """
    _TwoPosition - 两位继电控制
    """
    return FunctionBlockMeta(
        name="_TwoPosition",
        type_id=41852,
        memory_size=32,
        category="tso",
        description="两位继电控制",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_relayctrl2() -> FunctionBlockMeta:
    """
    _RelayCtrl2 - 继电器控制2
    """
    return FunctionBlockMeta(
        name="_RelayCtrl2",
        type_id=41853,
        memory_size=32,
        category="tso",
        description="继电器控制2",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


