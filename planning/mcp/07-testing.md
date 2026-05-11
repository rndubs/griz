# 07 — Testing Strategy

## Scope

This document outlines the comprehensive testing strategy for the MCP implementation, covering unit tests, integration tests, system tests, and performance validation. The strategy addresses testing at all layers: C server binary, Python API package, MCP adapter, and end-to-end workflows. Special attention is given to testing the JSON protocol, subprocess management, and real-world usage scenarios.

## Related

- [01-architecture.md](01-architecture.md) — system architecture and components
- [02-server-binary.md](02-server-binary.md) — server implementation details
- [03-python-api.md](03-python-api.md) — Python package design  
- [04-mcp-adapter.md](04-mcp-adapter.md) — MCP tool implementations
- [05-protocol.md](05-protocol.md) — JSON protocol specification

## 1. Testing Philosophy

### 1.1 Testing Principles

**Comprehensive Coverage**: Test all layers independently and together
**Realistic Scenarios**: Use real databases and realistic workflows
**Reliability**: Tests should be deterministic and fast
**Maintainability**: Easy to understand, modify, and extend
**CI-Friendly**: Run efficiently in automated environments

### 1.2 Quality Gates

**Unit Tests**: Must achieve >90% code coverage
**Integration Tests**: Must cover all major user workflows  
**Performance Tests**: Must meet latency and throughput targets
**Compatibility Tests**: Must work with multiple Griz versions
**Error Handling**: Must gracefully handle all failure modes

## 2. Test Architecture

### 2.1 Testing Layers

```
┌─────────────────────────────────────────────────┐
│                 System Tests                    │  Real MCP clients
│  (End-to-end workflows with real clients)      │  Real databases
├─────────────────────────────────────────────────┤
│               Integration Tests                 │  Real griz-server
│  (Python + C server, subprocess management)    │  Test databases
├─────────────────────────────────────────────────┤
│                 Unit Tests                      │  Mocked dependencies
│  (Individual components in isolation)          │  Fast execution
└─────────────────────────────────────────────────┘
```

### 2.2 Test Data Management

**Test databases**: Small, well-defined Mili databases for testing
**Mock responses**: JSON response fixtures for protocol testing
**Fixture management**: Shared test data across test suites
**Environment setup**: Automated test environment configuration

## 3. C Server Testing

### 3.1 Unit Tests (C)

**Test framework**: Custom C test harness or existing framework (Check, Unity)
**Coverage**: Core server functions, JSON parsing, command dispatch

```c
// tests/test_server_core.c
#include "server_core.h"
#include "test_framework.h"

void test_json_request_parsing() {
    const char *json = "{\"type\":\"request\",\"id\":\"123\",\"cmd\":\"state 42\"}";
    ServerRequest req;
    
    int result = parse_json_request(json, &req);
    
    assert_int_equal(result, 0);
    assert_string_equal(req.type, "request");
    assert_string_equal(req.id, "123");
    assert_string_equal(req.cmd, "state 42");
}

void test_json_response_generation() {
    ServerResponse resp = {
        .type = "response",
        .id = "123", 
        .status = "ok",
        .stdout_data = "Time state set to 42\n",
        .stderr_data = "",
        .data = NULL
    };
    
    char *json = generate_json_response(&resp);
    
    assert_non_null(json);
    assert_true(strstr(json, "\"type\":\"response\"") != NULL);
    assert_true(strstr(json, "\"id\":\"123\"") != NULL);
    assert_true(strstr(json, "\"status\":\"ok\"") != NULL);
    
    free(json);
}

void test_output_capture() {
    CaptureBuffers buffers;
    capture_start(&buffers);
    
    // Test captured output
    griz_out("Test output\n");
    griz_err("Test error\n");
    
    capture_stop(&buffers);
    
    assert_string_equal(buffers.stdout_data, "Test output\n");
    assert_string_equal(buffers.stderr_data, "Test error\n");
    
    capture_cleanup(&buffers);
}

void test_command_execution() {
    Analysis *analy = init_test_analysis();
    ServerState state;
    init_server_state(&state, analy);
    
    // Test valid command
    execute_command_with_capture(&state, "state 42", "test_123");
    
    // Verify response was generated
    assert_true(state.last_response != NULL);
    assert_string_equal(state.last_response->id, "test_123");
    assert_string_equal(state.last_response->status, "ok");
    
    cleanup_server_state(&state);
}

int main() {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_json_request_parsing),
        cmocka_unit_test(test_json_response_generation),
        cmocka_unit_test(test_output_capture),
        cmocka_unit_test(test_command_execution),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

### 3.2 Integration Tests (C)

**Test full stdio protocol**: End-to-end message exchange
**Database loading**: Test with various database formats
**Error scenarios**: Test invalid commands, malformed JSON

```c
// tests/test_server_integration.c
#include "server_stdio.h"
#include <unistd.h>

