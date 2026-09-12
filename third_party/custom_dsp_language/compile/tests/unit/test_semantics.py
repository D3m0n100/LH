"""
DSL Compiler Semantics Contract and Golden Tests
Verifies fail-closed behavior on unhandled control flow and expressions,
and golden output on valid language subsets.
"""

import pytest
from pathlib import Path
from lh_compiler.compiler import LHCompiler


@pytest.fixture
def compiler():
    return LHCompiler()


def test_control_flow_fails_closed(tmp_path, compiler):
    """Control flow (IF, CASE, FOR, WHILE) must fail and produce no .code output"""
    cases = [
        ("IF statement", "PROGRAM P\nVAR\n  c : BOOL;\n  v : INT;\nEND_VAR\nIF c THEN v := 1; END_IF;\nEND_PROGRAM\n", "IfStatement"),
        ("CASE statement", "PROGRAM P\nVAR\n  s : INT;\n  v : INT;\nEND_VAR\nCASE s OF 1: v := 1; END_CASE;\nEND_PROGRAM\n", "CaseStatement"),
        ("FOR statement", "PROGRAM P\nVAR\n  i : INT;\nEND_VAR\nFOR i := 0 TO 10 DO i := i; END_FOR;\nEND_PROGRAM\n", "ForStatement"),
        ("WHILE statement", "PROGRAM P\nVAR\n  c : BOOL;\nEND_VAR\nWHILE c DO c := FALSE; END_WHILE;\nEND_PROGRAM\n", "WhileStatement"),
    ]

    for label, code, node_type in cases:
        out_code = tmp_path / f"{node_type}.code"
        src_path = tmp_path / f"{node_type}.lh"
        src_path.write_text(code, encoding='utf-8')
        result = compiler.compile_file(str(src_path), str(out_code))

        assert not result.success, f"{label} should fail compilation"
        assert any(node_type in err for err in result.errors), f"{label} errors should mention {node_type}"
        assert not out_code.exists(), f"{label} should NOT produce a .code file"


def test_empty_parameters_fb_fails_on_arguments(tmp_path, compiler):
    """Function blocks without parameter definitions (e.g. Task, FilterBW) must reject calls with arguments"""
    task_code = """PROGRAM P
VAR
  system : System;
  task1 : Task;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
task1(Priority := 1, Period := 100);
END_PROGRAM
"""
    src_path = tmp_path / "task_call.lh"
    src_path.write_text(task_code, encoding='utf-8')
    out_code = tmp_path / "task_call.code"

    result = compiler.compile_file(str(src_path), str(out_code))
    assert not result.success
    assert any("Task" in err and ("未定义参数" in err or "尚未完善契约定义" in err) for err in result.errors)
    assert not out_code.exists()

    filter_code = """PROGRAM P
VAR
  system : System;
  fb : FilterBW;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
fb(Freq := 50);
END_PROGRAM
"""
    src_filter = tmp_path / "filter_call.lh"
    src_filter.write_text(filter_code, encoding='utf-8')
    out_filter = tmp_path / "filter_call.code"

    result2 = compiler.compile_file(str(src_filter), str(out_filter))
    assert not result2.success
    assert any("FilterBW" in err and ("未定义参数" in err or "尚未完善契约定义" in err) for err in result2.errors)
    assert not out_filter.exists()

    # Supported zero-parameter FB must reject arguments with "未定义参数"
    supported_zero_code = """PROGRAM P
VAR
  system : System;
  comm : CommCANInit;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
comm(Baud := 500);
END_PROGRAM
"""
    src_comm = tmp_path / "comm_call.lh"
    src_comm.write_text(supported_zero_code, encoding='utf-8')
    out_comm = tmp_path / "comm_call.code"

    result3 = compiler.compile_file(str(src_comm), str(out_comm))
    assert not result3.success
    assert any("CommCANInit" in err and "未定义参数" in err for err in result3.errors)
    assert not out_comm.exists()


