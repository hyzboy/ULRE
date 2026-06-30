"""MCP server for the ULRE logger system.

The server is intended for AI agents and code reviewers. It provides:
- a canonical logger usage guide,
- a reusable review prompt,
- workspace scans for forbidden ad-hoc logging patterns,
- and file-level checks for logger misuse.

The server runs over stdio through the default FastMCP transport.
"""

from __future__ import annotations

import os
import re
from pathlib import Path
from typing import TypedDict

from mcp.server.fastmcp import FastMCP


ROOT_DIR = Path(__file__).resolve().parents[2]

IGNORED_DIR_NAMES = {
    ".git",
    ".idea",
    ".vs",
    "backup",
    "bin",
    "build",
    "Testing",
    "tmp",
    "node_modules",
}

TEXT_EXTENSIONS = {
    ".c",
    ".cc",
    ".cmake",
    ".cpp",
    ".cxx",
    ".h",
    ".hpp",
    ".inl",
    ".md",
    ".py",
    ".txt",
}


class Finding(TypedDict):
    path: str
    line: int
    pattern: str
    severity: str
    snippet: str
    logger_entry_point: str
    replacement: str
    why_it_matters: str
    auto_fixable: bool


class ScanSummary(TypedDict):
    total_matches: int
    by_pattern: dict[str, int]
    files_with_matches: int


class ScanResult(TypedDict):
    root: str
    scanned_files: int
    summary: ScanSummary
    matches: list[Finding]
    guidance: str
    recommended_next_step: str


class ReviewResult(TypedDict):
    verdict: str
    summary: ScanSummary
    matches: list[Finding]
    guidance: str
    recommended_next_step: str


FORBIDDEN_PATTERNS: list[tuple[str, re.Pattern[str], str]] = [
    (
        "std::cout",
        re.compile(r"\bstd::cout\b"),
        "Use the logger macros instead of stdout for application logging.",
    ),
    (
        "std::cerr",
        re.compile(r"\bstd::cerr\b"),
        "Use the logger macros instead of stderr for application logging.",
    ),
    (
        "printf",
        re.compile(r"\bprintf\s*\("),
        "Replace printf-based diagnostics with the Logger macros.",
    ),
    (
        "fprintf(stderr, ...)",
        re.compile(r"\bfprintf\s*\(\s*stderr\s*,"),
        "Replace fprintf(stderr, ...) diagnostics with the Logger macros.",
    ),
]


LOGGER_GUIDE = """# ULRE Logger Guide

## Goal

Use the Logger system for all diagnostic and runtime messages. Do not use `std::cout`, `std::cerr`, or `printf` for logging.

## Common Entry Points

- `GLogInfo(...)` for global logging.
- `FLogInfo(...)` for free-function/module logging where the logger instance is already bound.
- `MLogInfo(...)` for module-scoped logging.
- `LogVerbose(...)`, `LogInfo(...)`, `LogWarning(...)`, `LogError(...)` for severity-based messages.
- `OBJECT_LOGGER` inside classes that need member-bound logging.
- `DEFINE_LOGGER_MODULE(Name)` and `USE_MODULE_LOGGER(Name)` for module wiring.

## Class Usage Pattern

1. Put `OBJECT_LOGGER` in the class definition.
2. Bind a logger instance name with `Log.SetLoggerInstanceName(...)` when needed.
3. Emit logs through the logger macros rather than writing directly to stdout or stderr.

## Encoding Rule

The logger record keeps structured metadata and text payloads so sinks can choose the best encoding for the current platform. The caller should not guess the target console encoding.

## Preferred Behavior

- Prefer the logger macros for all normal diagnostic output.
- Prefer structured messages and clear severity levels.
- Keep user-facing output separate from diagnostics.

## Do Not Use

- `std::cout` for log messages.
- `std::cerr` for log messages.
- `printf` or `fprintf(stderr, ...)` for log messages.

## Reference Examples

- `CMCore/inc/hgl/log/Log.h`
- `CMCore/examples/system/LoggerTest.cpp`
- `CMCore/examples/log/LogOutputWaysMain.cpp`
"""


def _is_text_file(path: Path) -> bool:
    return path.suffix.lower() in TEXT_EXTENSIONS or path.suffix == ""


def _should_skip(path: Path) -> bool:
    return any(part in IGNORED_DIR_NAMES for part in path.parts)


def _relative_path(path: Path, root: Path) -> str:
    try:
        relative_path = path.relative_to(root)
    except ValueError:
        relative_path = path
    return str(relative_path).replace(os.sep, "/")


def _display_root_for_file(path: Path) -> Path:
    try:
        path.relative_to(ROOT_DIR)
    except ValueError:
        return path.parent
    return ROOT_DIR


def _scan_text(path: Path, root: Path, text: str) -> list[Finding]:
    findings: list[Finding] = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        for pattern_name, regex, recommendation in FORBIDDEN_PATTERNS:
            if regex.search(line):
                findings.append(
                    {
                        "path": _relative_path(path, root),
                        "line": line_number,
                        "pattern": pattern_name,
                        "severity": "error",
                        "snippet": line.strip(),
                        "logger_entry_point": "GLogInfo / FLogInfo / MLogInfo / LogInfo / LogWarning / LogError",
                        "replacement": recommendation,
                        "why_it_matters": (
                            "Direct console output bypasses the structured logger pipeline and can be lost, "
                            "mis-encoded, or hidden from sinks that need record metadata."
                        ),
                        "auto_fixable": True,
                    }
                )
    return findings