void test_stdio_protocol() {
    // Create pipes for testing
    int stdin_pipe[2], stdout_pipe[2];
    pipe(stdin_pipe);
    pipe(stdout_pipe);
    
    // Fork process for server
    pid_t server_pid = fork();
    if (server_pid == 0) {
        // Child process - run server
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        
        // Initialize server with test database
        Analysis *analy = load_test_database("tests/fixtures/small.plt");
        ServerConfig config = {.transport = TRANSPORT_STDIO};
        process_server_mode_stdio(analy, &config);
        exit(0);
    }
    
    // Parent process - test client
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    
    FILE *server_stdin = fdopen(stdin_pipe[1], "w");
    FILE *server_stdout = fdopen(stdout_pipe[0], "r");
    
    // Wait for ready event
    char line[1024];
    fgets(line, sizeof(line), server_stdout);
    assert_true(strstr(line, "\"event\":\"ready\"") != NULL);
    
    // Send hello handshake
    fprintf(server_stdin, "{\"type\":\"hello\",\"version\":\"1.0\"}\n");
    fflush(server_stdin);
    
    // Receive hello_ack
    fgets(line, sizeof(line), server_stdout);
    assert_true(strstr(line, "\"type\":\"hello_ack\"") != NULL);
    
    // Send test command
    fprintf(server_stdin, "{\"type\":\"request\",\"id\":\"test1\",\"cmd\":\"state 42\"}\n");
    fflush(server_stdin);
    
    // Receive response
    fgets(line, sizeof(line), server_stdout);
    assert_true(strstr(line, "\"id\":\"test1\"") != NULL);
    assert_true(strstr(line, "\"status\":\"ok\"") != NULL);
    
    // Cleanup
    fprintf(server_stdin, "{\"type\":\"request\",\"id\":\"quit\",\"cmd\":\"quit\"}\n");
    fflush(server_stdin);
    
    fclose(server_stdin);
    fclose(server_stdout);
    
    int status;
    waitpid(server_pid, &status, 0);
    assert_int_equal(WEXITSTATUS(status), 0);
}
```

## 4. Python Package Testing

### 4.1 Unit Tests (Python)

**Test framework**: pytest with fixtures and mocking
**Coverage**: All modules with comprehensive edge cases

```python
# tests/test_worker.py
import pytest
import json
from unittest.mock import Mock, patch, MagicMock
from griz.worker import Worker
from griz.exceptions import GrizConnectionError, GrizCommandError

class TestWorker:
    
    @pytest.fixture
    def mock_process(self):
        """Mock subprocess.Popen for testing."""
        process = MagicMock()
        process.poll.return_value = None  # Process running
        process.stdin = MagicMock()
        process.stdout = MagicMock()
        process.stderr = MagicMock()
        return process
    
    @patch('griz.worker.subprocess.Popen')
    def test_worker_initialization(self, mock_popen, mock_process):
        """Test worker startup and handshake."""
        mock_popen.return_value = mock_process
        
        # Mock ready event and handshake
        mock_process.stdout.readline.side_effect = [
            '{"type":"event","event":"ready"}\n',
            '{"type":"hello_ack","version":"1.0"}\n'
        ]
        
        worker = Worker("griz-server", 1024, 1024, 30.0)
        
        # Verify subprocess was started correctly
        mock_popen.assert_called_once()
        args, kwargs = mock_popen.call_args
        assert "griz-server" in args[0]
        assert "--transport=stdio" in args[0]
        assert "-w" in args[0]
        
        # Verify handshake was sent
        written_data = ''.join(call[0][0] for call in mock_process.stdin.write.call_args_list)
        assert '"type":"hello"' in written_data
    
    def test_command_execution(self, mock_process):
        """Test command execution with response parsing."""
        worker = Worker.__new__(Worker)  # Skip __init__
        worker._proc = mock_process
        worker._lock = MagicMock()
        worker._stderr_queue = MagicMock()
        worker._next_id = 1
        
        # Mock successful response
        response_json = {
            "type": "response",
            "id": "1",
            "status": "ok", 
            "stdout": "Time state set to 42\n",
            "stderr": "",
            "data": None
        }
        mock_process.stdout.readline.return_value = json.dumps(response_json) + '\n'
        
        result = worker._send_command("state 42", 30.0)
        
        # Verify request was sent
        written_data = ''.join(call[0][0] for call in mock_process.stdin.write.call_args_list)
        request = json.loads(written_data.strip())
        assert request["type"] == "request"
        assert request["cmd"] == "state 42"
        assert request["id"] == "1"
        
        # Verify response was parsed correctly
        assert result == response_json
    
    def test_command_error_handling(self, mock_process):
        """Test error response handling."""
        worker = Worker.__new__(Worker)
        worker._proc = mock_process
        worker._lock = MagicMock()
        worker._stderr_queue = MagicMock()
        worker._next_id = 1
        
        # Mock error response
        error_response = {
            "type": "response",
            "id": "1", 
            "status": "error",
            "error": {
                "code": "unknown_command",
                "message": "unknown command: badcmd"
            }
        }
        mock_process.stdout.readline.return_value = json.dumps(error_response) + '\n'
        
        result = worker._send_command("badcmd", 30.0)
        
        assert result["status"] == "error"
        assert result["error"]["code"] == "unknown_command"
    
    def test_timeout_handling(self, mock_process):
        """Test command timeout handling."""
        worker = Worker.__new__(Worker)
        worker._proc = mock_process
        worker._lock = MagicMock()
        worker._stderr_queue = MagicMock()
        worker._next_id = 1
        
        # Mock timeout scenario
        with patch('griz.worker.select.select') as mock_select:
            mock_select.return_value = ([], [], [])  # No data available
            
            with pytest.raises(TimeoutError):
                worker._send_command("slow_command", 0.1)
    
    def test_cleanup(self, mock_process):
        """Test worker cleanup on shutdown."""
        worker = Worker.__new__(Worker)
        worker._proc = mock_process
        
        worker.cleanup()
        
        mock_process.stdin.close.assert_called_once()
        mock_process.wait.assert_called_once()

