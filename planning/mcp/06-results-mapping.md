# 06 — Results Mapping Implementation

## Scope

This document details the implementation of the field/results mapping system that translates between human-readable field names (e.g., "stress", "displacement") and Griz's internal command names (e.g., "sx", "dy"). The mapping is defined in a shared YAML file and consumed by both the Python API and the Qt UI client to provide consistent, user-friendly field interfaces across both front ends.

## Related

- `../shared/results-map.md` — authoritative specification for shared mapping
- [01-architecture.md](01-architecture.md) — overall system design
- [03-python-api.md](03-python-api.md) — Python API implementation
- Qt UI field selection — shares same YAML file

## 1. System Overview

### 1.1 Design Goals

**Single source of truth**: One YAML file defines all field mappings
**Shared across clients**: Both Python and Qt UI use identical mapping
**User-friendly names**: Hide Griz's terse internal names from users
**Extensible**: Easy to add new fields without code changes
**Validated**: Clear error messages for unknown fields/components
**Documented**: Self-documenting field reference

### 1.2 Architecture

```
User Input: g.field.show("stress", component="von_mises")
     ↓
Python API: results_map.resolve("stress", "von_mises") → "svm" 
     ↓
Griz Commands: "res svm; show result"
     ↓
Server Execution: Display von Mises stress field
```

The mapping layer sits between the user-facing API and the underlying Griz commands, providing a clean abstraction that can evolve independently.

## 2. YAML Schema Design

### 2.1 File Location

**Canonical location**: `Src/data/results_map.yaml`
- Checked into version control with Griz source
- Installed alongside Python package as data file
- Loaded by Qt client from installation directory

**Python access**: Symlinked into package data directory
- `Src/python/griz/data/results_map.yaml` → `../../data/results_map.yaml`
- Allows pip-installed package to find mapping file
- Single file maintained in source tree

### 2.2 YAML Structure

```yaml
# Src/data/results_map.yaml
# Field name mapping for Griz Python API and Qt UI
#
# Structure:
#   field_name:
#     component_name: "griz_command"
#   OR:
#   field_name: "griz_command"  # For scalar fields

# Stress tensor components
stress:
  description: "Stress tensor components"
  default: "von_mises"
  components:
    xx: "sx"
    yy: "sy" 
    zz: "sz"
    xy: "sxy"
    yz: "syz"
    xz: "sxz"
    von_mises: "svm"
    pressure: "pressure"
    principal_1: "sp1"
    principal_2: "sp2"
    principal_3: "sp3"
    max_shear: "smax_shear"

# Displacement vector components  
displacement:
  description: "Nodal displacement vector"
  default: "magnitude"
  components:
    x: "dx"
    y: "dy"
    z: "dz"
    magnitude: "dmag"

# Velocity vector components
velocity:
  description: "Nodal velocity vector"
  default: "magnitude"
  components:
    x: "vx"
    y: "vy"
    z: "vz"
    magnitude: "vmag"

# Acceleration vector components
acceleration:
  description: "Nodal acceleration vector"
  default: "magnitude"
  components:
    x: "ax"
    y: "ay"
    z: "az"
    magnitude: "amag"

# Scalar fields (no components)
temperature: 
  description: "Temperature field"
  command: "temp"

pressure:
  description: "Pressure field"  
  command: "pressure"

density:
  description: "Material density"
  command: "density"

plastic_strain:
  description: "Equivalent plastic strain"
  command: "eplas"

# Material properties
young_modulus:
  description: "Young's modulus"
  command: "young"

poisson_ratio:
  description: "Poisson's ratio"
  command: "poisson"

# Element quality metrics
aspect_ratio:
  description: "Element aspect ratio"
  command: "aspect"

volume:
  description: "Element volume"
  command: "volume"

# Special meta-fields
material_id:
  description: "Material ID per element"
  command: "matid"

element_id:
  description: "Element ID"
  command: "elemid"

node_id:
  description: "Node ID"
  command: "nodeid"
```

