# 04 — MCP Adapter Implementation

## Scope

This document details the `griz-mcp` package implementation, which provides the MCP (Model Context Protocol) adapter layer on top of the `griz` Python package. This includes MCP tool definitions, session management, image handling, error translation, and the MCP server configuration. The adapter is designed to be a thin, declarative layer that exposes Griz functionality to AI assistants.

## Related

- [01-architecture.md](01-architecture.md) — overall system design
- [03-python-api.md](03-python-api.md) — underlying Python API
- [05-protocol.md](05-protocol.md) — JSON communication protocol
- MCP specification: https://spec.modelcontextprotocol.io/

## 1. Package Overview

### 1.1 Package Structure

```
Src/python/griz_mcp/
├── __init__.py          # Package exports
├── pyproject.toml       # Build configuration  
├── README.md           # MCP-specific documentation
├── server.py           # MCP server entrypoint
├── tools.py            # MCP tool implementations
├── session.py          # Session management
├── images.py           # Image handling utilities
├── examples/
│   ├── basic_usage.py  # Example MCP client usage
│   └── prompts.md      # Example AI prompts
└── tests/
    ├── test_tools.py
    ├── test_session.py
    └── test_images.py
```

### 1.2 MCP Integration

The package implements the MCP server specification with the following capabilities:

- **Tools**: Expose Griz operations as callable MCP tools
- **Resources**: Provide access to database metadata and screenshots
- **Prompts**: Pre-defined prompt templates for common workflows
- **Session Management**: Handle connection lifecycle and cleanup

## 2. MCP Server Implementation

### 2.1 Server Entrypoint

```python
# server.py
import asyncio
import logging
from mcp.server import Server, NotificationOptions
from mcp.server.models import InitializationOptions
from mcp.server.stdio import stdio_server
from mcp.types import (
    CallToolRequest, 
    ListToolsRequest,
    GetPromptRequest,
    ListPromptsRequest
)

from .tools import register_tools
from .session import SessionManager
from .prompts import register_prompts

# Global server instance
server = Server("griz-mcp")
session_manager = SessionManager()

@server.list_tools()
async def handle_list_tools() -> list[Tool]:
    """Return list of available tools."""
    return register_tools()

@server.call_tool()
async def handle_call_tool(request: CallToolRequest) -> list[TextContent | ImageContent]:
    """Execute a tool call."""
    try:
        tool_name = request.name
        arguments = request.arguments or {}
        
        # Get the global Griz session
        griz_session = await session_manager.get_session()
        
        # Dispatch to appropriate tool handler
        result = await execute_tool(tool_name, arguments, griz_session)
        
        return result
        
    except Exception as e:
        logging.error(f"Tool execution failed: {e}")
        return [TextContent(type="text", text=f"Error: {str(e)}")]

async def main():
    """Main server loop."""
    # Configure logging
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s"
    )
    
    # Run server
    options = InitializationOptions(
        server_name="griz-mcp",
        server_version="1.0.0",
    )
    
    async with stdio_server() as (read_stream, write_stream):
        await server.run(
            read_stream, 
            write_stream, 
            options
        )

if __name__ == "__main__":
    asyncio.run(main())
```

### 2.2 Session Management

```python
# session.py
import asyncio
import logging
from typing import Optional
from griz import Griz
from griz.exceptions import GrizError

class SessionManager:
    """Manages the global Griz session for MCP clients."""
    
    def __init__(self):
        self._session: Optional[Griz] = None
        self._lock = asyncio.Lock()
        self._current_database: Optional[str] = None
    
    async def get_session(self) -> Griz:
        """Get or create the global Griz session."""
        async with self._lock:
            if self._session is None:
                self._session = Griz()
            return self._session
    
    async def open_database(self, path: str) -> None:
        """Open a database in the current session."""
        async with self._lock:
            session = await self.get_session()
            
            # Use asyncio thread pool for blocking operation
            await asyncio.get_event_loop().run_in_executor(
                None, session.open, path
            )
            
            self._current_database = path
            logging.info(f"Opened database: {path}")
    
    async def close_database(self) -> None:
        """Close current database."""
        async with self._lock:
            if self._session:
                await asyncio.get_event_loop().run_in_executor(
                    None, self._session.close
                )
                self._current_database = None
                logging.info("Closed database")
    
    async def restart_session(self) -> None:
        """Restart the Griz session (cleanup and recreate)."""
        async with self._lock:
            if self._session:
                try:
                    await asyncio.get_event_loop().run_in_executor(
                        None, self._session.close
                    )
                except Exception as e:
                    logging.warning(f"Error closing session: {e}")
                
            self._session = None
            self._current_database = None
            logging.info("Restarted Griz session")
    
    async def get_status(self) -> dict:
        """Get current session status."""
        return {
            "session_active": self._session is not None,
            "database_open": self._current_database is not None,
            "database_path": self._current_database
        }
```

