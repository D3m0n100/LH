# Development Health Workflow

Use this workflow as the single local entry point for checking whether the project is healthy.

## Fast check against the existing build

```powershell
.\tools\workflow_dev.bat -SkipConfigure -SkipBuild
```

This verifies that CMake and CTest are available, then runs the registered CTest suite in `build_current_mingw`.

## Full configure, build, and test

```powershell
.\tools\workflow_dev.bat
```

Defaults:

- Build directory: `build_current_mingw`
- CMake generator: `MinGW Makefiles`
- Qt prefix: `C:/Qt/5.15.2/mingw81_64`
- Parallel build jobs: `4`

Override them when needed:

```powershell
.\tools\workflow_dev.bat -BuildDir build -QtPrefix C:/Qt/5.15.2/mingw81_64 -Jobs 8
```

## Optional app launch

```powershell
.\tools\workflow_dev.bat -LaunchApp
```

`-LaunchApp` also checks that the DSL Python virtual environment exists at:

```text
third_party/custom_dsp_language/compile/venv/Scripts/python.exe
```

## Reports

Each run writes a timestamped report to:

```text
workflow_reports/dev_health_YYYYMMDD_HHMMSS.txt
```

A healthy run exits with code `0` and prints `RESULT: PASS`.
A failed run exits with code `1` and prints `RESULT: FAIL`.

If the local DSL Python runtime is broken or missing, the workflow records a `WARN`
line in the report and continues. That keeps the entry point usable while still
showing the environment gap explicitly.
