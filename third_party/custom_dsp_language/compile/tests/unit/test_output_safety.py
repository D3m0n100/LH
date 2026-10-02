import os
from pathlib import Path
import pytest
from lh_compiler.compiler import LHCompiler

SOURCE = ('PROGRAM P\nVAR\n system : System;\nEND_VAR\n'
          'system(Author := 1, Config := 100, Date := 2601);\nEND_PROGRAM\n')


@pytest.mark.parametrize('suffix', ['.code', '.list', '.typ', '.rep'])
@pytest.mark.parametrize('invalid', [False, True])
def test_source_hardlink_is_preserved_on_success_and_failure(tmp_path, suffix, invalid):
    source = tmp_path / 'main.lh'
    source.write_text('invalid' if invalid else SOURCE, encoding='utf-8')
    os.link(source, source.with_suffix(suffix))
    before = source.read_bytes()
    result = LHCompiler().compile_file(str(source))
    assert not result.success
    assert source.read_bytes() == before
    assert any('同一文件' in error for error in result.errors)


def test_failure_report_cannot_replace_input(tmp_path):
    source = tmp_path / 'main.rep'
    source.write_text('invalid', encoding='utf-8')
    result = LHCompiler().compile_file(str(source))
    assert not result.success
    assert source.read_text(encoding='utf-8') == 'invalid'


def test_guessed_temporary_hardlink_never_written(tmp_path):
    source = tmp_path / 'main.lh'
    source.write_text(SOURCE, encoding='utf-8')
    old_temporary = tmp_path / f'main.code.tmp.{os.getpid()}'
    os.link(source, old_temporary)
    result = LHCompiler().compile_file(str(source))
    assert result.success, result.errors
    assert source.read_text(encoding='utf-8') == SOURCE
    assert old_temporary.read_text(encoding='utf-8') == SOURCE
    assert not list(tmp_path.glob('.lh-*.tmp'))


def test_sidecar_symlink_preserves_input(tmp_path):
    source = tmp_path / 'main.lh'
    source.write_text(SOURCE, encoding='utf-8')
    try:
        source.with_suffix('.typ').symlink_to(source)
    except OSError:
        pytest.skip('Symbolic links require platform permission')
    result = LHCompiler().compile_file(str(source))
    assert not result.success
    assert source.read_text(encoding='utf-8') == SOURCE