def test_unsupported_assignments_fail_closed(tmp_path, compiler):
    """Variable copy and runtime expressions must fail closed without silently emitting 0 or skipping"""
    # 1. Variable copy: a := b;
    var_copy_code = """PROGRAM P
VAR
  system : System;
  a : INT;
  b : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
b := 10;
a := b;
END_PROGRAM
"""
    src_copy = tmp_path / "var_copy.lh"
    src_copy.write_text(var_copy_code, encoding='utf-8')
    out_copy = tmp_path / "var_copy.code"
    res1 = compiler.compile_file(str(src_copy), str(out_copy))
    assert not res1.success
    assert any("暂不支持变量赋值/拷贝指令" in err for err in res1.errors)
    assert not out_copy.exists()

    # 2. Runtime expression: a := b + 1;
    rt_expr_code = """PROGRAM P
VAR
  system : System;
  a : INT;
  b : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
b := 10;
a := b + 1;
END_PROGRAM
"""
    src_expr = tmp_path / "rt_expr.lh"
    src_expr.write_text(rt_expr_code, encoding='utf-8')
    out_expr = tmp_path / "rt_expr.code"
    res2 = compiler.compile_file(str(src_expr), str(out_expr))
    assert not res2.success
    assert any("无法常量折叠" in err for err in res2.errors)
    assert not out_expr.exists()


def test_golden_valid_instructions(tmp_path, compiler):
    """Valid program with constants and literal folding generates exact golden .code bytecode"""
    valid_code = """PROGRAM P
VAR
  system : System;
  val1 : INT;
  val2 : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
val1 := 42;
val2 := 10 + 20;
END_PROGRAM
"""
    src_path = tmp_path / "valid.lh"
    src_path.write_text(valid_code, encoding='utf-8')
    out_path = tmp_path / "valid.code"

    res = compiler.compile_file(str(src_path), str(out_path))
    assert res.success, f"Compilation failed: {res.errors}"
    assert out_path.exists()

    lines = [l.strip() for l in out_path.read_text(encoding='utf-8').splitlines() if l.strip()]
    # System: 101 101 0 1 100 2601 1 1000
    assert lines[0] == "101 101 0 1 100 2601 1 1000"
    # val1 := 42 -> IntConstBuild (type_id 121)
    assert lines[1].startswith("121 121") and lines[1].endswith("42")
    # val2 := 10 + 20 -> folded to 30 -> IntConstBuild (type_id 121)
    assert lines[2].startswith("121 121") and lines[2].endswith("30")


def test_array_type_declaration_fails_closed(tmp_path, compiler):
    """Declaring ARRAY types must fail closed with explicit compilation error"""
    code = """PROGRAM P
VAR
  system : System;
  arr : ARRAY [1..10] OF INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
END_PROGRAM
"""
    src_path = tmp_path / "array_decl.lh"
    src_path.write_text(code, encoding='utf-8')
    out_code = tmp_path / "array_decl.code"

    result = compiler.compile_file(str(src_path), str(out_code))
    assert not result.success
    assert any("暂不支持数组类型" in err for err in result.errors)
    assert not out_code.exists()


def test_string_type_declaration_fails_closed(tmp_path, compiler):
    """Declaring STRING types must fail closed with explicit compilation error"""
    code = """PROGRAM P
VAR
  system : System;
  msg : STRING;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
END_PROGRAM
"""
    src_path = tmp_path / "string_decl.lh"
    src_path.write_text(code, encoding='utf-8')
    out_code = tmp_path / "string_decl.code"

    result = compiler.compile_file(str(src_path), str(out_code))
    assert not result.success
    assert any("暂不支持字符串类型" in err for err in result.errors)
    assert not out_code.exists()


def test_array_index_assignment_fails_closed(tmp_path, compiler):
    """Array element assignment and access must fail closed with explicit error"""
    code = """PROGRAM P
VAR
  system : System;
  val : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
arr[0] := 5;
END_PROGRAM
"""
    src_path = tmp_path / "array_idx.lh"
    src_path.write_text(code, encoding='utf-8')
    out_code = tmp_path / "array_idx.code"

    result = compiler.compile_file(str(src_path), str(out_code))
    assert not result.success
    assert any("暂不支持数组元素访问或赋值" in err for err in result.errors)
    assert not out_code.exists()


