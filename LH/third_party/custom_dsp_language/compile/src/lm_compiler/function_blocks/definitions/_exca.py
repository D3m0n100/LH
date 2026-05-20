# -*- coding: utf-8 -*-
"""
EXCA（工程机械）专用功能块定义
适用于挖掘机等工程机械控制场景
"""

from typing import List
from ..registry import FunctionBlockMeta


def get_exca_blocks() -> List[FunctionBlockMeta]:
    """返回所有 EXCA 功能块定义"""
    return [
        _define_excakeyparamt(),
        _define_excamodifyparamt(),
        _define_excaopercomp(),
        _define_excaspdunify(),
        _define_excatmaintain(),
    ]


def _define_excakeyparamt() -> FunctionBlockMeta:
    """
    _EXCAKeyParamt - 工程机械关键参数管理
    管理发动机转速、液压压力、油温、燃油液位、电池电压等关键参数
    """
    return FunctionBlockMeta(
        name="_EXCAKeyParamt",
        type_id=41209,
        memory_size=32,
        category="exca",
        description="工程机械关键参数管理",
        parameters=[]
    )


def _define_excamodifyparamt() -> FunctionBlockMeta:
    """
    _EXCAModifyParamt - 工程机械参数修改
    负责运行时参数的动态修改与更新
    """
    return FunctionBlockMeta(
        name="_EXCAModifyParamt",
        type_id=41210,
        memory_size=32,
        category="exca",
        description="工程机械参数修改",
        parameters=[]
    )


def _define_excaopercomp() -> FunctionBlockMeta:
    """
    _EXCAOperComp - 工程机械操作补偿
    负责负载补偿、温度补偿、磨损补偿、海拔补偿等
    """
    return FunctionBlockMeta(
        name="_EXCAOperComp",
        type_id=41211,
        memory_size=32,
        category="exca",
        description="工程机械操作补偿",
        parameters=[]
    )


def _define_excaspdunify() -> FunctionBlockMeta:
    """
    _EXCASpdUnify - 工程机械速度统一
    负责各执行机构（斗杆/动臂/铲斗/回转/行走）速度的统一协调
    """
    return FunctionBlockMeta(
        name="_EXCASpdUnify",
        type_id=41212,
        memory_size=32,
        category="exca",
        description="工程机械速度统一协调",
        parameters=[]
    )


def _define_excatmaintain() -> FunctionBlockMeta:
    """
    _EXCATMaintain - 工程机械定时维护管理
    负责工作小时统计、维护提醒和维护周期管理
    """
    return FunctionBlockMeta(
        name="_EXCATMaintain",
        type_id=41213,
        memory_size=24,
        category="exca",
        description="工程机械定时维护管理",
        parameters=[]
    )
