# -*- coding: utf-8 -*-
"""
DATA 数据处理功能块定义
用于数据构建、比较、最值选择、限幅、缓冲、滤波、数据统一等
"""

from typing import List
from ..registry import FunctionBlockMeta


def get_data_blocks() -> List[FunctionBlockMeta]:
    """返回所有数据处理功能块定义"""
    return [
        _define_intbuild(),
        _define_intdirectbuild(),
        _define_floatbuild(),
        _define_floatconstbuild(),
        _define_intcomp(),
        _define_floatcomp(),
        _define_intmax(),
        _define_intmin(),
        _define_floatmax(),
        _define_floatmin(),
        _define_intlimt(),
        _define_floatlimt(),
        _define_floatvarialimt(),
        _define_intbuffoper(),
        _define_floatbuffoper(),
        _define_intbufflogicoper(),
        _define_intmodoper(),
        _define_floatroughfilter(),
        _define_dataredundancy(),
        _define_dealinunify(),
        _define_dealoutunify(),
        _define_dealtodata(),
        _define_dealtoswit(),
    ]


def _define_intbuild() -> FunctionBlockMeta:
    """_IntBuild - 整数数据构建"""
    return FunctionBlockMeta(
        name="_IntBuild",
        type_id=42200,
        memory_size=16,
        category="data",
        description="整数数据构建",
        parameters=[]
    )


def _define_intdirectbuild() -> FunctionBlockMeta:
    """_IntDirectBuild - 直接整数构建"""
    return FunctionBlockMeta(
        name="_IntDirectBuild",
        type_id=42201,
        memory_size=16,
        category="data",
        description="直接整数构建",
        parameters=[]
    )


def _define_floatbuild() -> FunctionBlockMeta:
    """_FloatBuild - 浮点数据构建"""
    return FunctionBlockMeta(
        name="_FloatBuild",
        type_id=42202,
        memory_size=16,
        category="data",
        description="浮点数据构建",
        parameters=[]
    )


def _define_floatconstbuild() -> FunctionBlockMeta:
    """_FloatConstBuild - 浮点常量构建"""
    return FunctionBlockMeta(
        name="_FloatConstBuild",
        type_id=42203,
        memory_size=16,
        category="data",
        description="浮点常量构建",
        parameters=[]
    )


def _define_intcomp() -> FunctionBlockMeta:
    """_IntComp - 整数比较"""
    return FunctionBlockMeta(
        name="_IntComp",
        type_id=42204,
        memory_size=16,
        category="data",
        description="整数比较运算",
        parameters=[]
    )


def _define_floatcomp() -> FunctionBlockMeta:
    """_FloatComp - 浮点比较"""
    return FunctionBlockMeta(
        name="_FloatComp",
        type_id=42205,
        memory_size=16,
        category="data",
        description="浮点比较运算",
        parameters=[]
    )


def _define_intmax() -> FunctionBlockMeta:
    """_IntMax - 整数最大值选择"""
    return FunctionBlockMeta(
        name="_IntMax",
        type_id=42206,
        memory_size=16,
        category="data",
        description="整数最大值选择",
        parameters=[]
    )


def _define_intmin() -> FunctionBlockMeta:
    """_IntMin - 整数最小值选择"""
    return FunctionBlockMeta(
        name="_IntMin",
        type_id=42207,
        memory_size=16,
        category="data",
        description="整数最小值选择",
        parameters=[]
    )


def _define_floatmax() -> FunctionBlockMeta:
    """_FloatMax - 浮点最大值选择"""
    return FunctionBlockMeta(
        name="_FloatMax",
        type_id=42208,
        memory_size=16,
        category="data",
        description="浮点最大值选择",
        parameters=[]
    )


def _define_floatmin() -> FunctionBlockMeta:
    """_FloatMin - 浮点最小值选择"""
    return FunctionBlockMeta(
        name="_FloatMin",
        type_id=42209,
        memory_size=16,
        category="data",
        description="浮点最小值选择",
        parameters=[]
    )


