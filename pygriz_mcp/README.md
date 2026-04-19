# llnl-griz-mcp

MCP (Model Context Protocol) server adapter for the
[Griz](https://github.com/LLNL/griz) visualization engine.  Exposes
the `griz` Python API as MCP tools so AI assistants can drive Griz
through natural language.

## Installation

```bash
# From the repo root (development install)
cd pygriz_mcp
unset SSL_CERT_FILE          # LLNL TOSS workaround — see CERTS.md
uv sync --extra test
```

This installs `llnl-griz-mcp` along with `llnl-griz` (resolved from
the sibling `pygriz/` directory via `[tool.uv.sources]`).

## Running the server

```bash
uv run griz-mcp              # stdio transport (default)
```

The server speaks MCP over stdin/stdout.  Connect to it from any MCP
client (Claude Code, Claude Desktop, etc.) by adding to your MCP
config:

```json
{
  "mcpServers": {
    "griz": {
      "command": "uv",
      "args": ["--directory", "/path/to/pygriz_mcp", "run", "griz-mcp"]
    }
  }
}
```

## Tools

| Tool | Description |
|------|-------------|
| `open_database(path)` | Open a Griz database file |
| `close_database()` | Close the current database |
| `show_field(name, component?)` | Display a field on the mesh |
| `list_fields()` | List available fields |
| `rotate_view(x?, y?, z?)` | Rotate the camera (degrees) |
| `reset_view()` | Reset to the default view |
| `set_time_state(state)` | Jump to a time state index |
| `animate(start?, end?, step?, delay?)` | Animate through time states |
| `hide_materials(material_ids)` | Hide materials by ID |
| `show_materials(material_ids)` | Show materials by ID |
| `screenshot()` | Capture the current frame |
| `get_state()` | Return the viewer state |
| `restart_session()` | Discard the current session |
| `raw_command(command)` | Send a raw Griz command |

## End-to-end transcript

Below is a representative MCP session over the stdio transport.  Each
`-->` line is a JSON-RPC request sent by the client; each `<--` line
is the server's response.  Long payloads are abbreviated with `...`.

### 1. Initialize

```
--> {"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"example","version":"0.1.0"}}}
<-- {"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"griz-mcp","version":"0.1.0"}}}

--> {"jsonrpc":"2.0","method":"notifications/initialized"}
```

### 2. List tools

```
--> {"jsonrpc":"2.0","id":2,"method":"tools/list"}
<-- {"jsonrpc":"2.0","id":2,"result":{"tools":[{"name":"open_database","description":"Open a Griz database file. Returns the initial viewer state.","inputSchema":{"type":"object","properties":{"path":{"type":"string"}},"required":["path"]}},{"name":"close_database","description":"Close the current database and tear down the session.","inputSchema":{"type":"object","properties":{}}},{"name":"show_field","description":"Display a field on the mesh. Returns the updated viewer state.","inputSchema":{"type":"object","properties":{"name":{"type":"string"},"component":{"anyOf":[{"type":"string"},{"type":"null"}],"default":null}},"required":["name"]}}, ... ]}}
```

### 3. Open a database

```
--> {"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"open_database","arguments":{"path":"/path/to/bar71.pltA"}}}
<-- {"jsonrpc":"2.0","id":3,"result":{"content":[{"type":"text","text":"{\"time_state\": 0, \"max_time_state\": 71, \"state_count\": 72, ...}"}]}}
```

### 4. Show a field

```
--> {"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"show_field","arguments":{"name":"stress","component":"xx"}}}
<-- {"jsonrpc":"2.0","id":4,"result":{"content":[{"type":"text","text":"{\"time_state\": 0, \"current_field\": \"sx\", ...}"}]}}
```

### 5. Rotate and screenshot

```
--> {"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"rotate_view","arguments":{"y":45}}}
<-- {"jsonrpc":"2.0","id":5,"result":{"content":[{"type":"text","text":"{\"time_state\": 0, ...}"}]}}

--> {"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"screenshot","arguments":{}}}
<-- {"jsonrpc":"2.0","id":6,"result":{"content":[{"type":"image","data":"AQEAAAA...","mimeType":"application/octet-stream"}]}}
```

### 6. Animate

```
--> {"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"animate","arguments":{"start":0,"end":5,"step":1}}}
<-- {"jsonrpc":"2.0","id":7,"result":{"content":[{"type":"text","text":"[{\"time_state\": 0, ...}, {\"time_state\": 1, ...}, ...]"}]}}
```

### 7. Close

```
--> {"jsonrpc":"2.0","id":8,"method":"tools/call","params":{"name":"close_database","arguments":{}}}
<-- {"jsonrpc":"2.0","id":8,"result":{"content":[{"type":"text","text":"Database closed."}]}}
```

## Testing

```bash
uv run pytest tests/ -v --cov=griz_mcp
```

## Architecture

```
pygriz_mcp/src/griz_mcp/
├── __init__.py     # exports mcp instance
├── server.py       # FastMCP instance + 14 @mcp.tool functions + main()
└── session.py      # Module-level Griz session singleton
```

The MCP adapter is a thin layer over the `griz` Python API (`llnl-griz`).
Each tool function calls `session.require_session()` to get the active
`Griz` instance, delegates to the appropriate namespace method, and
catches domain exceptions (`GrizError`, `UnknownFieldError`, etc.),
translating them to `fastmcp.exceptions.ToolError` with descriptive
messages.
