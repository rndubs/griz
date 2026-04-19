# 05 — Protocol Specification

## Scope

This document specifies the JSON communication protocol between the Python `griz` package and the `griz-server` C binary over stdio. This includes message formats, handshake sequences, error handling, state queries, and protocol versioning. The protocol is designed to be simple, reliable, and extensible while supporting both the MCP use case and potential future clients.

## Related

- `../shared/command-protocol.md` — authoritative shared protocol specification
- [01-architecture.md](01-architecture.md) — overall system design  
- [02-server-binary.md](02-server-binary.md) — server implementation
- [03-python-api.md](03-python-api.md) — Python client implementation
- Qt UI protocol — shares same envelope, different transport

## 1. Protocol Overview

### 1.1 Transport Layer

**Framing**: Line-delimited JSON (JSONL) over stdio
- Each message is a single JSON object on one line
- Messages are terminated by newline (`\n`)
- UTF-8 encoding throughout
- No length prefixes or binary framing

**Channels**: Single bidirectional stream
- Commands: Python → C
- Responses: C → Python  
- Events: C → Python (handshake, errors)

**Buffering**: Line-buffered for responsiveness
- Server calls `fflush(stdout)` after each message
- Client processes messages as soon as newline received

### 1.2 Message Types

```
Request  (Python → C): Command execution
Response (C → Python): Command result  
Event    (C → Python): Asynchronous notifications
```

## 2. Message Formats

### 2.1 Request Messages

Sent by Python client to execute commands:

```json
{
  "type": "request",
  "id": "req_123",
  "cmd": "rx 30"
}
```

**Fields**:
- `type`: Always `"request"`
- `id`: Unique request identifier (string, required)
- `cmd`: Griz command string (required)

**Request ID**: Opaque string chosen by client, echoed in response. Used for correlation in async scenarios. Format recommendation: `"req_" + counter`.

**Command**: Raw Griz command string that would be valid at the interactive prompt. Examples:
- `"state 42"`
- `"res sx"`
- `"show result"`
- `"q_state"`
- `"outpng /tmp/screenshot.png"`

### 2.2 Response Messages

Sent by server after executing each request:

**Success response**:
```json
{
  "type": "response",
  "id": "req_123", 
  "status": "ok",
  "stdout": "Time state set to 30\n",
  "stderr": "",
  "data": null
}
```

**Error response**:
```json
{
  "type": "response",
  "id": "req_123",
  "status": "error", 
  "error": {
    "code": "unknown_command",
    "message": "unknown command: badcmd"
  }
}
```

**Query response** (when `data` field populated):
```json
{
  "type": "response",
  "id": "req_456",
  "status": "ok",
  "stdout": "",
  "stderr": "",
  "data": {
    "time_state": 42,
    "max_time_state": 100,
    "current_field": "sx",
    "camera": {
      "position": [0, 0, 10],
      "rotation": [30, 0, 0]
    }
  }
}
```

**Response fields**:
- `type`: Always `"response"`
- `id`: Request ID from corresponding request
- `status`: `"ok"` | `"error"`
- `stdout`: Captured stdout from command (string, may be empty)
- `stderr`: Captured stderr from command (string, may be empty)  
- `data`: Structured data for query commands (object or null)
- `error`: Error details object (present only when `status: "error"`)

**Error object**:
- `code`: Machine-readable error code (see error taxonomy)
- `message`: Human-readable error description

### 2.3 Event Messages

Sent by server for protocol management:

**Ready event** (server startup):
```json
{
  "type": "event",
  "event": "ready"
}
```

**Hello handshake** (protocol negotiation):
```json
{
  "type": "hello", 
  "version": "1.0",
  "server": "griz-server",
  "capabilities": ["queries", "screenshots"]
}
```

**Hello acknowledgment**:
```json
{
  "type": "hello_ack",
  "version": "1.0",
  "client": "griz-python"
}
```

## 3. Protocol Handshake

### 3.1 Startup Sequence

