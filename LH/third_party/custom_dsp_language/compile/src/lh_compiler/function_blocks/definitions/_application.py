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
    return FunctionBlockMeta(
        name="EXCATMaintain",
        type_id=151,
        memory_size=32,
        category="application",
        description="保养功能",
        parameters=[]
    )


def _define_excaintcheck() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="EXCAIntCheck",
        type_id=152,
        memory_size=24,
        category="application",
        description="整型检查",
        parameters=[]
    )


def _define_passwordverify2() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="PasswordVerify2",
        type_id=153,
        memory_size=32,
        category="application",
        description="密码验证2",
        parameters=[]
    )


def _define_lockcmdgenery() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="LockCmdGenery",
        type_id=154,
        memory_size=24,
        category="application",
        description="锁定命令生成",
        parameters=[]
    )


def _define_outrealtime3() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="OutRealTime3",
        type_id=155,
        memory_size=32,
        category="application",
        description="实时输出3",
        parameters=[]
    )
