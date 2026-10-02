"""Static architecture check. Run by acceptance, without importing application code."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
ALLOWED = {
    "common": {"common"},
    "core": {"common", "core"},
    "compiler": {"common", "compiler", "communication"},
    "communication": {"common", "communication"},
    "runtime": {"common", "runtime", "core", "compiler", "communication", "monitor"},
}
QT_UI = re.compile(r"^(?:QtWidgets|QtGui|QtCharts|QtSvg)(?:/|$)|"
                   r"^Q(?:Widget|Dialog|MainWindow|Application|MessageBox|FileDialog|"
                   r"TreeWidget|TextEdit|PlainTextEdit|DockWidget|Chart|Pixmap|Icon)$")
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"\n]+)[>"]', re.M)
errors = []
for module, permitted in ALLOWED.items():
    for source in (SRC / module).rglob("*"):
        if source.suffix not in {".h", ".cpp"}:
            continue
        for include in INCLUDE.findall(source.read_text(encoding="utf-8-sig")):
            if QT_UI.search(include):
                errors.append(f"{source.relative_to(ROOT)}: UI dependency {include}")
            candidates = (source.parent / include, ROOT / "include" / include, SRC / include)
            resolved = next((p.resolve() for p in candidates if p.is_file()), None)
            if resolved is None:
                matches = [p for name in ALLOWED.keys() | {"designer", "monitor", "diagnostics"}
                           for p in (SRC / name).glob(include) if p.is_file()]
                if len(matches) == 1:
                    resolved = matches[0].resolve()
            if resolved is not None and SRC in resolved.parents:
                target = resolved.relative_to(SRC).parts[0]
                if target not in permitted:
                    errors.append(f"{source.relative_to(ROOT)}: forbidden dependency {target}")
                if module == "runtime" and target == "monitor" and resolved.name != "IMonitorHistoryStore.h":
                    errors.append(f"{source.relative_to(ROOT)}: runtime may only use the QtCore history port")

# Public headers must resolve their explicit inter-module dependencies themselves.
for name in ("BackendSampler.h", "BackendSampler.cpp"):
    source = SRC / "monitor" / name
    for include in INCLUDE.findall(source.read_text(encoding="utf-8-sig")):
        if QT_UI.search(include):
            errors.append(f"{source.relative_to(ROOT)}: sampling port has UI dependency {include}")

for header in SRC.rglob("*.h"):
    for include in INCLUDE.findall(header.read_text(encoding="utf-8-sig")):
        if re.match(r"(?:common|core|compiler|communication|designer|monitor|diagnostics)/", include):
            errors.append(f"{header.relative_to(ROOT)}: requires exported src root: {include}")

for module in ("core", "compiler", "communication", "monitor", "designer", "runtime"):
    cmake = (SRC / module / "CMakeLists.txt").read_text(encoding="utf-8-sig")
    for body in re.findall(r"target_include_directories\((.*?)\)", cmake, re.S):
        for public in re.findall(r"(?:PUBLIC|INTERFACE)\s+(.*?)(?=\bPRIVATE\b|\bPUBLIC\b|\bINTERFACE\b|$)", body, re.S):
            if "LX_PRIVATE_INCLUDE_DIRS" in public or "${CMAKE_SOURCE_DIR}/src" in public:
                errors.append(f"src/{module}/CMakeLists.txt: exports private source directory")

if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print("Module boundaries checked.")
