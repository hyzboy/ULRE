#!/usr/bin/env python3
from __future__ import annotations

import argparse
import pathlib
import re
import sys
from dataclasses import dataclass

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]')

HEADER_EXTS = {".h", ".hpp", ".hh", ".hxx"}

# Hard blocks for ShaderGen public headers: runtime/renderer object dependencies.
FORBIDDEN_SUBSTRINGS = (
    "hgl/vk/VK.h",
    "hgl/graph/module/",
    "hgl/graph/core/",
    "hgl/vk/VKDevice",
    "hgl/vk/VKInstance",
    "hgl/vk/VKSwapchain",
    "hgl/vk/VKRenderTarget",
    "hgl/vk/VKFramebuffer",
    "hgl/vk/VKRenderPass",
    "hgl/vk/VKCommand",
    "hgl/vk/pipeline/",
)


@dataclass
class Violation:
    file: pathlib.Path
    line_no: int
    include: str
    reason: str


def load_allowlist(path: pathlib.Path) -> set[str]:
    allowlist: set[str] = set()
    if not path.exists():
        print(f"[WARN] allowlist file not found: {path}")
        return allowlist

    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except Exception as exc:
        print(f"[WARN] failed to read allowlist file '{path}': {exc}")
        return allowlist

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        allowlist.add(normalize_include(line))

    return allowlist


def normalize_include(include_path: str) -> str:
    return include_path.replace("\\", "/").strip()


def should_scan(path: pathlib.Path) -> bool:
    if path.suffix.lower() not in HEADER_EXTS:
        return False

    norm = path.as_posix()
    if "/inc/hgl/shadergen/" not in norm:
        return False

    return True


def check_file(path: pathlib.Path, allowlist_exact: set[str]) -> list[Violation]:
    violations: list[Violation] = []
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except Exception as exc:
        violations.append(
            Violation(path, 0, "<read-error>", f"failed to read file: {exc}")
        )
        return violations

    for i, line in enumerate(text.splitlines(), start=1):
        m = INCLUDE_RE.match(line)
        if not m:
            continue

        inc = normalize_include(m.group(1))
        if inc in allowlist_exact:
            continue

        for token in FORBIDDEN_SUBSTRINGS:
            if token in inc:
                violations.append(
                    Violation(path, i, inc, f"forbidden dependency token: {token}")
                )
                break

    return violations


def collect_header_files(root: pathlib.Path) -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for p in root.rglob("*"):
        if p.is_file() and should_scan(p):
            files.append(p)
    return sorted(files)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check ShaderGen public-header boundary against renderer runtime dependencies."
    )
    parser.add_argument(
        "--repo-root",
        default=".",
        help="Repository root path (default: current directory)",
    )
    parser.add_argument(
        "--allowlist-file",
        default=None,
        help="Path to exact-include allowlist file (default: <repo-root>/tools/shadergen_boundary_allowlist.txt)",
    )
    args = parser.parse_args()

    root = pathlib.Path(args.repo_root).resolve()
    if not root.exists():
        print(f"[ERROR] repo root not found: {root}")
        return 2

    allowlist_file = pathlib.Path(args.allowlist_file).resolve() if args.allowlist_file else (root / "tools" / "shadergen_boundary_allowlist.txt")
    allowed_exact = load_allowlist(allowlist_file)

    headers = collect_header_files(root)
    if not headers:
        print("[WARN] no shadergen public headers found under inc/hgl/shadergen")
        return 0

    all_violations: list[Violation] = []
    for h in headers:
        all_violations.extend(check_file(h, allowed_exact))

    print(f"[INFO] scanned headers: {len(headers)}")
    print(f"[INFO] allowlist entries: {len(allowed_exact)} ({allowlist_file})")

    if not all_violations:
        print("[PASS] shadergen public-header boundary check passed")
        return 0

    print(f"[FAIL] violations: {len(all_violations)}")
    for v in all_violations:
        rel = v.file.relative_to(root) if v.file.is_relative_to(root) else v.file
        print(f" - {rel}:{v.line_no} include='{v.include}' ({v.reason})")

    return 1


if __name__ == "__main__":
    sys.exit(main())