# tests/test_session.py
class TestGrizSession:
    
    @pytest.fixture 
    def mock_worker(self):
        """Mock worker for session testing."""
        worker = MagicMock()
        worker.cmd.return_value = {"status": "ok", "stdout": ""}
        return worker
    
    @pytest.fixture
    def test_database(self):
        """Path to test database file."""
        return "tests/fixtures/small.plt"
    
    def test_context_manager(self, mock_worker, test_database):
        """Test Griz context manager lifecycle."""
        with patch('griz.session.Worker', return_value=mock_worker):
            with patch('os.path.exists', return_value=True):
                
                with Griz(test_database) as g:
                    assert g._database_path == test_database
                    assert g._worker is mock_worker
                
                # Verify cleanup was called
                mock_worker.cleanup.assert_called_once()
    
    def test_manual_lifecycle(self, mock_worker, test_database):
        """Test manual open/close lifecycle."""
        with patch('griz.session.Worker', return_value=mock_worker):
            with patch('os.path.exists', return_value=True):
                
                g = Griz()
                assert g._worker is None
                
                g.open(test_database)
                assert g._worker is mock_worker
                assert g._database_path == test_database
                
                g.close()
                mock_worker.cleanup.assert_called_once()
                assert g._worker is None
    
    def test_field_api_integration(self, mock_worker):
        """Test field API method calls."""
        with patch('griz.session.Worker', return_value=mock_worker):
            mock_worker.cmd.return_value = {"status": "ok", "data": {"time_state": 42}}
            
            g = Griz.__new__(Griz)  # Skip database loading
            g._worker = mock_worker
            
            # Test field.show()
            result = g.field.show("stress", component="von_mises")
            
            # Verify correct commands were sent
            expected_commands = ["res svm", "show result"] 
            actual_commands = [call[0][0] for call in mock_worker.cmd.call_args_list]
            assert actual_commands == expected_commands
    
    def test_view_api_integration(self, mock_worker):
        """Test view API method calls."""
        with patch('griz.session.Worker', return_value=mock_worker):
            mock_worker.cmd.return_value = {"status": "ok"}
            
            g = Griz.__new__(Griz)
            g._worker = mock_worker
            
            # Test view.rotate()
            g.view.rotate(x=30, y=45, z=90)
            
            # Verify rotation commands
            actual_commands = [call[0][0] for call in mock_worker.cmd.call_args_list]
            assert "rx 30" in actual_commands
            assert "ry 45" in actual_commands
            assert "rz 90" in actual_commands

# tests/test_results_map.py  
class TestResultsMapping:
    
    @pytest.fixture
    def sample_mapping_yaml(self):
        """Sample YAML content for testing."""
        return """
stress:
  description: "Stress tensor"
  default: "von_mises"
  components:
    xx: "sx"
    yy: "sy"
    von_mises: "svm"

displacement:
  description: "Displacement vector"
  default: "magnitude" 
  components:
    x: "dx"
    y: "dy"
    z: "dz"
    magnitude: "dmag"

temperature:
  description: "Temperature field"
  command: "temp"
"""
    
    def test_field_resolution(self, sample_mapping_yaml, tmp_path):
        """Test field name resolution."""
        yaml_file = tmp_path / "test_mapping.yaml"
        yaml_file.write_text(sample_mapping_yaml)
        
        results_map = ResultsMap(str(yaml_file))
        
        # Test vector field resolution
        assert results_map.resolve("stress", "xx") == "sx"
        assert results_map.resolve("stress", "von_mises") == "svm"
        assert results_map.resolve("stress") == "svm"  # default
        
        # Test scalar field resolution
        assert results_map.resolve("temperature") == "temp"
        
        # Test error cases
        with pytest.raises(UnknownFieldError):
            results_map.resolve("unknown_field")
        
        with pytest.raises(UnknownFieldError):
            results_map.resolve("stress", "unknown_component")
        
        with pytest.raises(UnknownFieldError):
            results_map.resolve("temperature", "x")  # scalar field with component
