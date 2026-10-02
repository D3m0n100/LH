"""File identity checks and atomic publication shared by all compiler outputs."""
import os
import tempfile
from pathlib import Path


def artifact_paths(output_path):
    output = Path(output_path)
    base, _ = os.path.splitext(str(output))
    return {"code": output, **{key: Path(base + "." + key)
                              for key in ("list", "typ", "rep")}}


def same_file(left, right):
    if os.path.normcase(str(Path(left).resolve())) == os.path.normcase(str(Path(right).resolve())):
        return True
    try:
        return os.path.samefile(left, right)
    except FileNotFoundError:
        return False


def validate_outputs(output_path, source_path=""):
    paths = artifact_paths(output_path)
    for key, path in paths.items():
        if source_path and same_file(source_path, path):
            raise ValueError(f"输入源文件路径与输出路径不能相同或指向同一文件: {path}")
        if path.is_dir():
            raise ValueError(f"输出产物路径是目录，禁止覆盖: {path}")
        for previous_key, previous in paths.items():
            if previous_key == key:
                break
            if same_file(previous, path):
                raise ValueError(f"输出产物路径相互冲突: {previous} / {path}")
    return paths


def atomic_write(path, content):
    """Write through an exclusively created temporary descriptor, never a guessed name."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=".lh-", suffix=".tmp", dir=str(path.parent))
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    except BaseException as error:
        try:
            Path(temporary).unlink(missing_ok=True)
        except OSError as cleanup_error:
            raise OSError(f"{error}; 无法清理临时产物 '{temporary}': {cleanup_error}") from error
        raise