### 2.3 Schema Extensions

**Future field additions**:
```yaml
# Damage/failure fields
damage:
  description: "Damage parameter"
  default: "total"
  components:
    total: "damage"
    tensile: "damage_t"
    compressive: "damage_c"

# Thermal fields
heat_flux:
  description: "Heat flux vector"
  default: "magnitude"
  components:
    x: "qx"
    y: "qy"
    z: "qz"
    magnitude: "qmag"

# Contact forces
contact_force:
  description: "Contact force components"
  default: "normal"
  components:
    normal: "fn"
    tangential: "ft"
    magnitude: "fmag"
```

## 3. Python Implementation

### 3.1 Results Map Loader

```python
# griz/results_map.py
import yaml
import os
from typing import Dict, Any, List, Optional
from .exceptions import UnknownFieldError

class ResultsMap:
    """Handles field name resolution from human-readable to Griz names."""
    
    def __init__(self, yaml_path: Optional[str] = None):
        """Initialize results mapping.
        
        Args:
            yaml_path: Path to YAML file (default: package data file)
        """
        if yaml_path is None:
            yaml_path = self._get_default_yaml_path()
        
        self._mapping = self._load_mapping(yaml_path)
        self._validate_mapping()
    
    def _get_default_yaml_path(self) -> str:
        """Get default YAML file path relative to package."""
        package_dir = os.path.dirname(__file__)
        yaml_path = os.path.join(package_dir, "data", "results_map.yaml")
        
        if not os.path.exists(yaml_path):
            # Fallback to source tree location
            src_path = os.path.join(package_dir, "..", "..", "..", "data", "results_map.yaml")
            src_path = os.path.normpath(src_path)
            
            if os.path.exists(src_path):
                return src_path
            else:
                raise FileNotFoundError(f"Results mapping file not found. Searched: {yaml_path}, {src_path}")
        
        return yaml_path
    
    def _load_mapping(self, yaml_path: str) -> Dict[str, Any]:
        """Load and parse YAML mapping file."""
        try:
            with open(yaml_path, 'r', encoding='utf-8') as f:
                mapping = yaml.safe_load(f)
                
            if not isinstance(mapping, dict):
                raise ValueError("YAML file must contain a dictionary")
                
            return mapping
            
        except FileNotFoundError:
            raise FileNotFoundError(f"Results mapping file not found: {yaml_path}")
        except yaml.YAMLError as e:
            raise ValueError(f"Invalid YAML in results mapping: {e}")
        except Exception as e:
            raise ValueError(f"Error loading results mapping: {e}")
    
    def _validate_mapping(self) -> None:
        """Validate mapping structure and content."""
        for field_name, field_config in self._mapping.items():
            if not isinstance(field_name, str):
                raise ValueError(f"Field name must be string: {field_name}")
                
            if isinstance(field_config, dict):
                # Vector field with components
                if "components" not in field_config:
                    raise ValueError(f"Vector field '{field_name}' missing 'components' section")
                
                components = field_config["components"]
                if not isinstance(components, dict):
                    raise ValueError(f"Components for '{field_name}' must be a dictionary")
                
                # Validate default component if specified
                if "default" in field_config:
                    default = field_config["default"]
                    if default not in components:
                        raise ValueError(f"Default component '{default}' not found in '{field_name}' components")
                        
            elif isinstance(field_config, dict) and "command" in field_config:
                # Scalar field
                if not isinstance(field_config["command"], str):
                    raise ValueError(f"Command for '{field_name}' must be a string")
                    
            else:
                raise ValueError(f"Invalid configuration for field '{field_name}'")

    def resolve(self, field: str, component: Optional[str] = None) -> str:
        """Resolve field and component to Griz command name.
        
        Args:
            field: Human-readable field name (e.g., "stress", "displacement")
            component: Optional component name (e.g., "von_mises", "magnitude")
            
        Returns:
            Griz command name (e.g., "svm", "dmag")
            
        Raises:
            UnknownFieldError: If field or component not found
        """
        if field not in self._mapping:
            available = list(self._mapping.keys())
            raise UnknownFieldError(f"Unknown field '{field}'. Available fields: {', '.join(available)}")
        
        field_config = self._mapping[field]
        
        # Scalar field
        if isinstance(field_config, dict) and "command" in field_config:
            if component is not None:
                raise UnknownFieldError(f"Scalar field '{field}' does not support components")
            return field_config["command"]
        
        # Vector field with components
        if isinstance(field_config, dict) and "components" in field_config:
            components = field_config["components"]
            
            if component is None:
                # Use default component if available
                if "default" in field_config:
                    component = field_config["default"]
                else:
                    available = list(components.keys())
                    raise UnknownFieldError(f"Field '{field}' requires a component. Available: {', '.join(available)}")
            
            if component not in components:
                available = list(components.keys())
                raise UnknownFieldError(f"Unknown component '{component}' for field '{field}'. Available: {', '.join(available)}")
            
            return components[component]
        
        # Legacy format (direct string mapping)
        if isinstance(field_config, str):
            if component is not None:
                raise UnknownFieldError(f"Field '{field}' does not support components")
            return field_config
        
        raise UnknownFieldError(f"Invalid configuration for field '{field}'")
    
    def list_fields(self) -> List[str]:
        """List all available field names."""
        return list(self._mapping.keys())
    
    def list_components(self, field: str) -> List[str]:
        """List available components for a field.
        
        Args:
            field: Field name
            
        Returns:
            List of component names, empty for scalar fields
            
        Raises:
            UnknownFieldError: If field not found
        """
        if field not in self._mapping:
            available = list(self._mapping.keys())
            raise UnknownFieldError(f"Unknown field '{field}'. Available: {', '.join(available)}")
        
        field_config = self._mapping[field]
        
        if isinstance(field_config, dict) and "components" in field_config:
            return list(field_config["components"].keys())
        else:
            return []  # Scalar field
    
    def get_field_info(self, field: str) -> Dict[str, Any]:
        """Get detailed information about a field.
        
        Args:
            field: Field name
            
        Returns:
            Dictionary with field metadata
        """
        if field not in self._mapping:
            raise UnknownFieldError(f"Unknown field '{field}'")
        
        field_config = self._mapping[field]
        
        info = {
            "name": field,
            "description": field_config.get("description", ""),
        }
        
        if isinstance(field_config, dict) and "components" in field_config:
            info["type"] = "vector"
            info["components"] = list(field_config["components"].keys())
            info["default_component"] = field_config.get("default")
        else:
            info["type"] = "scalar" 
            info["command"] = field_config.get("command", field_config)
        
        return info
    
    def search_fields(self, query: str) -> List[str]:
        """Search for fields matching a query string.
        
        Args:
            query: Search string (case insensitive)
            
        Returns:
            List of matching field names
        """
        query_lower = query.lower()
        matches = []
        
        for field_name, field_config in self._mapping.items():
            # Match field name
            if query_lower in field_name.lower():
                matches.append(field_name)
                continue
            
            # Match description
            description = field_config.get("description", "")
            if query_lower in description.lower():
                matches.append(field_name)
                continue
        
        return matches

# Global instance for convenience
_global_results_map = None

def get_results_map() -> ResultsMap:
    """Get the global results map instance."""
    global _global_results_map
    if _global_results_map is None:
        _global_results_map = ResultsMap()
    return _global_results_map

def resolve_field(field: str, component: Optional[str] = None) -> str:
    """Convenience function for field resolution."""
    return get_results_map().resolve(field, component)
```

