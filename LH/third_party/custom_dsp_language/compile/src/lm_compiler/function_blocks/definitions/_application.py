# -*- coding: utf-8 -*-
"""
APPLICATION 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_application_blocks() -> List[FunctionBlockMeta]:
    """返回application类别的功能块定义"""
    return [
        _define_excatmaintain(),
        _define_excaintcheck(),
        _define_passwordverify2(),
        _define_lockcmdgenery(),
        _define_outrealtime3(),
    ]


def _define_excatmaintain() -> FunctionBlockMeta:
    """
    _EXCATMaintain - 保养功能
    """
    return FunctionBlockMeta(
        name="_EXCATMaintain",
        type_id=42100,
        memory_size=32,
        category="application",
        description="保养功能",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_excaintcheck() -> FunctionBlockMeta:
    """
    _EXCAIntCheck - 整型检查
    """
    return FunctionBlockMeta(
        name="_EXCAIntCheck",
        type_id=42101,
        memory_size=24,
        category="application",
        description="整型检查",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_passwordverify2() -> FunctionBlockMeta:
    """
    _PasswordVerify2 - 密码验证2
    """
    return FunctionBlockMeta(
        name="_PasswordVerify2",
        type_id=42102,
        memory_size=32,
        category="application",
        description="密码验证2",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_lockcmdgenery() -> FunctionBlockMeta:
    """
    _LockCmdGenery - 锁定命令生成
    """
    return FunctionBlockMeta(
        name="_LockCmdGenery",
        type_id=42103,
        memory_size=24,
        category="application",
        description="锁定命令生成",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_outrealtime3() -> FunctionBlockMeta:
    """
    _OutRealTime3 - 实时输出3
    """
    return FunctionBlockMeta(
        name="_OutRealTime3",
        type_id=42104,
        memory_size=32,
        category="application",
        description="实时输出3",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


