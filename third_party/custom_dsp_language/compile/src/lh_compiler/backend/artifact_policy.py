"""Artifact execution status, independent of the compilation entry point."""

UNCONFIRMED_EXECUTION_MARKER = "// LH-EXECUTION-UNCONFIRMED: scalar-assignment-v1"
# Retain the existing file signature so already-running clients also refuse
# unverified output. It is an artifact identifier, not a selectable mode.
PREVIOUS_OFFLINE_MARKER = "// LH-OFFLINE-COMPATIBILITY: legacy-constants-v1"
UNCONFIRMED_EXECUTION_WARNING = "赋值写回尚未通过目标固件验证，当前产物不可下载或运行。"