### 3.2 Integration with Field API

```python
# griz/field.py (modified to use results mapping)
from .results_map import get_results_map
from .exceptions import UnknownFieldError

class FieldAPI:
    """Field and result operations."""
    
    def __init__(self, griz: "Griz"):
        self._griz = griz
        self._results_map = get_results_map()
    
    def show(self, name: str, *, component: str | None = None) -> dict:
        """Display a field/result with optional component.
        
        Args:
            name: Human-readable field name (e.g., "stress", "displacement")
            component: Optional component (e.g., "von_mises", "magnitude")
            
        Returns:
            Current state dict after field change
            
        Raises:
            UnknownFieldError: If field or component not found
        """
        try:
            # Resolve field name through mapping
            griz_command = self._results_map.resolve(name, component)
            
            # Send commands to server
            self._griz._worker.cmd(f"res {griz_command}")
            self._griz._worker.cmd("show result")
            
            # Return current state
            return self._griz.state()
            
        except UnknownFieldError:
            # Re-raise with additional context
            raise
        except Exception as e:
            raise GrizError(f"Failed to show field '{name}': {e}")
    
    def list_available(self) -> List[Dict[str, Any]]:
        """List all available fields from the mapping.
        
        Returns:
            List of field information dictionaries
        """
        fields = []
        for field_name in self._results_map.list_fields():
            try:
                field_info = self._results_map.get_field_info(field_name)
                fields.append(field_info)
            except Exception:
                # Skip invalid fields
                continue
        
        return fields
    
    def list_database_results(self) -> List[Dict[str, Any]]:
        """List results actually available in current database.
        
        Returns:
            List of result descriptors from server
        """
        result = self._griz._worker.cmd("q_results")
        if result["status"] == "error":
            raise GrizError(f"Failed to query database results: {result['error']['message']}")
        
        return result.get("data", {}).get("results", [])
    
    def search(self, query: str) -> List[str]:
        """Search for fields matching query string.
        
        Args:
            query: Search term
            
        Returns:
            List of matching field names
        """
        return self._results_map.search_fields(query)
```

