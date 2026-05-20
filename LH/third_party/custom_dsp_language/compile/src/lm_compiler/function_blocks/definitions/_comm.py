# -*- coding: utf-8 -*-
"""
COMM 通信功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_comm_blocks() -> List[FunctionBlockMeta]:
    """返回comm类别的功能块定义"""
    return [
        _define_commcaninit(),
        _define_commgprs(),
        _define_commj1939trans(),
        _define_commtrans(),
        _define_commwatch(),
    ]


def _define_commcaninit() -> FunctionBlockMeta:
    """
    _CommCANInit - CAN总线初始化
    """
    return FunctionBlockMeta(
        name="_CommCANInit",
        type_id=42200,
        memory_size=32,
        category="comm",
        description="CAN总线初始化",
        parameters=[]
    )


def _define_commgprs() -> FunctionBlockMeta:
    """
    _CommGPRS - GPRS远程通信
    """
    return FunctionBlockMeta(
        name="_CommGPRS",
        type_id=42201,
        memory_size=48,
        category="comm",
        description="GPRS远程通信",
        parameters=[]
    )


def _define_commj1939trans() -> FunctionBlockMeta:
    """
    _CommJ1939Trans - J1939车载网络传输
    """
    return FunctionBlockMeta(
        name="_CommJ1939Trans",
        type_id=42202,
        memory_size=32,
        category="comm",
        description="J1939车载网络传输",
        parameters=[]
    )


def _define_commtrans() -> FunctionBlockMeta:
    """
    _CommTrans - 通用通信传输
    """
    return FunctionBlockMeta(
        name="_CommTrans",
        type_id=42203,
        memory_size=32,
        category="comm",
        description="通用通信传输",
        parameters=[]
    )


def _define_commwatch() -> FunctionBlockMeta:
    """
    _CommWatch - 通信监视
    """
    return FunctionBlockMeta(
        name="_CommWatch",
        type_id=42204,
        memory_size=24,
        category="comm",
        description="通信链路监视",
        parameters=[]
    )
