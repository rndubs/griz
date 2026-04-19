# 08 — Implementation Phases and Milestones

## Scope

This document outlines the phased implementation strategy for the MCP feature, breaking down the work into manageable increments with clear milestones and deliverables. Each phase builds upon previous phases, with testing and validation at each stage. The phasing balances early value delivery with technical risk management and allows for feedback incorporation throughout development.

## Related

- [01-architecture.md](01-architecture.md) — system design and components
- [02-server-binary.md](02-server-binary.md) — server implementation details
- [03-python-api.md](03-python-api.md) — Python package design
- [04-mcp-adapter.md](04-mcp-adapter.md) — MCP tool implementations
- [07-testing.md](07-testing.md) — testing strategy across phases

## 1. Phasing Philosophy

### 1.1 Incremental Value Delivery

**Phase Goals**:
- Each phase delivers working, testable functionality
- Early phases provide value to developers and early adopters
- Later phases add polish, performance, and production readiness
- Clear success criteria and exit conditions for each phase

### 1.2 Risk Management

**Technical Risks**:
- Server binary integration with existing Griz build system
- JSON protocol stability and performance
- Subprocess communication reliability
- MCP specification compliance

**Mitigation Strategy**:
- Prototype critical components early (Phase 1)
- Incremental integration with extensive testing
- Regular stakeholder reviews and feedback incorporation
- Clear rollback plans at each phase

## 2. Phase Breakdown

### 2.1 Phase 1: Foundation and Smoke Test (2-3 weeks)

**Goal**: Prove the basic architecture works end-to-end with minimal features.

**Deliverables**:
- Basic `griz-server` binary with stdio transport
- Minimal Python worker with subprocess management
- Simple JSON request/response cycle
- Screenshot capture functionality
- Basic unit tests

**Technical Scope**:

**C Server**:
- New `griz-server` target in Makefile
- Basic `process_server_mode_stdio()` function
- Plain text command processing (no JSON envelope yet)
- OSMesa rendering integration
- `outpng` command for screenshot capture

**Python Worker**:
- `Worker` class with subprocess spawning
- Command execution via stdin/stdout
- File-based screenshot retrieval
- Basic error handling

**Integration**:
- End-to-end smoke test: start server, send commands, capture screenshot
- Verify no stdout contamination
- Test clean shutdown

**Success Criteria**:
```python
# This should work:
from griz.worker import Worker
worker = Worker("griz-server", width=1024, height=1024)
worker.send_command("time 42")  # Plain text command
worker.send_command("outpng /tmp/test.png")
worker.cleanup()
# File /tmp/test.png should exist and be valid
```

**Risks**:
- OSMesa integration issues
- Subprocess buffering problems
- Build system complications

**Milestone Criteria**:
- [ ] `griz-server` binary builds successfully from `batchopt` objects
- [ ] Server starts and accepts plain text commands via stdin
- [ ] Screenshot capture produces valid PNG files
- [ ] Python worker can spawn server and send commands
- [ ] Clean shutdown with no resource leaks
- [ ] Basic integration test passes

---

### 2.2 Phase 2: JSON Protocol and Output Capture (2-3 weeks)

**Goal**: Implement the full JSON protocol with robust output capture.

**Deliverables**:
- Complete JSON request/response envelope
- Output capture system (`griz_out`/`griz_err`)
- Protocol handshake sequence
- Error taxonomy and handling
- Query commands for state inspection
- Enhanced testing suite

**Technical Scope**:

**C Server Enhancements**:
- JSON parsing with cJSON library
- Request/response envelope implementation
- Output capture system implementation
- Audit and replace `printf` calls on batch paths
- Error code taxonomy and structured error responses
- Handshake sequence (ready → hello → hello_ack)

**Protocol Implementation**:
- Version negotiation
- Request ID correlation
- Query commands: `q_state`, `q_view`, `q_time`
- Structured data responses

**Python Worker Enhancements**:
- JSON request generation
- JSON response parsing
- Timeout handling for commands
- Error translation to Python exceptions
- Protocol compliance validation

