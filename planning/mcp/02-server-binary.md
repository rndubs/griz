# 02 — Server Binary Implementation

## Scope

This document details the C-side implementation of `griz-server`, the headless binary that provides JSON-over-stdio interface for the MCP bridge. It covers the new server loop, command dispatch, output capture, and integration with existing Griz infrastructure. This focuses on the stdio transport; the RPC transport is handled by the Qt UI effort.

## Related

- `../shared/server-binary.md` — authoritative specification for shared components
- `../shared/command-protocol.md` — JSON envelope and error handling
- `../shared/output-capture.md` — stdout/stderr redirection
- `../shared/query-commands.md` — structured state queries
- [01-architecture.md](01-architecture.md) — overall system design
- [05-protocol.md](05-protocol.md) — detailed protocol specification

## 1. Implementation Overview

`griz-server` is built from the existing `batchopt` object set with additional server-specific modules. It supports two transports via `--transport={stdio,rpc}`:

- **stdio**: Line-delimited JSON over stdin/stdout for MCP bridge
- **rpc**: Socket-based protocol for Qt UI (implemented separately)

The core command processing, output capture, and query commands are shared between both transports.

## 2. Source Code Organization

### 2.1 New Files

**Server core (shared between transports)**:
- `Src/server_core.c` — JSON envelope, handshake, command dispatch
- `Src/server_core.h` — shared server types and interfaces
- `Src/output_capture.c` — `griz_out()`/`griz_err()` implementation
- `Src/output_capture.h` — output sink interfaces

**Transport-specific**:
- `Src/server_stdio.c` — stdio transport loop and framing
- `Src/server_rpc.c` — RPC transport (for Qt UI, separate effort)

**Query commands**:
- `Src/query_commands.c` — `q_state`, `q_view`, `q_results`, etc.
- `Src/query_commands.h` — query interfaces and state schema

### 2.2 Modified Files

**Core viewer modifications**:
- `Src/viewer.c` — new `process_server_mode()` function
- `Src/interpret.c` — integration with output capture system
- `Src/Makefile.Library` — new `griz-server` target

**Output audit sites** (identified during implementation):
- All `printf()`, `fprintf(stdout,...)`, `puts()` calls on batch-reachable paths
- Replace with `griz_out()` calls to prevent stdout corruption

## 3. Server Startup and Lifecycle

### 3.1 Command Line Interface

```bash
griz-server --transport=stdio -i database.plt -w 1024 1024
griz-server --transport=rpc -i database.plt -w 1024 1024 --port=0
```

**Required arguments**:
- `--transport={stdio|rpc}` — communication mode
- `-i <database>` — initial database to load

**Optional arguments**:
- `-w <width> <height>` — framebuffer dimensions (default: 1024x1024)
- `--port=N` — RPC port (rpc transport only, 0 for auto-assign)

### 3.2 Startup Sequence

```c
int main(int argc, char *argv[])
{
    Analysis *analy;
    ServerConfig config;
    
    // Parse arguments, set transport mode
    if (scan_server_args(argc, argv, &config) != 0) {
        server_usage();
        return 1;
    }
    
    // Initialize Griz core (existing code path)
    analy = init_analysis();
    if (load_database(analy, config.database) != 0) {
        fprintf(stderr, "Failed to load database: %s\n", config.database);
        return 1;
    }
    
    // Initialize OSMesa context
    init_offscreen_rendering(config.width, config.height);
    init_mesh_window(analy);
    analy->update_display(analy);
    
    // Initialize output capture system
    init_output_capture(CAPTURE_MODE_SERVER);
    
    // Dispatch to transport-specific main loop
    switch (config.transport) {
        case TRANSPORT_STDIO:
            return process_server_mode_stdio(analy, &config);
        case TRANSPORT_RPC:
            return process_server_mode_rpc(analy, &config);
        default:
            fprintf(stderr, "Unknown transport mode\n");
            return 1;
    }
}
```

