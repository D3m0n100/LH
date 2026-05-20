"""
Tests for Function Block Registry
"""

import json
from pathlib import Path

import pytest
from lm_compiler.function_blocks.registry import FunctionBlockRegistry
from lm_compiler.function_blocks.base import FunctionBlockDefinition


def test_registry_creation():
    """Test creating a registry instance"""
    registry = FunctionBlockRegistry()
    assert len(registry) == 0


def test_load_metadata(tmp_path):
    """Test loading metadata from JSON files"""
    # Create a test metadata file
    metadata_dir = tmp_path / "metadata"
    metadata_dir.mkdir()
    
    test_metadata = """
    {
        "version": "1.0.0",
        "category": "test",
        "function_blocks": [
            {
                "name": "_TestBlock",
                "type_id": 99999,
                "description": "Test block",
                "parameters": []
            }
        ]
    }
    """
    
    (metadata_dir / "test.json").write_text(test_metadata)
    
    # Load metadata
    registry = FunctionBlockRegistry()
    registry.load_metadata(metadata_dir)
    
    # Verify
    assert len(registry) == 1
    assert "_TestBlock" in registry
    
    block = registry.get("_TestBlock")
    assert block is not None
    assert block.type_id == 99999


def test_get_by_name():
    """Test getting block by name"""
    registry = FunctionBlockRegistry()
    
    definition = FunctionBlockDefinition(
        name="_TestBlock",
        type_id=12345,
        category="test",
        description="Test"
    )
    registry.register(definition)
    
    result = registry.get("_TestBlock")
    assert result == definition


def test_get_by_id():
    """Test getting block by type ID"""
    registry = FunctionBlockRegistry()
    
    definition = FunctionBlockDefinition(
        name="_TestBlock",
        type_id=12345,
        category="test",
        description="Test"
    )
    registry.register(definition)
    
    result = registry.get_by_id(12345)
    assert result == definition


def test_list_categories():
    """Test listing categories"""
    registry = FunctionBlockRegistry()
    
    registry.register(FunctionBlockDefinition(
        name="_Test1",
        type_id=1,
        category="cat1",
        description="Test"
    ))
    
    registry.register(FunctionBlockDefinition(
        name="_Test2",
        type_id=2,
        category="cat2",
        description="Test"
    ))
    
    categories = registry.list_categories()
    assert "cat1" in categories
    assert "cat2" in categories


def test_search():
    """Test search functionality"""
    registry = FunctionBlockRegistry()
    
    registry.register(FunctionBlockDefinition(
        name="_SystemInit",
        type_id=1,
        category="system",
        description="Initialize the system"
    ))
    
    registry.register(FunctionBlockDefinition(
        name="_MathAdd",
        type_id=2,
        category="math",
        description="Add two numbers"
    ))
    
    # Search by name
    results = registry.search("system")
    assert len(results) == 1
    assert results[0].name == "_SystemInit"
    
    # Search by description
    results = registry.search("add")
    assert len(results) == 1
    assert results[0].name == "_MathAdd"


def test_default_registry_includes_high_frequency_io_blocks():
    """Test that the default registry includes the new core IO blocks."""
    registry = FunctionBlockRegistry()
    registry.load_defaults()

    assert registry.has("_DrvT1PWMAO")
    assert registry.has("_DrvT2PulseIn")
    assert registry.has("_DrvQEPIn")

    drv_t1_pwmao = registry.get("_DrvT1PWMAO")
    drv_t2_pulse_in = registry.get("_DrvT2PulseIn")
    drv_qep_in = registry.get("_DrvQEPIn")

    assert drv_t1_pwmao is not None
    assert drv_t2_pulse_in is not None
    assert drv_qep_in is not None
    assert drv_t1_pwmao.category == "io"
    assert drv_t2_pulse_in.category == "io"
    assert drv_qep_in.category == "io"

    snippets_path = Path(__file__).resolve().parents[5] / "resources" / "snippets" / "default_snippets.json"
    snippets = json.loads(snippets_path.read_text(encoding="utf-8"))
    snippet_ids = {item["id"] for item in snippets}

    assert "_DrvT1PWMAO" in snippet_ids
    assert "_DrvT2PulseIn" in snippet_ids
    assert "_DrvQEPIn" in snippet_ids


if __name__ == '__main__':
    pytest.main([__file__, '-v'])