## 3. Tool Implementations

### 3.1 Tool Registration

```python
# tools.py
from mcp.types import Tool, TextContent, ImageContent
from mcp.server.models import ToolArg

def register_tools() -> list[Tool]:
    """Register all available MCP tools."""
    return [
        # Database operations
        Tool(
            name="open_database",
            description="Open a Mili database file for visualization",
            inputSchema={
                "type": "object",
                "properties": {
                    "path": {
                        "type": "string",
                        "description": "Path to the Mili database file"
                    }
                },
                "required": ["path"]
            }
        ),
        
        # Field operations  
        Tool(
            name="show_field",
            description="Display a field/result with optional component",
            inputSchema={
                "type": "object", 
                "properties": {
                    "name": {
                        "type": "string",
                        "description": "Field name (e.g., stress, displacement, temperature)"
                    },
                    "component": {
                        "type": "string", 
                        "description": "Component name (e.g., von_mises, magnitude, xx)",
                        "default": None
                    }
                },
                "required": ["name"]
            }
        ),
        
        # View operations
        Tool(
            name="rotate_view",
            description="Rotate the 3D view by specified angles",
            inputSchema={
                "type": "object",
                "properties": {
                    "x": {"type": "number", "description": "X rotation in degrees", "default": 0},
                    "y": {"type": "number", "description": "Y rotation in degrees", "default": 0}, 
                    "z": {"type": "number", "description": "Z rotation in degrees", "default": 0}
                }
            }
        ),
        
        # Time operations
        Tool(
            name="set_time_state", 
            description="Set the current time state/frame",
            inputSchema={
                "type": "object",
                "properties": {
                    "state": {
                        "type": "integer",
                        "description": "Time state number (0-based)",
                        "minimum": 0
                    }
                },
                "required": ["state"]
            }
        ),
        
        # Material operations
        Tool(
            name="hide_materials",
            description="Hide specified materials from view", 
            inputSchema={
                "type": "object",
                "properties": {
                    "material_ids": {
                        "type": "array",
                        "items": {"type": "integer"},
                        "description": "List of material IDs to hide"
                    }
                },
                "required": ["material_ids"]
            }
        ),
        
        Tool(
            name="show_materials",
            description="Show specified materials in view",
            inputSchema={
                "type": "object", 
                "properties": {
                    "material_ids": {
                        "type": "array",
                        "items": {"type": "integer"},
                        "description": "List of material IDs to show"
                    }
                },
                "required": ["material_ids"]
            }
        ),
        
        # Screenshot
        Tool(
            name="screenshot",
            description="Capture a screenshot of the current view",
            inputSchema={
                "type": "object",
                "properties": {
                    "width": {
                        "type": "integer", 
                        "description": "Image width in pixels",
                        "default": 1024
                    },
                    "height": {
                        "type": "integer",
                        "description": "Image height in pixels", 
                        "default": 1024
                    }
                }
            }
        ),
        
        # State queries
        Tool(
            name="get_state",
            description="Get current viewer state (time, camera, fields, etc.)",
            inputSchema={"type": "object", "properties": {}}
        ),
        
        Tool(
            name="list_fields",
            description="List available fields/results in current database",
            inputSchema={"type": "object", "properties": {}}
        ),
        
        Tool(
            name="list_materials", 
            description="List all materials in current database",
            inputSchema={"type": "object", "properties": {}}
        ),
        
        # Session management
        Tool(
            name="restart_session",
            description="Restart the Griz session (useful for recovery)",
            inputSchema={"type": "object", "properties": {}}
        ),
        
        # Escape hatch
        Tool(
            name="raw_command",
            description="Execute a raw Griz command (advanced users only)",
            inputSchema={
                "type": "object",
                "properties": {
                    "command": {
                        "type": "string",
                        "description": "Raw Griz command string"
                    }
                },
                "required": ["command"]
            }
        ),
    ]
```

