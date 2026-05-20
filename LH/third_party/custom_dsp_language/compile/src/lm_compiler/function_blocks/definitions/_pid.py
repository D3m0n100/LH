# -*- coding: utf-8 -*-
"""
PID 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_pid_blocks() -> List[FunctionBlockMeta]:
    """返回pid类别的功能块定义"""
    return [
        _define_pid(),
        _define_pidautotune(),
    ]


def _define_pid() -> FunctionBlockMeta:
    """
    _PID - PID控制器
    """
    return FunctionBlockMeta(
        name="_PID",
        type_id=41800,
        memory_size=64,
        category="pid",
        description="PID控制器",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_pidautotune() -> FunctionBlockMeta:
    """
    _PIDAutoTune - 自整定PID
    """
    return FunctionBlockMeta(
        name="_PIDAutoTune",
        type_id=41801,
        memory_size=64,
        category="pid",
        description="自整定PID",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


