"""Symbolic operands. Addresses are never disguised as numeric literals.

These nodes describe compiler intent, not a certified firmware serialization.
"""
from dataclasses import dataclass
from typing import Any, Union


@dataclass(frozen=True)
class Immediate:
    data_type: str
    value: Any


@dataclass(frozen=True)
class VariableRef:
    name: str
    data_type: str
    read_only: bool = False


@dataclass(frozen=True)
class MemberRef:
    instance: str
    field: str
    data_type: str
    offset: int
    size: int
    direction: str


Operand = Union[Immediate, VariableRef, MemberRef]
Destination = Union[VariableRef, MemberRef]


def validate_write(target: Destination, source: Operand):
    if isinstance(target, VariableRef) and target.read_only:
        raise ValueError(f"只读变量: {target.name}")
    if isinstance(target, MemberRef) and target.direction not in {"IN", "IN_OUT"}:
        raise ValueError(f"运行时字段不可写: {target.instance}.{target.field}")
    if not isinstance(target, (VariableRef, MemberRef)):
        raise ValueError("写回目标必须是变量或成员引用")
    if target.data_type.upper() != source.data_type.upper():
        raise ValueError("写回类型不一致；没有隐式目标转换契约")


@dataclass(frozen=True)
class Init:
    target: Destination
    value: Immediate

    def __post_init__(self):
        if not isinstance(self.target, (VariableRef, MemberRef)) or not isinstance(self.value, Immediate):
            raise ValueError("初始化必须使用符号目标与即时值")
        if self.target.data_type.upper() != self.value.data_type.upper():
            raise ValueError("初始化类型不一致")


@dataclass(frozen=True)
class Write:
    target: Destination
    value: Operand

    def __post_init__(self):
        validate_write(self.target, self.value)


@dataclass(frozen=True)
class NoInstanceOperation:
    name: str
    target_contract_id: str

    def __post_init__(self):
        if not self.target_contract_id:
            raise ValueError("无实例操作必须有明确的目标指令形式契约")


def encode_immediate(operand: Operand, encode):
    if not isinstance(operand, Immediate):
        raise ValueError("引用操作数目标编码未定义，不能按 REAL/整数即时值编码")
    return encode(operand.data_type, operand.value)