```

### 4.2 Integration Tests (Python)

**Real subprocess testing**: Test with actual griz-server binary
**Protocol validation**: End-to-end JSON protocol testing
**Error recovery**: Test failure modes and recovery

```python
# tests/test_integration.py
import pytest
import subprocess
import tempfile
import time
from griz import Griz
from griz.exceptions import GrizError

@pytest.mark.integration
class TestGrizIntegration:
    """Integration tests using real griz-server."""
    
    @pytest.fixture(scope="session")
    def griz_server_binary(self):
        """Path to griz-server binary for testing."""
        # Try to find griz-server on PATH
        binary_path = subprocess.run(
            ["which", "griz-server"], 
            capture_output=True, 
            text=True
        ).stdout.strip()
        
        if not binary_path:
            pytest.skip("griz-server binary not found on PATH")
        
        return binary_path
    
    @pytest.fixture(scope="session") 
    def test_database(self):
        """Path to test database."""
        db_path = "tests/fixtures/small.plt"
        if not os.path.exists(db_path):
            pytest.skip(f"Test database not found: {db_path}")
        return db_path
    
    def test_basic_workflow(self, griz_server_binary, test_database):
        """Test basic Griz workflow with real server."""
        os.environ["GRIZ_BIN"] = griz_server_binary
        
        with Griz(test_database) as g:
            # Test state query
            state = g.state()
            assert "time_state" in state
            assert "max_time_state" in state
            
            # Test time manipulation
            max_state = state["max_time_state"]
            if max_state > 0:
                g.time.set_state(max_state)
                new_state = g.state()
                assert new_state["time_state"] == max_state
            
            # Test field operations
            fields = g.field.list_available()
            assert len(fields) > 0
            
            # Try to show a field (if stress available)
            stress_fields = [f for f in fields if f["name"] == "stress"]
            if stress_fields:
                g.field.show("stress", component="von_mises")
                state_after = g.state()
                # Verify field was set (exact format depends on implementation)
    
    def test_screenshot_capture(self, griz_server_binary, test_database):
        """Test screenshot functionality."""
        os.environ["GRIZ_BIN"] = griz_server_binary
        
        with Griz(test_database) as g:
            # Capture screenshot as bytes
            image_data = g.screenshot()
            assert isinstance(image_data, bytes)
            assert len(image_data) > 0
            
            # Verify it's a valid PNG (simple check)
            assert image_data[:8] == b'\x89PNG\r\n\x1a\n'
            
            # Capture screenshot to file
            with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as f:
                temp_path = f.name
            
            try:
                result_path = g.screenshot(temp_path)
                assert result_path == temp_path
                assert os.path.exists(temp_path)
                assert os.path.getsize(temp_path) > 0
            finally:
                if os.path.exists(temp_path):
                    os.unlink(temp_path)
    
    def test_error_handling(self, griz_server_binary, test_database):
        """Test error handling with real server."""
        os.environ["GRIZ_BIN"] = griz_server_binary
        
        with Griz(test_database) as g:
            # Test invalid command
            with pytest.raises(GrizError):
                g.raw("invalid_command_xyz")
            
            # Test invalid field
            with pytest.raises(UnknownFieldError):
                g.field.show("nonexistent_field")
            
            # Test invalid time state
            with pytest.raises(GrizError):
                g.time.set_state(99999)
    
    def test_concurrent_sessions(self, griz_server_binary, test_database):
        """Test multiple concurrent Griz sessions."""
        os.environ["GRIZ_BIN"] = griz_server_binary
        
        # Create multiple sessions
        sessions = []
        try:
            for i in range(3):
                g = Griz(test_database)
                sessions.append(g)
            
            # Test that each session is independent
            for i, g in enumerate(sessions):
                g.time.set_state(i)
                state = g.state()
                assert state["time_state"] == i
        
        finally:
            for g in sessions:
                g.close()
    
    def test_session_recovery(self, griz_server_binary, test_database):
        """Test session recovery after errors.""" 
        os.environ["GRIZ_BIN"] = griz_server_binary
        
        with Griz(test_database) as g:
            # Verify normal operation
            initial_state = g.state()
            
            # Force an error that might corrupt state
            try:
                g.raw("some_bad_command")
            except GrizError:
                pass  # Expected
            
            # Verify session still works
            recovery_state = g.state()
            assert "time_state" in recovery_state
            
            # Test normal operations still work
            g.time.set_state(0)
            final_state = g.state()
            assert final_state["time_state"] == 0
```

## 5. MCP Adapter Testing

### 5.1 MCP Tool Testing

```python
# tests/test_mcp_tools.py
import pytest
import asyncio
from unittest.mock import AsyncMock, MagicMock, patch
from mcp.types import CallToolRequest
from griz_mcp.tools import execute_tool, handle_call_tool
from griz_mcp.session import SessionManager

