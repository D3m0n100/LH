# -*- coding: utf-8 -*-
"""
安全监控功能块定义

注意：原 _PasswordVerify2 与 _EXCAIntCheck 已迁移至 _application.py，
此处不再保留，避免功能块名称重复。
"""

from typing import List
from ..registry import FunctionBlockMeta


def get_safety_blocks() -> List[FunctionBlockMeta]:
    blocks = []
    blocks.append(_define_floatbelowwatch())
    blocks.append(_define_floatoverwatch())
    blocks.append(_define_excasfc())
    return blocks


def _define_floatbelowwatch() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="FloatBelowWatch",
        type_id=781,
        memory_size=24,
        category="safety",
        description="浮点数下限监视",
        parameters=[]
    )


def _define_floatoverwatch() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="FloatOverWatch",
        type_id=782,
        memory_size=24,
        category="safety",
        description="浮点数上限监视",
        parameters=[]
    )


def _define_excasfc() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="EXCASFC",
        type_id=783,
        memory_size=32,
        category="safety",
        description="工程机械安全功能控制",
        parameters=[]
    )