def _define_intlimt() -> FunctionBlockMeta:
    """_IntLimt - 整数限幅"""
    return FunctionBlockMeta(
        name="_IntLimt",
        type_id=42210,
        memory_size=16,
        category="data",
        description="整数限幅功能",
        parameters=[]
    )


def _define_floatlimt() -> FunctionBlockMeta:
    """_FloatLimt - 浮点限幅"""
    return FunctionBlockMeta(
        name="_FloatLimt",
        type_id=42211,
        memory_size=16,
        category="data",
        description="浮点限幅功能",
        parameters=[]
    )


def _define_floatvarialimt() -> FunctionBlockMeta:
    """_FloatVariLimt - 浮点可变限幅"""
    return FunctionBlockMeta(
        name="_FloatVariLimt",
        type_id=42212,
        memory_size=24,
        category="data",
        description="浮点可变限幅功能",
        parameters=[]
    )


def _define_intbuffoper() -> FunctionBlockMeta:
    """_IntBuffOper - 整数缓冲区操作"""
    return FunctionBlockMeta(
        name="_IntBuffOper",
        type_id=42213,
        memory_size=32,
        category="data",
        description="整数缓冲区操作",
        parameters=[]
    )


def _define_floatbuffoper() -> FunctionBlockMeta:
    """_FloatBuffOper - 浮点缓冲区操作"""
    return FunctionBlockMeta(
        name="_FloatBuffOper",
        type_id=42214,
        memory_size=32,
        category="data",
        description="浮点缓冲区操作",
        parameters=[]
    )


def _define_intbufflogicoper() -> FunctionBlockMeta:
    """_IntBuffLogicOper - 整数逻辑缓冲区操作"""
    return FunctionBlockMeta(
        name="_IntBuffLogicOper",
        type_id=42215,
        memory_size=32,
        category="data",
        description="整数逻辑缓冲区操作",
        parameters=[]
    )


def _define_intmodoper() -> FunctionBlockMeta:
    """_IntModOper - 整数模运算"""
    return FunctionBlockMeta(
        name="_IntModOper",
        type_id=42216,
        memory_size=16,
        category="data",
        description="整数模运算",
        parameters=[]
    )


def _define_floatroughfilter() -> FunctionBlockMeta:
    """_FloatRoughFilter - 浮点粗滤波"""
    return FunctionBlockMeta(
        name="_FloatRoughFilter",
        type_id=42217,
        memory_size=24,
        category="data",
        description="浮点粗滤波",
        parameters=[]
    )


def _define_dataredundancy() -> FunctionBlockMeta:
    """_DataRedundancy - 数据冗余处理"""
    return FunctionBlockMeta(
        name="_DataRedundancy",
        type_id=42218,
        memory_size=32,
        category="data",
        description="数据冗余处理",
        parameters=[]
    )


def _define_dealinunify() -> FunctionBlockMeta:
    """_DealInUnify - 输入数据统一处理"""
    return FunctionBlockMeta(
        name="_DealInUnify",
        type_id=42219,
        memory_size=24,
        category="data",
        description="输入数据统一处理",
        parameters=[]
    )


def _define_dealoutunify() -> FunctionBlockMeta:
    """_DealOutUnify - 输出数据统一处理"""
    return FunctionBlockMeta(
        name="_DealOutUnify",
        type_id=42220,
        memory_size=24,
        category="data",
        description="输出数据统一处理",
        parameters=[]
    )


def _define_dealtodata() -> FunctionBlockMeta:
    """_DealToData - 切换到数据模式"""
    return FunctionBlockMeta(
        name="_DealToData",
        type_id=42221,
        memory_size=16,
        category="data",
        description="切换到数据模式",
        parameters=[]
    )


def _define_dealtoswit() -> FunctionBlockMeta:
    """_DealToSwit - 切换到开关模式"""
    return FunctionBlockMeta(
        name="_DealToSwit",
        type_id=42222,
        memory_size=16,
        category="data",
        description="切换到开关模式",
        parameters=[]
    )
