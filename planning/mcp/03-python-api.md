# 03 — Python API Design

## Scope

This document details the design and implementation of the `griz` Python package, which provides the primary programmatic interface to Griz. This includes the `Griz` class, namespaced sub-APIs, the subprocess worker, results mapping, and all supporting infrastructure. The package serves both direct Python users and the MCP adapter layer.

## Related

- [01-architecture.md](01-architecture.md) — overall system design
- [02-server-binary.md](02-server-binary.md) — server implementation
- [04-mcp-adapter.md](04-mcp-adapter.md) — MCP tool wrappers
- [05-protocol.md](05-protocol.md) — JSON communication protocol
- `../shared/results-map.md` — field name mapping specification

## 1. Package Overview

### 1.1 Package Structure

```
Src/python/griz/
├── __init__.py           # Public exports: Griz, exceptions
├── pyproject.toml        # Build configuration
├── README.md            # User documentation
├── session.py           # Griz class and lifecycle
├── worker.py            # Subprocess management and protocol
├── field.py             # Field/result operations
├── view.py              # Camera and view controls  
├── time.py              # Time state management
├── materials.py         # Material visibility
├── selection.py         # Picking and selection
├── results_map.py       # YAML field name resolution
├── exceptions.py        # Exception hierarchy
├── data/
│   └── results_map.yaml # Field mapping (symlink to shared)
└── tests/
    ├── test_session.py
    ├── test_worker.py
    ├── test_field.py
    ├── fixtures/
    │   └── small.plt    # Test database
    └── mock_server.py   # Mock griz-server for testing
```

### 1.2 Public API Surface

**Primary interface**:
```python
from griz import Griz

# Context manager (recommended)
with Griz("data.plt", width=1024, height=1024) as g:
    g.time.set_state(42)
    g.field.show("stress", component="von_mises")
    g.view.rotate(x=30, y=0, z=0)
    g.materials.hide([3, 7])
    screenshot_data = g.screenshot()

# Manual lifecycle
g = Griz()
g.open("data.plt")
g.close()
```

## 2. Core Session Class

### 2.1 Griz Class Design

```python
class Griz:
    """Main interface to Griz visualization engine."""
    
    def __init__(self, database: str | None = None, *,
                 griz_bin: str | None = None,
                 width: int = 1024, 
                 height: int = 1024,
                 timeout: float = 30.0) -> None:
        """Initialize Griz session.
        
        Args:
            database: Path to database file to open immediately
            griz_bin: Path to griz-server binary (default: from PATH)
            width: Render width in pixels
            height: Render height in pixels
            timeout: Default timeout for commands in seconds
        """
        
    # Lifecycle management
    def open(self, path: str) -> None:
        """Open a database file."""
        
    def reload(self) -> None:
        """Reload the current database."""
        
    def close(self) -> None:
        """Close current database and session."""
        
    def __enter__(self) -> "Griz":
        """Context manager entry."""
        
    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """Context manager exit with cleanup."""
    
    # State inspection
    def state(self) -> dict:
        """Get current viewer state as structured dict."""
        
    def screenshot(self, path: str | None = None) -> str | bytes:
        """Capture screenshot. Returns path if given, bytes otherwise."""
        
    # Escape hatch
    def raw(self, command: str, *, timeout: float | None = None) -> dict:
        """Send raw command to server, return JSON response."""
        
    # Namespaced APIs (properties)
    @property
    def field(self) -> "FieldAPI":
        """Field and result operations."""
        
    @property  
    def view(self) -> "ViewAPI":
        """Camera and view controls."""
        
    @property
    def time(self) -> "TimeAPI":
        """Time state management."""
        
    @property
    def materials(self) -> "MaterialsAPI":
        """Material visibility controls."""
```

### 2.2 Lifecycle Implementation

