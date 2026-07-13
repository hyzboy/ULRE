# ULRE Policy Guard MCP Server

This directory contains a small MCP server for logger/hash/C-string/ULRE-string policy enforcement.

## Quick Start

### Option 1: TypeScript/Node (recommended)

```bash
cd .ai/mcp/policy-guard-ts
npm install
npm run dev
```

### Option 2: From VS Code

Press **Ctrl+Shift+D**, select **"Run ULRE Policy Guard MCP Server"**, and the server starts automatically in a terminal.

## What It Does

- Exposes the canonical logger guide as a resource.
- Exposes the canonical hash guide as a resource.
- Exposes the canonical C-string policy guide as a resource.
- Exposes the canonical ULRE String/StringView/StringList policy guide as a resource.
- Provides a prompt that tells an agent how to review logger usage.
- Provides a prompt that tells an agent how to review hash usage.
- Provides a prompt that tells an agent how to review C-string usage.
- Provides a prompt that tells an agent how to review std::string vs ULRE String usage.
- Scans the workspace for forbidden logging patterns such as `std::cout`, `std::cerr`, `printf`, and `fprintf(stderr, ...)`.
- Checks a single file for the same anti-patterns.
- Scans code for forbidden hash patterns such as hardcoded hash literals, ad-hoc hash loops, and direct `wyhash(...)` usage outside core hash files.
- Scans code for forbidden direct C standard string API usage outside ULRE `StrChar/Str.*` core files.
- Scans code for direct `std::string`/`std::string_view` style usage outside ULRE string bridge/core files.

## Runtime

The server uses stdio transport.

- TypeScript server: `@modelcontextprotocol/sdk`

## Dependencies

Install Node dependencies for the TypeScript server:

```bash
cd .ai/mcp/policy-guard-ts
npm install
```

## Run

```bash
npm run dev --prefix .ai/mcp/policy-guard-ts
```

## Resources and Tools

- `logger://guide`
- `logger://forbidden-patterns`
- `hash://guide`
- `hash://forbidden-patterns`
- `cstring://guide`
- `cstring://forbidden-patterns`
- `hglstring://guide`
- `hglstring://forbidden-patterns`
- `get_logger_guide`
- `scan_workspace_for_forbidden_logging`
- `check_file_for_forbidden_logging`
- `review_code_for_logger_misuse`
- `get_hash_guide`
- `scan_workspace_for_forbidden_hashing`
- `check_file_for_forbidden_hashing`
- `review_code_for_hash_misuse`
- `get_cstring_guide`
- `scan_workspace_for_forbidden_cstring_usage`
- `check_file_for_forbidden_cstring_usage`
- `review_code_for_cstring_misuse`
- `get_hgl_string_guide`
- `scan_workspace_for_forbidden_std_string_usage`
- `check_file_for_forbidden_std_string_usage`
- `review_code_for_std_string_misuse`

## Scan Output Shape

The scan tools return structured results with:

- `summary` for quick counts.
- `matches` with file, line, pattern, severity, logger entry point, replacement, and rationale.
- `recommended_next_step` for the next mechanical action.
- `review_code_for_logger_misuse` returns a `pass`/`fail` verdict for an isolated snippet.

This is meant to be easy for an agent to turn into a repair patch without re-parsing prose.