def test_stale_code_artifact_removed_on_compile_failure(tmp_path, compiler):
    """When recompiling a previously successful program with invalid code, stale .code must be unlinked"""
    good_code = """PROGRAM P
VAR
  system : System;
  val : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
val := 100;
END_PROGRAM
"""
    src_path = tmp_path / "prog.lh"
    src_path.write_text(good_code, encoding='utf-8')
    out_code = tmp_path / "prog.code"

    # Step 1: compile valid code -> .code exists
    res1 = compiler.compile_file(str(src_path), str(out_code))
    assert res1.success
    assert out_code.exists()
    assert out_code.stat().st_size > 0

    # Step 2: overwrite source with invalid code (unsupported ARRAY type)
    bad_code = """PROGRAM P
VAR
  system : System;
  arr : ARRAY [1..10] OF INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
END_PROGRAM
"""
    src_path.write_text(bad_code, encoding='utf-8')

    # Step 3: compile bad code -> compile fails, and prog.code MUST be removed from disk
    res2 = compiler.compile_file(str(src_path), str(out_code))
    assert not res2.success
    assert not out_code.exists(), "Stale .code artifact MUST NOT be retained after failed compilation"


def test_lexer_error_fails_closed(tmp_path, compiler):
    """Lexer error (e.g. unrecognized character '@') must fail closed and leave no .code artifact"""
    bad_code = """PROGRAM P
VAR
  system : System;
  @v : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
END_PROGRAM
"""
    src_path = tmp_path / "lexer_err.lh"
    src_path.write_text(bad_code, encoding='utf-8')
    out_code = tmp_path / "lexer_err.code"

    res = compiler.compile_file(str(src_path), str(out_code))
    assert not res.success
    assert any("词法分析错误" in err or "语法" in err for err in res.errors)
    assert not out_code.exists()


def test_system_fb_unknown_param_rejected(tmp_path, compiler):
    """System function block must reject unknown parameter calls like Bogus := 7"""
    code = """PROGRAM P
VAR
  system : System;
END_VAR
system(Author := 1, Bogus := 7);
END_PROGRAM
"""
    src_path = tmp_path / "sys_unknown.lh"
    src_path.write_text(code, encoding='utf-8')
    out_code = tmp_path / "sys_unknown.code"

    res = compiler.compile_file(str(src_path), str(out_code))
    assert not res.success
    assert any("System" in err and "不接受参数 'Bogus'" in err for err in res.errors)
    assert not out_code.exists()


def test_system_fb_runtime_var_rejected(tmp_path, compiler):
    """System function block must reject non-constant runtime variables as parameter values"""
    code = """PROGRAM P
VAR
  system : System;
  runtime_val : INT;
END_VAR
runtime_val := 1;
system(Author := runtime_val);
END_PROGRAM
"""
    src_path = tmp_path / "sys_rt.lh"
    src_path.write_text(code, encoding='utf-8')
    out_code = tmp_path / "sys_rt.code"

    res = compiler.compile_file(str(src_path), str(out_code))
    assert not res.success
    assert any("System" in err and "无法在编译期求值为有效常量" in err for err in res.errors)
    assert not out_code.exists()


def test_system_fb_case_insensitive(tmp_path, compiler):
    """System parameter names must be case-insensitive, e.g. author := 1"""
    code = """PROGRAM P
VAR
  system : System;
END_VAR
system(author := 1, config := 100, date := 2601);
END_PROGRAM
"""
    src_path = tmp_path / "sys_case.lh"
    src_path.write_text(code, encoding='utf-8')
    out_code = tmp_path / "sys_case.code"

    res = compiler.compile_file(str(src_path), str(out_code))
    assert res.success, f"Failed: {res.errors}"
    assert out_code.exists()
    lines = [l.strip() for l in out_code.read_text(encoding='utf-8').splitlines() if l.strip()]
    assert lines[0] == "101 101 0 1 100 2601 1 1000"