```python
class Griz:
    def __init__(self, database=None, *, griz_bin=None, width=1024, height=1024, timeout=30.0):
        self._worker = None
        self._database_path = None
        self._griz_bin = griz_bin or self._find_griz_server()
        self._width = width
        self._height = height
        self._timeout = timeout
        
        # Initialize namespaced APIs
        self._field = FieldAPI(self)
        self._view = ViewAPI(self)  
        self._time = TimeAPI(self)
        self._materials = MaterialsAPI(self)
        
        # Auto-open database if provided
        if database:
            self.open(database)
    
    def open(self, path: str) -> None:
        """Open database, spawning server if needed."""
        if not os.path.exists(path):
            raise FileNotFoundError(f"Database not found: {path}")
            
        # Spawn worker if not already running
        if self._worker is None:
            self._worker = Worker(
                griz_bin=self._griz_bin,
                width=self._width, 
                height=self._height,
                timeout=self._timeout
            )
        
        # Send open command
        result = self._worker.cmd(f"open {shlex.quote(path)}")
        if result["status"] == "error":
            raise GrizError(f"Failed to open database: {result['error']['message']}")
            
        self._database_path = path
    
    def close(self) -> None:
        """Close database and terminate worker."""
        if self._worker:
            try:
                self._worker.cmd("quit")
            except Exception:
                pass  # Worker may already be dead
            finally:
                self._worker.cleanup()
                self._worker = None
        self._database_path = None
    
    def __enter__(self):
        return self
        
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
```

## 3. Worker Implementation

### 3.1 Subprocess Management

```python
import subprocess
import json
import threading
import queue
from typing import Dict, Any

class Worker:
    """Manages griz-server subprocess and JSON protocol."""
    
    def __init__(self, griz_bin: str, width: int, height: int, timeout: float):
        self.griz_bin = griz_bin
        self.timeout = timeout
        self._proc = None
        self._lock = threading.Lock()
        self._stderr_thread = None
        self._stderr_queue = queue.Queue()
        self._next_id = 1
        
        self._spawn_server(width, height)
    
    def _spawn_server(self, width: int, height: int) -> None:
        """Spawn griz-server subprocess."""
        cmd = [
            self.griz_bin,
            "--transport=stdio",
            "-w", str(width), str(height)
        ]
        
        self._proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1  # Line buffered
        )
        
        # Start stderr monitoring thread
        self._stderr_thread = threading.Thread(
            target=self._monitor_stderr,
            daemon=True
        )
        self._stderr_thread.start()
        
        # Wait for ready handshake
        ready_event = self._read_event()
        if ready_event.get("event") != "ready":
            raise GrizError("Server failed to start properly")
        
        # Complete handshake
        self._send_hello()
    
    def _monitor_stderr(self) -> None:
        """Background thread to capture stderr output."""
        while self._proc and self._proc.poll() is None:
            try:
                line = self._proc.stderr.readline()
                if line:
                    self._stderr_queue.put(line.strip())
            except Exception:
                break
    
    def cmd(self, command: str, *, timeout: float | None = None) -> Dict[str, Any]:
        """Send command and wait for response."""
        with self._lock:
            return self._send_command(command, timeout or self.timeout)
    
    def _send_command(self, command: str, timeout: float) -> Dict[str, Any]:
        """Send command with timeout (called under lock)."""
        if not self._proc or self._proc.poll() is not None:
            raise GrizError("Server process is not running")
        
        # Generate request ID
        request_id = str(self._next_id)
        self._next_id += 1
        
        # Build request
        request = {
            "type": "request",
            "id": request_id,
            "cmd": command
        }
        
        # Send request
        request_line = json.dumps(request) + "\n"
        self._proc.stdin.write(request_line)
        self._proc.stdin.flush()
        
        # Wait for response
        response = self._read_response_with_timeout(request_id, timeout)
        
        # Check for stderr messages
        self._check_stderr_messages()
        
        return response
    
    def _read_response_with_timeout(self, request_id: str, timeout: float) -> Dict[str, Any]:
        """Read response with timeout."""
        import select
        
        if hasattr(select, 'poll'):
            # Unix-like systems
            return self._read_response_unix(request_id, timeout)
        else:
            # Windows fallback
            return self._read_response_fallback(request_id, timeout)
    
    def cleanup(self) -> None:
        """Clean up subprocess and resources."""
        if self._proc:
            # Try graceful shutdown first
            try:
                if self._proc.poll() is None:
                    self._proc.stdin.close()
                    self._proc.wait(timeout=5.0)
            except (subprocess.TimeoutExpired, Exception):
                # Force kill if needed
                self._proc.kill()
                self._proc.wait()
            finally:
                self._proc = None
```

### 3.2 Protocol Handling

