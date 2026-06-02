# -*- coding: utf-8 -*-
"""
功能块定义汇总
"""

from typing import List
from ..registry import FunctionBlockMeta

from ._application import get_application_blocks
from ._comm import get_comm_blocks
from ._compare import get_compare_blocks
from ._constant import get_constant_blocks
from ._control import get_control_blocks
from ._counter import get_counter_blocks
from ._display import get_display_blocks
from ._filter import get_filter_blocks
from ._io import get_io_blocks
from ._logic import get_logic_blocks
from ._math import get_math_blocks
from ._pid import get_pid_blocks
from ._system import get_system_blocks
from ._task import get_task_blocks
from ._timer import get_timer_blocks
from ._tso import get_tso_blocks
from ._exca import get_exca_blocks
from ._data import get_data_blocks
from ._safety import get_safety_blocks
from ._param import get_param_blocks


def get_all_definitions() -> List[FunctionBlockMeta]:
    """收集所有功能块定义"""
    all_defs = []

    all_defs.extend(get_application_blocks())
    all_defs.extend(get_comm_blocks())
    all_defs.extend(get_compare_blocks())
    all_defs.extend(get_constant_blocks())
    all_defs.extend(get_control_blocks())
    all_defs.extend(get_counter_blocks())
    all_defs.extend(get_display_blocks())
    all_defs.extend(get_filter_blocks())
    all_defs.extend(get_io_blocks())
    all_defs.extend(get_logic_blocks())
    all_defs.extend(get_math_blocks())
    all_defs.extend(get_pid_blocks())
    all_defs.extend(get_system_blocks())
    all_defs.extend(get_task_blocks())
    all_defs.extend(get_timer_blocks())
    all_defs.extend(get_tso_blocks())
    all_defs.extend(get_exca_blocks())
    all_defs.extend(get_data_blocks())
    all_defs.extend(get_safety_blocks())
    all_defs.extend(get_param_blocks())

    return all_defs
