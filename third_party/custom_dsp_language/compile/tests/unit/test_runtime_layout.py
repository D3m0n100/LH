import pytest
from lh_compiler.function_blocks.registry import FunctionBlockMeta, ParameterDef, RuntimeFieldDef
from lh_compiler.backend.codegen import CodeGenerator
from lh_compiler.frontend.ast_nodes import BinaryOp, BinaryOperator, Literal, DataType


def test_constructor_offsets_do_not_imply_runtime_fields():
    meta = FunctionBlockMeta("Fixture", 1, 8, [ParameterDef("Count", "INT", offset=0)])
    assert meta.get_parameter_offset("Count") is None
    assert meta.runtime_field_layout() == ()


def test_explicit_layout_retains_offsets_and_direction():
    meta = FunctionBlockMeta("Fixture", 1, 8, runtime_fields=[
        RuntimeFieldDef("Input", "INT", 0, 2),
        RuntimeFieldDef("Output", "REAL", 4, 4, "OUT"),
    ])
    assert meta.get_parameter_offset("output") == 4
    assert meta.runtime_field_layout()[1].direction == "OUT"


@pytest.mark.parametrize("fields", [
    [RuntimeFieldDef("A", "INT", -1, 2)],
    [RuntimeFieldDef("A", "INT", 7, 2)],
    [RuntimeFieldDef("A", "INT", 0, 2), RuntimeFieldDef("B", "INT", 1, 2)],
    [RuntimeFieldDef("A", "INT", 0, 2), RuntimeFieldDef("a", "INT", 2, 2)],
    [RuntimeFieldDef("A", "INT", 0, 2, "unknown")],
])
def test_layout_rejects_invalid_contract(fields):
    with pytest.raises(ValueError):
        FunctionBlockMeta("Fixture", 1, 8, runtime_fields=fields).runtime_field_layout()


def test_integer_division_still_folds_without_claiming_variable_writeback():
    generator = CodeGenerator()
    expression = BinaryOp(BinaryOperator.DIV, Literal(5, DataType.INT), Literal(2, DataType.INT))
    assert generator._eval_expression(expression) == 2
    assert not generator.instructions