## 4. CLI Tools and Utilities

### 4.1 Field Information Tool

```python
#!/usr/bin/env python3
# griz/tools/field_info.py
"""
Tool for exploring available fields and their mappings.
"""

import argparse
import sys
from griz.results_map import get_results_map

def main():
    parser = argparse.ArgumentParser(description="Explore Griz field mappings")
    parser.add_argument("--list", action="store_true", help="List all available fields")
    parser.add_argument("--field", type=str, help="Show info for specific field")
    parser.add_argument("--search", type=str, help="Search fields by keyword")
    parser.add_argument("--components", type=str, help="List components for field")
    parser.add_argument("--resolve", nargs=2, metavar=("FIELD", "COMPONENT"), 
                       help="Resolve field+component to Griz command")
    
    args = parser.parse_args()
    
    try:
        results_map = get_results_map()
        
        if args.list:
            print("Available fields:")
            for field in sorted(results_map.list_fields()):
                info = results_map.get_field_info(field)
                print(f"  {field:20} - {info['description']}")
        
        elif args.field:
            info = results_map.get_field_info(args.field)
            print(f"Field: {info['name']}")
            print(f"Description: {info['description']}")
            print(f"Type: {info['type']}")
            
            if info['type'] == 'vector':
                print(f"Components: {', '.join(info['components'])}")
                if info['default_component']:
                    print(f"Default: {info['default_component']}")
            else:
                print(f"Command: {info['command']}")
        
        elif args.search:
            matches = results_map.search_fields(args.search)
            if matches:
                print(f"Fields matching '{args.search}':")
                for field in matches:
                    info = results_map.get_field_info(field)
                    print(f"  {field:20} - {info['description']}")
            else:
                print(f"No fields found matching '{args.search}'")
        
        elif args.components:
            components = results_map.list_components(args.components)
            if components:
                print(f"Components for '{args.components}': {', '.join(components)}")
            else:
                print(f"Field '{args.components}' is scalar (no components)")
        
        elif args.resolve:
            field, component = args.resolve
            command = results_map.resolve(field, component)
            print(f"{field}::{component} → {command}")
        
        else:
            parser.print_help()
    
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
```

