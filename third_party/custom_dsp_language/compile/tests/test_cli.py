import pytest
from click.testing import CliRunner

from lh_compiler.cli.commands import main


def test_cli_compile_and_check(tmp_path):
    runner = CliRunner()

    # Valid program
    valid_source = tmp_path / "valid.lh"
    valid_source.write_text("PROGRAM Main\nVAR\n    system : System;\nEND_VAR\nsystem(Author := 1, Config := 100, Date := 2601);\nEND_PROGRAM\n")

    check_res = runner.invoke(main, ["check", str(valid_source)])
    assert check_res.exit_code == 0
    assert "Check passed" in check_res.output

    out_code = tmp_path / "valid.code"
    compile_res = runner.invoke(main, ["compile", str(valid_source), "-o", str(out_code)])
    assert compile_res.exit_code == 0
    assert "Compile success" in compile_res.output
    assert out_code.exists()

    # Invalid program with unsupported control flow
    invalid_source = tmp_path / "invalid.lh"
    invalid_source.write_text("PROGRAM Broken\nVAR\n    cond : BOOL;\n    val : INT;\nEND_VAR\nIF cond THEN val := 1; END_IF;\nEND_PROGRAM\n")

    invalid_res = runner.invoke(main, ["compile", str(invalid_source)])
    assert invalid_res.exit_code != 0
    assert "暂不支持控制流语句" in invalid_res.output


def test_cli_registry_commands():
    runner = CliRunner()

    list_res = runner.invoke(main, ["list-blocks"])
    assert list_res.exit_code == 0
    assert "System" in list_res.output

    cat_res = runner.invoke(main, ["categories"])
    assert cat_res.exit_code == 0

    desc_res = runner.invoke(main, ["describe", "System"])
    assert desc_res.exit_code == 0
    assert "Author" in desc_res.output
    assert "supported" in desc_res.output

    desc_incomplete = runner.invoke(main, ["describe", "FilterBW"])
    assert desc_incomplete.exit_code == 0
    assert "incomplete" in desc_incomplete.output
    assert "TODO" in desc_incomplete.output

    list_res2 = runner.invoke(main, ["list-blocks"])
    assert list_res2.exit_code == 0
    assert "Status" in list_res2.output


def test_cli_version():
    runner = CliRunner()
    res = runner.invoke(main, ["--version"])
    assert res.exit_code == 0
    assert "1.0.0" in res.output


def test_lmc_subcommands_compatibility(tmp_path, capsys):
    import importlib.util
    from pathlib import Path

    lmc_path = Path(__file__).resolve().parent.parent / "lmc.py"
    spec = importlib.util.spec_from_file_location("lmc", lmc_path)
    lmc_mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(lmc_mod)

    # 1. Check normalization of subcommands
    assert lmc_mod.normalize_cli_args(["compile", "test.lh", "-o", "test.code"]) == ["test.lh", "-o", "test.code"]
    assert lmc_mod.normalize_cli_args(["check", "test.lh"]) == ["--check", "test.lh"]
    assert lmc_mod.normalize_cli_args(["list-blocks"]) == ["--list-blocks"]
    assert lmc_mod.normalize_cli_args(["categories"]) == ["--categories"]
    assert lmc_mod.normalize_cli_args(["describe", "System"]) == ["--describe", "System"]
    assert lmc_mod.normalize_cli_args(["version"]) == ["--version"]

    # 2. Check traditional options remain intact
    assert lmc_mod.normalize_cli_args(["test.lh", "-o", "test.code"]) == ["test.lh", "-o", "test.code"]
    assert lmc_mod.normalize_cli_args(["--check", "test.lh"]) == ["--check", "test.lh"]
    assert lmc_mod.normalize_cli_args(["--list-blocks"]) == ["--list-blocks"]

    # 3. Test compile and check via lmc_mod.main
    valid_source = tmp_path / "lmc_test.lh"
    valid_source.write_text("PROGRAM Main\nVAR\n    system : System;\nEND_VAR\nsystem(Author := 1, Config := 100, Date := 2601);\nEND_PROGRAM\n")
    out_code = tmp_path / "lmc_test.code"

    assert lmc_mod.main(["check", str(valid_source)]) == 0
    assert lmc_mod.main(["compile", str(valid_source), "-o", str(out_code)]) == 0
    assert out_code.exists()

    # 4. Test registry queries via lmc_mod.main
    assert lmc_mod.main(["categories"]) == 0
    assert lmc_mod.main(["describe", "System"]) == 0
    captured_desc_sys = capsys.readouterr()
    assert "支持 (supported)" in captured_desc_sys.out

    assert lmc_mod.main(["describe", "FilterBW"]) == 0
    captured_desc = capsys.readouterr()
    assert "未完善 (incomplete)" in captured_desc.out
    assert "TODO" in captured_desc.out

    assert lmc_mod.main(["list-blocks"]) == 0
    captured_list = capsys.readouterr()
    assert "[INCOMPLETE]" in captured_list.out

    # 5. T09: Source and output identical rejected with non-zero exit code
    same_path_exit = lmc_mod.main(["compile", str(valid_source), "-o", str(valid_source)])
    assert same_path_exit != 0
    captured = capsys.readouterr()
    assert "输入源文件路径与输出路径不能相同" in captured.out or "输入源文件路径与输出路径不能相同" in captured.err

    # 6. T09: Cleanup failure on compile failure returns non-zero and prints cleanup diagnostic
    bad_source = tmp_path / "lmc_bad.lh"
    bad_source.write_text("PROGRAM Broken\nVAR\n    v : BAD_TYPE;\nEND_VAR\nEND_PROGRAM\n", encoding='utf-8')
    bad_code = tmp_path / "lmc_bad.code"
    bad_code.write_text("pre-existing", encoding='utf-8')

    orig_unlink = Path.unlink
    def locked_unlink(self, *args, **kwargs):
        if self.name == "lmc_bad.code":
            raise PermissionError("Access is denied (locked by test)")
        return orig_unlink(self, *args, **kwargs)

    Path.unlink = locked_unlink
    try:
        cleanup_fail_exit = lmc_mod.main(["compile", str(bad_source), "-o", str(bad_code)])
        assert cleanup_fail_exit != 0
        captured2 = capsys.readouterr()
        assert "无法清理残留产物文件" in captured2.out or "无法清理残留产物文件" in captured2.err
        assert "Access is denied" in captured2.out or "Access is denied" in captured2.err
    finally:
        Path.unlink = orig_unlink

