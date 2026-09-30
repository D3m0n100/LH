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
    ("PROGRAM P VAR x : INT; END_VAR x := 12; x := 13; END_PROGRAM", "赋值写回"),
    ("PROGRAM P VAR x : REAL; END_VAR x := 1.5; END_PROGRAM", "赋值写回"),
    ("PROGRAM P VAR x : BOOL; END_VAR x := TRUE; END_PROGRAM", "赋值写回"),
    ("PROGRAM P VAR sys : System; END_VAR sys(Author => 1); END_PROGRAM", "输出"),
    ("PROGRAM P VAR pid : PID; x : REAL; END_VAR pid(Kp => x); END_PROGRAM", "输出"),
])
def test_reject_language_boundary_and_remove_stale_code(tmp_path, source, reason):
    output = tmp_path / "boundary.code"
    output.write_text("stale", encoding="utf-8")
    result = LHCompiler().compile_string(source, str(output))
    assert not result.success
    assert any(reason in error for error in result.errors)
    assert not output.exists()
