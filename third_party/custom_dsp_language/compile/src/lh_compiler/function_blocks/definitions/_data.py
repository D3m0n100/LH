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
    """IntBuild - 整数数据构建"""
    return FunctionBlockMeta(
        name="IntBuild",
        type_id=701,
        memory_size=16,
        category="data",
        description="整数数据构建",
        parameters=[]
    )


def _define_intdirectbuild() -> FunctionBlockMeta:
    """IntDirectBuild - 直接整数构建"""
    return FunctionBlockMeta(
        name="IntDirectBuild",
        type_id=702,
        memory_size=16,
        category="data",
        description="直接整数构建",
        parameters=[]
    )


def _define_floatbuild() -> FunctionBlockMeta:
    """FloatBuild - 浮点数据构建"""
    return FunctionBlockMeta(
        name="FloatBuild",
        type_id=703,
        memory_size=16,
        category="data",
        description="浮点数据构建",
        parameters=[]
    )


def _define_floatconstbuild() -> FunctionBlockMeta:
    """FloatConstBuild - 浮点常量构建"""
    return FunctionBlockMeta(
        name="FloatConstBuild",
        type_id=704,
        memory_size=16,
        category="data",
        description="浮点常量构建",
        parameters=[]
    )


def _define_intcomp() -> FunctionBlockMeta:
    """IntComp - 整数比较"""
    return FunctionBlockMeta(
        name="IntComp",
        type_id=705,
        memory_size=16,
        category="data",
        description="整数比较运算",
        parameters=[]
    )


def _define_floatcomp() -> FunctionBlockMeta:
    """FloatComp - 浮点比较"""
    return FunctionBlockMeta(
        name="FloatComp",
        type_id=706,
        memory_size=16,
        category="data",
        description="浮点比较运算",
        parameters=[]
    )


def _define_intmax() -> FunctionBlockMeta:
    """IntMax - 整数最大值选择"""
    return FunctionBlockMeta(
        name="IntMax",
        type_id=707,
        memory_size=16,
        category="data",
        description="整数最大值选择",
        parameters=[]
    )


def _define_intmin() -> FunctionBlockMeta:
    """IntMin - 整数最小值选择"""
    return FunctionBlockMeta(
        name="IntMin",
        type_id=708,
        memory_size=16,
        category="data",
        description="整数最小值选择",
        parameters=[]
    )


def _define_floatmax() -> FunctionBlockMeta:
    """FloatMax - 浮点最大值选择"""
    return FunctionBlockMeta(
        name="FloatMax",
        type_id=709,
        memory_size=16,
        category="data",
        description="浮点最大值选择",
        parameters=[]
    )


def _define_floatmin() -> FunctionBlockMeta:
    """FloatMin - 浮点最小值选择"""
    return FunctionBlockMeta(
        name="FloatMin",
        type_id=710,
        memory_size=16,
        category="data",
        description="浮点最小值选择",
        parameters=[]
    )


def _define_intlimt() -> FunctionBlockMeta:
    """IntLimt - 整数限幅"""
    return FunctionBlockMeta(
        name="IntLimt",
        type_id=711,
        memory_size=16,
        category="data",
        description="整数限幅功能",
        parameters=[]
    )


def _define_floatlimt() -> FunctionBlockMeta:
    """FloatLimt - 浮点限幅"""
    return FunctionBlockMeta(
        name="FloatLimt",
        type_id=712,
        memory_size=16,
        category="data",
        description="浮点限幅功能",
        parameters=[]
    )


def _define_floatvarialimt() -> FunctionBlockMeta:
    """FloatVariLimt - 浮点可变限幅"""
    return FunctionBlockMeta(
        name="FloatVariLimt",
        type_id=713,
        memory_size=24,
        category="data",
        description="浮点可变限幅功能",
        parameters=[]
    )


def _define_intbuffoper() -> FunctionBlockMeta:
    """IntBuffOper - 整数缓冲区操作"""
    return FunctionBlockMeta(
        name="IntBuffOper",
        type_id=714,
        memory_size=32,
        category="data",
        description="整数缓冲区操作",
        parameters=[]
    )


def _define_floatbuffoper() -> FunctionBlockMeta:
    """FloatBuffOper - 浮点缓冲区操作"""
    return FunctionBlockMeta(
        name="FloatBuffOper",
        type_id=715,
        memory_size=32,
        category="data",
        description="浮点缓冲区操作",
        parameters=[]
    )


def _define_intbufflogicoper() -> FunctionBlockMeta:
    """IntBuffLogicOper - 整数逻辑缓冲区操作"""
    return FunctionBlockMeta(
        name="IntBuffLogicOper",
        type_id=716,
        memory_size=32,
        category="data",
        description="整数逻辑缓冲区操作",
        parameters=[]
    )


def _define_intmodoper() -> FunctionBlockMeta:
    """IntModOper - 整数模运算"""
    return FunctionBlockMeta(
        name="IntModOper",
        type_id=717,
        memory_size=16,
        category="data",
        description="整数模运算",
        parameters=[]
    )


def _define_floatroughfilter() -> FunctionBlockMeta:
    """FloatRoughFilter - 浮点粗滤波"""
    return FunctionBlockMeta(
        name="FloatRoughFilter",
        type_id=718,
        memory_size=24,
        category="data",
        description="浮点粗滤波",
        parameters=[]
    )


def _define_dataredundancy() -> FunctionBlockMeta:
    """DataRedundancy - 数据冗余处理"""
    return FunctionBlockMeta(
        name="DataRedundancy",
        type_id=719,
        memory_size=32,
        category="data",
        description="数据冗余处理",
        parameters=[]
    )


def _define_dealinunify() -> FunctionBlockMeta:
    """DealInUnify - 输入数据统一处理"""
    return FunctionBlockMeta(
        name="DealInUnify",
        type_id=720,
        memory_size=24,
        category="data",
        description="输入数据统一处理",
        parameters=[]
    )


def _define_dealoutunify() -> FunctionBlockMeta:
    """DealOutUnify - 输出数据统一处理"""
    return FunctionBlockMeta(
        name="DealOutUnify",
        type_id=721,
        memory_size=24,
        category="data",
        description="输出数据统一处理",
        parameters=[]
    )


def _define_dealtodata() -> FunctionBlockMeta:
    """DealToData - 切换到数据模式"""
    return FunctionBlockMeta(
        name="DealToData",
        type_id=722,
        memory_size=16,
        category="data",
        description="切换到数据模式",
        parameters=[]
    )


def _define_dealtoswit() -> FunctionBlockMeta:
    """DealToSwit - 切换到开关模式"""
    return FunctionBlockMeta(
        name="DealToSwit",
        type_id=723,
        memory_size=16,
        category="data",
        description="切换到开关模式",
        parameters=[]
    )