### 4.2 YAML Validation Tool

```python
#!/usr/bin/env python3
# tools/validate_results_map.py
"""
Validate results mapping YAML file.
"""

import argparse
import sys
from griz.results_map import ResultsMap

def main():
    parser = argparse.ArgumentParser(description="Validate results mapping YAML")
    parser.add_argument("yaml_file", help="Path to YAML file to validate")
    parser.add_argument("--verbose", action="store_true", help="Verbose output")
    
    args = parser.parse_args()
    
    try:
        # Load and validate mapping
        results_map = ResultsMap(args.yaml_file)
        
        print(f"✓ YAML file '{args.yaml_file}' is valid")
        
        if args.verbose:
            fields = results_map.list_fields()
            print(f"  Found {len(fields)} fields:")
            
            for field in sorted(fields):
                info = results_map.get_field_info(field)
                print(f"    {field} ({info['type']})")
                
                if info['type'] == 'vector':
                    components = info['components']
                    print(f"      Components: {', '.join(components)}")
    
    except Exception as e:
        print(f"✗ Validation failed: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
```

## 5. Documentation Generation

### 5.1 Automated Documentation

```python
# tools/generate_field_docs.py
"""
Generate documentation from results mapping YAML.
"""

def generate_markdown_docs(results_map: ResultsMap) -> str:
    """Generate Markdown documentation for all fields."""
    
    docs = "# Griz Field Reference\n\n"
    docs += "This document lists all available fields for visualization in Griz.\n\n"
    
    # Group fields by category
    categories = {
        "Mechanical": ["stress", "displacement", "velocity", "acceleration"],
        "Thermal": ["temperature", "heat_flux"],
        "Material": ["density", "young_modulus", "poisson_ratio"],
        "Quality": ["aspect_ratio", "volume"],
        "Identifiers": ["material_id", "element_id", "node_id"],
    }
    
    for category, field_names in categories.items():
        docs += f"## {category} Fields\n\n"
        
        for field_name in field_names:
            if field_name not in results_map.list_fields():
                continue
                
            info = results_map.get_field_info(field_name)
            docs += f"### {field_name}\n\n"
            docs += f"{info['description']}\n\n"
            
            if info['type'] == 'vector':
                docs += "**Components:**\n"
                for comp in info['components']:
                    griz_cmd = results_map.resolve(field_name, comp)
                    docs += f"- `{comp}` → `{griz_cmd}`\n"
                
                if info['default_component']:
                    docs += f"\n**Default:** `{info['default_component']}`\n"
            else:
                docs += f"**Command:** `{info['command']}`\n"
            
            docs += f"\n**Usage:**\n"
            if info['type'] == 'vector':
                docs += f"```python\n"
                docs += f"g.field.show('{field_name}', component='{info.get('default_component', 'x')}')\n"
                docs += f"```\n"
            else:
                docs += f"```python\n"
                docs += f"g.field.show('{field_name}')\n"
                docs += f"```\n"
            
            docs += "\n"
    
    return docs

def main():
    """Generate field documentation."""
    from griz.results_map import get_results_map
    
    results_map = get_results_map()
    docs = generate_markdown_docs(results_map)
    
    with open("docs/field_reference.md", "w") as f:
        f.write(docs)
    
    print("Generated docs/field_reference.md")

if __name__ == "__main__":
    main()
```

## 6. Testing Strategy

### 6.1 Unit Tests

```python
# tests/test_results_map.py
import pytest
import tempfile
import os
from griz.results_map import ResultsMap
from griz.exceptions import UnknownFieldError