**Success Criteria**:
```python
# This should work:
from griz.worker import Worker
worker = Worker("griz-server", database="test.plt", width=1024, height=1024)
response = worker.cmd("state 42")  # JSON request/response
assert response["status"] == "ok"
state = worker.cmd("q_state")  # Query command
assert "time_state" in state["data"]
worker.cleanup()
```

**Risks**:
- JSON library integration complexity
- Output capture completeness
- Protocol version compatibility

**Milestone Criteria**:
- [ ] Full JSON request/response cycle works
- [ ] Output capture prevents stdout contamination
- [ ] Protocol handshake completes successfully
- [ ] Query commands return structured data
- [ ] Error responses contain proper error codes
- [ ] Integration tests cover protocol edge cases

---

### 2.3 Phase 3: Python API Package (3-4 weeks)

**Goal**: Build the complete `griz` Python package with clean, usable APIs.

**Deliverables**:
- Complete `Griz` class with context manager support
- Namespaced sub-APIs (`field`, `view`, `time`, `materials`)
- Results mapping system with YAML configuration
- Comprehensive unit test suite
- Package documentation and examples
- PyPI-ready package structure

**Technical Scope**:

**Core Session Management**:
- `Griz` class with lifecycle management
- Context manager implementation (`__enter__`/`__exit__`)
- Database open/close operations
- State query and caching

**Namespaced APIs**:
- `FieldAPI`: field operations with results mapping
- `ViewAPI`: camera and view controls
- `TimeAPI`: time state management
- `MaterialsAPI`: material visibility controls

**Results Mapping**:
- YAML-based field name resolution
- `resolve_field("stress", "von_mises") → "svm"`
- Error handling for unknown fields
- Field discovery and listing

**Package Infrastructure**:
- `pyproject.toml` configuration
- Entry points and command-line tools
- Documentation with examples
- Unit tests with mocking

**Success Criteria**:
```python
# This should work:
from griz import Griz

with Griz("test.plt", width=1024, height=1024) as g:
    g.time.set_state(42)
    g.field.show("stress", component="von_mises")  # Uses results mapping
    g.view.rotate(x=30, y=0, z=0)
    g.materials.hide([3, 7])
    screenshot_data = g.screenshot()
    state = g.state()
    assert state["time_state"] == 42
```

**Risks**:
- API design complexity and usability
- Results mapping completeness
- Performance of Python layer

**Milestone Criteria**:
- [ ] Complete `Griz` class with all namespaced APIs
- [ ] Results mapping system resolves common field names
- [ ] Context manager handles cleanup properly
- [ ] Unit tests achieve >90% coverage
- [ ] Documentation includes comprehensive examples
- [ ] Package installs cleanly with pip

---

### 2.4 Phase 4: MCP Adapter Package (2-3 weeks)

**Goal**: Create the MCP server adapter exposing Griz as MCP tools.

**Deliverables**:
- Complete `griz-mcp` package with MCP server
- All essential MCP tools implemented
- Session management for MCP clients
- Image handling for screenshots
- MCP prompt templates
- End-to-end MCP integration tests

**Technical Scope**:

**MCP Server Implementation**:
- MCP protocol compliance
- Tool registration and dispatch
- Session lifecycle management
- Async/await integration with sync Griz API

**MCP Tools**:
- Database operations: `open_database`, `close_database`
- Field operations: `show_field`, `list_fields`
- View operations: `rotate_view`, `reset_view`
- Time operations: `set_time_state`, `animate`
- Material operations: `hide_materials`, `show_materials`
- Output operations: `screenshot`
- State operations: `get_state`
- Utility operations: `restart_session`, `raw_command`

**Image Handling**:
- Screenshot capture as MCP `ImageContent`
- Base64 encoding for MCP transport
- Temporary file management

**Session Management**:
- Global session state for MCP clients
- Session restart and recovery
- Error handling and graceful degradation

**Success Criteria**:
```python
# MCP client interaction:
# Tool: open_database(path="/path/to/data.plt")
# Tool: show_field(name="stress", component="von_mises") 
# Tool: rotate_view(x=30, y=0, z=0)
# Tool: screenshot() → returns MCP Image content
# All tools return structured responses
```

