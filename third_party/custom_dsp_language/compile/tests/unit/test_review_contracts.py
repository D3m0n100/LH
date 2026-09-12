"""Regressions found by the engineering review, including artifact failures."""
import pytest
from lh_compiler.compiler import LHCompiler


@pytest.mark.parametrize('declarations', [
    'x : INT; x : REAL;',
    'sys : System; sys : System;',
    'x : INT; END_VAR VAR x : INT;',
])
def test_duplicate_declaration_rejects_stale_output(tmp_path, declarations):
    output = tmp_path / 'duplicate.code'
    output.write_text('stale', encoding='utf-8')
    result = LHCompiler().compile_string(
        f'PROGRAM P\nVAR {declarations} END_VAR\nEND_PROGRAM', str(output))
    assert not result.success
    assert any('重复声明' in error for error in result.errors)
    assert not output.exists()


def test_failure_report_write_error_is_returned(tmp_path, monkeypatch):
    compiler = LHCompiler()

    def unavailable(**kwargs):
        raise OSError('diagnostic destination unavailable')

    monkeypatch.setattr(compiler.support_emitter, 'emit', unavailable)
    result = compiler.compile_string('invalid program', str(tmp_path / 'failure.code'))
    assert not result.success
    assert any('diagnostic destination unavailable' in error for error in result.errors)
    assert any('语法' in error for error in result.errors)
    assert not (tmp_path / 'failure.code').exists()


@pytest.mark.parametrize('declarations', [
    'x : INT := 99999;', 'x : BOOL := 7;', 'x : UINT := -1;',
    'x : INT := 1.5;', 'x : INT; y : INT := x;',
])
def test_invalid_initializers_reject_stale_output(tmp_path, declarations):
    output = tmp_path / 'init.code'
    output.write_text('stale', encoding='utf-8')
    result = LHCompiler().compile_string(
        f'PROGRAM P\nVAR {declarations} END_VAR\nEND_PROGRAM', str(output))
    assert not result.success
    assert result.errors
    assert not output.exists()
