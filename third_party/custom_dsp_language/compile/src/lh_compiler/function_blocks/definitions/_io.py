# -*- coding: utf-8 -*-
"""
IO 功能块定义
修改内容：去掉下划线前缀、type_id改为三位数（401-449段）
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_io_blocks() -> List[FunctionBlockMeta]:
    """返回io类别的功能块定义"""
    return [
        _define_drvai(),
        _define_drvao(),
        _define_drvdi(),
        _define_drvdo(),
        _define_portset(),
        _define_portreset(),
        _define_porttoggle(),
        _define_drvt2periodin(),
        _define_drvtimein(),
    ]


def _define_drvai() -> FunctionBlockMeta:
    """DrvAI - 模拟量输入驱动，读取ADC通道数据"""
    return FunctionBlockMeta(
        name="DrvAI",
        type_id=401,
        memory_size=32,
        category="io",
        description="模拟量输入驱动",
        parameters=[
            ParameterDef(name="NumChannels",  data_type="INT", default_value=1),
            ParameterDef(name="InputNum",     data_type="INT", default_value=0),
            ParameterDef(name="DivisionNum",  data_type="INT", default_value=4096),
        ]
    )


def _define_drvao() -> FunctionBlockMeta:
    """DrvAO - 模拟量输出驱动，写入DAC/PWM通道"""
    return FunctionBlockMeta(
        name="DrvAO",
        type_id=402,
        memory_size=32,
        category="io",
        description="模拟量输出驱动",
        parameters=[
            ParameterDef(name="channel", data_type="INT",  default_value=0),
            ParameterDef(name="value",   data_type="REAL", default_value=0),
        ]
    )


def _define_drvdi() -> FunctionBlockMeta:
    """DrvDI - 开关量输入驱动，读取GPIO端口"""
    return FunctionBlockMeta(
        name="DrvDI",
        type_id=403,
        memory_size=16,
        category="io",
        description="开关量输入驱动",
        parameters=[
            ParameterDef(name="Port",   data_type="INT",  default_value=0),
            ParameterDef(name="Mask",   data_type="INT",  default_value=255),
            ParameterDef(name="Action", data_type="BOOL", default_value=1),
        ]
    )


def _define_drvdo() -> FunctionBlockMeta:
    """DrvDO - 开关量输出驱动，写入GPIO端口"""
    return FunctionBlockMeta(
        name="DrvDO",
        type_id=404,
        memory_size=16,
        category="io",
        description="开关量输出驱动",
        parameters=[
            ParameterDef(name="OutputWord", data_type="INT",  default_value=0),
            ParameterDef(name="Port",       data_type="INT",  default_value=0),
            ParameterDef(name="Mask",       data_type="INT",  default_value=255),
            ParameterDef(name="Action",     data_type="BOOL", default_value=1),
        ]
    )


def _define_portset() -> FunctionBlockMeta:
    """PortSet - GPIO端口置位"""
    return FunctionBlockMeta(
        name="PortSet",
        type_id=405,
        memory_size=16,
        category="io",
        description="端口置位",
        parameters=[
            ParameterDef(name="port", data_type="INT", default_value=0),
            ParameterDef(name="pin",  data_type="INT", default_value=0),
        ]
    )


def _define_portreset() -> FunctionBlockMeta:
    """PortReset - GPIO端口复位"""
    return FunctionBlockMeta(
        name="PortReset",
        type_id=406,
        memory_size=16,
        category="io",
        description="端口复位",
        parameters=[
            ParameterDef(name="port", data_type="INT", default_value=0),
            ParameterDef(name="pin",  data_type="INT", default_value=0),
        ]
    )


def _define_porttoggle() -> FunctionBlockMeta:
    """PortToggle - GPIO端口翻转"""
    return FunctionBlockMeta(
        name="PortToggle",
        type_id=407,
        memory_size=16,
        category="io",
        description="端口翻转",
        parameters=[
            ParameterDef(name="port", data_type="INT", default_value=0),
            ParameterDef(name="pin",  data_type="INT", default_value=0),
        ]
    )


def _define_drvt2periodin() -> FunctionBlockMeta:
    """DrvT2PeriodIn - 周期量输入（T2 CAP模块）"""
    return FunctionBlockMeta(
        name="DrvT2PeriodIn",
        type_id=408,
        memory_size=24,
        category="io",
        description="周期量输入",
        parameters=[
            ParameterDef(name="Channel", data_type="INT", default_value=0),
            ParameterDef(name="Mode",    data_type="INT", default_value=0),
            ParameterDef(name="Scale",   data_type="INT", default_value=1000),
            ParameterDef(name="Mask",    data_type="INT", default_value=65535),
        ]
    )


def _define_drvtimein() -> FunctionBlockMeta:
    """DrvTimeIn - 脉冲量输入（TIME模块）"""
    return FunctionBlockMeta(
        name="DrvTimeIn",
        type_id=409,
        memory_size=24,
        category="io",
        description="脉冲量输入",
        parameters=[
            ParameterDef(name="Channel", data_type="INT", default_value=0),
            ParameterDef(name="Mode",    data_type="INT", default_value=0),
            ParameterDef(name="Scale",   data_type="INT", default_value=1),
            ParameterDef(name="Mask",    data_type="INT", default_value=65535),
        ]
    )
