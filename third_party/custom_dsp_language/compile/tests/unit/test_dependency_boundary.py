"""Dependency failures must remain catchable library errors, never SystemExit."""
import builtins
import importlib
import sys

import pytest


def test_missing_antlr_is_catchable(monkeypatch):
    original_import = builtins.__import__

    def without_antlr(name, *args, **kwargs):
        if name == "antlr4" or name.startswith("antlr4."):
            raise ImportError("antlr4 intentionally unavailable")
        return original_import(name, *args, **kwargs)

    module_name = "lh_compiler.compiler"
    previous = sys.modules.pop(module_name, None)
    try:
        with monkeypatch.context() as context:
            context.setattr(builtins, "__import__", without_antlr)
            module = importlib.import_module(module_name)
            with pytest.raises(module.CompilerDependencyError, match="ANTLR"):
                module.LHCompiler()
    finally:
        sys.modules.pop(module_name, None)
        if previous is not None:
            sys.modules[module_name] = previous