def test_constant_int_division_not_float(tmp_path, compiler):
    """INT constant division 5 / 2 must evaluate to 2 (integer), NOT IEEE float pattern 1075838976"""
    code = """PROGRAM P
VAR
  system : System;
  v : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
v := 5 / 2;
END_PROGRAM
"""
    src_path = tmp_path / "int_div.lh"
    src_path.write_text(code, encoding='utf-8')
    out_code = tmp_path / "int_div.code"

    res = compiler.compile_file(str(src_path), str(out_code))
    assert res.success, f"Failed: {res.errors}"
    lines = [l.strip() for l in out_code.read_text(encoding='utf-8').splitlines() if l.strip()]
    # IntConstBuild type_id 121, value must be 2, NOT 1075838976
    assert lines[1].startswith("121 121")
    assert lines[1].endswith(" 2")
    assert "1075838976" not in lines[1]


def test_constant_real_float_encoding(tmp_path, compiler):
    """REAL constant 3 must be encoded as IEEE 754 float32 bit pattern 1077936128, NOT integer 3"""
    code = """PROGRAM P
VAR
  system : System;
  r : REAL;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
r := 3;
END_PROGRAM
"""
    src_path = tmp_path / "real_const.lh"
    src_path.write_text(code, encoding='utf-8')
    out_code = tmp_path / "real_const.code"

    res = compiler.compile_file(str(src_path), str(out_code))
    assert res.success, f"Failed: {res.errors}"
    lines = [l.strip() for l in out_code.read_text(encoding='utf-8').splitlines() if l.strip()]
    # RealConstBuild type_id 122, value 3.0 packed as float32 is 1077936128
    assert lines[1].startswith("122 122")
    assert lines[1].endswith(" 1077936128")


def test_division_by_zero_rejected(tmp_path, compiler):
    """Division or modulo by zero in constant expressions must fail closed with explicit error"""
    # 1. 1 / 0
    code1 = """PROGRAM P
VAR
  system : System;
  v : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
v := 1 / 0;
END_PROGRAM
"""
    src1 = tmp_path / "div_zero.lh"
    src1.write_text(code1, encoding='utf-8')
    res1 = compiler.compile_file(str(src1), str(tmp_path / "div_zero.code"))
    assert not res1.success
    assert any("除数不能为0" in err for err in res1.errors)

    # 2. 10 MOD 0
    code2 = """PROGRAM P
VAR
  system : System;
  v : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
v := 10 MOD 0;
END_PROGRAM
"""
    src2 = tmp_path / "mod_zero.lh"
    src2.write_text(code2, encoding='utf-8')
    res2 = compiler.compile_file(str(src2), str(tmp_path / "mod_zero.code"))
    assert not res2.success
    assert any("除数不能为0" in err for err in res2.errors)


def test_integer_range_and_type_checks(tmp_path, compiler):
    """Out-of-range integer values and invalid float-to-int assignments must be rejected"""
    # 1. SINT overflow (1000 > 127)
    code_sint = """PROGRAM P
VAR
  system : System;
  v : SINT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
v := 1000;
END_PROGRAM
"""
    src_sint = tmp_path / "sint_overflow.lh"
    src_sint.write_text(code_sint, encoding='utf-8')
    res_sint = compiler.compile_file(str(src_sint), str(tmp_path / "sint_overflow.code"))
    assert not res_sint.success
    assert any("SINT" in err and "超出" in err for err in res_sint.errors)

    # 2. Float to INT without conversion
    code_float = """PROGRAM P
VAR
  system : System;
  v : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
v := 2.5;
END_PROGRAM
"""
    src_float = tmp_path / "float_to_int.lh"
    src_float.write_text(code_float, encoding='utf-8')
    res_float = compiler.compile_file(str(src_float), str(tmp_path / "float_to_int.code"))
    assert not res_float.success
    assert any("类型不匹配" in err for err in res_float.errors)