### 3.2 Tool Execution

```python
async def execute_tool(tool_name: str, arguments: dict, griz_session: Griz) -> list[TextContent | ImageContent]:
    """Execute a specific tool with given arguments."""
    
    try:
        if tool_name == "open_database":
            return await _open_database(arguments, griz_session)
        elif tool_name == "show_field":
            return await _show_field(arguments, griz_session)
        elif tool_name == "rotate_view":
            return await _rotate_view(arguments, griz_session)
        elif tool_name == "set_time_state":
            return await _set_time_state(arguments, griz_session)
        elif tool_name == "hide_materials":
            return await _hide_materials(arguments, griz_session)
        elif tool_name == "show_materials":
            return await _show_materials(arguments, griz_session)
        elif tool_name == "screenshot":
            return await _screenshot(arguments, griz_session)
        elif tool_name == "get_state":
            return await _get_state(arguments, griz_session)
        elif tool_name == "list_fields":
            return await _list_fields(arguments, griz_session)
        elif tool_name == "list_materials":
            return await _list_materials(arguments, griz_session)
        elif tool_name == "restart_session":
            return await _restart_session(arguments)
        elif tool_name == "raw_command":
            return await _raw_command(arguments, griz_session)
        else:
            return [TextContent(type="text", text=f"Unknown tool: {tool_name}")]
            
    except Exception as e:
        logging.error(f"Tool {tool_name} failed: {e}")
        return [TextContent(type="text", text=f"Tool execution failed: {str(e)}")]

# Tool implementation functions
async def _open_database(args: dict, session: Griz) -> list[TextContent]:
    """Open database tool implementation."""
    path = args["path"]
    
    # Use thread pool for blocking I/O
    await asyncio.get_event_loop().run_in_executor(
        None, session.open, path
    )
    
    # Get state after opening
    state = await asyncio.get_event_loop().run_in_executor(
        None, session.state
    )
    
    return [TextContent(
        type="text", 
        text=f"Opened database: {path}\nTime states: {state.get('max_time_state', 0) + 1}\nCurrent state: {state.get('time_state', 0)}"
    )]

async def _show_field(args: dict, session: Griz) -> list[TextContent]:
    """Show field tool implementation."""
    name = args["name"] 
    component = args.get("component")
    
    # Execute field.show() in thread pool
    result = await asyncio.get_event_loop().run_in_executor(
        None, lambda: session.field.show(name, component=component)
    )
    
    return [TextContent(
        type="text",
        text=f"Showing field: {name}" + (f" ({component})" if component else "") + f"\nTime state: {result.get('time_state', 'unknown')}"
    )]

async def _screenshot(args: dict, session: Griz) -> list[ImageContent]:
    """Screenshot tool implementation."""
    # Capture screenshot in thread pool
    image_data = await asyncio.get_event_loop().run_in_executor(
        None, session.screenshot
    )
    
    # Convert to base64 for MCP
    import base64
    if isinstance(image_data, bytes):
        # Raw bytes
        b64_data = base64.b64encode(image_data).decode('utf-8')
    else:
        # File path - read and encode
        with open(image_data, 'rb') as f:
            b64_data = base64.b64encode(f.read()).decode('utf-8')
    
    return [ImageContent(
        type="image",
        data=b64_data,
        mimeType="image/png"
    )]

async def _get_state(args: dict, session: Griz) -> list[TextContent]:
    """Get state tool implementation."""
    state = await asyncio.get_event_loop().run_in_executor(
        None, session.state
    )
    
    import json
    return [TextContent(
        type="text",
        text=f"Current state:\n{json.dumps(state, indent=2)}"
    )]
```