**Risks**:
- MCP specification compliance
- Async/sync integration complexity
- Session state management

**Milestone Criteria**:
- [ ] MCP server starts and handles tool calls
- [ ] All essential tools implemented and tested
- [ ] Screenshot returns valid MCP Image content
- [ ] Session management handles errors gracefully
- [ ] Integration tests with mock MCP client pass
- [ ] Package publishes to PyPI

---

### 2.5 Phase 5: Polish and Production Readiness (3-4 weeks)

**Goal**: Prepare for production use with performance optimization, robustness, and documentation.

**Deliverables**:
- Performance optimization and profiling
- Comprehensive error handling and recovery
- Production deployment documentation
- Advanced MCP features (prompts, resources)
- CI/CD pipeline setup
- User guides and tutorials

**Technical Scope**:

**Performance Optimization**:
- Command batching for efficiency
- Caching for repeated operations
- Memory usage optimization
- Startup time reduction

**Robustness**:
- Timeout and watchdog mechanisms
- Session recovery and restart
- Resource cleanup and leak prevention
- Stress testing and load testing

**Advanced Features**:
- MCP prompt templates for common workflows
- MCP resources for database metadata
- Configuration management
- Logging and diagnostics

**Production Infrastructure**:
- CI/CD pipeline with multiple Python versions
- Automated testing including performance benchmarks
- Package signing and security
- Documentation website

**User Experience**:
- Getting started tutorials
- Example workflows and scripts
- Troubleshooting guides
- API reference documentation

**Success Criteria**:
- Performance benchmarks meet targets
- Stress tests pass without failures
- Complete documentation available
- CI/CD pipeline runs all tests
- Production deployment guide available

**Milestone Criteria**:
- [ ] Performance tests meet latency/throughput targets
- [ ] Stress tests run without memory leaks or crashes
- [ ] Advanced MCP features implemented and tested
- [ ] Complete documentation and tutorials published
- [ ] CI/CD pipeline running on multiple platforms
- [ ] Ready for production deployment

## 3. Cross-Phase Considerations

### 3.1 Testing Strategy by Phase

**Phase 1**: Manual testing and basic smoke tests
**Phase 2**: Unit tests for protocol components
**Phase 3**: Comprehensive unit tests for Python APIs
**Phase 4**: Integration tests including MCP compliance
**Phase 5**: Performance, stress, and system tests

### 3.2 Documentation Evolution

**Phase 1**: Technical notes and implementation details
**Phase 2**: Protocol specification and API design
**Phase 3**: Python API documentation with examples
**Phase 4**: MCP integration guides and tool reference
**Phase 5**: Complete user guides and tutorials

### 3.3 Shared Component Coordination

**Qt UI Integration**: Phases 1-2 share server binary and protocol work
**Results Mapping**: Coordinate YAML schema with Qt UI needs
**Build System**: Ensure changes don't break existing targets

## 4. Risk Mitigation by Phase

### 4.1 Phase 1 Risks

**Risk**: OSMesa integration fails
**Mitigation**: Test on target platforms early; have VNC fallback plan

**Risk**: Subprocess communication unreliable  
**Mitigation**: Implement robust buffering; test edge cases

**Risk**: Build system integration complex
**Mitigation**: Start with minimal changes; coordinate with build maintainers

### 4.2 Phase 2 Risks

**Risk**: JSON library adds complexity
**Mitigation**: Choose simple, stable library (cJSON); comprehensive testing

**Risk**: Output capture incomplete
**Mitigation**: Systematic audit of printf calls; runtime validation

**Risk**: Protocol version compatibility
**Mitigation**: Design forward-compatible protocol; version negotiation

### 4.3 Phase 3 Risks

**Risk**: Python API design poor usability
**Mitigation**: Early user feedback; iterative design; follow Python conventions

**Risk**: Results mapping incomplete
**Mitigation**: Coordinate with domain experts; extensible design

**Risk**: Performance overhead significant
**Mitigation**: Benchmark early; optimize hot paths; consider caching

### 4.4 Phase 4 Risks