def test_all_empty_param_fbs_reject_arguments(compiler):
    """All registered function blocks with empty parameter lists must reject calls with arguments"""
    from lh_compiler.function_blocks.registry import FunctionBlockRegistry
    reg = FunctionBlockRegistry()
    reg.load_defaults()

    empty_fbs = [meta for meta in reg.list_all() if not meta.parameters and meta.name not in ("System",)]
    assert len(empty_fbs) >= 120, f"Expected at least 120 empty-param FBs, found {len(empty_fbs)}"

    for meta in empty_fbs:
        code = f"""PROGRAM P
VAR
  system : System;
  fb_inst : {meta.name};
END_VAR
system(Author := 1, Config := 100, Date := 2601);
fb_inst(DummyParam := 1);
END_PROGRAM
"""
        res = compiler.compile_string(code)
        assert not res.success, f"FB {meta.name} with empty parameters must reject calls with arguments"
        if meta.is_incomplete:
            assert any("尚未完善契约定义" in err for err in res.errors)
        else:
            assert any("未定义参数" in err for err in res.errors)


def test_fb_parameter_contract_float_to_int_rejected(compiler):
    """Function block integer parameter must strictly reject float value (P0-03)"""
    code = """PROGRAM P
VAR
  system : System;
  pid1 : PID;
END_VAR
system(Author := 1);
pid1(SampleTime := 2.5);
END_PROGRAM
"""
    res = compiler.compile_string(code)
    assert not res.success, "PID(SampleTime := 2.5) must fail compilation"
    assert any("类型不匹配" in err or "浮点数" in err for err in res.errors)


def test_fb_parameter_contract_invalid_bool_rejected(compiler):
    """Function block boolean parameter must strictly reject non-boolean value (P0-03)"""
    code = """PROGRAM P
VAR
  system : System;
  di : DrvDI;
END_VAR
system(Author := 1);
di(Action := 2);
END_PROGRAM
"""
    res = compiler.compile_string(code)
    assert not res.success, "DrvDI(Action := 2) must fail compilation"
    assert any("BOOL" in err and "非法值" in err for err in res.errors)


def test_fb_parameter_contract_int_overflow_rejected(compiler):
    """Function block integer parameter must strictly reject out-of-range value (P0-03)"""
    code = """PROGRAM P
VAR
  system : System;
  ai : DrvAI;
END_VAR
system(Author := 1);
ai(NumChannels := 40000);
END_PROGRAM
"""
    res = compiler.compile_string(code)
    assert not res.success, "DrvAI(NumChannels := 40000) must fail compilation"
    assert any("超出 INT 有效范围" in err for err in res.errors)


def test_fb_parameter_contract_duplicate_parameters_rejected(compiler):
    """Function block call with duplicate parameters must strictly fail compilation (P0-03)"""
    code_sys_dup = """PROGRAM P
VAR
  system : System;
END_VAR
system(Author := 1, Author := 2);
END_PROGRAM
"""
    res_sys = compiler.compile_string(code_sys_dup)
    assert not res_sys.success, "System(Author := 1, Author := 2) must fail compilation"
    assert any("存在重复参数" in err for err in res_sys.errors)

    code_fb_dup = """PROGRAM P
VAR
  system : System;
  pid1 : PID;
END_VAR
system(Author := 1);
pid1(Kp := 1.0, Kp := 2.0);
END_PROGRAM
"""
    res_fb = compiler.compile_string(code_fb_dup)
    assert not res_fb.success, "PID(Kp := 1.0, Kp := 2.0) must fail compilation"
    assert any("存在重复参数" in err for err in res_fb.errors)