## 4. stdio Transport Implementation

### 4.1 Main Loop

```c
static int
process_server_mode_stdio(Analysis *analy, ServerConfig *config)
{
    char line_buffer[MAX_STRING_LENGTH];
    ServerState state;
    
    // Initialize server state
    init_server_state(&state, analy);
    
    // Send ready handshake
    server_send_event(&state, "ready", NULL);
    fflush(stdout);
    
    // Wait for hello handshake
    if (server_handshake_stdio(&state) != 0) {
        return 1;
    }
    
    // Main command loop
    while (fgets(line_buffer, sizeof(line_buffer), stdin) != NULL) {
        strip_newline(line_buffer);
        
        // Skip empty lines and comments
        if (line_buffer[0] == '\0' || line_buffer[0] == '#') {
            continue;
        }
        
        // Check for termination commands
        if (is_terminator_command(line_buffer)) {
            break;
        }
        
        // Process command (may be bare text or JSON)
        process_server_command(&state, line_buffer);
        
        // Flush output to prevent blocking
        fflush(stdout);
    }
    
    // Cleanup
    cleanup_server_state(&state);
    return 0;
}
```

### 4.2 Command Processing

The server accepts both raw command strings (for debugging) and JSON-wrapped commands:

**Raw command**:
```
rx 30
```

**JSON command**:
```json
{"type": "request", "id": "123", "cmd": "rx 30"}
```

```c
static void
process_server_command(ServerState *state, const char *line)
{
    cJSON *json = NULL;
    const char *command;
    const char *request_id = NULL;
    
    // Try to parse as JSON
    json = cJSON_Parse(line);
    if (json != NULL) {
        // Extract command and ID from JSON request
        cJSON *cmd_item = cJSON_GetObjectItem(json, "cmd");
        cJSON *id_item = cJSON_GetObjectItem(json, "id");
        
        if (cmd_item && cJSON_IsString(cmd_item)) {
            command = cmd_item->valuestring;
            request_id = (id_item && cJSON_IsString(id_item)) ? id_item->valuestring : NULL;
        } else {
            send_error_response(state, request_id, "invalid_request", "Missing or invalid 'cmd' field");
            goto cleanup;
        }
    } else {
        // Treat as raw command string
        command = line;
    }
    
    // Execute command through existing interpreter
    execute_command_with_capture(state, command, request_id);
    
cleanup:
    if (json) cJSON_Delete(json);
}
```

### 4.3 Command Execution and Capture

```c
static void
execute_command_with_capture(ServerState *state, const char *command, const char *request_id)
{
    CaptureBuffers buffers;
    int result;
    cJSON *response = NULL;
    cJSON *query_data = NULL;
    
    // Reset capture buffers
    capture_reset_buffers(&buffers);
    capture_start(&buffers);
    
    // Execute command through existing interpreter
    result = parse_command((char *)command);
    
    // Stop capture and get output
    capture_stop(&buffers);
    
    // Check if this was a query command
    if (strncmp(command, "q_", 2) == 0) {
        query_data = execute_query_command(command);
    }
    
    // Build JSON response
    response = build_command_response(request_id, result, &buffers, query_data);
    
    // Send response
    char *response_str = cJSON_Print(response);
    printf("%s\n", response_str);
    
    // Cleanup
    free(response_str);
    cJSON_Delete(response);
    if (query_data) cJSON_Delete(query_data);
    capture_cleanup(&buffers);
}
```

## 5. Output Capture System

### 5.1 Capture Infrastructure

The output capture system redirects all Griz text output to per-command buffers:

```c
typedef struct {
    char *stdout_buf;
    char *stderr_buf;
    size_t stdout_size;
    size_t stderr_size;
    size_t stdout_capacity;
    size_t stderr_capacity;
} CaptureBuffers;

// Redirect functions
void griz_out(const char *format, ...);
void griz_err(const char *format, ...);

// Capture control
void capture_start(CaptureBuffers *buffers);
void capture_stop(CaptureBuffers *buffers);
void capture_reset_buffers(CaptureBuffers *buffers);
```

