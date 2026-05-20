# -*- coding: utf-8 -*-
"""
IO 功能块定义
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
        _define_drvt1pwmao(),
        _define_portset(),
        _define_portreset(),
        _define_porttoggle(),
        _define_drvt2pulsein(),
        _define_drvt2periodin(),
        _define_drvqepin(),
        _define_drvtimein(),
    ]


def _define_drvai() -> FunctionBlockMeta:
    """_DrvAI - 模拟量输入"""
    return FunctionBlockMeta(
        name="_DrvAI",
        type_id=41200,
        memory_size=32,
        category="io",
        description="模拟量输入",
        parameters=[
            ParameterDef(name="NumChannels",  data_type="INT", default_value=1),
            ParameterDef(name="InputNum",     data_type="INT", default_value=0),
            ParameterDef(name="DivisionNum",  data_type="INT", default_value=4096),
        ]
    )


def _define_drvao() -> FunctionBlockMeta:
    """_DrvAO - 模拟量输出"""
    return FunctionBlockMeta(
        name="_DrvAO",
        type_id=41201,
        memory_size=32,
        category="io",
        description="模拟量输出",
        parameters=[
            ParameterDef(name="channel", data_type="INT",  default_value=0),
            ParameterDef(name="value",   data_type="REAL", default_value=0),
        ]
    )


def _define_drvdi() -> FunctionBlockMeta:
    """_DrvDI - 开关量输入"""
    return FunctionBlockMeta(
        name="_DrvDI",
        type_id=41202,
        memory_size=16,
        category="io",
        description="开关量输入",
        parameters=[
            ParameterDef(name="Port",   data_type="INT",  default_value=0),
            ParameterDef(name="Mask",   data_type="INT",  default_value=255),
            ParameterDef(name="Action", data_type="BOOL", default_value=1),
        ]
    )


def _define_drvdo() -> FunctionBlockMeta:
    """_DrvDO - 开关量输出"""
    return FunctionBlockMeta(
        name="_DrvDO",
        type_id=41203,
        memory_size=16,
        category="io",
        description="开关量输出",
        parameters=[
            ParameterDef(name="OutputWord", data_type="INT",  default_value=0),
            ParameterDef(name="Port",       data_type="INT",  default_value=0),
            ParameterDef(name="Mask",       data_type="INT",  default_value=255),
            ParameterDef(name="Action",     data_type="BOOL", default_value=1),
        ]
    )


def _define_drvt1pwmao() -> FunctionBlockMeta:
    """_DrvT1PWMAO - PWM输出（T1模块）"""
    return FunctionBlockMeta(
        name="_DrvT1PWMAO",
        type_id=41209,
        memory_size=24,
        category="io",
        description="PWM输出（T1模块）",
        parameters=[
            ParameterDef(name="Channel", data_type="INT", default_value=0),
            ParameterDef(name="Period", data_type="INT", default_value=1000),
            ParameterDef(name="Duty", data_type="INT", default_value=50),
        ]
    )


def _define_portset() -> FunctionBlockMeta:
    """_PortSet - 端口置位"""
    return FunctionBlockMeta(
        name="_PortSet",
        type_id=41204,
        memory_size=16,
        category="io",
        description="端口置位",
        parameters=[
            ParameterDef(name="port", data_type="INT", default_value=0),
            ParameterDef(name="pin",  data_type="INT", default_value=0),
        ]
    )


def _define_portreset() -> FunctionBlockMeta:
    """_PortReset - 端口复位"""
    return FunctionBlockMeta(
        name="_PortReset",
        type_id=41205,
        memory_size=16,
        category="io",
        description="端口复位",
        parameters=[
            ParameterDef(name="port", data_type="INT", default_value=0),
            ParameterDef(name="pin",  data_type="INT", default_value=0),
        ]
    )


def _define_porttoggle() -> FunctionBlockMeta:
    """_PortToggle - 端口翻转"""
    return FunctionBlockMeta(
        name="_PortToggle",
        type_id=41206,
        memory_size=16,
        category="io",
        description="端口翻转",
        parameters=[
            ParameterDef(name="port", data_type="INT", default_value=0),
            ParameterDef(name="pin",  data_type="INT", default_value=0),
        ]
    )


def _define_drvt2periodin() -> FunctionBlockMeta:
    """_DrvT2PeriodIn - 周期量输入（T2 CAP模块）"""
    return FunctionBlockMeta(
        name="_DrvT2PeriodIn",
        type_id=41207,
        memory_size=24,
        category="io",
        description="周期量输入（T2 CAP模块）",
        parameters=[
            ParameterDef(name="Channel", data_type="INT", default_value=0),
            ParameterDef(name="Mode",    data_type="INT", default_value=0),
            ParameterDef(name="Scale",   data_type="INT", default_value=1000),
            ParameterDef(name="Mask",    data_type="INT", default_value=65535),
        ]
    )


def _define_drvtimein() -> FunctionBlockMeta:
    """_DrvTimeIn - 脉冲量输入（TIME模块）"""
    return FunctionBlockMeta(
        name="_DrvTimeIn",
        type_id=41208,
        memory_size=24,
        category="io",
        description="脉冲量输入（TIME模块）",
        parameters=[
            ParameterDef(name="Channel", data_type="INT", default_value=0),
            ParameterDef(name="Mode",    data_type="INT", default_value=0),
            ParameterDef(name="Scale",   data_type="INT", default_value=1),
            ParameterDef(name="Mask",    data_type="INT", default_value=65535),
        ]
    )


def _define_drvt2pulsein() -> FunctionBlockMeta:
    """_DrvT2PulseIn - 脉冲量输入（T2脉冲模块）"""
    return FunctionBlockMeta(
        name="_DrvT2PulseIn",
        type_id=41214,
        memory_size=24,
        category="io",
        description="脉冲量输入（T2脉冲模块）",
        parameters=[
            ParameterDef(name="Channel", data_type="INT", default_value=0),
            ParameterDef(name="Mode", data_type="INT", default_value=0),
            ParameterDef(name="Scale", data_type="INT", default_value=1),
            ParameterDef(name="Mask", data_type="INT", default_value=65535),
        ]
    )


def _define_drvqepin() -> FunctionBlockMeta:
    """_DrvQEPIn - 正交编码器输入（QEP模块）"""
    return FunctionBlockMeta(
        name="_DrvQEPIn",
        type_id=41215,
        memory_size=24,
        category="io",
        description="正交编码器输入（QEP模块）",
        parameters=[
            ParameterDef(name="Channel", data_type="INT", default_value=0),
            ParameterDef(name="Mode", data_type="INT", default_value=0),
            ParameterDef(name="Scale", data_type="INT", default_value=1),
            ParameterDef(name="Mask", data_type="INT", default_value=65535),
        ]
    )