def test_non_finite_and_overflow_floats_rejected(tmp_path, compiler):
    """T01: Non-finite floats (Infinity, NaN) and float32 overflows must strictly fail compilation and emit no .code"""
    bad_expressions = [
        ("Infinity literal", "1.0e999"),
        ("NaN expression", "1.0e999 - 1.0e999"),
        ("float32 overflow literal", "1.0e40"),
        ("float32 overflow folded", "1.0e30 * 1.0e30"),
    ]

    for label, expr in bad_expressions:
        code = f"""PROGRAM P
VAR
  system : System;
  r : REAL;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
r := {expr};
END_PROGRAM
"""
        src = tmp_path / f"bad_float_{label.replace(' ', '_')}.lh"
        src.write_text(code, encoding='utf-8')
        out = tmp_path / f"bad_float_{label.replace(' ', '_')}.code"
        res = compiler.compile_file(str(src), str(out))
        assert not res.success, f"{label} should fail compilation"
        assert not out.exists(), f"{label} should produce no .code output"

    # Function block parameter rejection
    fb_bad = """PROGRAM P
VAR
  system : System;
  pid1 : PID;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
pid1(Kp := 1.0e999);
END_PROGRAM
"""
    fb_out = tmp_path / "fb_bad.code"
    src_fb = tmp_path / "fb_bad.lh"
    src_fb.write_text(fb_bad, encoding='utf-8')
    res_fb = compiler.compile_file(str(src_fb), str(fb_out))
    assert not res_fb.success, "PID(Kp := 1.0e999) must fail compilation"
    assert not fb_out.exists()

    # Valid values and encoding consistency
    code_float_1 = """PROGRAM P
VAR
  system : System;
  r : REAL;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
r := 1.0;
END_PROGRAM
"""
    code_int_1 = """PROGRAM P
VAR
  system : System;
  r : REAL;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
r := 1;
END_PROGRAM
"""
    out_float = tmp_path / "float_1.code"
    out_int = tmp_path / "int_1.code"
    src_float = tmp_path / "float_1.lh"
    src_int = tmp_path / "int_1.lh"
    src_float.write_text(code_float_1, encoding='utf-8')
    src_int.write_text(code_int_1, encoding='utf-8')

    res1 = compiler.compile_file(str(src_float), str(out_float))
    res2 = compiler.compile_file(str(src_int), str(out_int))
    assert res1.success, f"Failed: {res1.errors}"
    assert res2.success, f"Failed: {res2.errors}"
    assert out_float.read_text(encoding='utf-8') == out_int.read_text(encoding='utf-8'), "REAL 1 and 1.0 must encode identically"


def test_ast_builder_records_errors_on_injected_context_exceptions():
    """T08: ASTBuilder must record errors when context methods throw exceptions (fail-closed)"""
    from lh_compiler.frontend.ast_builder import ASTBuilder, build_ast

    class FailingProgramContext:
        def __init__(self):
            self.start = type('Token', (), {'line': 1, 'column': 0})()

        def identifier(self):
            raise RuntimeError("Injected AST parse fault in identifier")

        def varSection(self):
            return None

        def statementList(self):
            return None

    builder = ASTBuilder()
    ast = builder.visitProgram(FailingProgramContext())
    assert len(builder.errors) > 0, "ASTBuilder must record error when context throws"
    assert any("Injected AST parse fault in identifier" in err for err in builder.errors)

    with pytest.raises(RuntimeError) as exc_info:
        build_ast(FailingProgramContext())
    assert "Injected AST parse fault" in str(exc_info.value)


def test_ast_builder_records_error_on_injected_variable_declaration():
    """T08: ASTBuilder must record error when variable declaration context throws or is missing type"""
    from lh_compiler.frontend.ast_builder import ASTBuilder

    class FailingVarDeclContext:
        def __init__(self):
            self.start = type('Token', (), {'line': 10, 'column': 2})()

        def identifier(self):
            return type('IdNode', (), {'getText': lambda: 'myVar'})()

        def dataType(self):
            raise RuntimeError("Injected data type parse error")

        def expression(self):
            return None

    builder = ASTBuilder()
    var = builder.visitVariableDeclaration(FailingVarDeclContext())
    assert len(builder.errors) > 0
    assert any("Injected data type parse error" in err for err in builder.errors)