```
1. Server starts, initializes database and rendering
2. Server emits: {"type": "event", "event": "ready"}
3. Client sends: {"type": "hello", "version": "1.0", "client": "griz-python"}
4. Server responds: {"type": "hello_ack", "version": "1.0", "server": "griz-server"}
5. Normal request/response communication begins
```

**Ready event**: Signals that server initialization is complete (database loaded, OSMesa context created, etc.). Client must wait for this before sending hello.

**Version negotiation**: Client and server exchange version information. Compatible versions can proceed; incompatible versions result in connection termination.

**Timeout**: Client should timeout if ready event not received within reasonable period (e.g., 30 seconds for large database loads).

### 3.2 Version Compatibility

**Version format**: Semantic versioning (`MAJOR.MINOR.PATCH`)
- Major version changes indicate breaking protocol changes
- Minor version changes add features but maintain compatibility
- Patch versions are bug fixes only

**Compatibility rules**:
- Client and server must have same major version
- Server minor version >= client minor version (server backward compatible)
- Patch versions ignored for compatibility

**Example negotiations**:
```
Client 1.0.0 + Server 1.0.5 → Compatible
Client 1.1.0 + Server 1.0.0 → Incompatible (server lacks client features)
Client 1.0.0 + Server 1.1.0 → Compatible (server supports client)
Client 2.0.0 + Server 1.x.x → Incompatible (major version mismatch)
```

## 4. Command Execution Model

### 4.1 Request/Response Cycle

```
1. Client sends request with unique ID
2. Server begins command execution
3. Server captures all stdout/stderr during execution  
4. Server completes command (success or failure)
5. Server sends response with captured output and result
6. Client processes response and updates state
```

**Synchronous execution**: Server processes one command at a time. No pipelining or concurrent execution.

**Output capture**: All `printf()`, `griz_out()`, `griz_err()`, etc. calls during command execution are captured and returned in the response. This prevents text output from corrupting the JSON stream.

**Timeout handling**: Client should implement timeouts for long-running commands. Server does not enforce timeouts in v1.

### 4.2 Command Categories

**State mutation commands**: Change viewer state
- Examples: `"state 42"`, `"rx 30"`, `"res sx"`, `"show result"`
- Always return `status: "ok"` on success
- May produce stdout output describing the change

**Query commands**: Return structured data without mutation
- Examples: `"q_state"`, `"q_view"`, `"q_results"`  
- Return structured data in `data` field
- Minimal or no stdout output

**Action commands**: Perform operations with side effects
- Examples: `"outpng /path.png"`, `"quit"`
- May affect files, rendering, or session state
- Return status and any relevant output

**Invalid commands**: Syntax errors or unknown commands
- Return `status: "error"` with appropriate error code
- No state changes occur

## 5. Error Taxonomy

### 5.1 Error Codes

Following the shared command protocol specification:

**Command errors**:
- `unknown_command`: Unrecognized command name
- `invalid_syntax`: Malformed command syntax
- `invalid_argument`: Bad argument values or types

**Database errors**:
- `database_not_loaded`: No database currently open
- `database_load_failed`: Failed to load specified database
- `database_corrupt`: Database file is corrupted or invalid

**State errors**:
- `invalid_time_state`: Time state out of valid range
- `invalid_material`: Material ID not found in database
- `invalid_result`: Result/field not available

**Rendering errors**:
- `render_failed`: OpenGL or rendering error
- `screenshot_failed`: Failed to capture or save image

**System errors**:
- `out_of_memory`: Insufficient memory for operation
- `file_error`: File I/O error
- `internal_error`: Unexpected server error

### 5.2 Error Handling Patterns

**Recoverable errors**: Client can continue after error
```json
{
  "status": "error",
  "error": {
    "code": "invalid_time_state", 
    "message": "Time state 150 is out of range (0-100)"
  }
}
```

**Fatal errors**: Require session restart
```json
{
  "status": "error",
  "error": {
    "code": "database_corrupt",
    "message": "Database file is corrupted or unsupported format"
  }
}
```

**Client error handling**:
1. Check `status` field of each response
2. For errors, examine `error.code` for programmatic handling
3. Present `error.message` to user for debugging
4. For fatal errors, close session and restart