def _iter_workspace_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if _should_skip(path):
            continue
        if not _is_text_file(path):
            continue
        files.append(path)
    return files


mcp = FastMCP(
    "ULRE Logger Guard",
    instructions=(
        "Teach and enforce the ULRE logger usage rules. Prefer the logger macros, "
        "flag direct stdout/stderr logging, and provide the canonical logger guide."
    ),
)


@mcp.resource("logger://guide")
def logger_guide() -> str:
    """Return the canonical logger usage guide."""

    return LOGGER_GUIDE


@mcp.resource("logger://forbidden-patterns")
def forbidden_patterns() -> str:
    """Return the patterns that should not be used for logging."""

    return "\n".join(
        [
            "- std::cout",
            "- std::cerr",
            "- printf",
            "- fprintf(stderr, ...)",
        ]
    )


@mcp.prompt(title="Logger Usage Review")
def logger_usage_review(source: str) -> str:
    """Prompt for reviewing code against the logger policy."""

    return (
        "Review the following code against the ULRE logger policy. "
        "Reject any use of std::cout, std::cerr, printf, or fprintf(stderr, ...). "
        "Prefer the logger macros and explain how the code should be rewritten.\n\n"
        f"{source}"
    )


@mcp.tool()
def get_logger_guide() -> str:
    """Return the concise logger usage guide."""

    return LOGGER_GUIDE


@mcp.tool()
def scan_workspace_for_forbidden_logging(root: str | None = None) -> ScanResult:
    """Scan the workspace for direct logging anti-patterns."""

    scan_root = Path(root).resolve() if root else ROOT_DIR
    matches: list[Finding] = []
    scanned_files = 0

    for path in _iter_workspace_files(scan_root):
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        scanned_files += 1
        matches.extend(_scan_text(path, scan_root, text))

    by_pattern: dict[str, int] = {}
    for finding in matches:
        by_pattern[finding["pattern"]] = by_pattern.get(finding["pattern"], 0) + 1

    summary: ScanSummary = {
        "total_matches": len(matches),
        "by_pattern": by_pattern,
        "files_with_matches": len({finding["path"] for finding in matches}),
    }

    guidance = (
        "Use the logger macros for diagnostics. If you need a quick replacement, "
        "map stdout/stderr/printf usage to the nearest logger severity and keep "
        "the message on the logger path. Prefer one-for-one replacements so the call site "
        "can be rewritten mechanically."
    )

    return {
        "root": str(scan_root),
        "scanned_files": scanned_files,
        "summary": summary,
        "matches": matches,
        "guidance": guidance,
        "recommended_next_step": "Replace each finding with the listed logger entry point, then rerun the scan.",
    }


@mcp.tool()
def check_file_for_forbidden_logging(path: str) -> dict[str, object]:
    """Check a single file for forbidden logging patterns."""

    file_path = Path(path).resolve()
    if not file_path.exists():
        return {
            "path": str(file_path),
            "exists": False,
            "summary": {
                "total_matches": 0,
                "by_pattern": {},
            },
            "matches": [],
            "message": "File does not exist.",
        }

    try:
        text = file_path.read_text(encoding="utf-8", errors="ignore")
    except OSError as exc:
        return {
            "path": str(file_path),
            "exists": True,
            "summary": {
                "total_matches": 0,
                "by_pattern": {},
            },
            "matches": [],
            "message": f"Failed to read file: {exc}",
        }

    matches = _scan_text(file_path, _display_root_for_file(file_path), text)
    by_pattern: dict[str, int] = {}
    for finding in matches:
        by_pattern[finding["pattern"]] = by_pattern.get(finding["pattern"], 0) + 1

    return {
        "path": str(file_path),
        "exists": True,
        "summary": {
            "total_matches": len(matches),
            "by_pattern": by_pattern,
        },
        "matches": matches,
        "message": "No forbidden logging patterns found." if not matches else "Forbidden logging patterns found.",
    }


@mcp.tool()
def review_code_for_logger_misuse(code: str) -> ReviewResult:
    """Review a code snippet for forbidden logging patterns."""

    synthetic_path = ROOT_DIR / "<snippet>"
    matches = _scan_text(synthetic_path, ROOT_DIR, code)

    by_pattern: dict[str, int] = {}
    for finding in matches:
        by_pattern[finding["pattern"]] = by_pattern.get(finding["pattern"], 0) + 1

    summary: ScanSummary = {
        "total_matches": len(matches),
        "by_pattern": by_pattern,
        "files_with_matches": 1 if matches else 0,
    }

    return {
        "verdict": "fail" if matches else "pass",
        "summary": summary,
        "matches": matches,
        "guidance": (
            "Rewrite the snippet to use the logger macros and keep diagnostics out of stdout/stderr."
        ),
        "recommended_next_step": (
            "Replace each forbidden print call with the nearest logger macro, then rerun this review."
        ),
    }


def main() -> None:
    """Run the MCP server."""

    mcp.run()


if __name__ == "__main__":
    main()