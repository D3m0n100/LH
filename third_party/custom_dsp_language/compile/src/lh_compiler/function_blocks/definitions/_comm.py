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
    return FunctionBlockMeta(
        name="CommCANInit",
        type_id=601,
        memory_size=32,
        category="comm",
        description="CAN总线初始化",
        parameters=[]
    )


def _define_commgprs() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="CommGPRS",
        type_id=602,
        memory_size=48,
        category="comm",
        description="GPRS远程通信",
        parameters=[]
    )


def _define_commj1939trans() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="CommJ1939Trans",
        type_id=603,
        memory_size=32,
        category="comm",
        description="J1939车载网络传输",
        parameters=[]
    )


def _define_commtrans() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="CommTrans",
        type_id=604,
        memory_size=32,
        category="comm",
        description="通用通信传输",
        parameters=[]
    )


def _define_commwatch() -> FunctionBlockMeta:
    return FunctionBlockMeta(
        name="CommWatch",
        type_id=605,
        memory_size=24,
        category="comm",
        description="通信链路监视",
        parameters=[]
    )