## 4. Image Handling

### 4.1 Image Utilities

```python
# images.py
import base64
import tempfile
import os
from typing import Union
from mcp.types import ImageContent

class ImageHandler:
    """Utilities for handling images in MCP context."""
    
    @staticmethod
    def create_image_content(data: Union[str, bytes], mime_type: str = "image/png") -> ImageContent:
        """Create MCP ImageContent from image data.
        
        Args:
            data: Either file path (str) or raw image bytes
            mime_type: MIME type of the image
            
        Returns:
            MCP ImageContent object
        """
        if isinstance(data, str):
            # File path - read and encode
            with open(data, 'rb') as f:
                image_bytes = f.read()
        else:
            # Raw bytes
            image_bytes = data
        
        # Encode to base64
        b64_data = base64.b64encode(image_bytes).decode('utf-8')
        
        return ImageContent(
            type="image",
            data=b64_data,
            mimeType=mime_type
        )
    
    @staticmethod
    def save_temp_image(image_data: bytes, suffix: str = ".png") -> str:
        """Save image bytes to temporary file.
        
        Args:
            image_data: Raw image bytes
            suffix: File suffix/extension
            
        Returns:
            Path to temporary file
        """
        with tempfile.NamedTemporaryFile(delete=False, suffix=suffix) as f:
            f.write(image_data)
            return f.name
    
    @staticmethod
    def cleanup_temp_file(path: str) -> None:
        """Clean up temporary file."""
        try:
            os.unlink(path)
        except OSError:
            pass  # File already deleted or doesn't exist
```

## 5. Prompt Templates

### 5.1 Prompt Registration

```python
# prompts.py
from mcp.types import Prompt, PromptArgument

def register_prompts() -> list[Prompt]:
    """Register pre-defined prompt templates."""
    return [
        Prompt(
            name="analyze_stress",
            description="Analyze stress distribution in a simulation",
            arguments=[
                PromptArgument(
                    name="database_path",
                    description="Path to the Mili database file",
                    required=True
                ),
                PromptArgument(
                    name="time_state",
                    description="Time state to analyze (default: final)",
                    required=False
                )
            ]
        ),
        
        Prompt(
            name="create_animation",
            description="Create an animation through time states",
            arguments=[
                PromptArgument(
                    name="database_path", 
                    description="Path to the Mili database file",
                    required=True
                ),
                PromptArgument(
                    name="field_name",
                    description="Field to visualize (default: von Mises stress)",
                    required=False
                ),
                PromptArgument(
                    name="frame_count", 
                    description="Number of frames to generate (default: 10)",
                    required=False
                )
            ]
        ),
        
        Prompt(
            name="compare_materials",
            description="Compare different materials in a simulation",
            arguments=[
                PromptArgument(
                    name="database_path",
                    description="Path to the Mili database file", 
                    required=True
                ),
                PromptArgument(
                    name="material_ids",
                    description="Comma-separated material IDs to compare",
                    required=True
                )
            ]
        )
    ]

@server.get_prompt()
async def handle_get_prompt(request: GetPromptRequest) -> GetPromptResult:
    """Handle prompt requests."""
    name = request.name
    args = request.arguments or {}
    
    if name == "analyze_stress":
        return _get_analyze_stress_prompt(args)
    elif name == "create_animation":
        return _get_create_animation_prompt(args)
    elif name == "compare_materials":
        return _get_compare_materials_prompt(args)
    else:
        raise ValueError(f"Unknown prompt: {name}")

def _get_analyze_stress_prompt(args: dict) -> GetPromptResult:
    """Generate stress analysis prompt."""
    database_path = args["database_path"]
    time_state = args.get("time_state", "final")
    
    prompt_text = f"""
Please analyze the stress distribution in the simulation at {database_path}.

Tasks:
1. Open the database: {database_path}
2. Navigate to time state: {time_state}
3. Display von Mises stress
4. Take a screenshot to show the stress distribution
5. Identify areas of high stress concentration
6. Rotate the view to show different perspectives
7. Hide low-stress materials to focus on critical regions

Provide insights about:
- Maximum stress values and locations
- Stress concentration patterns
- Potential failure modes
- Recommendations for design improvements
"""
    
    return GetPromptResult(
        description="Stress analysis workflow",
        messages=[
            PromptMessage(
                role="user",
                content=TextContent(type="text", text=prompt_text)
            )
        ]
    )
```

