# -*- coding: utf-8 -*-
"""
安全监控功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta


def get_safety_blocks() -> List[FunctionBlockMeta]:
    blocks = []
    blocks.append(_define_floatbelowwatch())
    blocks.append(_define_floatoverwatch())
    blocks.append(_define_passwordverify2())
    blocks.append(_define_excasfc())
    blocks.append(_define_excaintcheck())
    return blocks


def _define_floatbelowwatch() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="_FloatBelowWatch",
        type_id=42300,
        memory_size=24,
        category="safety",
        description="浮点数下限监视",
        parameters=[]
    )


def _define_floatoverwatch() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="_FloatOverWatch",
        type_id=42301,
        memory_size=24,
        category="safety",
        description="浮点数上限监视",
        parameters=[]
    )


def _define_passwordverify2() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="_PasswordVerify2",
        type_id=42302,
        memory_size=24,
        category="safety",
        description="密码验证（带重试次数限制）",
        parameters=[]
    )


def _define_excasfc() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="_EXCASFC",
        type_id=42303,
        memory_size=32,
        category="safety",
        description="工程机械安全功能控制",
        parameters=[]
    )


def _define_excaintcheck() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="_EXCAIntCheck",
        type_id=42304,
        memory_size=24,
        category="safety",
        description="工程机械完整性检查",
        parameters=[]
    )