@pytest.mark.asyncio
class TestMCPTools:
    
    @pytest.fixture
    def mock_griz_session(self):
        """Mock Griz session for MCP testing."""
        session = MagicMock()
        session.open = MagicMock()
        session.field.show = MagicMock(return_value={"time_state": 42})
        session.view.rotate = MagicMock(return_value={"camera": {"x": 0}})
        session.screenshot = MagicMock(return_value=b"fake_png_data")
        session.state = MagicMock(return_value={"time_state": 42})
        return session
    
    async def test_open_database_tool(self, mock_griz_session):
        """Test open database MCP tool."""
        args = {"path": "/path/to/test.plt"}
        
        with patch('asyncio.get_event_loop') as mock_loop:
            mock_loop.return_value.run_in_executor = AsyncMock(return_value=None)
            
            result = await execute_tool("open_database", args, mock_griz_session)
        
        assert len(result) == 1
        assert result[0].type == "text"
        assert "Opened database" in result[0].text
        
        # Verify session method was called via executor
        mock_loop.return_value.run_in_executor.assert_called_once()
    
    async def test_show_field_tool(self, mock_griz_session):
        """Test show field MCP tool."""
        args = {"name": "stress", "component": "von_mises"}
        
        with patch('asyncio.get_event_loop') as mock_loop:
            mock_loop.return_value.run_in_executor = AsyncMock(return_value={"time_state": 42})
            
            result = await execute_tool("show_field", args, mock_griz_session)
        
        assert len(result) == 1
        assert result[0].type == "text"
        assert "stress" in result[0].text
        assert "von_mises" in result[0].text
    
    async def test_screenshot_tool(self, mock_griz_session):
        """Test screenshot MCP tool."""
        args = {}
        
        with patch('asyncio.get_event_loop') as mock_loop:
            mock_loop.return_value.run_in_executor = AsyncMock(return_value=b"fake_png_data")
            
            result = await execute_tool("screenshot", args, mock_griz_session)
        
        assert len(result) == 1
        assert result[0].type == "image"
        assert result[0].mimeType == "image/png"
        # Verify base64 encoding
        import base64
        decoded = base64.b64decode(result[0].data)
        assert decoded == b"fake_png_data"
    
    async def test_error_handling(self, mock_griz_session):
        """Test MCP tool error handling."""
        # Mock an exception in the Griz session
        mock_griz_session.field.show.side_effect = Exception("Test error")
        
        args = {"name": "stress"}
        
        with patch('asyncio.get_event_loop') as mock_loop:
            mock_loop.return_value.run_in_executor = AsyncMock(side_effect=Exception("Test error"))
            
            result = await execute_tool("show_field", args, mock_griz_session)
        
        assert len(result) == 1
        assert result[0].type == "text"
        assert "error" in result[0].text.lower()
        assert "Test error" in result[0].text

@pytest.mark.asyncio 
class TestSessionManager:
    
    async def test_session_lifecycle(self):
        """Test MCP session manager lifecycle."""
        manager = SessionManager()
        
        with patch('griz.Griz') as mock_griz_class:
            mock_session = MagicMock()
            mock_griz_class.return_value = mock_session
            
            # Get session (should create new one)
            session = await manager.get_session()
            assert session is mock_session
            mock_griz_class.assert_called_once()
            
            # Get session again (should reuse)
            session2 = await manager.get_session()
            assert session2 is mock_session
            # Should not create another instance
            assert mock_griz_class.call_count == 1
    
    async def test_database_operations(self):
        """Test database open/close through session manager."""
        manager = SessionManager()
        
        with patch('griz.Griz') as mock_griz_class:
            mock_session = MagicMock()
            mock_griz_class.return_value = mock_session
            
            # Test database opening
            await manager.open_database("/path/to/test.plt")
            
            # Verify session was created and database opened
            mock_griz_class.assert_called_once()
            # open() should have been called via executor
            
            # Test database closing
            await manager.close_database()
            # close() should have been called via executor
    
    async def test_session_restart(self):
        """Test session restart functionality."""
        manager = SessionManager()
        
        with patch('griz.Griz') as mock_griz_class:
            mock_session = MagicMock()
            mock_griz_class.return_value = mock_session
            
            # Create initial session
            session1 = await manager.get_session()
            
            # Restart session
            await manager.restart_session()
            
            # Get new session (should be different)
            session2 = await manager.get_session()
            
            # Should have created two sessions
            assert mock_griz_class.call_count == 2
            
            # Cleanup should have been called on first session
            # (verify via executor calls)
```

### 5.2 End-to-End MCP Testing

```python
# tests/test_mcp_integration.py
import pytest
import asyncio
import json
import subprocess
from unittest.mock import patch, AsyncMock