## 6. Configuration and Environment

### 6.1 Environment Variables

```python
import os

class MCPConfig:
    """Configuration for MCP server."""
    
    @staticmethod
    def get_log_level() -> str:
        """Get logging level from environment."""
        return os.environ.get("GRIZ_MCP_LOG_LEVEL", "INFO")
    
    @staticmethod 
    def get_timeout() -> float:
        """Get command timeout from environment."""
        return float(os.environ.get("GRIZ_MCP_TIMEOUT", "60.0"))
    
    @staticmethod
    def get_temp_dir() -> str:
        """Get temporary directory for screenshots."""
        return os.environ.get("GRIZ_MCP_TEMP_DIR", "/tmp")
    
    @staticmethod
    def get_max_image_size() -> int:
        """Get maximum image size in bytes."""
        return int(os.environ.get("GRIZ_MCP_MAX_IMAGE_SIZE", "10485760"))  # 10MB
```

### 6.2 Launch Script

```python
#!/usr/bin/env python3
# Launch script for griz-mcp server

import sys
import os
import asyncio
import logging

def setup_logging():
    """Configure logging for the MCP server."""
    log_level = os.environ.get("GRIZ_MCP_LOG_LEVEL", "INFO")
    
    logging.basicConfig(
        level=getattr(logging, log_level.upper()),
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        handlers=[
            logging.FileHandler(os.path.expanduser("~/.griz/mcp.log")),
            logging.StreamHandler(sys.stderr)
        ]
    )

def main():
    """Main entrypoint for griz-mcp server."""
    setup_logging()
    
    try:
        from griz_mcp.server import main as server_main
        asyncio.run(server_main())
    except KeyboardInterrupt:
        logging.info("Server stopped by user")
    except Exception as e:
        logging.error(f"Server error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
```

## 7. Error Handling and Logging

### 7.1 Error Translation

```python
from griz.exceptions import GrizError, UnknownFieldError, DatabaseError
from mcp.types import TextContent

def translate_griz_error(error: Exception) -> TextContent:
    """Translate Griz exceptions to user-friendly MCP responses."""
    
    if isinstance(error, UnknownFieldError):
        return TextContent(
            type="text",
            text=f"Unknown field or component: {str(error)}\n"
                 f"Use the list_fields tool to see available options."
        )
    
    elif isinstance(error, DatabaseError):
        return TextContent(
            type="text", 
            text=f"Database error: {str(error)}\n"
                 f"Please check that the file exists and is a valid Mili database."
        )
    
    elif isinstance(error, GrizError):
        return TextContent(
            type="text",
            text=f"Griz error: {str(error)}"
        )
    
    else:
        # Generic error
        return TextContent(
            type="text",
            text=f"Unexpected error: {str(error)}\n"
                 f"You may want to try restarting the session."
        )

def log_tool_call(tool_name: str, arguments: dict, success: bool, duration: float):
    """Log tool execution for debugging."""
    status = "SUCCESS" if success else "FAILED"
    logging.info(f"Tool {tool_name} {status} in {duration:.2f}s - args: {arguments}")
```

### 7.2 Graceful Degradation

```python
async def safe_execute_tool(tool_name: str, arguments: dict, session: Griz) -> list[TextContent | ImageContent]:
    """Execute tool with comprehensive error handling."""
    start_time = time.time()
    
    try:
        # Execute the tool
        result = await execute_tool(tool_name, arguments, session)
        
        # Log successful execution
        duration = time.time() - start_time
        log_tool_call(tool_name, arguments, True, duration)
        
        return result
        
    except Exception as e:
        # Log failed execution
        duration = time.time() - start_time 
        log_tool_call(tool_name, arguments, False, duration)
        
        # Try to provide helpful error response
        error_response = translate_griz_error(e)
        
        # For certain errors, suggest recovery actions
        if isinstance(e, (DatabaseError, GrizError)):
            recovery_text = "\n\nSuggested recovery:\n"
            recovery_text += "1. Check database path and permissions\n"
            recovery_text += "2. Try restarting the session with restart_session tool\n"
            recovery_text += "3. Verify griz-server is properly installed"
            
            error_response.text += recovery_text
        
        return [error_response]
```

