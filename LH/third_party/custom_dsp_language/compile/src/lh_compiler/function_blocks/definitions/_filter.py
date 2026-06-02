# -*- coding: utf-8 -*-
"""
FILTER 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_filter_blocks() -> List[FunctionBlockMeta]:
    """返回filter类别的功能块定义"""
    return [
        _define_filterbw(),
    ]


def _define_filterbw() -> FunctionBlockMeta:
    """
    FilterBW - 巴特沃斯滤波器
    """
    return FunctionBlockMeta(
        name="FilterBW",
        type_id=851,
        memory_size=48,
        category="filter",
        description="巴特沃斯滤波器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )
