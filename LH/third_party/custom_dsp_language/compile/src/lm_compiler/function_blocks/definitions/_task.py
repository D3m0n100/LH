# -*- coding: utf-8 -*-
"""
TASK 功能块定义
"""

from typing import List
from ..registry import FunctionBlockMeta, ParameterDef


def get_task_blocks() -> List[FunctionBlockMeta]:
    """返回task类别的功能块定义"""
    return [
        _define_task(),
        _define_taskperiodic(),
        _define_taskwake(),
        _define_taskdatawake(),
        _define_tasklock(),
        _define_taskunlock(),
        _define_tasksemdef(),
        _define_tasksemwait(),
        _define_tasksempost(),
        _define_taskend(),
    ]


def _define_task() -> FunctionBlockMeta:
    """
    _Task - 创建时间触发任务
    """
    return FunctionBlockMeta(
        name="_Task",
        type_id=41000,
        memory_size=16,
        category="task",
        description="创建时间触发任务",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_taskperiodic() -> FunctionBlockMeta:
    """
    _TaskPeriodic - 创建周期性任务
    """
    return FunctionBlockMeta(
        name="_TaskPeriodic",
        type_id=41001,
        memory_size=16,
        category="task",
        description="创建周期性任务",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_taskwake() -> FunctionBlockMeta:
    """
    _TaskWake - 唤醒任务
    """
    return FunctionBlockMeta(
        name="_TaskWake",
        type_id=41002,
        memory_size=8,
        category="task",
        description="唤醒任务",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_taskdatawake() -> FunctionBlockMeta:
    """
    _TaskDataWake - 数据唤醒任务
    """
    return FunctionBlockMeta(
        name="_TaskDataWake",
        type_id=41003,
        memory_size=16,
        category="task",
        description="数据唤醒任务",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tasklock() -> FunctionBlockMeta:
    """
    _TaskLock - 任务上锁
    """
    return FunctionBlockMeta(
        name="_TaskLock",
        type_id=41004,
        memory_size=8,
        category="task",
        description="任务上锁",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_taskunlock() -> FunctionBlockMeta:
    """
    _TaskUnlock - 任务解锁
    """
    return FunctionBlockMeta(
        name="_TaskUnlock",
        type_id=41005,
        memory_size=8,
        category="task",
        description="任务解锁",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tasksemdef() -> FunctionBlockMeta:
    """
    _TaskSemDef - 信号量定义
    """
    return FunctionBlockMeta(
        name="_TaskSemDef",
        type_id=41006,
        memory_size=16,
        category="task",
        description="信号量定义",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tasksemwait() -> FunctionBlockMeta:
    """
    _TaskSemWait - 信号量等待
    """
    return FunctionBlockMeta(
        name="_TaskSemWait",
        type_id=41007,
        memory_size=16,
        category="task",
        description="信号量等待",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tasksempost() -> FunctionBlockMeta:
    """
    _TaskSemPost - 信号量发送
    """
    return FunctionBlockMeta(
        name="_TaskSemPost",
        type_id=41008,
        memory_size=16,
        category="task",
        description="信号量发送",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_taskend() -> FunctionBlockMeta:
    """
    _TaskEnd - 任务结束标志
    """
    return FunctionBlockMeta(
        name="_TaskEnd",
        type_id=41009,
        memory_size=8,
        category="task",
        description="任务结束标志",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


