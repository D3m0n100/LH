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
    Task - 创建时间触发任务
    """
    return FunctionBlockMeta(
        name="Task",
        type_id=751,
        memory_size=16,
        category="task",
        description="创建时间触发任务",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_taskperiodic() -> FunctionBlockMeta:
    """
    TaskPeriodic - 创建周期性任务
    """
    return FunctionBlockMeta(
        name="TaskPeriodic",
        type_id=752,
        memory_size=16,
        category="task",
        description="创建周期性任务",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_taskwake() -> FunctionBlockMeta:
    """
    TaskWake - 唤醒任务
    """
    return FunctionBlockMeta(
        name="TaskWake",
        type_id=753,
        memory_size=8,
        category="task",
        description="唤醒任务",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_taskdatawake() -> FunctionBlockMeta:
    """
    TaskDataWake - 数据唤醒任务
    """
    return FunctionBlockMeta(
        name="TaskDataWake",
        type_id=754,
        memory_size=16,
        category="task",
        description="数据唤醒任务",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tasklock() -> FunctionBlockMeta:
    """
    TaskLock - 任务上锁
    """
    return FunctionBlockMeta(
        name="TaskLock",
        type_id=755,
        memory_size=8,
        category="task",
        description="任务上锁",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_taskunlock() -> FunctionBlockMeta:
    """
    TaskUnlock - 任务解锁
    """
    return FunctionBlockMeta(
        name="TaskUnlock",
        type_id=756,
        memory_size=8,
        category="task",
        description="任务解锁",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tasksemdef() -> FunctionBlockMeta:
    """
    TaskSemDef - 信号量定义
    """
    return FunctionBlockMeta(
        name="TaskSemDef",
        type_id=757,
        memory_size=16,
        category="task",
        description="信号量定义",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tasksemwait() -> FunctionBlockMeta:
    """
    TaskSemWait - 信号量等待
    """
    return FunctionBlockMeta(
        name="TaskSemWait",
        type_id=758,
        memory_size=16,
        category="task",
        description="信号量等待",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_tasksempost() -> FunctionBlockMeta:
    """
    TaskSemPost - 信号量发送
    """
    return FunctionBlockMeta(
        name="TaskSemPost",
        type_id=759,
        memory_size=16,
        category="task",
        description="信号量发送",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )


def _define_taskend() -> FunctionBlockMeta:
    """
    TaskEnd - 任务结束标志
    """
    return FunctionBlockMeta(
        name="TaskEnd",
        type_id=760,
        memory_size=8,
        category="task",
        description="任务结束标志",
        parameters=[
            # TODO: 根据实际需求添加参数定义
        ]
    )