## 6. State Query Interface

### 6.1 Query Commands

**Full state query**: `q_state`
```json
{
  "data": {
    "time_state": 42,
    "max_time_state": 100,
    "current_field": "sx",
    "field_component": null,
    "camera": {
      "position": [0.0, 0.0, 10.0],
      "rotation": [30.0, 0.0, 0.0],
      "scale": 1.0
    },
    "visible_materials": [1, 2, 3, 5],
    "render_mode": "solid",
    "colormap": {
      "min": 0.0,
      "max": 1000.0,
      "mode": "rainbow"
    }
  }
}
```

**View state query**: `q_view`
```json
{
  "data": {
    "camera": {
      "position": [0.0, 0.0, 10.0],
      "rotation": [30.0, 0.0, 0.0],
      "scale": 1.0,
      "center": [0.0, 0.0, 0.0]
    },
    "render_mode": "solid",
    "viewport": {
      "width": 1024,
      "height": 1024
    }
  }
}
```

**Time state query**: `q_time`
```json
{
  "data": {
    "time_state": 42,
    "max_time_state": 100, 
    "time_value": 0.042,
    "max_time_value": 0.1
  }
}
```

**Results query**: `q_results`
```json
{
  "data": {
    "results": [
      {
        "name": "sx",
        "description": "Stress XX component",
        "type": "stress",
        "components": ["scalar"]
      },
      {
        "name": "displacement",
        "description": "Nodal displacement",
        "type": "displacement", 
        "components": ["x", "y", "z", "magnitude"]
      }
    ]
  }
}
```

**Materials query**: `q_materials`
```json
{
  "data": {
    "materials": [
      {
        "id": 1,
        "name": "Steel",
        "visible": true,
        "element_count": 12500
      },
      {
        "id": 2, 
        "name": "Aluminum",
        "visible": false,
        "element_count": 3200
      }
    ]
  }
}
```

### 6.2 State Schema Versioning

The state schema returned by query commands may evolve over time. Schema version is indicated in the handshake:

```json
{
  "type": "hello_ack",
  "version": "1.0",
  "schema_version": "1.0"
}
```

**Schema evolution rules**:
- New fields may be added to existing objects
- Existing fields will not change type or meaning
- Fields will not be removed within same major version
- Clients should ignore unknown fields for forward compatibility

## 7. Raw Command Mode

### 7.1 Backward Compatibility

For debugging and advanced use, the server also accepts raw command strings without JSON wrapping:

**Raw input**:
```
rx 30
```

**JSON response**:
```json
{
  "type": "response",
  "status": "ok", 
  "stdout": "Rotated 30 degrees around X axis\n",
  "stderr": ""
}
```

**Use cases**:
- Interactive debugging of the server
- Quick testing of command syntax
- Migration of existing scripts

**Limitations**:
- No request ID correlation
- Cannot distinguish from JSON commands
- Deprecated for programmatic use

## 8. Protocol Extensions

### 8.1 Future Extensions

**Binary data support**: For large datasets
```json
{
  "type": "request",
  "id": "req_123", 
  "cmd": "get_mesh",
  "binary_response": true
}
```

**Streaming responses**: For long operations
```json
{
  "type": "response_chunk",
  "id": "req_123",
  "chunk": 1,
  "total_chunks": 10,
  "data": "partial_data_here"
}
```

**Server-initiated events**: For state change notifications
```json
{
  "type": "state_change",
  "changes": {
    "time_state": 43,
    "camera.rotation": [31.0, 0.0, 0.0]
  }
}
```

### 8.2 Extension Negotiation

Extensions are negotiated during handshake via capabilities:

```json
{
  "type": "hello",
  "version": "1.1",
  "capabilities": ["binary_data", "streaming", "state_events"]
}
```

```json
{
  "type": "hello_ack", 
  "version": "1.1",
  "capabilities": ["binary_data"]
}
```

Only capabilities supported by both client and server are enabled.

## 9. Implementation Guidelines

### 9.1 Client Implementation

