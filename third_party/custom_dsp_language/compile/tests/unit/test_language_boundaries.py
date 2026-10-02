"""Language boundaries must fail without leaving stale executable artifacts."""
import pytest
from lh_compiler.compiler import LHCompiler


@pytest.mark.parametrize("source, reason", [
    ("PROGRAM P VAR x : LREAL; END_VAR END_PROGRAM", "LREAL"),
    ("PROGRAM P VAR_CONSTANT x : INT := 1; END_VAR x := 2; END_PROGRAM", "VAR_CONSTANT"),
    ("PROGRAM P VAR_CONSTANT x : INT; END_VAR END_PROGRAM", "初始值"),
    ("PROGRAM P VAR_CONSTANT x : System; END_VAR END_PROGRAM", "常量"),
    ("PROGRAM P VAR x : INT := 12; END_VAR END_PROGRAM", "初始化写回"),
    ("PROGRAM P VAR x : REAL := 1.5; END_VAR END_PROGRAM", "初始化写回"),
    ("PROGRAM P VAR x : BOOL := TRUE; END_VAR END_PROGRAM", "初始化写回"),
    ("PROGRAM P VAR pid : PID; x : REAL; END_VAR pid(Kp => x); END_PROGRAM", "输出"),
])
def test_reject_language_boundary_and_remove_stale_code(tmp_path, source, reason):
    output = tmp_path / "boundary.code"
    output.write_text("stale", encoding="utf-8")
    result = LHCompiler().compile_string(source, str(output))
    assert not result.success
    assert any(reason in error for error in result.errors)
    assert not output.exists()


@pytest.mark.parametrize("source, values", [
    ("PROGRAM P VAR x : INT; END_VAR x := 12; x := 13; END_PROGRAM", [12, 13]),
    ("PROGRAM P VAR x : REAL; END_VAR x := 1.5; END_PROGRAM", [1069547520]),
    ("PROGRAM P VAR x : BOOL; END_VAR x := TRUE; END_PROGRAM", [1]),
])
def test_scalar_assignment_generation_is_restored(tmp_path, source, values):
    output = tmp_path / "main.code"
    result = LHCompiler().compile_string(source, str(output))
    assert result.success and result.compile_only
    assert [instruction.params[-1] for instruction in result.instructions[1:]] == values
    assert "target_execution_confirmed\t0" in output.with_suffix(".rep").read_text(encoding="utf-8")


@pytest.mark.parametrize("literal", [True, False])
@pytest.mark.parametrize("block, parameter, datatype", [
    ("System", "Author", "INT"), ("PID", "Kp", "REAL"),
])
def test_output_binding_failure_stage_and_stale_artifacts(tmp_path, literal, block,
                                                          parameter, datatype):
    target = "1" if literal else "x"
    source = (f"PROGRAM P VAR fb : {block}; x : {datatype}; END_VAR "
              f"fb({parameter} => {target}); END_PROGRAM")
    output = tmp_path / "output.code"
    for suffix in (".code", ".list", ".typ"):
        output.with_suffix(suffix).write_text("stale", encoding="utf-8")
    result = LHCompiler().compile_string(source, str(output))
    assert not result.success
    if literal:
        assert result.ast is None
        assert any("语法分析错误" in error for error in result.errors)
        assert not any("输出引用目标契约" in error or "输出绑定目标契约" in error
                       for error in result.errors)
    else:
        assert result.ast is not None
        assert any("输出" in error and "目标契约未定义" in error
                   for error in result.errors)
        assert not any("语法分析错误" in error for error in result.errors)
    for suffix in (".code", ".list", ".typ"):
        assert not output.with_suffix(suffix).exists()
    assert output.with_suffix(".rep").exists()