## 8. Testing Strategy

### 8.1 Unit Tests

```python
# tests/test_tools.py
import pytest
from unittest.mock import AsyncMock, MagicMock
from griz_mcp.tools import execute_tool
from griz_mcp.session import SessionManager

@pytest.fixture
def mock_griz_session():
    """Mock Griz session for testing."""
    session = MagicMock()
    session.open = MagicMock()
    session.field.show = MagicMock(return_value={"time_state": 42})
    session.view.rotate = MagicMock(return_value={"time_state": 42})
    session.screenshot = MagicMock(return_value=b"fake_png_data")
    session.state = MagicMock(return_value={"time_state": 42, "max_time_state": 100})
    return session

@pytest.mark.asyncio
async def test_open_database_tool(mock_griz_session):
    """Test open database tool."""
    args = {"path": "/path/to/test.plt"}
    result = await execute_tool("open_database", args, mock_griz_session)
    
    assert len(result) == 1
    assert "Opened database" in result[0].text
    mock_griz_session.open.assert_called_once_with("/path/to/test.plt")

@pytest.mark.asyncio 
async def test_show_field_tool(mock_griz_session):
    """Test show field tool."""
    args = {"name": "stress", "component": "von_mises"}
    result = await execute_tool("show_field", args, mock_griz_session)
    
    assert len(result) == 1
    assert "Showing field: stress (von_mises)" in result[0].text
    mock_griz_session.field.show.assert_called_once_with("stress", component="von_mises")

@pytest.mark.asyncio
async def test_screenshot_tool(mock_griz_session):
    """Test screenshot tool."""
    args = {}
    result = await execute_tool("screenshot", args, mock_griz_session)
    
    assert len(result) == 1 
    assert result[0].type == "image"
    assert result[0].mimeType == "image/png"
    mock_griz_session.screenshot.assert_called_once()
```

### 8.2 Integration Tests

```python
# tests/test_integration.py
import pytest
import asyncio
from mcp.types import CallToolRequest

@pytest.mark.integration
@pytest.mark.asyncio
async def test_full_workflow():
    """Test complete workflow from MCP client perspective."""
    # This would use a real test database and griz-server
    
    # 1. Open database
    open_request = CallToolRequest(
        name="open_database",
        arguments={"path": "tests/fixtures/small.plt"}
    )
    open_result = await handle_call_tool(open_request)
    assert "Opened database" in open_result[0].text
    
    # 2. Show field
    field_request = CallToolRequest(
        name="show_field", 
        arguments={"name": "stress", "component": "von_mises"}
    )
    field_result = await handle_call_tool(field_request)
    assert "Showing field" in field_result[0].text
    
    # 3. Take screenshot
    screenshot_request = CallToolRequest(name="screenshot", arguments={})
    screenshot_result = await handle_call_tool(screenshot_request)
    assert screenshot_result[0].type == "image"
```

## 9. Documentation and Examples

### 9.1 Usage Examples

```python
# examples/basic_usage.py
"""
Example usage of griz-mcp with Claude or other MCP clients.

This demonstrates common workflows for analyzing simulation data.
"""

# Example prompts for AI assistants:

STRESS_ANALYSIS_PROMPT = """
I have a Mili database at /path/to/simulation.plt that contains results from a structural analysis. 
Can you help me analyze the stress distribution? I'd like to:

1. Open the database
2. Go to the final time state
3. Display von Mises stress  
4. Take a screenshot
5. Identify areas of highest stress
6. Rotate the view to show different angles
7. Hide materials with low stress to focus on critical regions

Please walk me through this analysis step by step.
"""

ANIMATION_PROMPT = """
I want to create an animation showing how stress evolves over time in my simulation at /path/to/dynamic.plt.
Can you help me:

1. Open the database
2. Set up stress visualization
3. Step through time states from 0 to the final state
4. Capture screenshots at regular intervals
5. Describe what's happening in the stress evolution

Let's create about 10 frames covering the full time range.
"""

MATERIAL_COMPARISON_PROMPT = """
I have a multi-material simulation at /path/to/materials.plt and want to compare how different materials behave.
The database contains materials 1-5. Can you:

1. Open the database and go to peak load (around state 50)
2. Show displacement magnitude
3. Hide all materials except material 1, take a screenshot
4. Hide all materials except material 2, take a screenshot  
5. Continue for materials 3-5
6. Show all materials together for comparison
7. Comment on which materials show the largest deformations
"""
```