**Buffering**: Use line-buffered I/O to process responses immediately
```python
# Good - line buffered
proc = subprocess.Popen(..., bufsize=1, text=True)

# Bad - may block indefinitely  
proc = subprocess.Popen(..., bufsize=0, text=True)
```

**JSON parsing**: Be defensive about malformed responses
```python
try:
    response = json.loads(line)
    if response.get("type") != "response":
        handle_protocol_error()
except json.JSONDecodeError:
    handle_malformed_response()
```

**Timeout handling**: Implement timeouts for long operations
```python
import select

def read_response_with_timeout(proc, timeout=30.0):
    ready, _, _ = select.select([proc.stdout], [], [], timeout)
    if ready:
        return proc.stdout.readline()
    else:
        raise TimeoutError("Command timed out")
```

**Error handling**: Check every response status
```python
response = send_command(cmd)
if response["status"] == "error":
    error_code = response["error"]["code"]
    if error_code in FATAL_ERRORS:
        restart_session()
    else:
        raise GrizCommandError(response["error"]["message"])
```

### 9.2 Server Implementation

**Output capture**: Redirect all stdio during command execution
```c
// Before command
capture_start_command();

// Execute command  
result = parse_command(cmd);

// After command
captured = capture_stop_command();
send_response(request_id, result, captured);
```

**JSON generation**: Use robust JSON library (cJSON)
```c
cJSON *response = cJSON_CreateObject();
cJSON_AddStringToObject(response, "type", "response");
cJSON_AddStringToObject(response, "id", request_id);
cJSON_AddStringToObject(response, "status", result == 0 ? "ok" : "error");

char *json_string = cJSON_Print(response);
printf("%s\n", json_string);
fflush(stdout);
```

**Flushing**: Always flush stdout after sending response
```c
printf("%s\n", json_response);
fflush(stdout);  // Critical - client will block without this
```

## 10. Performance Considerations

### 10.1 Latency Optimization

**Minimize JSON overhead**: Keep message structure simple
**Buffer management**: Use appropriate buffer sizes
**Batch operations**: Combine multiple commands where possible

**Example - efficient rotation**:
```python
# Good - single command
griz.raw("rx 30; ry 45; rz 90")

# Less efficient - multiple round trips
griz.view.rotate(x=30)
griz.view.rotate(y=45) 
griz.view.rotate(z=90)
```

### 10.2 Memory Management

**Output buffering**: Limit captured output size
```c
#define MAX_CAPTURE_SIZE (1024 * 1024)  // 1MB limit

if (capture_size > MAX_CAPTURE_SIZE) {
    truncate_capture();
    append_truncation_warning();
}
```

**JSON cleanup**: Free JSON objects promptly
```c
char *response = cJSON_Print(json);
printf("%s\n", response);
free(response);
cJSON_Delete(json);
```

## 11. Testing and Validation

### 11.1 Protocol Compliance Tests

**Message format validation**:
- All required fields present
- Correct JSON schema
- Proper newline termination

**Error handling validation**:  
- All error codes properly handled
- Graceful degradation on protocol errors
- Timeout behavior

**Handshake validation**:
- Version negotiation works correctly
- Capability detection functions
- Invalid handshakes rejected

### 11.2 Stress Testing

**Long-running commands**: Test timeout handling
**Large output**: Test output capture limits
**Rapid commands**: Test request/response correlation
**Protocol violations**: Test error recovery

## 12. Open Questions

1. **Maximum message size**: Should we enforce limits on request/response size? Probably yes, with configurable limits.

2. **Keep-alive mechanism**: For long-running servers, should we implement keep-alive? Not needed for MCP use case.

3. **Authentication**: For remote use, should protocol support authentication? Out of scope for stdio transport.

4. **Compression**: For large responses, should we support compression? Could be added as extension.

5. **Transaction support**: Should multiple commands be atomic? Complex and not needed for v1.

## Success Criteria

- Reliable message framing with no corruption
- Comprehensive error handling and recovery
- Efficient request/response correlation  
- Proper version negotiation and capability detection
- Full capture of command output without stdio leakage
- Performance suitable for interactive use
- Extensible design for future enhancements
- Compatibility with both MCP and potential future clients