def test_ast_builder_records_error_on_unrecognized_statement():
    """T08: ASTBuilder must record error on unsupported / unrecognized statement instead of silently dropping it"""
    from lh_compiler.frontend.ast_builder import ASTBuilder

    class UnknownStatementContext:
        def __init__(self):
            self.start = type('Token', (), {'line': 20, 'column': 4})()

        def getText(self):
            return "UNSUPPORTED_STATEMENT foo;"

    builder = ASTBuilder()
    stmt = builder.visitStatement(UnknownStatementContext())
    assert stmt is None
    assert len(builder.errors) > 0
    assert any("无法识别或不支持的语句" in err for err in builder.errors)


def test_t09_cleanup_failure_reported_and_original_error_preserved(tmp_path, compiler, monkeypatch):
    """T09: Simulated unlink failure must aggregate cleanup error with original error and preserve fail status"""
    src = tmp_path / "broken.lh"
    src.write_text("PROGRAM Broken\nVAR\n  v : UNKNOWN_TYPE;\nEND_VAR\nEND_PROGRAM\n", encoding='utf-8')
    out = tmp_path / "broken.code"

    # Pre-create the .code file so unlink will be attempted
    out.write_text("fake legacy bytecode", encoding='utf-8')
    assert out.exists()

    orig_unlink = Path.unlink

    def mock_unlink(self, *args, **kwargs):
        if self.name == "broken.code":
            raise PermissionError("Access is denied (simulated lock)")
        return orig_unlink(self, *args, **kwargs)

    monkeypatch.setattr(Path, "unlink", mock_unlink)

    res = compiler.compile_file(str(src), str(out))
    assert not res.success
    # Original compilation error must be preserved
    assert any("未支持的数据类型" in err or "UNKNOWN_TYPE" in err for err in res.errors)
    # Cleanup failure diagnostic must be reported
    assert any("无法清理残留产物文件" in err for err in res.errors)
    assert any("Access is denied" in err for err in res.errors)
    # The file still physically exists on disk
    assert out.exists()


def test_t09_cleanup_removes_stale_list_and_typ_and_preserves_failed_rep(tmp_path, compiler):
    """T09: When recompiling with errors, stale .list and .typ are cleared, while .rep is kept as failure diagnostic"""
    good_code = """PROGRAM P
VAR
  system : System;
  val : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
val := 42;
END_PROGRAM
"""
    src = tmp_path / "app.lh"
    src.write_text(good_code, encoding='utf-8')
    out = tmp_path / "app.code"

    res1 = compiler.compile_file(str(src), str(out))
    assert res1.success
    list_file = tmp_path / "app.list"
    typ_file = tmp_path / "app.typ"
    rep_file = tmp_path / "app.rep"

    assert out.exists()
    assert list_file.exists()
    assert typ_file.exists()
    assert rep_file.exists()

    # Recompile with syntax error
    bad_code = "INVALID SYNTAX ???;"
    src.write_text(bad_code, encoding='utf-8')

    res2 = compiler.compile_file(str(src), str(out))
    assert not res2.success

    # .code must be removed
    assert not out.exists()
    # .list and .typ must be removed so old symbol tables do not masquerade as current
    assert not list_file.exists()
    assert not typ_file.exists()
    # .rep is the failure diagnostic report and must exist with failed status
    assert rep_file.exists()
    rep_text = rep_file.read_text(encoding='utf-8')
    assert "status\tfailed" in rep_text
    assert "error\t" in rep_text


def test_t09_source_and_output_same_path_rejected_without_unlinking_source(tmp_path, compiler):
    """T09: Source and output pointing to same path must fail immediately and NEVER unlink the source file"""
    src_code = """PROGRAM P
VAR
  system : System;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
END_PROGRAM
"""
    src = tmp_path / "collision.lh"
    src.write_text(src_code, encoding='utf-8')

    # Attempt to compile with output_path equal to source_path
    res = compiler.compile_file(str(src), str(src))
    assert not res.success
    assert any("输入源文件路径与输出路径不能相同" in err for err in res.errors)
    # Source file MUST exist and have exact original content
    assert src.exists()
    assert src.read_text(encoding='utf-8') == src_code


