# -*- coding: utf-8 -*-
"""
EXCA（工程机械）专用功能块定义
适用于挖掘机等工程机械控制场景

注意：原 _EXCATMaintain 已迁移至 _application.py，
此处不再保留，避免功能块名称重复。
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
    ]


def _define_excakeyparamt() -> FunctionBlockMeta:
    """
    EXCAKeyParamt - 工程机械关键参数管理
    管理发动机转速、液压压力、油温、燃油液位、电池电压等关键参数
    """
    return FunctionBlockMeta(
        name="EXCAKeyParamt",
        type_id=831,
        memory_size=32,
        category="exca",
        description="工程机械关键参数管理",
        parameters=[]
    )


def _define_excamodifyparamt() -> FunctionBlockMeta:
    """
    EXCAModifyParamt - 工程机械参数修改
    负责运行时参数的动态修改与更新
    """
    return FunctionBlockMeta(
        name="EXCAModifyParamt",
        type_id=832,
        memory_size=32,
        category="exca",
        description="工程机械参数修改",
        parameters=[]
    )


def _define_excaopercomp() -> FunctionBlockMeta:
    """
    EXCAOperComp - 工程机械操作补偿
    负责负载补偿、温度补偿、磨损补偿、海拔补偿等
    """
    return FunctionBlockMeta(
        name="EXCAOperComp",
        type_id=833,
        memory_size=32,
        category="exca",
        description="工程机械操作补偿",
        parameters=[]
    )


def _define_excaspdunify() -> FunctionBlockMeta:
    """
    EXCASpdUnify - 工程机械速度统一
    负责各执行机构（斗杆/动臂/铲斗/回转/行走）速度的统一协调
    """
    return FunctionBlockMeta(
        name="EXCASpdUnify",
        type_id=834,
        memory_size=32,
        category="exca",
        description="工程机械速度统一协调",
        parameters=[]
    )