@pytest.mark.mcp_integration
class TestMCPIntegration:
    """End-to-end MCP integration tests."""
    
    @pytest.fixture
    async def mcp_server_process(self):
        """Start MCP server for testing."""
        # Start the MCP server as subprocess
        process = await asyncio.create_subprocess_exec(
            "python", "-m", "griz_mcp.server",
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )
        
        yield process
        
        # Cleanup
        process.terminate()
        await process.wait()
    
    async def test_mcp_server_startup(self, mcp_server_process):
        """Test MCP server starts and responds to initialization."""
        # Send MCP initialization
        init_request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {
                "protocolVersion": "2024-11-05",
                "clientInfo": {"name": "test-client", "version": "1.0.0"}
            }
        }
        
        request_json = json.dumps(init_request) + '\n'
        mcp_server_process.stdin.write(request_json.encode())
        await mcp_server_process.stdin.drain()
        
        # Read response
        response_line = await mcp_server_process.stdout.readline()
        response = json.loads(response_line.decode())
        
        assert response["id"] == 1
        assert "result" in response
        assert "capabilities" in response["result"]
    
    async def test_mcp_tool_execution(self, mcp_server_process):
        """Test MCP tool execution through server."""
        # Initialize server first
        await self._initialize_mcp_server(mcp_server_process)
        
        # Mock Griz session for testing
        with patch('griz_mcp.session.SessionManager.get_session') as mock_get_session:
            mock_session = AsyncMock()
            mock_session.state.return_value = {"time_state": 42}
            mock_get_session.return_value = mock_session
            
            # Send tool call request
            tool_request = {
                "jsonrpc": "2.0",
                "id": 2,
                "method": "tools/call",
                "params": {
                    "name": "get_state",
                    "arguments": {}
                }
            }
            
            request_json = json.dumps(tool_request) + '\n'
            mcp_server_process.stdin.write(request_json.encode())
            await mcp_server_process.stdin.drain()
            
            # Read response
            response_line = await mcp_server_process.stdout.readline()
            response = json.loads(response_line.decode())
            
            assert response["id"] == 2
            assert "result" in response
            assert "content" in response["result"]
    
    async def _initialize_mcp_server(self, process):
        """Helper to initialize MCP server."""
        init_request = {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize", 
            "params": {
                "protocolVersion": "2024-11-05",
                "clientInfo": {"name": "test", "version": "1.0.0"}
            }
        }
        
        request_json = json.dumps(init_request) + '\n'
        process.stdin.write(request_json.encode())
        await process.stdin.drain()
        
        # Read and discard response
        await process.stdout.readline()
```

## 6. System and Performance Testing

### 6.1 Performance Benchmarks

```python
# tests/test_performance.py
import pytest
import time
import statistics
from griz import Griz

@pytest.mark.performance
class TestPerformance:
    """Performance tests for MCP implementation."""
    
    @pytest.fixture(scope="session")
    def large_database(self):
        """Path to large test database for performance testing."""
        db_path = "tests/fixtures/large.plt"
        if not os.path.exists(db_path):
            pytest.skip("Large test database not available")
        return db_path
    
    def test_session_startup_time(self, large_database):
        """Test session startup performance."""
        startup_times = []
        
        for i in range(5):
            start_time = time.time()
            
            with Griz(large_database) as g:
                # Just ensure session is ready
                state = g.state()
            
            startup_time = time.time() - start_time
            startup_times.append(startup_time)
        
        avg_startup = statistics.mean(startup_times)
        max_startup = max(startup_times)
        
        # Performance thresholds
        assert avg_startup < 30.0, f"Average startup time too slow: {avg_startup:.2f}s"
        assert max_startup < 60.0, f"Max startup time too slow: {max_startup:.2f}s"
        
        print(f"Startup times: avg={avg_startup:.2f}s, max={max_startup:.2f}s")
    
    def test_command_latency(self, large_database):
        """Test command execution latency."""
        with Griz(large_database) as g:
            # Warm up
            g.state()
            
            # Measure command latencies
            latencies = []
            
            for state in range(0, min(10, g.state()["max_time_state"] + 1)):
                start_time = time.time()
                g.time.set_state(state)
                latency = time.time() - start_time
                latencies.append(latency)
            
            avg_latency = statistics.mean(latencies)
            max_latency = max(latencies)
            
            # Performance thresholds
            assert avg_latency < 2.0, f"Average command latency too slow: {avg_latency:.3f}s"
            assert max_latency < 5.0, f"Max command latency too slow: {max_latency:.3f}s"
            
            print(f"Command latencies: avg={avg_latency:.3f}s, max={max_latency:.3f}s")
    
    def test_screenshot_performance(self, large_database):
        """Test screenshot capture performance."""
        with Griz(large_database) as g:
            screenshot_times = []
            
            for i in range(5):
                start_time = time.time()
                image_data = g.screenshot()
                screenshot_time = time.time() - start_time
                screenshot_times.append(screenshot_time)
                
                # Verify reasonable image size
                assert len(image_data) > 1000, "Screenshot too small"
                assert len(image_data) < 10*1024*1024, "Screenshot too large"
            
            avg_time = statistics.mean(screenshot_times)
            
            # Performance threshold
            assert avg_time < 5.0, f"Screenshot capture too slow: {avg_time:.2f}s"
            
            print(f"Screenshot times: avg={avg_time:.2f}s")
    
    def test_memory_usage(self, large_database):
        """Test memory usage during session."""
        import psutil
        
        process = psutil.Process()
        initial_memory = process.memory_info().rss
        
        with Griz(large_database) as g:
            # Perform various operations
            for i in range(10):
                g.time.set_state(i % (g.state()["max_time_state"] + 1))
                g.screenshot()
            
            peak_memory = process.memory_info().rss
        
        final_memory = process.memory_info().rss
        
        # Memory growth checks
        memory_growth = peak_memory - initial_memory
        memory_leak = final_memory - initial_memory
        
        # Thresholds (adjust based on typical database sizes)
        max_growth = 1024 * 1024 * 1024  # 1GB
        max_leak = 100 * 1024 * 1024     # 100MB
        
        assert memory_growth < max_growth, f"Memory growth too large: {memory_growth // 1024 // 1024}MB"
        assert memory_leak < max_leak, f"Memory leak detected: {memory_leak // 1024 // 1024}MB"
        
        print(f"Memory: growth={memory_growth // 1024 // 1024}MB, leak={memory_leak // 1024 // 1024}MB")
