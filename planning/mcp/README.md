# MCP Implementation Planning

This directory contains the detailed planning documentation for implementing Model Context Protocol (MCP) support in Griz. The MCP implementation will enable AI assistants to control Griz visualization capabilities through a structured, programmatic interface.

## Overview

The MCP implementation consists of three main components:

1. **C Server Binary** (`griz-server`): A headless version of Griz that accepts JSON commands via stdio
2. **Python API Package** (`griz`): A clean Python interface for programmatic control of Griz
3. **MCP Adapter Package** (`griz-mcp`): MCP-compliant tools that expose the Python API to AI assistants

## Documentation Structure

### Core Design Documents

| Document | Purpose | Key Topics |
|----------|---------|------------|
| [01-architecture.md](01-architecture.md) | System overview and design principles | Component layering, data flow, integration points |
| [02-server-binary.md](02-server-binary.md) | C server implementation details | JSON protocol, output capture, query commands |
| [03-python-api.md](03-python-api.md) | Python package design | Session management, namespaced APIs, results mapping |
| [04-mcp-adapter.md](04-mcp-adapter.md) | MCP tool implementations | Tool registration, session management, image handling |
| [05-protocol.md](05-protocol.md) | JSON communication specification | Message formats, handshake, error handling |

### Implementation Support Documents

| Document | Purpose | Key Topics |
|----------|---------|------------|
| [06-results-mapping.md](06-results-mapping.md) | Field name mapping system | YAML configuration, user-friendly names, validation |
| [07-testing.md](07-testing.md) | Comprehensive testing strategy | Unit tests, integration tests, performance validation |
| [08-phasing.md](08-phasing.md) | Implementation phases and milestones | Incremental delivery, risk management, success criteria |

## Quick Start

### For Architects and Reviewers

1. Start with [01-architecture.md](01-architecture.md) for the system overview
2. Review [05-protocol.md](05-protocol.md) for the communication layer
3. Check [08-phasing.md](08-phasing.md) for implementation strategy

### For Developers

1. Read [02-server-binary.md](02-server-binary.md) for C implementation details
2. Study [03-python-api.md](03-python-api.md) for Python package design  
3. Review [07-testing.md](07-testing.md) for testing approaches
4. Follow [08-phasing.md](08-phasing.md) for implementation phases

### For Users and Integrators

1. See [04-mcp-adapter.md](04-mcp-adapter.md) for MCP tool capabilities
2. Check [06-results-mapping.md](06-results-mapping.md) for field name conventions
3. Review [03-python-api.md](03-python-api.md) for direct Python usage

## Key Design Principles

### Layered Architecture
- **Separation of concerns**: Each layer has distinct responsibilities
- **Reusability**: Python API serves both MCP and direct users
- **Maintainability**: Clean interfaces enable independent evolution

### Shared Infrastructure
- **Server binary**: Common `griz-server` supports both stdio (MCP) and RPC (Qt UI)
- **Protocol components**: JSON envelope, query commands, results mapping shared across clients
- **Build integration**: Extends existing Griz build system without disruption

### User Experience
- **Intuitive APIs**: Human-readable field names hide internal complexity
- **Error handling**: Clear error messages with recovery suggestions
- **Performance**: Suitable for both interactive and batch use cases

## Implementation Status

This is a planning repository. Implementation will proceed according to the phases defined in [08-phasing.md](08-phasing.md):

- **Phase 1**: Foundation and smoke test (2-3 weeks)
- **Phase 2**: JSON protocol and output capture (2-3 weeks)
- **Phase 3**: Python API package (3-4 weeks)
- **Phase 4**: MCP adapter package (2-3 weeks)
- **Phase 5**: Polish and production readiness (3-4 weeks)

## Shared Components

Several components are shared between the MCP effort and the parallel Qt UI modernization effort:

- **Server binary**: `griz-server` with multiple transports
- **Command protocol**: JSON envelope and handshake sequence
- **Output capture**: `griz_out()`/`griz_err()` redirection system
- **Query commands**: Structured state inspection interface
- **Results mapping**: YAML-based field name resolution

These shared components are specified in the [`../shared/`](../shared/) directory and referenced throughout the MCP documentation.

## Dependencies

### Build Dependencies
- **C Compiler**: gcc/clang with C99 support
- **JSON Library**: cJSON for JSON parsing and generation
- **OSMesa**: For headless rendering (already required for batch builds)
- **Python**: 3.8+ with development headers

### Runtime Dependencies
- **Operating System**: Linux, macOS (Windows support in later phases)
- **Python Libraries**: pyyaml, mcp (for adapter package)
- **Hardware**: Sufficient memory for 3D databases and rendering

### Development Dependencies
- **Testing**: pytest, pytest-asyncio, pytest-cov
- **Documentation**: sphinx or similar documentation generator
- **CI/CD**: GitHub Actions or equivalent automation platform

## Contributing

This planning documentation is living and should be updated as implementation proceeds:

1. **Design changes**: Update relevant design documents with rationale
2. **Implementation learnings**: Add notes about what worked vs. what didn't
3. **API evolution**: Keep examples and specifications synchronized
4. **Testing insights**: Document effective testing patterns and gotchas

## Success Criteria

The MCP implementation will be considered successful when:

- AI assistants can load databases, manipulate views, and capture images through natural language
- Python users can automate complex visualization workflows with clean, discoverable APIs
- Performance is suitable for interactive use (sub-second response times for most operations)
- Error handling is robust with clear recovery paths
- Documentation enables new users to be productive quickly
- Integration with existing Griz workflows is seamless

## Related Efforts

### Qt UI Modernization
The parallel Qt UI modernization effort ([`../UI.md`](../UI.md)) shares several components with the MCP implementation. Coordination points are documented in the shared component specifications and architectural decisions.

### Existing Griz Features
The MCP implementation preserves compatibility with existing Griz command vocabulary and workflows. Legacy batch scripts and interactive usage patterns continue to work unchanged.

## Questions and Feedback

For questions about the MCP implementation plan:

1. **Architecture questions**: Review [01-architecture.md](01-architecture.md) and shared component specs
2. **Implementation details**: Check the relevant component documentation
3. **Testing strategy**: See [07-testing.md](07-testing.md) for comprehensive testing approach
4. **Timeline and phasing**: Reference [08-phasing.md](08-phasing.md) for delivery schedule

This planning documentation represents the current thinking and will evolve as implementation proceeds and new requirements emerge.