### 5.2 Integration Points

**Replace direct stdio calls**:
```c
// Before:
printf("Time state set to %d\n", state);
fprintf(stdout, "Result: %s\n", result);

// After:  
griz_out("Time state set to %d\n", state);
griz_out("Result: %s\n", result);
```

**GUI compatibility**:
```c
void griz_out(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    
    if (server_mode_active()) {
        // Append to capture buffer
        capture_append_stdout(format, args);
    } else {
        // Forward to console (GUI/batch mode)
        vprintf(format, args);
    }
    
    va_end(args);
}
```

## 6. Query Commands Implementation

### 6.1 Command Dispatch

```c
cJSON *execute_query_command(const char *command)
{
    if (strcmp(command, "q_state") == 0) {
        return query_full_state();
    } else if (strcmp(command, "q_view") == 0) {
        return query_view_state();
    } else if (strcmp(command, "q_time") == 0) {
        return query_time_state();
    } else if (strcmp(command, "q_results") == 0) {
        return query_results_list();
    } else if (strcmp(command, "q_materials") == 0) {
        return query_materials_list();
    } else if (strcmp(command, "q_selection") == 0) {
        return query_selection_state();
    }
    
    return NULL; // Not a query command
}
```

### 6.2 State Serialization

```c
cJSON *query_full_state(void)
{
    cJSON *state = cJSON_CreateObject();
    extern Analysis *current_analysis; // Global viewer state
    
    // Time state
    cJSON_AddNumberToObject(state, "time_state", current_analysis->time_state);
    cJSON_AddNumberToObject(state, "max_time_state", current_analysis->max_time_state);
    
    // Current result
    if (current_analysis->current_result) {
        cJSON_AddStringToObject(state, "current_field", current_analysis->current_result);
    }
    
    // Camera state
    cJSON *camera = cJSON_CreateObject();
    cJSON_AddNumberToObject(camera, "x", current_analysis->camera_x);
    cJSON_AddNumberToObject(camera, "y", current_analysis->camera_y);
    cJSON_AddNumberToObject(camera, "z", current_analysis->camera_z);
    cJSON_AddNumberToObject(camera, "rotation_x", current_analysis->rotation_x);
    cJSON_AddNumberToObject(camera, "rotation_y", current_analysis->rotation_y);
    cJSON_AddNumberToObject(camera, "rotation_z", current_analysis->rotation_z);
    cJSON_AddItemToObject(state, "camera", camera);
    
    // Material visibility
    cJSON *materials = cJSON_CreateArray();
    for (int i = 0; i < current_analysis->num_materials; i++) {
        if (current_analysis->material_visible[i]) {
            cJSON_AddItemToArray(materials, cJSON_CreateNumber(i + 1));
        }
    }
    cJSON_AddItemToObject(state, "visible_materials", materials);
    
    return state;
}
```

## 7. Build System Integration

### 7.1 Makefile Changes

```makefile
# New target in Src/Makefile.Library
griz-server: $(BATCHOPT_OBJECTS) server_core.o server_stdio.o server_rpc.o \
             output_capture.o query_commands.o
	$(CC) $(LDFLAGS) -o $@ $^ $(BATCH_LIBS) $(JSON_LIBS)

# Object files
server_core.o: server_core.c server_core.h
	$(CC) $(CFLAGS) $(BATCH_DEFINES) -c $< -o $@

server_stdio.o: server_stdio.c server_core.h
	$(CC) $(CFLAGS) $(BATCH_DEFINES) -c $< -o $@

output_capture.o: output_capture.c output_capture.h  
	$(CC) $(CFLAGS) $(BATCH_DEFINES) -c $< -o $@

query_commands.o: query_commands.c query_commands.h
	$(CC) $(CFLAGS) $(BATCH_DEFINES) -c $< -o $@
```

### 7.2 Dependencies