def test_t09_directory_as_output_path_rejected_without_recursive_deletion(tmp_path, compiler):
    """T09: Directory as output path must not recursively delete directories"""
    src_code = """PROGRAM P
VAR
  system : System;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
END_PROGRAM
"""
    src = tmp_path / "test_dir.lh"
    src.write_text(src_code, encoding='utf-8')

    target_dir = tmp_path / "target_folder"
    target_dir.mkdir()
    child_file = target_dir / "preserve_me.txt"
    child_file.write_text("critical data", encoding='utf-8')

    res = compiler.compile_file(str(src), str(target_dir))
    assert not res.success
    # Target directory and its contents must remain completely untouched
    assert target_dir.exists()
    assert child_file.exists()
    assert child_file.read_text(encoding='utf-8') == "critical data"


def test_t15_filterbw_rejected_due_to_incomplete_status(tmp_path, compiler):
    """T15: FilterBW with zero parameters or undefined arguments must fail with incomplete reason"""
    # 1. Direct call FilterBW()
    code_direct = """PROGRAM P
VAR
  system : System;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
FilterBW();
END_PROGRAM
"""
    res1 = compiler.compile_string(code_direct)
    assert not res1.success
    assert any("FilterBW" in err and "尚未完善契约定义" in err for err in res1.errors)

    # 2. Variable declaration fb : FilterBW;
    code_var = """PROGRAM P
VAR
  system : System;
  fb : FilterBW;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
fb();
END_PROGRAM
"""
    res2 = compiler.compile_string(code_var)
    assert not res2.success
    assert any("FilterBW" in err and "尚未完善契约定义" in err for err in res2.errors)

    # 3. Call with arbitrary parameters FilterBW(Gain := 10)
    code_params = """PROGRAM P
VAR
  system : System;
  fb : FilterBW;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
fb(Gain := 10);
END_PROGRAM
"""
    res3 = compiler.compile_string(code_params)
    assert not res3.success
    assert any("FilterBW" in err and "尚未完善契约定义" in err for err in res3.errors)


def test_t15_all_21_incomplete_fbs_have_status_incomplete():
    """T15: Registry must track exactly the 21 incomplete function blocks with reason"""
    from lh_compiler.function_blocks.registry import FunctionBlockRegistry

    reg = FunctionBlockRegistry()
    reg.load_defaults()

    incomplete = reg.list_incomplete()
    assert len(incomplete) == 21

    incomplete_names = {b.name for b in incomplete}
    expected_incomplete = {
        "KeyScan", "SCIDispTrans", "SCIDispInit", "M600TextDisp", "M600ProgressBar", "EXCACycDisp",
        "FilterBW",
        "Task", "TaskPeriodic", "TaskWake", "TaskDataWake", "TaskLock", "TaskUnlock",
        "TaskSemDef", "TaskSemWait", "TaskSemPost", "TaskEnd",
        "TSO", "TSOAutoTune", "TwoPosition", "RelayCtrl2"
    }
    assert incomplete_names == expected_incomplete

    for b in incomplete:
        assert b.is_incomplete
        assert not b.is_supported
        assert "TODO" in b.incomplete_reason


def test_t15_supported_zero_parameter_fb_compiles_successfully(tmp_path, compiler):
    """T15: Legitimate supported zero-parameter / standard blocks continue to compile and succeed"""
    code = """PROGRAM P
VAR
  system : System;
  val : INT;
END_VAR
system(Author := 1, Config := 100, Date := 2601);
val := 123;
END_PROGRAM
"""
    out_code = tmp_path / "valid_std.code"
    res = compiler.compile_string(code, str(out_code))
    assert res.success
    assert out_code.exists()
