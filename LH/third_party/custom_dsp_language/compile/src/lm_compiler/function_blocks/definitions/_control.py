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
    """
    _Switch1 - 单分支选择
    """
    return FunctionBlockMeta(
        name="_Switch1",
        type_id=41100,
        memory_size=16,
        category="control",
        description="单分支选择",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_switch2() -> FunctionBlockMeta:
    """
    _Switch2 - 双分支选择
    """
    return FunctionBlockMeta(
        name="_Switch2",
        type_id=41101,
        memory_size=16,
        category="control",
        description="双分支选择",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_switch3() -> FunctionBlockMeta:
    """
    _Switch3 - 三分支选择
    """
    return FunctionBlockMeta(
        name="_Switch3",
        type_id=41102,
        memory_size=16,
        category="control",
        description="三分支选择",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_switch4() -> FunctionBlockMeta:
    """
    _Switch4 - 四分支选择
    """
    return FunctionBlockMeta(
        name="_Switch4",
        type_id=41103,
        memory_size=16,
        category="control",
        description="四分支选择",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_switch5() -> FunctionBlockMeta:
    """
    _Switch5 - 五分支选择
    """
    return FunctionBlockMeta(
        name="_Switch5",
        type_id=41104,
        memory_size=16,
        category="control",
        description="五分支选择",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_switchcase() -> FunctionBlockMeta:
    """
    _SwitchCase - Switch分支标记
    """
    return FunctionBlockMeta(
        name="_SwitchCase",
        type_id=41105,
        memory_size=8,
        category="control",
        description="Switch分支标记",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_switchbreak() -> FunctionBlockMeta:
    """
    _SwitchBreak - 退出Switch
    """
    return FunctionBlockMeta(
        name="_SwitchBreak",
        type_id=41106,
        memory_size=8,
        category="control",
        description="退出Switch",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_switchend() -> FunctionBlockMeta:
    """
    _SwitchEnd - Switch结束标志
    """
    return FunctionBlockMeta(
        name="_SwitchEnd",
        type_id=41107,
        memory_size=8,
        category="control",
        description="Switch结束标志",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_if1() -> FunctionBlockMeta:
    """
    _If1 - 单条件判断
    """
    return FunctionBlockMeta(
        name="_If1",
        type_id=41110,
        memory_size=16,
        category="control",
        description="单条件判断",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_if2() -> FunctionBlockMeta:
    """
    _If2 - 双条件判断
    """
    return FunctionBlockMeta(
        name="_If2",
        type_id=41111,
        memory_size=16,
        category="control",
        description="双条件判断",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_if3() -> FunctionBlockMeta:
    """
    _If3 - 三条件判断
    """
    return FunctionBlockMeta(
        name="_If3",
        type_id=41112,
        memory_size=16,
        category="control",
        description="三条件判断",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_ifelse() -> FunctionBlockMeta:
    """
    _IfElse - If分支Else标记
    """
    return FunctionBlockMeta(
        name="_IfElse",
        type_id=41113,
        memory_size=8,
        category="control",
        description="If分支Else标记",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_ifend() -> FunctionBlockMeta:
    """
    _IfEnd - 条件判断结束
    """
    return FunctionBlockMeta(
        name="_IfEnd",
        type_id=41114,
        memory_size=8,
        category="control",
        description="条件判断结束",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_for() -> FunctionBlockMeta:
    """
    _For - 固定次数循环
    """
    return FunctionBlockMeta(
        name="_For",
        type_id=41120,
        memory_size=24,
        category="control",
        description="固定次数循环",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_forend() -> FunctionBlockMeta:
    """
    _ForEnd - For循环结束
    """
    return FunctionBlockMeta(
        name="_ForEnd",
        type_id=41121,
        memory_size=8,
        category="control",
        description="For循环结束",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_while() -> FunctionBlockMeta:
    """
    _While - 条件循环
    """
    return FunctionBlockMeta(
        name="_While",
        type_id=41122,
        memory_size=16,
        category="control",
        description="条件循环",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_whileend() -> FunctionBlockMeta:
    """
    _WhileEnd - While循环结束
    """
    return FunctionBlockMeta(
        name="_WhileEnd",
        type_id=41123,
        memory_size=8,
        category="control",
        description="While循环结束",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_loopbreak() -> FunctionBlockMeta:
    """
    _LoopBreak - 退出循环
    """
    return FunctionBlockMeta(
        name="_LoopBreak",
        type_id=41124,
        memory_size=8,
        category="control",
        description="退出循环",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_loopcontinue() -> FunctionBlockMeta:
    """
    _LoopContinue - 继续下次循环
    """
    return FunctionBlockMeta(
        name="_LoopContinue",
        type_id=41125,
        memory_size=8,
        category="control",
        description="继续下次循环",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_return() -> FunctionBlockMeta:
    """
    _Return - 返回
    """
    return FunctionBlockMeta(
        name="_Return",
        type_id=41126,
        memory_size=8,
        category="control",
        description="返回",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