**Risk**: MCP specification changes
**Mitigation**: Track spec development; implement core features first

**Risk**: Async/sync impedance mismatch
**Mitigation**: Clean abstraction layers; thorough testing

### 4.5 Phase 5 Risks

**Risk**: Performance targets missed
**Mitigation**: Continuous benchmarking; early optimization of critical paths

**Risk**: Production deployment issues
**Mitigation**: Testing on realistic environments; deployment automation

## 5. Dependencies and Prerequisites

### 5.1 External Dependencies

**Build Tools**:
- cJSON library for JSON parsing
- OSMesa for headless rendering
- Python 3.8+ with development headers

**Development Tools**:
- pytest for testing
- MCP library for adapter implementation
- Documentation generation tools

**Infrastructure**:
- CI/CD platform (GitHub Actions)
- Package registry access (PyPI)
- Documentation hosting

### 5.2 Internal Dependencies

**Griz Core**: Stable `batchopt` build with working OSMesa
**UI Effort**: Coordinate shared components and protocols
**Build System**: Maintainer support for new targets

## 6. Success Metrics

### 6.1 Technical Metrics

**Phase 1**:
- Server starts in <10 seconds
- Screenshot generation works reliably
- Memory usage stable

**Phase 2**:
- Protocol latency <100ms per command
- JSON parsing success rate 100%
- Error recovery time <5 seconds

**Phase 3**:
- API call overhead <10ms
- Unit test coverage >90%
- Import time <1 second

**Phase 4**:
- MCP tool execution <2 seconds
- Session startup <30 seconds
- All MCP compliance tests pass

**Phase 5**:
- End-to-end workflow <60 seconds
- Stress test runs 24+ hours
- Documentation completeness >95%

### 6.2 User Experience Metrics

**Usability**: API discoverable and intuitive
**Reliability**: Works consistently across platforms
**Performance**: Suitable for interactive use
**Documentation**: Clear and comprehensive

## 7. Contingency Planning

### 7.1 Schedule Delays

**Mitigation**: Each phase has buffer time; non-critical features can be deferred
**Fallback**: Reduce scope of later phases while maintaining core functionality

### 7.2 Technical Blockers

**Server Integration Issues**: Fall back to file-based communication
**Performance Problems**: Optimize critical paths; consider caching
**Protocol Complexity**: Simplify envelope; defer advanced features

### 7.3 Resource Constraints

**Developer Availability**: Prioritize core features; defer polish
**Infrastructure Limits**: Use simpler CI/CD; manual testing if needed

## 8. Quality Gates

### 8.1 Phase Exit Criteria

Each phase must meet all milestone criteria before proceeding:
- All deliverables complete and tested
- No critical bugs or regressions
- Documentation updated
- Stakeholder review completed

### 8.2 Go/No-Go Decisions

**Continue Criteria**:
- Technical feasibility demonstrated
- Performance targets achievable
- User feedback positive

**Stop Criteria**:
- Fundamental technical blockers
- Performance unacceptable
- Resource constraints prohibitive

## 9. Communication and Coordination

### 9.1 Regular Reviews

**Weekly**: Development team progress review
**Bi-weekly**: Stakeholder demonstration of current phase
**Monthly**: Cross-team coordination (UI effort, build system)

### 9.2 Documentation Updates

**Phase Completion**: Update architectural decisions and lessons learned
**Milestone Reviews**: Revise subsequent phases based on learnings
**Final Delivery**: Complete implementation guide and retrospective

## 10. Success Criteria Summary

**Phase 1 Complete**: Basic end-to-end functionality demonstrated
**Phase 2 Complete**: Robust JSON protocol working reliably  
**Phase 3 Complete**: Usable Python API ready for power users
**Phase 4 Complete**: MCP integration working with AI assistants
**Phase 5 Complete**: Production-ready system with full documentation

**Overall Success**: AI assistants can successfully drive Griz through natural language to perform complex visualization workflows, with performance and reliability suitable for regular use by scientists and engineers.

The phased approach ensures early validation of core concepts while building incrementally toward a complete, production-ready MCP implementation.