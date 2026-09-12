# -*- coding: utf-8 -*-
"""
CONTROL 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_control_blocks() -> List[FunctionBlockMeta]:
    """返回control类别的功能块定义"""
    return [
        _define_switch1(),
        _define_switch2(),
        _define_switch3(),
        _define_switch4(),
        _define_switch5(),
        _define_switchcase(),
        _define_switchbreak(),
        _define_switchend(),
        _define_if1(),
        _define_if2(),
        _define_if3(),
        _define_ifelse(),
        _define_ifend(),
        _define_for(),
        _define_forend(),
        _define_while(),
        _define_whileend(),
        _define_loopbreak(),
        _define_loopcontinue(),
        _define_return(),
    ]


def _define_switch1() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Switch1",
        type_id=171,
        memory_size=16,
        category="control",
        description="单分支选择",
        parameters=[]
    )


def _define_switch2() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Switch2",
        type_id=172,
        memory_size=16,
        category="control",
        description="双分支选择",
        parameters=[]
    )


def _define_switch3() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Switch3",
        type_id=173,
        memory_size=16,
        category="control",
        description="三分支选择",
        parameters=[]
    )


def _define_switch4() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Switch4",
        type_id=174,
        memory_size=16,
        category="control",
        description="四分支选择",
        parameters=[]
    )


def _define_switch5() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Switch5",
        type_id=175,
        memory_size=16,
        category="control",
        description="五分支选择",
        parameters=[]
    )


def _define_switchcase() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="SwitchCase",
        type_id=176,
        memory_size=8,
        category="control",
        description="Switch分支标记",
        parameters=[]
    )


def _define_switchbreak() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="SwitchBreak",
        type_id=177,
        memory_size=8,
        category="control",
        description="退出Switch",
        parameters=[]
    )


def _define_switchend() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="SwitchEnd",
        type_id=178,
        memory_size=8,
        category="control",
        description="Switch结束标志",
        parameters=[]
    )


def _define_if1() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="If1",
        type_id=179,
        memory_size=16,
        category="control",
        description="单条件判断",
        parameters=[]
    )


def _define_if2() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="If2",
        type_id=180,
        memory_size=16,
        category="control",
        description="双条件判断",
        parameters=[]
    )


def _define_if3() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="If3",
        type_id=181,
        memory_size=16,
        category="control",
        description="三条件判断",
        parameters=[]
    )


def _define_ifelse() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="IfElse",
        type_id=182,
        memory_size=8,
        category="control",
        description="If分支Else标记",
        parameters=[]
    )


def _define_ifend() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="IfEnd",
        type_id=183,
        memory_size=8,
        category="control",
        description="条件判断结束",
        parameters=[]
    )


def _define_for() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="For",
        type_id=184,
        memory_size=24,
        category="control",
        description="固定次数循环",
        parameters=[]
    )


def _define_forend() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="ForEnd",
        type_id=185,
        memory_size=8,
        category="control",
        description="For循环结束",
        parameters=[]
    )


def _define_while() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="While",
        type_id=186,
        memory_size=16,
        category="control",
        description="条件循环",
        parameters=[]
    )


def _define_whileend() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="WhileEnd",
        type_id=187,
        memory_size=8,
        category="control",
        description="While循环结束",
        parameters=[]
    )


def _define_loopbreak() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="LoopBreak",
        type_id=188,
        memory_size=8,
        category="control",
        description="退出循环",
        parameters=[]
    )


def _define_loopcontinue() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="LoopContinue",
        type_id=189,
        memory_size=8,
        category="control",
        description="继续下次循环",
        parameters=[]
    )


def _define_return() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="Return",
        type_id=190,
        memory_size=8,
        category="control",
        description="返回",
        parameters=[]
    )