```python
def _send_hello(self) -> None:
    """Send hello handshake."""
    hello = {
        "type": "hello",
        "version": "1.0.0",
        "client": "griz-python"
    }
    
    hello_line = json.dumps(hello) + "\n"
    self._proc.stdin.write(hello_line)
    self._proc.stdin.flush()
    
    # Wait for hello_ack
    ack = self._read_event()
    if ack.get("type") != "hello_ack":
        raise GrizError("Handshake failed")

def _read_event(self) -> Dict[str, Any]:
    """Read a single line and parse as JSON."""
    line = self._proc.stdout.readline()
    if not line:
        raise GrizError("Server closed connection")
    
    try:
        return json.loads(line.strip())
    except json.JSONDecodeError as e:
        raise GrizError(f"Invalid JSON from server: {e}")

def _check_stderr_messages(self) -> None:
    """Check for stderr messages and surface as warnings."""
    messages = []
    try:
        while True:
            msg = self._stderr_queue.get_nowait()
            messages.append(msg)
    except queue.Empty:
        pass
    
    if messages:
        import warnings
        warnings.warn(f"Server warnings: {'; '.join(messages)}")
```

## 4. Namespaced APIs

### 4.1 Field API

```python
class FieldAPI:
    """Field and result operations."""
    
    def __init__(self, griz: "Griz"):
        self._griz = griz
    
    def show(self, name: str, *, component: str | None = None) -> dict:
        """Display a field/result.
        
        Args:
            name: Field name (e.g., "stress", "displacement", "temperature")
            component: Component name (e.g., "xx", "von_mises", "magnitude")
        
        Returns:
            Current state dict after field change
        """
        # Resolve field name through results mapping
        griz_name = results_map.resolve(name, component)
        
        # Send commands to server
        self._griz._worker.cmd(f"res {griz_name}")
        self._griz._worker.cmd("show result")
        
        # Return current state
        return self._griz.state()
    
    def list(self) -> list[dict]:
        """List available fields in current database.
        
        Returns:
            List of field descriptors with name, type, components
        """
        result = self._griz._worker.cmd("q_results")
        if result["status"] == "error":
            raise GrizError(f"Failed to query results: {result['error']['message']}")
        
        return result["data"]["results"]
    
    def info(self, name: str) -> dict:
        """Get detailed information about a field.
        
        Args:
            name: Field name
            
        Returns:
            Field metadata (range, units, description, etc.)
        """
        # Implementation depends on available query commands
        griz_name = results_map.resolve(name)
        result = self._griz._worker.cmd(f"q_result_info {griz_name}")
        
        if result["status"] == "error":
            raise GrizError(f"Failed to get field info: {result['error']['message']}")
            
        return result["data"]
```

### 4.2 View API

```python
class ViewAPI:
    """Camera and view controls."""
    
    def __init__(self, griz: "Griz"):
        self._griz = griz
    
    def rotate(self, *, x: float = 0, y: float = 0, z: float = 0) -> dict:
        """Rotate view by specified angles (degrees).
        
        Args:
            x: Rotation around X axis
            y: Rotation around Y axis  
            z: Rotation around Z axis
            
        Returns:
            Current state after rotation
        """
        if x != 0:
            self._griz._worker.cmd(f"rx {x}")
        if y != 0:
            self._griz._worker.cmd(f"ry {y}")
        if z != 0:
            self._griz._worker.cmd(f"rz {z}")
            
        return self._griz.state()
    
    def translate(self, *, x: float = 0, y: float = 0, z: float = 0) -> dict:
        """Translate view by specified distances."""
        if x != 0:
            self._griz._worker.cmd(f"tx {x}")
        if y != 0:
            self._griz._worker.cmd(f"ty {y}")
        if z != 0:
            self._griz._worker.cmd(f"tz {z}")
            
        return self._griz.state()
    
    def scale(self, factor: float) -> dict:
        """Scale view by factor."""
        self._griz._worker.cmd(f"sc {factor}")
        return self._griz.state()
    
    def reset(self) -> dict:
        """Reset view to default."""
        self._griz._worker.cmd("home")
        return self._griz.state()
    
    def center_on_node(self, node_id: int) -> dict:
        """Center view on specified node."""
        self._griz._worker.cmd(f"center_node {node_id}")
        return self._griz.state()
```

### 4.3 Time API

