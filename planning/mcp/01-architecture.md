# 01 — MCP Architecture

## Scope

This document defines the overall system architecture for the MCP (Model Context Protocol) implementation in Griz. It establishes the layered design, component boundaries, data flow patterns, and key architectural decisions that guide all subsequent implementation work. This covers the three-tier architecture: MCP tools → Python API → C server binary, and how they interact through stdio JSON communication.

## Related

- `../MCP.md` — high-level goals and requirements
- `../shared/server-binary.md` — shared server binary design
- `../shared/command-protocol.md` — JSON envelope and handshake
- `../shared/output-capture.md` — stdout/stderr capture
- `../shared/query-commands.md` — structured query interface
- `../shared/results-map.md` — field name mapping
- [02-server-binary.md](02-server-binary.md) — server implementation details
- [03-python-api.md](03-python-api.md) — Python package design
- [04-mcp-adapter.md](04-mcp-adapter.md) — MCP tool wrappers

## 1. System Overview

The MCP implementation exposes Griz's visualization capabilities to AI assistants and Python scripts through a three-layer architecture:

```
 MCP client (Claude, ...)          User's Python script / notebook
       │                                      │
       ▼                                      │
 ┌───────────────┐                            │
 │   griz-mcp    │  @mcp.tool() wrappers,     │
 │   (Python)    │  image formatting, etc.    │
 └───────────────┘                            │
       │                                      │
       └────────────┬─────────────────────────┘
                    ▼
             ┌────────────┐
             │    griz    │  Public Python API.
             │  (Python   │  Griz class with .field, .view, .time,
             │  package)  │  .materials, .select, .screenshot, …
             └────────────┘
                    │
                    ▼    stdio + line-delimited JSON
             ┌──────────────────────┐
             │  griz-server         │  New binary (shared with Qt UI
             │  --transport=stdio   │  effort), produced from batchopt.
             └──────────────────────┘
```

### Key Architectural Properties

1. **One public API, two front ends**: The `griz` package is the primary interface for users. MCP is a thin adapter layer.
2. **Out-of-process Python**: Griz remains pure C. Python communicates over stdio, avoiding embedding complexity.
3. **Shared server binary**: `griz-server` supports both `--transport=stdio` (MCP) and `--transport=rpc` (Qt UI).
4. **Clean abstraction layers**: Each tier has well-defined responsibilities and can evolve independently.

## 2. Component Architecture

### 2.1 griz-server (C Binary)

**Purpose**: Headless Griz with JSON protocol support
**Built from**: Existing `batchopt` target + new server modules
**Key modules**:
- `server_stdio.c` — stdio transport handler
- `server_core.c` — shared command dispatch and JSON framing
- Modified `viewer.c` — new `process_server_mode_stdio()` function
- Enhanced `interpret.c` — output capture integration

**Responsibilities**:
- Accept commands via stdin in JSON format
- Execute commands through existing `interpret.c` dispatcher
- Return structured responses via stdout
- Capture and redirect all text output
- Provide query commands for state inspection
- Handle session lifecycle (startup handshake, clean shutdown)

### 2.2 griz (Python Package)

**Purpose**: Primary Python API for Griz automation
**Location**: `Src/python/griz/`
**Key modules**:
- `session.py` — `Griz` class and lifecycle management
- `worker.py` — subprocess management and JSON protocol
- `field.py` — field/result operations (`g.field.show()`, etc.)
- `view.py` — camera and view controls
- `time.py` — time state management
- `materials.py` — material visibility
- `selection.py` — picking and selection
- `results_map.py` — YAML-based field name resolution

**Responsibilities**:
- Spawn and manage `griz-server` subprocess
- Serialize Python method calls to Griz commands
- Parse and validate JSON responses
- Provide clean, typed Python interfaces
- Handle error translation and recovery
- Manage result name mapping (stress → sx, etc.)

### 2.3 griz-mcp (MCP Adapter)

**Purpose**: Expose griz package as MCP tools
**Location**: `Src/python/griz_mcp/`
**Key modules**:
- `server.py` — MCP server entrypoint
- `tools.py` — `@mcp.tool()` function wrappers

**Responsibilities**:
- Maintain single global `Griz` session
- Wrap Python API methods as MCP tools
- Handle MCP-specific serialization (Images, etc.)
- Provide tool metadata and documentation
- Manage session lifecycle for MCP clients

## 3. Data Flow Patterns

### 3.1 Command Execution Flow

```
MCP Tool Call → Python API Method → JSON Command → interpret.c → Response
     │                │                  │             │           │
     │                │                  │             │           ▼
     │                │                  │             │    Text Output Capture
     │                │                  │             │           │
     │                │                  │             ▼           │
     │                │                  │       State Changes     │
     │                │                  │             │           │
     │                │                  ▼             │           │
     │                │           Structured Query      │           │
     │                │                  │             │           │
     │                ▼                  ▼             ▼           ▼
     │         Python Result ◄─── JSON Response ◄─────────────────┘
     │                │
     ▼                ▼
MCP Response
```

### 3.2 Session Lifecycle

