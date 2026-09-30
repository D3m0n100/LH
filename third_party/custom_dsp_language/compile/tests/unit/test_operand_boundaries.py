import pytest
from lh_compiler.backend.operands import Immediate, VariableRef, MemberRef, Write, NoInstanceOperation, encode_immediate


@pytest.mark.parametrize("reference", [VariableRef("x", "REAL"), MemberRef("block", "Input", "REAL", 0, 4, "IN")])
def test_reference_never_reaches_numeric_encoder(reference):
    called = []
    with pytest.raises(ValueError, match="引用操作数"):
        encode_immediate(reference, lambda data_type, value: called.append(value))
    assert not called


def test_immediate_and_reference_write_remain_distinct():
    target = VariableRef("x", "REAL")
    immediate = Immediate("REAL", 1.0)
    reference = MemberRef("block", "Output", "REAL", 4, 4, "OUT")
    assert Write(target, immediate).value == immediate
    assert Write(target, reference).value == reference
    assert encode_immediate(immediate, lambda data_type, value: (data_type, value)) == ("REAL", 1.0)


@pytest.mark.parametrize("target,source", [
    (VariableRef("constant", "INT", True), Immediate("INT", 1)),
    (MemberRef("block", "Output", "REAL", 4, 4, "OUT"), Immediate("REAL", 1.0)),
    (VariableRef("x", "INT"), VariableRef("y", "REAL")),
])
def test_invalid_write_intent_is_rejected(target, source):
    with pytest.raises(ValueError):
        Write(target, source)


def test_no_instance_operation_cannot_use_an_unspecified_target_form():
    with pytest.raises(ValueError):
        NoInstanceOperation("TaskEnd", "")