class TestResultsMap:
    
    def test_load_valid_yaml(self):
        """Test loading valid YAML mapping."""
        yaml_content = """
stress:
  components:
    xx: "sx"
    von_mises: "svm"
  default: "von_mises"
  
temperature:
  command: "temp"
"""
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.yaml', delete=False) as f:
            f.write(yaml_content)
            yaml_path = f.name
        
        try:
            results_map = ResultsMap(yaml_path)
            assert "stress" in results_map.list_fields()
            assert "temperature" in results_map.list_fields()
        finally:
            os.unlink(yaml_path)
    
    def test_resolve_vector_field(self):
        """Test resolving vector field with component."""
        yaml_content = """
stress:
  components:
    xx: "sx"
    von_mises: "svm"
  default: "von_mises"
"""
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.yaml', delete=False) as f:
            f.write(yaml_content)
            yaml_path = f.name
        
        try:
            results_map = ResultsMap(yaml_path)
            
            # Explicit component
            assert results_map.resolve("stress", "xx") == "sx"
            assert results_map.resolve("stress", "von_mises") == "svm"
            
            # Default component
            assert results_map.resolve("stress") == "svm"
            
        finally:
            os.unlink(yaml_path)
    
    def test_resolve_scalar_field(self):
        """Test resolving scalar field."""
        yaml_content = """
temperature:
  command: "temp"
  description: "Temperature field"
"""
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.yaml', delete=False) as f:
            f.write(yaml_content)
            yaml_path = f.name
        
        try:
            results_map = ResultsMap(yaml_path)
            assert results_map.resolve("temperature") == "temp"
            
            # Should reject component for scalar field
            with pytest.raises(UnknownFieldError):
                results_map.resolve("temperature", "x")
                
        finally:
            os.unlink(yaml_path)
    
    def test_unknown_field_error(self):
        """Test error handling for unknown fields."""
        yaml_content = """
stress:
  components:
    xx: "sx"
"""
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.yaml', delete=False) as f:
            f.write(yaml_content)
            yaml_path = f.name
        
        try:
            results_map = ResultsMap(yaml_path)
            
            with pytest.raises(UnknownFieldError, match="Unknown field 'displacement'"):
                results_map.resolve("displacement")
                
            with pytest.raises(UnknownFieldError, match="Unknown component 'yy'"):
                results_map.resolve("stress", "yy")
                
        finally:
            os.unlink(yaml_path)
    
    def test_field_info(self):
        """Test field info extraction.""" 
        yaml_content = """
stress:
  description: "Stress tensor"
  components:
    xx: "sx"
    von_mises: "svm"
  default: "von_mises"
"""
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.yaml', delete=False) as f:
            f.write(yaml_content)
            yaml_path = f.name
        
        try:
            results_map = ResultsMap(yaml_path)
            info = results_map.get_field_info("stress")
            
            assert info["name"] == "stress"
            assert info["description"] == "Stress tensor"
            assert info["type"] == "vector"
            assert "xx" in info["components"]
            assert "von_mises" in info["components"]
            assert info["default_component"] == "von_mises"
            
        finally:
            os.unlink(yaml_path)
    
    def test_search_fields(self):
        """Test field search functionality."""
        yaml_content = """
stress:
  description: "Stress tensor components"
  components:
    von_mises: "svm"

displacement:
  description: "Nodal displacement vector"  
  components:
    magnitude: "dmag"
    
temperature:
  description: "Temperature field"
  command: "temp"
"""
        
        with tempfile.NamedTemporaryFile(mode='w', suffix='.yaml', delete=False) as f:
            f.write(yaml_content)
            yaml_path = f.name
        
        try:
            results_map = ResultsMap(yaml_path)
            
            # Search by field name
            assert "stress" in results_map.search_fields("stress")
            
            # Search by description
            assert "displacement" in results_map.search_fields("nodal")
            assert "temperature" in results_map.search_fields("temperature")
            
            # Case insensitive
            assert "stress" in results_map.search_fields("STRESS")
            
        finally:
            os.unlink(yaml_path)
