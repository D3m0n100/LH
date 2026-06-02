# -*- coding: utf-8 -*-
"""
PID 功能块定义
修改内容：去掉下划线前缀、type_id改为三位数、补齐参数定义
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
    PID - 增量式PID控制器
    实现标准的比例-积分-微分闭环控制算法，
    支持输出限幅、积分抗饱和、手动复位
    """
    return FunctionBlockMeta(
        name="PID",
        type_id=501,
        memory_size=64,
        category="pid",
        description="增量式PID控制器",
        parameters=[
            ParameterDef(name="Setpoint",   data_type="REAL", default_value=0.0),
            ParameterDef(name="Feedback",   data_type="REAL", default_value=0.0),
            ParameterDef(name="Kp",         data_type="REAL", default_value=1.0),
            ParameterDef(name="Ki",         data_type="REAL", default_value=0.0),
            ParameterDef(name="Kd",         data_type="REAL", default_value=0.0),
            ParameterDef(name="OutMax",     data_type="REAL", default_value=100.0),
            ParameterDef(name="OutMin",     data_type="REAL", default_value=-100.0),
            ParameterDef(name="SampleTime", data_type="INT",  default_value=1000),
            ParameterDef(name="Enable",     data_type="BOOL", default_value=1),
            ParameterDef(name="Reset",      data_type="BOOL", default_value=0),
        ]
    )


def _define_pidautotune() -> FunctionBlockMeta:
    """
    PIDAutoTune - 基于继电反馈法的PID自整定
    通过注入继电振荡信号，自动辨识被控对象的临界增益和临界周期，
    按Ziegler-Nichols公式计算Kp/Ki/Kd推荐值
    """
    return FunctionBlockMeta(
        name="PIDAutoTune",
        type_id=502,
        memory_size=64,
        category="pid",
        description="基于继电反馈法的PID自整定",
        parameters=[
            ParameterDef(name="Setpoint",   data_type="REAL", default_value=0.0),
            ParameterDef(name="Feedback",   data_type="REAL", default_value=0.0),
            ParameterDef(name="Amplitude",  data_type="REAL", default_value=10.0),
            ParameterDef(name="Hysteresis", data_type="REAL", default_value=0.5),
            ParameterDef(name="Cycles",     data_type="INT",  default_value=5),
            ParameterDef(name="Start",      data_type="BOOL", default_value=0),
        ]
    )
