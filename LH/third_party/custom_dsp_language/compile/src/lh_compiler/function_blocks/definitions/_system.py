# -*- coding: utf-8 -*-
"""
SYSTEM 功能块定义
修改内容：去掉下划线前缀、type_id改为三位数、补齐参数定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_system_blocks() -> List[FunctionBlockMeta]:
    """返回system类别的功能块定义"""
    return [
        _define_system(),
        _define_systemend(),
        _define_exception(),
        _define_nop(),
    ]


def _define_system() -> FunctionBlockMeta:
    """
    System - 系统初始化与全局配置
    每个LH程序的第一条指令，配置控制周期、版本等全局参数
    """
    return FunctionBlockMeta(
        name="System",
        type_id=101,
        memory_size=38,
        category="system",
        description="系统初始化与全局配置",
        parameters=[
            ParameterDef(name="Author",    data_type="INT",  default_value=0),
            ParameterDef(name="Config",    data_type="INT",  default_value=0),
            ParameterDef(name="Date",      data_type="INT",  default_value=0),
            ParameterDef(name="Version",   data_type="INT",  default_value=1),
            ParameterDef(name="CycleTime", data_type="INT",  default_value=1000),
        ]
    )


def _define_systemend() -> FunctionBlockMeta:
    """
    SystemEnd - 系统结束标志
    标记一个控制周期的终点，运行时解释器据此完成周期同步
    """
    return FunctionBlockMeta(
        name="SystemEnd",
        type_id=102,
        memory_size=8,
        category="system",
        description="系统结束标志",
        parameters=[
            ParameterDef(name="Status", data_type="INT", default_value=0),
        ]
    )


def _define_exception() -> FunctionBlockMeta:
    """
    Exception - 异常处理
    捕获并记录运行时异常，根据Action参数决定处理方式
    """
    return FunctionBlockMeta(
        name="Exception",
        type_id=103,
        memory_size=16,
        category="system",
        description="异常处理",
        parameters=[
            ParameterDef(name="Code",   data_type="INT",  default_value=0),
            ParameterDef(name="Action", data_type="INT",  default_value=0),
            ParameterDef(name="Enable", data_type="BOOL", default_value=1),
        ]
    )


def _define_nop() -> FunctionBlockMeta:
    """
    Nop - 空操作
    用于占位或时序对齐，不执行任何控制逻辑
    """
    return FunctionBlockMeta(
        name="Nop",
        type_id=104,
        memory_size=8,
        category="system",
        description="空操作",
        parameters=[]
    )