```python
class TimeAPI:
    """Time state management."""
    
    def __init__(self, griz: "Griz"):
        self._griz = griz
    
    def set_state(self, state: int) -> dict:
        """Set time state to specific value.
        
        Args:
            state: Time state number (0-based)
            
        Returns:
            Current state after change
        """
        self._griz._worker.cmd(f"state {state}")
        return self._griz.state()
    
    def set_time(self, time: float) -> dict:
        """Set time to specific value.
        
        Args:
            time: Time value
            
        Returns:
            Current state after change
        """
        self._griz._worker.cmd(f"time {time}")
        return self._griz.state()
    
    def next(self) -> dict:
        """Advance to next time state."""
        self._griz._worker.cmd("next")
        return self._griz.state()
    
    def prev(self) -> dict:
        """Go to previous time state."""
        self._griz._worker.cmd("prev")
        return self._griz.state()
    
    def animate(self, *, start: int | None = None, 
               end: int | None = None, 
               step: int = 1,
               delay: float = 0.1) -> None:
        """Animate through time states.
        
        Args:
            start: Starting state (current if None)
            end: Ending state (max if None)  
            step: Step size between states
            delay: Delay between frames in seconds
        """
        # Get current state info
        current_state = self._griz.state()
        
        start = start if start is not None else current_state["time_state"]
        end = end if end is not None else current_state["max_time_state"]
        
        for state in range(start, end + 1, step):
            self.set_state(state)
            if delay > 0:
                import time
                time.sleep(delay)
```

### 4.4 Materials API

```python
class MaterialsAPI:
    """Material visibility controls."""
    
    def __init__(self, griz: "Griz"):
        self._griz = griz
    
    def hide(self, material_ids: list[int]) -> dict:
        """Hide specified materials.
        
        Args:
            material_ids: List of material IDs to hide
            
        Returns:
            Current state after change
        """
        for mat_id in material_ids:
            self._griz._worker.cmd(f"matoff {mat_id}")
        return self._griz.state()
    
    def show(self, material_ids: list[int]) -> dict:
        """Show specified materials.
        
        Args:
            material_ids: List of material IDs to show
            
        Returns:
            Current state after change
        """
        for mat_id in material_ids:
            self._griz._worker.cmd(f"maton {mat_id}")
        return self._griz.state()
    
    def show_only(self, material_ids: list[int]) -> dict:
        """Show only specified materials, hide all others.
        
        Args:
            material_ids: List of material IDs to show exclusively
            
        Returns:
            Current state after change
        """
        # First hide all materials
        self._griz._worker.cmd("matoff all")
        
        # Then show specified ones
        for mat_id in material_ids:
            self._griz._worker.cmd(f"maton {mat_id}")
            
        return self._griz.state()
    
    def list(self) -> list[dict]:
        """List all materials in current database.
        
        Returns:
            List of material descriptors
        """
        result = self._griz._worker.cmd("q_materials")
        if result["status"] == "error":
            raise GrizError(f"Failed to query materials: {result['error']['message']}")
            
        return result["data"]["materials"]
```

## 5. Results Mapping

### 5.1 YAML Configuration

The results mapping loads from `Src/data/results_map.yaml` (shared with Qt UI):

```yaml
# Stress components
stress:
  xx: "sx"
  yy: "sy" 
  zz: "sz"
  xy: "sxy"
  yz: "syz"
  xz: "sxz"
  von_mises: "svm"
  pressure: "pressure"

# Displacement
displacement:
  x: "dx"
  y: "dy"
  z: "dz"
  magnitude: "dmag"

# Temperature
temperature: "temp"

# Velocity  
velocity:
  x: "vx"
  y: "vy"
  z: "vz"
  magnitude: "vmag"
```

### 5.2 Resolution Logic