```

### 6.2 Load Testing

```python
# tests/test_load.py
import pytest
import asyncio
import concurrent.futures
from griz import Griz
from griz_mcp.session import SessionManager

@pytest.mark.load
class TestLoadTesting:
    """Load tests for concurrent usage."""
    
    def test_concurrent_sessions(self):
        """Test multiple concurrent Griz sessions."""
        num_sessions = 5
        database = "tests/fixtures/medium.plt"
        
        def session_worker(session_id):
            """Worker function for each session."""
            results = []
            
            try:
                with Griz(database) as g:
                    # Perform operations
                    for i in range(10):
                        g.time.set_state(i % (g.state()["max_time_state"] + 1))
                        state = g.state()
                        results.append(state["time_state"])
                
                return {"session_id": session_id, "success": True, "results": results}
                
            except Exception as e:
                return {"session_id": session_id, "success": False, "error": str(e)}
        
        # Run concurrent sessions
        with concurrent.futures.ThreadPoolExecutor(max_workers=num_sessions) as executor:
            futures = [executor.submit(session_worker, i) for i in range(num_sessions)]
            results = [future.result(timeout=60) for future in futures]
        
        # Verify all sessions succeeded
        successes = [r for r in results if r["success"]]
        failures = [r for r in results if not r["success"]]
        
        assert len(successes) == num_sessions, f"Some sessions failed: {failures}"
        
        # Verify each session produced expected results
        for result in successes:
            assert len(result["results"]) == 10
    
    @pytest.mark.asyncio
    async def test_mcp_concurrent_tools(self):
        """Test concurrent MCP tool execution."""
        num_calls = 20
        session_manager = SessionManager()
        
        async def tool_worker(call_id):
            """Worker for individual tool calls."""
            try:
                from griz_mcp.tools import execute_tool
                
                with patch('griz.Griz') as mock_griz:
                    mock_session = AsyncMock()
                    mock_session.state.return_value = {"time_state": call_id}
                    mock_griz.return_value = mock_session
                    
                    # Simulate tool execution
                    result = await execute_tool("get_state", {}, mock_session)
                    
                return {"call_id": call_id, "success": True, "result": result}
                
            except Exception as e:
                return {"call_id": call_id, "success": False, "error": str(e)}
        
        # Run concurrent tool calls
        tasks = [tool_worker(i) for i in range(num_calls)]
        results = await asyncio.gather(*tasks, return_exceptions=True)
        
        # Verify results
        successes = [r for r in results if isinstance(r, dict) and r.get("success")]
        failures = [r for r in results if not (isinstance(r, dict) and r.get("success"))]
        
        assert len(successes) == num_calls, f"Some tool calls failed: {failures}"
```

## 7. Test Infrastructure

### 7.1 Test Data Management

```python
# tests/conftest.py
import pytest
import os
import tempfile
import shutil

@pytest.fixture(scope="session")
def test_data_dir():
    """Directory containing test databases and fixtures."""
    return os.path.join(os.path.dirname(__file__), "fixtures")

@pytest.fixture(scope="session") 
def small_database(test_data_dir):
    """Small test database for unit tests."""
    db_path = os.path.join(test_data_dir, "small.plt")
    if not os.path.exists(db_path):
        pytest.skip("Small test database not available")
    return db_path

@pytest.fixture(scope="session")
def medium_database(test_data_dir):
    """Medium test database for integration tests."""
    db_path = os.path.join(test_data_dir, "medium.plt") 
    if not os.path.exists(db_path):
        pytest.skip("Medium test database not available")
    return db_path

@pytest.fixture
def temp_dir():
    """Temporary directory for test outputs."""
    temp_path = tempfile.mkdtemp(prefix="griz_test_")
    yield temp_path
    shutil.rmtree(temp_path, ignore_errors=True)