### 9.2 Prompt Templates

```markdown
# examples/prompts.md

# Griz MCP Prompt Templates

## Stress Analysis

Use this template when you want to analyze stress distribution in a simulation:

```
Please analyze the stress in my simulation at [DATABASE_PATH]. I want to understand:
- Where are the highest stresses located?
- How do stresses vary across different materials?
- Are there any stress concentrations that could lead to failure?

Steps:
1. Open the database
2. Navigate to time state [TIME_STATE] (or final if not specified)
3. Display von Mises stress
4. Take screenshots from multiple angles
5. Hide low-stress regions to highlight critical areas
6. Provide analysis of the stress patterns
```

## Animation Creation

For time-dependent analyses:

```
Create an animation showing [FIELD_NAME] evolution in my simulation at [DATABASE_PATH].
I want to see how the field changes from initial loading to final state.

Requirements:
- Generate [FRAME_COUNT] frames evenly spaced through time
- Use appropriate field visualization (stress, displacement, etc.)
- Include time state information in the analysis
- Comment on key events or transitions you observe
```

## Material Comparison

For multi-material studies:

```
Compare the behavior of different materials in my simulation at [DATABASE_PATH].
Focus on materials [MATERIAL_LIST] at time state [TIME_STATE].

For each material:
1. Show only that material
2. Display [FIELD_NAME] (displacement, stress, etc.)
3. Take a screenshot
4. Comment on the material's response

Then show all materials together for overall comparison.
```
```

## 10. Dependencies and Requirements

### 10.1 Python Dependencies

```toml
# pyproject.toml
[build-system]
requires = ["setuptools>=45", "wheel"]
build-backend = "setuptools.build_meta"

[project]
name = "griz-mcp"
version = "1.0.0"
description = "MCP adapter for Griz visualization"
authors = [{name = "Griz Team", email = "griz@llnl.gov"}]
license = {text = "BSD-3-Clause"}
requires-python = ">=3.8"
dependencies = [
    "griz>=1.0.0",
    "mcp>=0.1.0",
]

[project.optional-dependencies]
test = [
    "pytest>=6.0",
    "pytest-asyncio>=0.18.0", 
    "pytest-cov",
    "pytest-mock",
]

[project.scripts]
griz-mcp = "griz_mcp.server:main"
```

### 10.2 Runtime Requirements

- Python 3.8+ with asyncio support
- `griz` package and `griz-server` binary  
- MCP-compatible client (Claude, etc.)
- Sufficient resources for 3D rendering

## 11. Open Questions

1. **Session persistence**: Should MCP sessions survive across client disconnects? Current design doesn't persist state.

2. **Multi-client support**: Can multiple MCP clients share one griz-server instance? Current design assumes single client.

3. **Resource limits**: How to handle large databases or long-running operations in MCP context? Need timeout and memory limits.

4. **Streaming updates**: For animations, should we stream frames or provide batch generation? Current design is batch-oriented.

5. **Advanced picking**: Should we expose element/node picking through MCP? Not implemented in v1.

## Success Criteria

- AI assistants can successfully drive Griz through natural language
- All common visualization workflows accessible via MCP tools  
- Robust error handling with helpful recovery suggestions
- Performance suitable for interactive use
- Comprehensive examples and prompt templates
- Easy integration with MCP clients
- Full compatibility with underlying `griz` package
- Comprehensive test coverage including integration tests