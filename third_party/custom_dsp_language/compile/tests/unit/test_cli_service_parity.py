import os
import subprocess
import sys
from pathlib import Path

import pytest


@pytest.mark.parametrize("valid", [True, False])
@pytest.mark.parametrize("check_only", [True, False])
def test_direct_and_installed_entry_points_share_business_results(tmp_path, valid, check_only):
    source = tmp_path / "输入.lh"
    source.write_text(
        "PROGRAM Main\nVAR\nsystem : System;\nEND_VAR\n"
        "system(Author := 1, Config := 100, Date := 2601);\nEND_PROGRAM\n"
        if valid else "PROGRAM Main\nVAR\nv : MISSING_TYPE;\nEND_VAR\nEND_PROGRAM\n",
        encoding="utf-8",
    )
    root = Path(__file__).resolve().parents[2]
    entry_points = [[sys.executable, str(root / "lmc.py")],
                    [sys.executable, "-m", "lh_compiler.cli.commands"]]
    env = dict(os.environ, PYTHONUTF8="1", PYTHONDONTWRITEBYTECODE="1")
    exit_codes, codes = [], []
    for index, entry in enumerate(entry_points):
        output = tmp_path / f"output-{index}.code"
        command = ["check", str(source)] if check_only else ["compile", str(source), "-o", str(output)]
        result = subprocess.run(entry + command, env=env, capture_output=True, text=True,
                                encoding="utf-8", timeout=10, cwd=root)
        exit_codes.append(result.returncode)
        assert (result.returncode == 0) == valid, result.stdout + result.stderr
        if valid and not check_only:
            codes.append(output.read_bytes())
        else:
            assert not output.exists()
    assert exit_codes[0] == exit_codes[1]
    if codes:
        assert codes[0] == codes[1]


def test_installed_cli_rejects_hardlinked_support_output(tmp_path):
    source = tmp_path / "main.lh"
    original = b"PROGRAM Main\nVAR\nsystem : System;\nEND_VAR\nsystem(Author := 1, Config := 100, Date := 2601);\nEND_PROGRAM\n"
    source.write_bytes(original)
    (tmp_path / "main.rep").hardlink_to(source)
    result = subprocess.run([sys.executable, "-m", "lh_compiler.cli.commands", "compile", str(source),
                             "-o", str(tmp_path / "main.code")], capture_output=True, timeout=10,
                            env=dict(os.environ, PYTHONUTF8="1", PYTHONDONTWRITEBYTECODE="1"))
    assert result.returncode != 0
    assert source.read_bytes() == original
    assert not (tmp_path / "main.code").exists()