**New dependencies**:
- **cJSON library**: For JSON parsing and generation
- **OSMesa**: Already required by batch builds

**Compile flags**:
- `SERIAL_BATCH` — enables headless/server mode code paths
- `SERVER_MODE` — enables server-specific features

## 8. Error Handling

### 8.1 Error Taxonomy

Following the shared command protocol specification:

```c
typedef enum {
    ERROR_UNKNOWN_COMMAND,
    ERROR_INVALID_SYNTAX,
    ERROR_DATABASE_ERROR,
    ERROR_RENDERING_ERROR,
    ERROR_INTERNAL_ERROR
} ServerErrorCode;

static void
send_error_response(ServerState *state, const char *request_id, 
                   ServerErrorCode code, const char *message)
{
    cJSON *response = cJSON_CreateObject();
    cJSON *error = cJSON_CreateObject();
    
    cJSON_AddStringToObject(response, "type", "response");
    if (request_id) {
        cJSON_AddStringToObject(response, "id", request_id);
    }
    cJSON_AddStringToObject(response, "status", "error");
    
    cJSON_AddStringToObject(error, "code", error_code_to_string(code));
    cJSON_AddStringToObject(error, "message", message);
    cJSON_AddItemToObject(response, "error", error);
    
    char *response_str = cJSON_Print(response);
    printf("%s\n", response_str);
    fflush(stdout);
    
    free(response_str);
    cJSON_Delete(response);
}
```

### 8.2 Recovery Strategies

- **Protocol errors**: Send error response, continue processing
- **Command errors**: Capture error output, send in response
- **Fatal errors**: Send error response, exit gracefully
- **Memory errors**: Attempt cleanup, exit if necessary

## 9. Testing Strategy

### 9.1 Unit Testing

**Output capture tests**:
- Verify `griz_out()`/`griz_err()` redirection
- Test buffer management and overflow handling
- Validate GUI/server mode switching

**JSON protocol tests**:
- Test command parsing and response generation
- Verify error handling and edge cases
- Test handshake sequence

### 9.2 Integration Testing

**Server lifecycle tests**:
- Test startup with various arguments
- Test database loading and initialization
- Test clean shutdown sequences

**Command execution tests**:
- Test basic commands (time state, view, etc.)
- Test query commands and state serialization  
- Test error propagation from interpret.c

### 9.3 Compatibility Testing

**Legacy compatibility**:
- Ensure existing batch scripts work unchanged
- Verify no regression in GUI mode
- Test existing command vocabulary

## 10. Dependencies and Prerequisites

### 10.1 Build Dependencies

**Required**:
- cJSON library (for JSON parsing/generation)
- OSMesa (already required for batch builds)
- Standard C library

**Optional**:
- valgrind (for memory testing)
- gdb (for debugging)

### 10.2 Runtime Dependencies

**Required**:
- OSMesa drivers
- Mili libraries
- Database files accessible to server process

**Environment**:
- Sufficient memory for database and framebuffer
- Filesystem access for temp files (screenshots)

## 11. Open Questions

1. **JSON library choice**: cJSON vs alternatives (jansson, json-c)? cJSON chosen for simplicity.

2. **Buffer size limits**: What are reasonable limits for stdout/stderr capture buffers? Need configurable limits.

3. **Command timeout**: Should the server support command timeouts? Probably not in v1.

4. **Memory management**: How to handle memory leaks in long-running sessions? Need periodic cleanup.

5. **Signal handling**: How to handle SIGTERM, SIGINT gracefully? Need signal handlers for cleanup.

6. **Log file support**: Should server maintain separate log files? Probably yes for debugging.

## Success Criteria

- `griz-server --transport=stdio` starts successfully
- JSON handshake completes properly
- Basic commands execute and return valid JSON
- Query commands return structured state data
- Output capture prevents stdout corruption
- Clean shutdown releases all resources
- No regression in existing GUI/batch functionality
- Integration tests pass with Python bridge