1. **Startup**: MCP server launches → first tool call → spawn griz-server → handshake
2. **Operation**: Tool calls → API methods → commands → responses
3. **State queries**: On-demand queries for current viewer state
4. **Shutdown**: MCP disconnect → cleanup → terminate griz-server

### 3.3 Error Handling

- **C errors**: Captured via output sinks → JSON error response → Python exception → MCP error
- **Python errors**: Direct exception → MCP error with traceback
- **Protocol errors**: JSON parsing failures → connection reset → recovery

## 4. Interface Contracts

### 4.1 JSON Protocol (Python ↔ C)

**Request format**:
```json
{"type": "request", "id": "42", "cmd": "rx 30"}
```

**Response format**:
```json
{
  "type": "response", 
  "id": "42", 
  "status": "ok|error",
  "stdout": "...",
  "stderr": "...",
  "data": null | {...}
}
```

**Event format** (query responses):
```json
{
  "type": "response",
  "id": "42",
  "status": "ok", 
  "data": {
    "time_state": 42,
    "current_field": "sx",
    "camera": {"x": 0, "y": 0, "z": 10},
    ...
  }
}
```

### 4.2 Python API Surface

**Core class**:
```python
class Griz:
    def __init__(database=None, *, griz_bin=None, width=1024, height=1024)
    def open(path: str) → None
    def close() → None
    def state() → dict
    def screenshot(path=None) → str | bytes
    def raw(command: str) → dict  # escape hatch
    
    # Namespaced APIs
    field: FieldAPI
    view: ViewAPI  
    time: TimeAPI
    materials: MaterialsAPI
```

### 4.3 MCP Tool Interface

**Representative tools**:
- `open_database(path: str) → dict`
- `show_field(name: str, component: str = None) → dict`
- `rotate_view(x: float, y: float, z: float) → dict`
- `screenshot() → Image`
- `set_time_state(state: int) → dict`
- `raw_command(cmd: str) → dict`

## 5. Key Design Decisions

### 5.1 Layered Architecture Rationale

**Why three layers?**
- Separation of concerns: MCP tools are stateless wrappers
- Python API is reusable outside MCP context  
- C server can support multiple transports (stdio + rpc)
- Each layer can evolve independently

**Why stdio instead of sockets?**
- Simpler deployment (no port management)
- Natural process lifecycle coupling
- Easier debugging and logging
- Matches existing batch mode patterns

### 5.2 Process Model

**Why subprocess instead of embedding?**
- Griz is not designed for in-process use (main() calls exit())
- Avoids complex C extension building
- Isolates crashes and memory leaks
- Enables timeout and watchdog capabilities
- Preserves existing Griz build system

### 5.3 State Management

**Server-authoritative state**:
- All viewer state lives in C (camera, time, materials, etc.)
- Python layer caches minimally, queries on demand
- Enables multiple client reconnection scenarios
- Matches existing Griz state model

## 6. Integration Points

### 6.1 Shared Components with UI Effort

The following components are shared between MCP and Qt UI implementations:

- **Server binary**: Same `griz-server` executable, different `--transport` flag
- **Command protocol**: Identical JSON envelope and handshake sequence  
- **Output capture**: Same `griz_out()`/`griz_err()` sink redirection
- **Query commands**: Same `q_state`, `q_view`, etc. structured queries
- **Results mapping**: Same `results_map.yaml` for field name resolution

### 6.2 Build System Integration

- `griz-server` built from `batchopt` + server modules
- Python packages in `Src/python/` with separate `pyproject.toml` files
- No changes to existing `Makefile.Library` for legacy targets
- CI builds both C server and Python packages

## 7. Constraints and Invariants

### 7.1 Compatibility Constraints

- **Existing scripts must work**: Legacy `griz` and `griz_batch` unchanged
- **Command vocabulary preserved**: All existing commands supported in server mode
- **OSMesa dependency**: Inherits batch mode's headless rendering requirements

### 7.2 Performance Constraints

- **Single-threaded C**: No concurrency in viewer state
- **Synchronous Python**: One command at a time, no pipelining
- **Memory isolation**: Python and C processes separate address spaces

### 7.3 Security Constraints  

- **Subprocess isolation**: C crashes don't affect Python
- **No network**: stdio-only communication
- **File access**: Server inherits user's filesystem permissions

## 8. Open Questions

1. **Timeout handling**: How long should Python wait for slow commands? Need configurable timeouts with sane defaults.

2. **Large result handling**: Some commands may produce large text output. Need streaming or chunking strategy.

3. **Concurrent MCP clients**: Should multiple MCP clients share one server instance, or one-per-client? Current design assumes one-per-client.

4. **Error recovery**: After a protocol error, should we restart the server or attempt recovery? Restart is safer but slower.

5. **Session persistence**: Should sessions survive across MCP client disconnects? Current design says no, but worth considering.

6. **Resource management**: How to handle compute resources in HPC environments? May need SLURM integration similar to UI effort.

## Success Criteria

- AI assistants can load databases, manipulate views, and capture images
- Python scripts can automate complex visualization workflows
- All functionality available through clean, typed Python APIs
- Minimal performance overhead compared to interactive Griz
- Robust error handling and recovery
- Full compatibility with existing Griz command vocabulary
- Shared C infrastructure reduces duplication with Qt UI effort