```python
# results_map.py
import yaml
import os
from typing import Dict, Any, Optional

class ResultsMap:
    """Handles field name resolution from human-readable to Griz names."""
    
    def __init__(self):
        self._mapping = self._load_mapping()
    
    def _load_mapping(self) -> Dict[str, Any]:
        """Load mapping from YAML file."""
        # Look for YAML file relative to package
        yaml_path = os.path.join(os.path.dirname(__file__), "data", "results_map.yaml")
        
        if not os.path.exists(yaml_path):
            raise FileNotFoundError(f"Results mapping not found: {yaml_path}")
        
        with open(yaml_path, "r") as f:
            return yaml.safe_load(f)
    
    def resolve(self, field: str, component: Optional[str] = None) -> str:
        """Resolve field and optional component to Griz command name.
        
        Args:
            field: Human-readable field name (e.g., "stress")
            component: Optional component (e.g., "von_mises")
            
        Returns:
            Griz command name (e.g., "svm")
            
        Raises:
            UnknownFieldError: If field/component combination not found
        """
        if field not in self._mapping:
            available = list(self._mapping.keys())
            raise UnknownFieldError(f"Unknown field '{field}'. Available: {available}")
        
        field_config = self._mapping[field]
        
        # Simple string mapping (e.g., temperature: "temp")
        if isinstance(field_config, str):
            if component is not None:
                raise UnknownFieldError(f"Field '{field}' does not support components")
            return field_config
        
        # Component mapping (e.g., stress: {xx: "sx", ...})
        if isinstance(field_config, dict):
            if component is None:
                # Return first/default component
                if "default" in field_config:
                    return field_config["default"]
                else:
                    available = list(field_config.keys())
                    raise UnknownFieldError(f"Field '{field}' requires component. Available: {available}")
            
            if component not in field_config:
                available = list(field_config.keys())
                raise UnknownFieldError(f"Unknown component '{component}' for field '{field}'. Available: {available}")
                
            return field_config[component]
        
        raise UnknownFieldError(f"Invalid mapping configuration for field '{field}'")
    
    def list_fields(self) -> list[str]:
        """List all available field names."""
        return list(self._mapping.keys())
    
    def list_components(self, field: str) -> list[str]:
        """List available components for a field."""
        if field not in self._mapping:
            return []
        
        field_config = self._mapping[field]
        if isinstance(field_config, dict):
            return [k for k in field_config.keys() if k != "default"]
        else:
            return []

# Global instance
results_map = ResultsMap()
```

## 6. Exception Hierarchy

```python
# exceptions.py
class GrizError(Exception):
    """Base exception for Griz Python package."""
    pass

class GrizConnectionError(GrizError):
    """Error connecting to or communicating with griz-server."""
    pass

class GrizCommandError(GrizError):
    """Error executing a Griz command."""
    
    def __init__(self, message: str, command: str, server_error: dict | None = None):
        super().__init__(message)
        self.command = command
        self.server_error = server_error

class UnknownFieldError(GrizError):
    """Unknown field or component name in results mapping."""
    pass

class DatabaseError(GrizError):
    """Error loading or accessing database."""
    pass

class RenderingError(GrizError):
    """Error during rendering or screenshot capture."""
    pass
```

## 7. Configuration and Environment

### 7.1 Environment Variables

```python
import os

def get_config_value(key: str, default: Any = None) -> Any:
    """Get configuration value from environment or default."""
    env_vars = {
        "GRIZ_BIN": ("griz_bin", "griz-server"),
        "GRIZ_DEFAULT_WIDTH": ("default_width", 1024),
        "GRIZ_DEFAULT_HEIGHT": ("default_height", 1024), 
        "GRIZ_WORKDIR": ("workdir", "/tmp"),
        "GRIZ_TIMEOUT": ("timeout", 30.0),
    }
    
    if key in env_vars:
        env_key, default_val = env_vars[key]
        value = os.environ.get(env_key.upper(), default_val)
        
        # Type conversion
        if key.endswith("_WIDTH") or key.endswith("_HEIGHT"):
            return int(value)
        elif key == "GRIZ_TIMEOUT":
            return float(value)
        else:
            return value
    
    return default
```

### 7.2 Path Resolution

```python
def find_griz_server() -> str:
    """Find griz-server binary on PATH or common locations."""
    # Check environment variable first
    if "GRIZ_BIN" in os.environ:
        path = os.environ["GRIZ_BIN"]
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    
    # Check PATH
    import shutil
    path = shutil.which("griz-server")
    if path:
        return path
    
    # Check common installation locations
    common_paths = [
        "/usr/local/bin/griz-server",
        "/usr/bin/griz-server",
        os.path.expanduser("~/bin/griz-server"),
    ]
    
    for path in common_paths:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    
    raise FileNotFoundError("griz-server binary not found. Set GRIZ_BIN environment variable.")
```

## 8. Testing Infrastructure

### 8.1 Mock Server

