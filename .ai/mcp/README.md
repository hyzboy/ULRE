# ULRE Logger MCP Server

This directory contains a small MCP server for the logger system.

## Quick Start

### Option 1: TypeScript/Node (recommended)

```bash
cd .ai/mcp/logger-guard-ts
npm install
npm run dev
```

### Option 2: From VS Code

Press **Ctrl+Shift+D**, select **"Run Logger Guard MCP Server"**, and the server starts automatically in a terminal.

### Option 3: Direct Python (fallback)

```bash
pip install "mcp[cli]"
python .ai/mcp/logger_guard_server.py
```

## What It Does

- Exposes the canonical logger guide as a resource.
- Provides a prompt that tells an agent how to review logger usage.
- Scans the workspace for forbidden logging patterns such as `std::cout`, `std::cerr`, `printf`, and `fprintf(stderr, ...)`.
- Checks a single file for the same anti-patterns.

## Runtime

The server uses stdio transport.

- TypeScript server: `@modelcontextprotocol/sdk`
- Python fallback server: FastMCP

## Dependencies

Install Node dependencies for the TypeScript server:

```bash
cd .ai/mcp/logger-guard-ts
npm install
```

## Run

```bash
npm run dev --prefix .ai/mcp/logger-guard-ts
```

## Resources and Tools

- `logger://guide`
- `logger://forbidden-patterns`
- `get_logger_guide`
- `scan_workspace_for_forbidden_logging`
- `check_file_for_forbidden_logging`
- `review_code_for_logger_misuse`

## Scan Output Shape

The scan tools return structured results with:

- `summary` for quick counts.
- `matches` with file, line, pattern, severity, logger entry point, replacement, and rationale.
- `recommended_next_step` for the next mechanical action.
- `review_code_for_logger_misuse` returns a `pass`/`fail` verdict for an isolated snippet.

This is meant to be easy for an agent to turn into a repair patch without re-parsing prose.