# -*- coding: utf-8 -*-
"""
PARAM 参数引擎功能块定义
用于工程参数管理与引擎构建
"""

from typing import List
from ..registry import FunctionBlockMeta


def get_param_blocks() -> List[FunctionBlockMeta]:
    """返回所有参数引擎功能块定义"""
    return [
        _define_paramtengbuild(),
    ]


def _define_paramtengbuild() -> FunctionBlockMeta:
    """_ParamtEngBuild - 工程参数引擎构建"""
    return FunctionBlockMeta(
        name="_ParamtEngBuild",
        type_id=42400,
        memory_size=32,
        category="param",
        description="工程参数引擎构建",
        parameters=[]
    )
