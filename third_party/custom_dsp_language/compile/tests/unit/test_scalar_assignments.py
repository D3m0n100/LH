"""Ordinary compilation restores constant output without weakening other checks."""
import os
import importlib.util
from pathlib import Path
import subprocess
import sys
import types

import pytest

from lh_compiler.compiler import LHCompiler
from lh_compiler.backend.artifact_policy import UNCONFIRMED_EXECUTION_MARKER, PREVIOUS_OFFLINE_MARKER


def program(declarations, statements):
    return f"PROGRAM Main\nVAR\n{declarations}\nEND_VAR\n{statements}\nEND_PROGRAM\n"


def test_constant_assignments_compile_without_a_mode_switch(tmp_path):
    source = program("flag : BOOL; gain : REAL;", "flag := TRUE; gain := 1.5;")
    output = tmp_path / "main.code"
    result = LHCompiler().compile_string(source, str(output))
    assert result.success and result.compile_only
    assert output.read_text(encoding="utf-8").startswith(UNCONFIRMED_EXECUTION_MARKER)
    # Golden records from the historical generator: distinct constant addresses.
    assert [x.to_code_line() for x in result.instructions] == [
        "101 101 0 0 0 0 1 1000", "123 123 43 1", "122 122 51 1069547520",
    ]
    report = output.with_suffix(".rep").read_text(encoding="utf-8")
    assert "compile_only\t1" in report and "target_execution_confirmed\t0" in report
    assert "warning\t" in report
    assert "compatibility_mode" not in report
    assert "历史兼容" not in report
    # A subsequent invalid program must remove the successful output.
    assert not LHCompiler().compile_string(program("x : INT;", "x := 1 / 0;"), str(output)).success
    assert not output.exists()


@pytest.mark.parametrize("declarations, statements", [
    ("x : INT;", "x := 32768;"),
    ("x : INT;", "x := 1.5;"),
    ("x : BOOL;", "x := 7;"),
    ("x : UINT;", "x := -1;"),
    ("x : REAL;", "x := TRUE;"),
    ("x : LREAL;", "x := 1.5;"),
    ("x : INT := 1;", ""),
    ("x : INT; x : INT;", "x := 1;"),
    ("x : INT; y : INT;", "x := y;"),
    ("x : INT; y : INT;", "x := y + 1;"),
    ("x : INT;", "IF TRUE THEN x := 1; ELSE x := 2; END_IF;"),
    ("x : INT;", "x := 1 / 0;"),
])
def test_restored_assignments_keep_existing_rejections(tmp_path, declarations, statements):
    output = tmp_path / "main.code"
    output.write_text("stale output", encoding="utf-8")
    result = LHCompiler().compile_string(
        program(declarations, statements), str(output))
    assert not result.success and result.errors
    assert not output.exists()


def test_restored_assignments_never_overwrite_a_source_alias(tmp_path):
    source = tmp_path / "main.lh"
    content = program("x : INT;", "x := 1;").encode()
    source.write_bytes(content)
    output = tmp_path / "main.code"
    output.hardlink_to(source)
    result = LHCompiler().compile_file(str(source), str(output))
    assert not result.success
    assert source.read_bytes() == content


def test_both_normal_cli_entries_emit_the_same_artifact(tmp_path):
    source = tmp_path / "输入.lh"
    source.write_text(program("x : INT;", "x := 1;"), encoding="utf-8")
    root = Path(__file__).resolve().parents[2]
    entries = [[sys.executable, str(root / "lmc.py")],
               [sys.executable, "-m", "lh_compiler.cli.commands"]]
    outputs = []
    for index, entry in enumerate(entries):
        output = tmp_path / f"output-{index}.code"
        result = subprocess.run(entry + ["compile", str(source), "-o", str(output)], cwd=root,
                               env=dict(os.environ, PYTHONUTF8="1", PYTHONDONTWRITEBYTECODE="1"),
                               capture_output=True, text=True, encoding="utf-8", timeout=10)
        assert result.returncode == 0, result.stdout + result.stderr
        assert "历史兼容" not in result.stdout
        assert "目标固件验证" in result.stdout
        outputs.append(output.read_bytes())
    assert outputs[0] == outputs[1]


@pytest.mark.parametrize("marker", [UNCONFIRMED_EXECUTION_MARKER, PREVIOUS_OFFLINE_MARKER])
def test_standalone_downloader_rejects_unverified_output_before_opening_serial(tmp_path, monkeypatch, marker):
    # The compiler environment need not install a hardware transport driver.
    # Replace only pyserial, so the real downloader/Modbus path is exercised.
    opened_ports = []
    serial = types.ModuleType("serial")
    def unexpected_serial_open(*args, **kwargs):
        opened_ports.append((args, kwargs))
        raise AssertionError("Offline output attempted to open a serial port")
    serial.Serial = unexpected_serial_open
    serial.SerialException = OSError
    serial.tools = types.ModuleType("serial.tools")
    serial.tools.list_ports = types.ModuleType("serial.tools.list_ports")
    monkeypatch.setitem(sys.modules, "serial", serial)
    monkeypatch.setitem(sys.modules, "serial.tools", serial.tools)
    monkeypatch.setitem(sys.modules, "serial.tools.list_ports", serial.tools.list_ports)
    root = Path(__file__).resolve().parents[2]
    spec = importlib.util.spec_from_file_location("offline_downloader_probe", root / "lh_download_v3.py")
    downloader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(downloader)
    output = tmp_path / "main.code"
    output.write_text(marker + "\n40001 0 1\n", encoding="utf-8")
    with pytest.raises(SystemExit) as error:
        downloader.download(str(output))
    assert error.value.code == 1
    assert not opened_ports
    # A normal legacy register artifact remains parseable.
    output.write_text("40001 0 1\n", encoding="utf-8")
    assert downloader.parse_code_file(str(output)) == [(40001, 1)]


def test_reused_compiler_does_not_mark_a_following_program_without_assignments(tmp_path):
    compiler = LHCompiler()
    output = tmp_path / "main.code"
    assert compiler.compile_string(program("x : INT;", "x := 1;"), str(output)).compile_only
    result = compiler.compile_string(program("x : INT;", ""), str(output))
    assert result.success and not result.compile_only
    assert "EXECUTION-UNCONFIRMED" not in output.read_text(encoding="utf-8")
    assert "compile_only" not in output.with_suffix(".rep").read_text(encoding="utf-8")