```python
# tests/mock_server.py
import json
import sys

class MockGrizServer:
    """Mock griz-server for testing without real binary."""
    
    def __init__(self):
        self.state = {
            "time_state": 0,
            "max_time_state": 100,
            "current_field": None,
            "camera": {"x": 0, "y": 0, "z": 10},
            "visible_materials": [1, 2, 3]
        }
    
    def run(self):
        """Main loop reading stdin and writing responses."""
        # Send ready event
        self._send_event("ready")
        
        # Wait for hello handshake
        hello = self._read_json()
        if hello.get("type") == "hello":
            self._send_event("hello_ack", {"version": "1.0.0"})
        
        # Command loop
        while True:
            try:
                line = input()
                if not line:
                    break
                
                # Parse request
                if line.startswith("{"):
                    request = json.loads(line)
                    self._handle_request(request)
                else:
                    # Handle raw commands
                    self._handle_command(line, None)
                    
            except (EOFError, KeyboardInterrupt):
                break
    
    def _handle_request(self, request):
        """Handle JSON request."""
        cmd = request.get("cmd", "")
        request_id = request.get("id")
        
        self._handle_command(cmd, request_id)
    
    def _handle_command(self, command, request_id):
        """Handle a command and send response."""
        stdout = ""
        stderr = ""
        data = None
        status = "ok"
        
        try:
            if command.startswith("q_"):
                data = self._handle_query(command)
            else:
                stdout = self._handle_action(command)
        except Exception as e:
            status = "error"
            error = {"code": "command_error", "message": str(e)}
        
        response = {
            "type": "response",
            "status": status,
            "stdout": stdout,
            "stderr": stderr
        }
        
        if request_id:
            response["id"] = request_id
        if data:
            response["data"] = data
        if status == "error":
            response["error"] = error
        
        print(json.dumps(response))
        sys.stdout.flush()
```

### 8.2 Test Cases

```python
# tests/test_session.py
import pytest
from griz import Griz
from griz.exceptions import GrizError

class TestGrizSession:
    
    def test_context_manager(self, mock_database):
        """Test context manager lifecycle."""
        with Griz(mock_database) as g:
            assert g._database_path == mock_database
            assert g._worker is not None
        
        # Should be cleaned up after exit
        assert g._worker is None
    
    def test_manual_lifecycle(self, mock_database):
        """Test manual open/close."""
        g = Griz()
        assert g._worker is None
        
        g.open(mock_database) 
        assert g._worker is not None
        assert g._database_path == mock_database
        
        g.close()
        assert g._worker is None
    
    def test_invalid_database(self):
        """Test error on invalid database path."""
        with pytest.raises(FileNotFoundError):
            Griz("/nonexistent/path.plt")
    
    def test_raw_command(self, mock_session):
        """Test raw command execution."""
        result = mock_session.raw("state 42")
        assert result["status"] == "ok"
        
    def test_state_query(self, mock_session):
        """Test state querying."""
        state = mock_session.state()
        assert "time_state" in state
        assert "camera" in state
```

## 9. Dependencies and Requirements

### 9.1 Python Dependencies

```toml
# pyproject.toml
[build-system]
requires = ["setuptools>=45", "wheel"]
build-backend = "setuptools.build_meta"

[project]
name = "griz"
version = "1.0.0"
description = "Python interface to Griz visualization"
authors = [{name = "Griz Team", email = "griz@llnl.gov"}]
license = {text = "BSD-3-Clause"}
requires-python = ">=3.8"
dependencies = [
    "pyyaml>=5.0",
]

[project.optional-dependencies]
test = [
    "pytest>=6.0",
    "pytest-cov",
    "pytest-mock",
]
dev = [
    "black",
    "isort", 
    "mypy",
    "flake8",
]

[project.urls]
"Homepage" = "https://github.com/llnl/griz"
"Documentation" = "https://griz.readthedocs.io/"
```

### 9.2 Runtime Requirements

- Python 3.8+
- `griz-server` binary accessible on PATH or via `GRIZ_BIN`
- OSMesa libraries (for headless rendering)
- Sufficient disk space for temporary screenshot files

## 10. Open Questions

1. **Async support**: Should we provide `asyncio`-compatible interfaces? Current design is synchronous.

2. **Connection pooling**: Should multiple `Griz` instances share server processes? Current design spawns per-instance.

3. **Progress callbacks**: For long operations, should we support progress callbacks? Not implemented in v1.

4. **Streaming results**: For very large output, should we support streaming? Current design buffers all output.

5. **Session persistence**: Should sessions survive across Python restarts? Current design doesn't support this.

## Success Criteria

- Clean, discoverable Python API following conventions
- Robust subprocess management with proper cleanup
- Comprehensive error handling and user-friendly messages  
- Full mapping of Griz functionality to Python methods
- Extensive test coverage with both unit and integration tests
- Performance suitable for interactive and batch use
- Easy installation and configuration
- Good documentation with examples