@pytest.fixture
def mock_griz_bin(tmp_path):
    """Mock griz-server binary for unit tests."""
    mock_script = tmp_path / "mock-griz-server"
    mock_script.write_text("""#!/usr/bin/env python3
import sys
import json
import time

# Simulate server behavior
print('{"type":"event","event":"ready"}')
sys.stdout.flush()

for line in sys.stdin:
    if line.strip():
        response = {
            "type": "response",
            "id": "test",
            "status": "ok",
            "stdout": "Mock response",
            "stderr": "",
            "data": {"time_state": 0}
        }
        print(json.dumps(response))
        sys.stdout.flush()
""")
    mock_script.chmod(0o755)
    return str(mock_script)
```

### 7.2 CI/CD Configuration

```yaml
# .github/workflows/mcp-tests.yml
name: MCP Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        python-version: [3.8, 3.9, "3.10", "3.11"]
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Set up Python ${{ matrix.python-version }}
      uses: actions/setup-python@v4
      with:
        python-version: ${{ matrix.python-version }}
    
    - name: Install dependencies
      run: |
        python -m pip install --upgrade pip
        pip install pytest pytest-cov pytest-asyncio pytest-mock
        pip install -e Src/python/griz/
        pip install -e Src/python/griz_mcp/
    
    - name: Run unit tests
      run: |
        pytest Src/python/griz/tests/ \
               Src/python/griz_mcp/tests/ \
               --cov=griz --cov=griz_mcp \
               --cov-report=xml \
               --cov-fail-under=90
    
    - name: Upload coverage
      uses: codecov/codecov-action@v3
      with:
        file: ./coverage.xml

  integration-tests:
    runs-on: ubuntu-latest
    needs: unit-tests
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Set up Python 3.10
      uses: actions/setup-python@v4
      with:
        python-version: "3.10"
    
    - name: Install system dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y build-essential gfortran libmesa-dev
    
    - name: Build griz-server
      run: |
        cd Src
        make griz-server
        export PATH=$PWD:$PATH
    
    - name: Install Python packages
      run: |
        pip install pytest pytest-asyncio
        pip install -e Src/python/griz/
        pip install -e Src/python/griz_mcp/
    
    - name: Run integration tests
      run: |
        export GRIZ_BIN=$PWD/Src/griz-server
        pytest Src/python/griz/tests/ \
               -m integration \
               --tb=short
    
    - name: Run MCP integration tests
      run: |
        export GRIZ_BIN=$PWD/Src/griz-server
        pytest Src/python/griz_mcp/tests/ \
               -m mcp_integration \
               --tb=short

  performance-tests:
    runs-on: ubuntu-latest
    needs: integration-tests
    if: github.event_name == 'push'
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Set up Python 3.10
      uses: actions/setup-python@v4
      with:
        python-version: "3.10"
    
    - name: Build and test performance
      run: |
        # Build griz-server
        cd Src && make griz-server
        export GRIZ_BIN=$PWD/griz-server
        
        # Install Python packages
        pip install pytest psutil
        pip install -e python/griz/
        
        # Run performance tests
        pytest python/griz/tests/ -m performance --tb=short
```

## 8. Test Execution Strategy

### 8.1 Test Categories and Execution

**Developer workflow**:
```bash
# Quick unit tests (< 30 seconds)
pytest tests/unit/ -x

# Integration tests (< 5 minutes)
pytest tests/integration/ 

# Full test suite (< 15 minutes)
pytest tests/ --cov=griz --cov=griz_mcp
```

**CI/CD pipeline**:
- **Unit tests**: Every commit, all Python versions
- **Integration tests**: Every commit, single Python version
- **Performance tests**: Nightly builds, performance tracking
- **System tests**: Release candidates, manual verification

### 8.2 Test Data Strategy

**Small fixtures**: Committed to repository
- `tests/fixtures/small.plt` — 1000 elements, 5 time states
- `tests/fixtures/simple.plt` — 100 elements, 1 time state

**Large fixtures**: Downloaded during CI or skipped
- Performance tests use realistic database sizes
- Automated generation of test databases

**Mock data**: Comprehensive for unit tests
- JSON response fixtures
- Mock server implementations
- Synthetic test scenarios

## 9. Open Questions

1. **Test database licensing**: Can we distribute small Mili databases with source code? Need to create or obtain permissive test data.

2. **Performance baselines**: What are realistic performance expectations for different database sizes? Need benchmarking on target hardware.

3. **Cross-platform testing**: Should we test on Windows/macOS in CI? Resource constraints may limit this.

4. **Memory leak detection**: Should we integrate valgrind or similar tools for C code testing? Adds complexity but valuable.

5. **Fuzz testing**: Should we implement fuzzing for JSON protocol and command parsing? Good for robustness but requires additional infrastructure.

## Success Criteria

- Comprehensive test coverage (>90% for Python, >80% for C)
- Fast unit tests (<30s) enabling tight development loops  
- Reliable integration tests catching real-world issues
- Performance tests preventing regressions
- Automated CI/CD pipeline with multiple Python versions
- Clear test documentation and easy local reproduction
- Test data management supporting various database sizes
- Robust error scenario testing covering all failure modes