# -*- coding: utf-8 -*-
"""
DISPLAY 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_display_blocks() -> List[FunctionBlockMeta]:
    """返回display类别的功能块定义"""
    return [
        _define_keyscan(),
        _define_scidisptrans(),
        _define_scidispinit(),
        _define_m600textdisp(),
        _define_m600progressbar(),
        _define_excacycdisp(),
    ]


def _define_keyscan() -> FunctionBlockMeta:
    """
    KeyScan - 按键扫描
    """
    return FunctionBlockMeta(
        name="KeyScan",
        type_id=801,
        memory_size=16,
        category="display",
        description="按键扫描",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_scidisptrans() -> FunctionBlockMeta:
    """
    SCIDispTrans - 串口显示发送
    """
    return FunctionBlockMeta(
        name="SCIDispTrans",
        type_id=802,
        memory_size=16,
        category="display",
        description="串口显示发送",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_scidispinit() -> FunctionBlockMeta:
    """
    SCIDispInit - 串口显示初始化
    """
    return FunctionBlockMeta(
        name="SCIDispInit",
        type_id=803,
        memory_size=24,
        category="display",
        description="串口显示初始化",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_m600textdisp() -> FunctionBlockMeta:
    """
    M600TextDisp - M600文本显示
    """
    return FunctionBlockMeta(
        name="M600TextDisp",
        type_id=804,
        memory_size=64,
        category="display",
        description="M600文本显示",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_m600progressbar() -> FunctionBlockMeta:
    """
    M600ProgressBar - M600进度条
    """
    return FunctionBlockMeta(
        name="M600ProgressBar",
        type_id=805,
        memory_size=64,
        category="display",
        description="M600进度条",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_excacycdisp() -> FunctionBlockMeta:
    """
    EXCACycDisp - EXCA循环显示
    """
    return FunctionBlockMeta(
        name="EXCACycDisp",
        type_id=806,
        memory_size=32,
        category="display",
        description="EXCA循环显示",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )
