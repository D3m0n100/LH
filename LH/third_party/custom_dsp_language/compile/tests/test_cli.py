import pytest
from click.testing import CliRunner

from lh_compiler.cli.commands import main


@pytest.mark.parametrize(
    ("command", "error"),
    [
        ("compile", "Error: compile command is not yet implemented"),
        ("check", "Error: check command is not yet implemented"),
    ],
)
def test_unimplemented_commands_fail_without_success_message(tmp_path, command, error):
    source = tmp_path / "program.lh"
    source.write_text("PROGRAM Empty END_PROGRAM\n")

    result = CliRunner().invoke(main, [command, str(source)])

    assert result.exit_code != 0
    assert error in result.output
    assert "Traceback" not in result.output
    assert "ImportError" not in result.output
    assert "passed" not in result.output.lower()
    assert "success" not in result.output.lower()
    assert "No syntax errors found" not in result.output
