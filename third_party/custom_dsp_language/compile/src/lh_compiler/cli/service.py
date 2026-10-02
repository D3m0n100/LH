"""Common compilation/check operation; presentation and argument parsing stay in each CLI."""
from pathlib import Path
from ..compiler import CompileResult


def compile_source(compiler, source_path, output_path=None, check_only=False):
    if not check_only:
        return compiler.compile_file(str(source_path), output_path)
    try:
        source = Path(source_path).read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        return CompileResult(success=False, errors=[f"无法读取源文件: {error}"])
    return compiler.compile_string(source, source_path=str(source_path))
