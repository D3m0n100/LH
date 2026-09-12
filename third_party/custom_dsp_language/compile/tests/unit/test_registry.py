"""
Tests for Function Block Registry
"""

import pytest
from lh_compiler.function_blocks.registry import FunctionBlockRegistry, FunctionBlockMeta, ParameterDef


def test_registry_creation():
    """Test creating a registry instance"""
    registry = FunctionBlockRegistry()
    assert len(registry) == 0


def test_load_defaults():
    """Test loading built-in default function blocks"""
    registry = FunctionBlockRegistry()
    registry.load_defaults()
    assert len(registry) > 0
    assert "System" in registry
    assert registry.has("System")
    meta = registry.get("System")
    assert meta is not None
    assert meta.type_id == 101


def test_get_by_name():
    """Test getting block by name"""
    registry = FunctionBlockRegistry()
    definition = FunctionBlockMeta(
        name="_TestBlock",
        type_id=12345,
        memory_size=16,
        category="test",
        description="Test block description"
    )
    registry.register(definition)

    result = registry.get("_TestBlock")
    assert result == definition
    assert "_TestBlock" in registry


def test_get_by_id():
    """Test getting block by type ID"""
    registry = FunctionBlockRegistry()
    definition = FunctionBlockMeta(
        name="_TestBlock",
        type_id=12345,
        memory_size=16,
        category="test",
        description="Test block description"
    )
    registry.register(definition)

    result = registry.get_by_id(12345)
    assert result == definition
    assert registry.get_by_id(99999) is None


def test_list_categories():
    """Test listing categories"""
    registry = FunctionBlockRegistry()
    registry.register(FunctionBlockMeta(
        name="_Test1",
        type_id=1,
        memory_size=8,
        category="cat1",
        description="Test"
    ))
    registry.register(FunctionBlockMeta(
        name="_Test2",
        type_id=2,
        memory_size=8,
        category="cat2",
        description="Test"
    ))

    categories = registry.list_categories()
    assert "cat1" in categories
    assert "cat2" in categories

    cat1_blocks = registry.get_category("cat1")
    assert len(cat1_blocks) == 1
    assert cat1_blocks[0].name == "_Test1"


def test_search():
    """Test search functionality"""
    registry = FunctionBlockRegistry()
    registry.register(FunctionBlockMeta(
        name="_SystemInit",
        type_id=1,
        memory_size=16,
        category="system",
        description="Initialize the system"
    ))
    registry.register(FunctionBlockMeta(
        name="_MathAdd",
        type_id=2,
        memory_size=16,
        category="math",
        description="Add two numbers"
    ))

    results = registry.search("system")
    assert len(results) == 1
    assert results[0].name == "_SystemInit"

    results = registry.search("add")
    assert len(results) == 1
    assert results[0].name == "_MathAdd"


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