```

### 6.2 Integration Tests

```python
# tests/test_field_api_integration.py
import pytest
from unittest.mock import MagicMock
from griz.field import FieldAPI
from griz.results_map import ResultsMap

def test_field_api_with_results_mapping():
    """Test field API integration with results mapping."""
    
    # Mock Griz session
    mock_griz = MagicMock()
    mock_worker = MagicMock()
    mock_griz._worker = mock_worker
    
    # Mock successful command execution
    mock_worker.cmd.return_value = {"status": "ok", "stdout": ""}
    mock_griz.state.return_value = {"time_state": 42}
    
    # Create field API
    field_api = FieldAPI(mock_griz)
    
    # Test field resolution and execution
    result = field_api.show("stress", component="von_mises")
    
    # Verify commands were sent correctly
    expected_calls = [
        ("res svm",),  # Resolved stress::von_mises → svm
        ("show result",)
    ]
    
    actual_calls = [call[0] for call in mock_worker.cmd.call_args_list]
    assert actual_calls == [args[0] for args in expected_calls]
    
    # Verify result
    assert result == {"time_state": 42}
```

## 7. Performance Considerations

### 7.1 Caching Strategy

```python
# Optimization: Cache resolved mappings
class ResultsMap:
    def __init__(self, yaml_path=None):
        # ... existing initialization ...
        self._resolution_cache = {}  # Cache for resolve() calls
    
    def resolve(self, field: str, component: str = None) -> str:
        # Check cache first
        cache_key = (field, component)
        if cache_key in self._resolution_cache:
            return self._resolution_cache[cache_key]
        
        # ... existing resolution logic ...
        
        # Cache result
        self._resolution_cache[cache_key] = result
        return result
```

### 7.2 Lazy Loading

```python
# Optimization: Lazy load YAML only when needed
_global_results_map = None

def get_results_map() -> ResultsMap:
    global _global_results_map
    if _global_results_map is None:
        _global_results_map = ResultsMap()  # Load on first access
    return _global_results_map
```

## 8. Maintenance and Evolution

### 8.1 Adding New Fields

**Process for adding new field**:
1. Add entry to `Src/data/results_map.yaml`
2. Update documentation generation
3. Add test cases
4. Update any hardcoded field lists in tools

**Example addition**:
```yaml
# New field for plastic deformation
plastic_work:
  description: "Plastic work per unit volume"
  command: "pwork"
```

### 8.2 Deprecation Strategy

**For removing/renaming fields**:
1. Mark as deprecated in YAML comments
2. Add deprecation warning in Python code
3. Provide migration guide in release notes
4. Remove after appropriate grace period

**Example deprecation**:
```yaml
# DEPRECATED: Use 'displacement' instead
displ:
  description: "DEPRECATED: Use 'displacement' field instead"
  components:
    x: "dx"  # Still works but warns
```

## 9. Open Questions

1. **Field validation**: Should we validate that mapped commands actually exist in the current database? Could be expensive.

2. **Dynamic fields**: How to handle database-specific custom fields not in the static mapping? Escape hatch via `raw()` command?

3. **Units**: Should the mapping include unit information? Would help with display formatting.

4. **Aliases**: Should we support multiple names for the same field? Could help with user migration.

5. **Localization**: Should field names and descriptions support multiple languages? Probably overkill for v1.

## Success Criteria

- Single YAML file successfully shared between Python and Qt UI
- Intuitive, discoverable field names for all common result types
- Clear error messages for unknown fields with helpful suggestions
- Performance suitable for interactive use (sub-millisecond resolution)
- Easy to extend with new fields without code changes
- Comprehensive test coverage for all mapping scenarios
- Generated documentation stays synchronized with YAML content
- Smooth migration path for any future field name changes