import { readdirSync, readFileSync, statSync, existsSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

type Finding = {
  path: string;
  line: number;
  pattern: string;
  severity: "error";
  snippet: string;
  logger_entry_point: string;
  replacement: string;
  why_it_matters: string;
  auto_fixable: boolean;
};

type ScanSummary = {
  total_matches: number;
  by_pattern: Record<string, number>;
  files_with_matches: number;
};

type HashFinding = {
  path: string;
  line: number;
  pattern: string;
  severity: "error" | "warning";
  snippet: string;
  preferred_api: string;
  replacement: string;
  why_it_matters: string;
  auto_fixable: boolean;
};

type CStringFinding = {
  path: string;
  line: number;
  pattern: string;
  severity: "error" | "warning";
  snippet: string;
  preferred_api: string;
  replacement: string;
  why_it_matters: string;
  auto_fixable: boolean;
};

type HglStringFinding = {
  path: string;
  line: number;
  pattern: string;
  severity: "error" | "warning";
  snippet: string;
  preferred_api: string;
  replacement: string;
  why_it_matters: string;
  auto_fixable: boolean;
};

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const ROOT_DIR = path.resolve(__dirname, "../../../..");

const IGNORED_DIR_NAMES = new Set([
  ".git",
  ".idea",
  ".vs",
  "backup",
  "bin",
  "build",
  "Testing",
  "tmp",
  "node_modules",
]);

const TEXT_EXTENSIONS = new Set([
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
]);

const FORBIDDEN_PATTERNS: Array<{ name: string; regex: RegExp; recommendation: string }> = [
  {
    name: "std::cout",
    regex: /\bstd::cout\b/,
    recommendation: "Use the logger macros instead of stdout for application logging.",
  },
  {
    name: "std::cerr",
    regex: /\bstd::cerr\b/,
    recommendation: "Use the logger macros instead of stderr for application logging.",
  },
  {
    name: "printf",
    regex: /\bprintf\s*\(/,
    recommendation: "Replace printf-based diagnostics with the Logger macros.",
  },
  {
    name: "fprintf(stderr, ...)",
    regex: /\bfprintf\s*\(\s*stderr\s*,/,
    recommendation: "Replace fprintf(stderr, ...) diagnostics with the Logger macros.",
  },
];

const CORE_HASH_FILES = new Set([
  "cmcoretype/inc/hgl/util/hash/fnv1a.h",
  "cmcoretype/inc/hgl/util/hash/quickhash.h",
  "cmcoretype/inc/hgl/util/hash/securehash.h",
  "cmcoretype/inc/hgl/util/hash/wyhash.h",
  "cmcoretype/inc/hgl/util/hash/wyhash32.h",
]);

const CORE_CSTRING_ROOT = "cmcoretype/inc/hgl/type/";

const CORE_HGL_STRING_FILES = new Set([
  "cmcore/inc/hgl/type/string.h",
  "cmcore/inc/hgl/type/stringview.h",
  "cmcore/inc/hgl/type/stringlist.h",
  "cmcore/inc/hgl/type/stringviewlist.h",
  "cmcore/inc/hgl/type/splitstring.h",
  "cmcore/inc/hgl/type/mergestring.h",
  "cmcore/inc/hgl/type/stdstring.h",
  "cmcore/inc/hgl/type/conststringset.h",
]);

const HASH_FORBIDDEN_PATTERNS: Array<{
  name: string;
  regex: RegExp;
  severity: "error" | "warning";
  recommendation: string;
  rationale: string;
  allowInCoreHashFiles?: boolean;
}> = [
  {
    name: "hardcoded_hash_literal_assignment",
    regex: /\b(?:hash|digest|fingerprint|checksum)[A-Za-z0-9_]*\s*=\s*0x[0-9a-fA-F]{8,16}(?:[uU](?:[lL]{1,2})?|[lL]{2})?\b/,
    severity: "error",
    recommendation:
      "Replace fixed hash constants with canonical runtime hashing: hgl::ComputeOptimalHash(value/data,size) or FNV1a helpers where incremental hashing is required.",
    rationale:
      "Hardcoded hash values hide intent and are fragile when string/token inputs change.",
  },
  {
    name: "switch_on_magic_hash",
    regex: /\bcase\s+0x[0-9a-fA-F]{8,16}(?:[uU](?:[lL]{1,2})?|[lL]{2})?\s*:/,
    severity: "error",
    recommendation:
      "Avoid switch/case on magic hash literals. Compute hashes from canonical inputs and keep a named mapping table.",
    rationale:
      "Magic hash dispatch values are opaque and encourage accidental collisions and copy-paste bugs.",
  },
  {
    name: "fnv_magic_constants",
    regex: /\b(?:2166136261|16777619|14695981039346656037|1099511628211)(?:u|ul|ull|ll)?\b/,
    severity: "warning",
    recommendation:
      "Do not re-implement FNV constants/loops in feature code. Prefer hgl::ComputeOptimalHash(...) or the canonical FNV1a helpers in hgl/util/hash/FNV1a.h.",
    rationale:
      "Inline ad-hoc hash constants usually indicate a local hash implementation that bypasses project policy.",
    allowInCoreHashFiles: true,
  },
  {
    name: "djb2_style_mix",
    regex: /\(\s*hash\s*<<\s*5\s*\)\s*\+\s*hash/,
    severity: "warning",
    recommendation:
      "Avoid manual djb2-style loops; route through hgl::ComputeOptimalHash or canonical FNV1a helpers for deterministic project behavior.",
    rationale:
      "Custom hash loops tend to diverge and break cross-module consistency.",
  },
  {
    name: "direct_wyhash_call",
    regex: /\bwyhash(?:32)?\s*\(/,
    severity: "warning",
    recommendation:
      "Outside core hash files, call hgl::ComputeOptimalHash(...) instead of raw wyhash(...)/wyhash32(...).",
    rationale:
      "Direct wyhash use spreads low-level details and causes inconsistent call conventions.",
    allowInCoreHashFiles: true,
  },
  {
    name: "direct_wyhash_header_include",
    regex: /#include\s*<(?:wyhash\/wyhash(?:32)?|hgl\/util\/hash\/wyhash(?:32)?)\.h>/,
    severity: "warning",
    recommendation:
      "Prefer including hgl/util/hash/QuickHash.h or hgl/util/hash/SecureHash.h in feature code. Reserve direct wyhash headers for core hash implementation files.",
    rationale:
      "Feature code should depend on project hash wrappers, not low-level backend headers.",
    allowInCoreHashFiles: true,
  },
  {
    name: "std_string_hash_usage",
    regex: /\bstd::hash\s*<\s*std::(?:basic_)?string/,
    severity: "warning",
    recommendation:
      "Prefer hgl::ComputeOptimalHash(str.data(), str.size()) for stable project-wide hashing.",
    rationale:
      "std::hash<std::string> is implementation-defined and may differ across toolchains.",
  },
];

const CSTRING_FORBIDDEN_PATTERNS: Array<{
  name: string;
  regex: RegExp;
  severity: "error" | "warning";
  recommendation: string;
  rationale: string;
  allowInCoreCStringFiles?: boolean;
}> = [
  {
    name: "include_cstring",
    regex: /#include\s*<cstring>/,
    severity: "error",
    recommendation:
      "Do not include <cstring> in feature code. Use ULRE string helpers from hgl/type/StrChar.h and split Str.* headers.",
    rationale:
      "ULRE intentionally centralizes string behavior to avoid cross-platform C library edge-case differences.",
    allowInCoreCStringFiles: true,
  },
  {
    name: "include_string_h",
    regex: /#include\s*<string\.h>/,
    severity: "error",
    recommendation:
      "Do not include <string.h> in feature code. Use ULRE string helpers from hgl/type/StrChar.h and split Str.* headers.",
    rationale:
      "Direct C string library usage can behave differently across compiler/runtime combinations for invalid inputs.",
    allowInCoreCStringFiles: true,
  },
  {
    name: "direct_c_string_function_call",
    regex:
      /\b(?:std::)?(?:strlen|strcmp|strncmp|strcpy|strncpy|strcat|strncat|strchr|strrchr|strstr|strtok|strspn|strcspn|strpbrk|sprintf|snprintf|vsprintf|vsnprintf|sscanf)\s*\(/,
    severity: "error",
    recommendation:
      "Replace direct C string calls with ULRE string wrappers in hgl/type/StrChar.h and the Str.* modules.",
    rationale:
      "Project policy prefers ULRE wrappers for deterministic edge-case handling across platforms instead of relying on C library details.",
    allowInCoreCStringFiles: true,
  },
];

const HGL_STRING_FORBIDDEN_PATTERNS: Array<{
  name: string;
  regex: RegExp;
  severity: "error" | "warning";
  recommendation: string;
  rationale: string;
  allowInCoreHglStringFiles?: boolean;
}> = [
  {
    name: "include_std_string_headers",
    regex: /#include\s*<(?:string|string_view)>/,
    severity: "warning",
    recommendation:
      "Prefer ULRE String stack headers (hgl/type/String.h, StringView.h, StringList.h) in feature code.",
    rationale:
      "Project policy keeps string behavior and APIs consistent through ULRE wrappers.",
    allowInCoreHglStringFiles: true,
  },
  {
    name: "std_string_type_usage",
    regex: /\bstd::(?:basic_string|string|wstring|u8string|u16string|u32string)\b/,
    severity: "error",
    recommendation:
      "Use hgl::String<T> aliases instead (AnsiString/U8String/U16String/U32String/WideString/OSString).",
    rationale:
      "Direct std::string usage bypasses ULRE string conventions and utility ecosystem.",
    allowInCoreHglStringFiles: true,
  },
  {
    name: "std_string_view_usage",
    regex: /\bstd::(?:basic_string_view|string_view|wstring_view|u8string_view|u16string_view|u32string_view)\b/,
    severity: "error",
    recommendation:
      "Use hgl::StringView<T> aliases instead (AnsiStringView/U8StringView/U16StringView/U32StringView/WideStringView/OSStringView).",
    rationale:
      "ULRE StringView keeps read-only slicing/search behavior aligned with project helpers.",
    allowInCoreHglStringFiles: true,
  },
  {
    name: "std_hash_std_string",
    regex: /\bstd::hash\s*<\s*std::(?:basic_string|string|wstring)/,
    severity: "warning",
    recommendation:
      "Prefer hgl::String<T> keys and the project hashing path (ComputeOptimalHash / hgl string hash specializations).",
    rationale:
      "Avoid mixing standard-library-specific string hashing paths with ULRE string/hash conventions.",
    allowInCoreHglStringFiles: true,
  },
];

const LOGGER_GUIDE = `# ULRE Logger Guide

## Goal

Use the Logger system for all diagnostic and runtime messages. Do not use std::cout, std::cerr, or printf for logging.

## Common Entry Points

- GLogInfo(...) for global logging.
- FLogInfo(...) for free-function/module logging where the logger instance is already bound.
- MLogInfo(...) for module-scoped logging.
- LogVerbose(...), LogInfo(...), LogWarning(...), LogError(...) for severity-based messages.
- OBJECT_LOGGER inside classes that need member-bound logging.
- DEFINE_LOGGER_MODULE(Name) and USE_MODULE_LOGGER(Name) for module wiring.

## Class Usage Pattern

1. Put OBJECT_LOGGER in the class definition.
2. Bind a logger instance name with Log.SetLoggerInstanceName(...) when needed.
3. Emit logs through the logger macros rather than writing directly to stdout or stderr.

## Preferred Behavior

- Prefer the logger macros for all normal diagnostic output.
- Prefer structured messages and clear severity levels.
- Keep user-facing output separate from diagnostics.

## Do Not Use

- std::cout for log messages.
- std::cerr for log messages.
- printf or fprintf(stderr, ...) for log messages.
`;

const HASH_GUIDE = `# ULRE Hash Guide

## Goal

Use ULRE's canonical hash wrappers and avoid hardcoded hash literals or ad-hoc hashing code in feature modules.

## Core Hash Files (Allowed Low-Level Area)

- CMCoreType/inc/hgl/util/hash/FNV1a.h
- CMCoreType/inc/hgl/util/hash/QuickHash.h
- CMCoreType/inc/hgl/util/hash/SecureHash.h
- CMCoreType/inc/hgl/util/hash/wyhash.h
- CMCoreType/inc/hgl/util/hash/wyhash32.h

## Preferred APIs for Feature Code

- hgl::ComputeOptimalHash(value) for integral/enum/pointer/value types.
- hgl::ComputeOptimalHash(data, size) for raw bytes / strings.

## Canonical Hash API Names and Usage

1. FNV1a helpers (incremental / compile-time-friendly)
- hgl::hash::FNV1aInit<uint32>() / hgl::hash::FNV1aInit<uint64>()
- hgl::hash::FNV1aAppend(hash, value)
- hgl::hash::FNV1aAppendBytes(hash, data, size)
- hgl::hash::FNV1aAppendValueBytes(hash, value)

2. QuickHash wrapper (default fast/common path)
- hgl::ComputeOptimalHash(const T &value)
- hgl::ComputeOptimalHash(const void *data, size_t size)

3. SecureHash wrapper (same API shape as QuickHash in current code)
- hgl::ComputeOptimalHash(const T &value)
- hgl::ComputeOptimalHash(const void *data, size_t size)

4. Low-level wyhash APIs (core hash files only)
- wyhash(data, len, seed, _wyp)
- wyhash32(data, len, seed)

Example for string-like data:

uint64 id_hash = hgl::ComputeOptimalHash(name.data(), name.size());

## Do Not Do in Feature Code

- Do not assign magic constants to hash/digest/fingerprint fields.
- Do not dispatch logic with switch-case on opaque hash literals.
- Do not re-implement djb2/fnv/hash loops.
- Do not call wyhash(...) / wyhash32(...) directly (except in core hash files above).
- Do not include raw wyhash headers directly outside core hash files.

## Why

- Keeps hashing behavior consistent across modules and compilers.
- Prevents opaque magic numbers from leaking into gameplay/render/business logic.
- Reduces AI-generated copy-paste hash code and future maintenance risks.
`;

const CSTRING_GUIDE = `# ULRE C String Policy Guide

## Goal

Avoid direct C standard string function usage in feature code. Use ULRE string wrappers so behavior stays consistent across compilers and platforms.

This policy is for consistency and correctness on edge cases (nullptr, invalid lengths, boundary behavior), not for performance tricks.

## Canonical Entry

- Include and use CMCoreType/inc/hgl/type/StrChar.h
- Prefer corresponding Str.* modules already split by responsibility

## Core Implementation Area (Allowed)

- CMCoreType/inc/hgl/type/StrChar.h
- CMCoreType/inc/hgl/type/Str.*.h

The core implementation files can contain low-level handling as needed. Feature/business code should not bypass them.

## Do Not Use in Feature Code

- #include <cstring>
- #include <string.h>
- Direct calls like strlen/strcmp/strcpy/strncpy/strcat/strstr/snprintf/sscanf and similar C string APIs

## Why

- Different standard libraries may handle illegal inputs and boundary cases differently.
- ULRE wrappers provide a single project-defined behavior contract.
- Prevents AI-generated copy-paste of platform-sensitive C string code.
`;

const HGL_STRING_GUIDE = `# ULRE String Stack Guide

## Goal

Prefer ULRE string abstractions over direct std::string/std::string_view usage in feature code.

This is mainly for project-level API consistency and behavior control, not to chase micro-performance.

## Canonical Types

- hgl::String<T> and aliases: AnsiString, U8String, U16String, U32String, WideString, OSString.
- hgl::StringView<T> and aliases for non-owning read-only views.
- hgl::StringList<T> for managed string collections.

## Bridge Layer (Allowed Interop)

- CMCore/inc/hgl/type/StdString.h is the interop boundary when conversion to/from std::string is required.

## Core Implementation Files (Allowed to Touch std types)

- CMCore/inc/hgl/type/String.h
- CMCore/inc/hgl/type/StringView.h
- CMCore/inc/hgl/type/StringList.h
- CMCore/inc/hgl/type/StringViewList.h
- CMCore/inc/hgl/type/SplitString.h
- CMCore/inc/hgl/type/MergeString.h
- CMCore/inc/hgl/type/StdString.h
- CMCore/inc/hgl/type/ConstStringSet.h

## Do Not Do in Feature Code

- Do not declare std::string/std::wstring/std::string_view directly.
- Do not include <string>/<string_view> unless unavoidable and reviewed.
- Do not introduce std::hash<std::string> flows where hgl::String and project hash wrappers should be used.

## Why

- Keeps APIs uniform across modules and avoids mixed string ecosystems.
- Preserves compatibility with existing hgl::strlen/hgl::strcmp and StrChar helper family.
- Reduces AI-generated code drifting to std-string-first style that conflicts with ULRE conventions.
`;

function isTextFile(filePath: string): boolean {
  const ext = path.extname(filePath).toLowerCase();
  return TEXT_EXTENSIONS.has(ext) || ext === "";
}

function shouldSkip(filePath: string): boolean {
  const parts = filePath.split(path.sep);
  return parts.some((p) => IGNORED_DIR_NAMES.has(p));
}

function toRelativePath(filePath: string, root: string): string {
  try {
    return path.relative(root, filePath).split(path.sep).join("/");
  } catch {
    return filePath.split(path.sep).join("/");
  }
}

function displayRootForFile(filePath: string): string {
  const normalizedRoot = ROOT_DIR.split(path.sep).join("/");
  const normalizedPath = filePath.split(path.sep).join("/");
  return normalizedPath.startsWith(normalizedRoot) ? ROOT_DIR : path.dirname(filePath);
}

function isCoreHashFile(filePath: string, root: string): boolean {
  const relative = toRelativePath(filePath, root).toLowerCase();
  return CORE_HASH_FILES.has(relative);
}

function isCoreCStringFile(filePath: string, root: string): boolean {
  const relative = toRelativePath(filePath, root).toLowerCase();
  if (!relative.startsWith(CORE_CSTRING_ROOT)) {
    return false;
  }

  const subpath = relative.slice(CORE_CSTRING_ROOT.length);
  return subpath === "strchar.h" || subpath.startsWith("str.");
}

function isCoreHglStringFile(filePath: string, root: string): boolean {
  const relative = toRelativePath(filePath, root).toLowerCase();
  return CORE_HGL_STRING_FILES.has(relative);
}

function scanText(filePath: string, root: string, text: string): Finding[] {
  const findings: Finding[] = [];
  const lines = text.split(/\r?\n/);

  for (let i = 0; i < lines.length; i += 1) {
    const line = lines[i] ?? "";
    for (const pattern of FORBIDDEN_PATTERNS) {
      if (pattern.regex.test(line)) {
        findings.push({
          path: toRelativePath(filePath, root),
          line: i + 1,
          pattern: pattern.name,
          severity: "error",
          snippet: line.trim(),
          logger_entry_point: "GLogInfo / FLogInfo / MLogInfo / LogInfo / LogWarning / LogError",
          replacement: pattern.recommendation,
          why_it_matters:
            "Direct console output bypasses the structured logger pipeline and can be lost, mis-encoded, or hidden from sinks that need record metadata.",
          auto_fixable: true,
        });
      }
    }
  }

  return findings;
}

function scanHashText(filePath: string, root: string, text: string): HashFinding[] {
  const findings: HashFinding[] = [];
  const lines = text.split(/\r?\n/);
  const inCoreHashFile = isCoreHashFile(filePath, root);

  for (let i = 0; i < lines.length; i += 1) {
    const line = lines[i] ?? "";
    for (const pattern of HASH_FORBIDDEN_PATTERNS) {
      if (inCoreHashFile && pattern.allowInCoreHashFiles) {
        continue;
      }
      if (pattern.regex.test(line)) {
        findings.push({
          path: toRelativePath(filePath, root),
          line: i + 1,
          pattern: pattern.name,
          severity: pattern.severity,
          snippet: line.trim(),
          preferred_api:
            "hgl::ComputeOptimalHash(const T&) / hgl::ComputeOptimalHash(const void*, size_t) / hgl::hash::FNV1aInit + FNV1aAppend(+Bytes)",
          replacement: pattern.recommendation,
          why_it_matters: pattern.rationale,
          auto_fixable: true,
        });
      }
    }
  }

  return findings;
}

function scanCStringText(filePath: string, root: string, text: string): CStringFinding[] {
  const findings: CStringFinding[] = [];
  const lines = text.split(/\r?\n/);
  const inCoreCStringFile = isCoreCStringFile(filePath, root);

  for (let i = 0; i < lines.length; i += 1) {
    const line = lines[i] ?? "";
    for (const pattern of CSTRING_FORBIDDEN_PATTERNS) {
      if (inCoreCStringFile && pattern.allowInCoreCStringFiles) {
        continue;
      }
      if (pattern.regex.test(line)) {
        findings.push({
          path: toRelativePath(filePath, root),
          line: i + 1,
          pattern: pattern.name,
          severity: pattern.severity,
          snippet: line.trim(),
          preferred_api: "hgl/type/StrChar.h + Str.* wrappers",
          replacement: pattern.recommendation,
          why_it_matters: pattern.rationale,
          auto_fixable: true,
        });
      }
    }
  }

  return findings;
}

function scanHglStringText(filePath: string, root: string, text: string): HglStringFinding[] {
  const findings: HglStringFinding[] = [];
  const lines = text.split(/\r?\n/);
  const inCoreHglStringFile = isCoreHglStringFile(filePath, root);

  for (let i = 0; i < lines.length; i += 1) {
    const line = lines[i] ?? "";
    for (const pattern of HGL_STRING_FORBIDDEN_PATTERNS) {
      if (inCoreHglStringFile && pattern.allowInCoreHglStringFiles) {
        continue;
      }
      if (pattern.regex.test(line)) {
        findings.push({
          path: toRelativePath(filePath, root),
          line: i + 1,
          pattern: pattern.name,
          severity: pattern.severity,
          snippet: line.trim(),
          preferred_api: "hgl::String<T> / hgl::StringView<T> / hgl::StringList<T>",
          replacement: pattern.recommendation,
          why_it_matters: pattern.rationale,
          auto_fixable: true,
        });
      }
    }
  }

  return findings;
}

function iterWorkspaceFiles(root: string): string[] {
  const files: string[] = [];

  function walk(dir: string): void {
    const entries = readdirSync(dir);
    for (const entry of entries) {
      const fullPath = path.join(dir, entry);
      if (shouldSkip(fullPath)) {
        continue;
      }
      const stat = statSync(fullPath);
      if (stat.isDirectory()) {
        walk(fullPath);
        continue;
      }
      if (stat.isFile() && isTextFile(fullPath)) {
        files.push(fullPath);
      }
    }
  }

  walk(root);
  return files;
}

function summarize(matches: Finding[]): ScanSummary {
  const byPattern: Record<string, number> = {};
  for (const m of matches) {
    byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
  }
  return {
    total_matches: matches.length,
    by_pattern: byPattern,
    files_with_matches: new Set(matches.map((m) => m.path)).size,
  };
}

const server = new McpServer(
  {
    name: "ULRE Policy Guard",
    version: "0.1.0",
  },
  {
    instructions:
      "Teach and enforce ULRE logger/hash/string usage rules. Prefer logger macros, hash wrappers, and ULRE string wrappers; flag direct stdout/stderr logging, hardcoded hash logic, and direct C string library calls in feature code.",
  }
);

server.registerResource(
  "logger-guide",
  "logger://guide",
  {
    title: "ULRE Logger Guide",
    description: "Canonical logger usage guide for ULRE.",
    mimeType: "text/markdown",
  },
  async () => ({
    contents: [{ uri: "logger://guide", text: LOGGER_GUIDE, mimeType: "text/markdown" }],
  })
);

server.registerResource(
  "forbidden-patterns",
  "logger://forbidden-patterns",
  {
    title: "Forbidden Logging Patterns",
    description: "Patterns that should not be used for logging.",
    mimeType: "text/plain",
  },
  async () => ({
    contents: [
      {
        uri: "logger://forbidden-patterns",
        text: "- std::cout\n- std::cerr\n- printf\n- fprintf(stderr, ...)",
        mimeType: "text/plain",
      },
    ],
  })
);

server.registerResource(
  "hash-guide",
  "hash://guide",
  {
    title: "ULRE Hash Guide",
    description: "Canonical hash usage guide for ULRE.",
    mimeType: "text/markdown",
  },
  async () => ({
    contents: [{ uri: "hash://guide", text: HASH_GUIDE, mimeType: "text/markdown" }],
  })
);

server.registerResource(
  "hash-forbidden-patterns",
  "hash://forbidden-patterns",
  {
    title: "Forbidden Hash Patterns",
    description: "Patterns that should not appear in feature code hashing.",
    mimeType: "text/plain",
  },
  async () => ({
    contents: [
      {
        uri: "hash://forbidden-patterns",
        text: [
          "- hardcoded hash literal assignments",
          "- switch/case on magic hash values",
          "- ad-hoc djb2/fnv style implementations",
          "- direct wyhash(...) / wyhash32(...) calls outside core hash files",
          "- direct include of raw wyhash headers outside core hash files",
        ].join("\n"),
        mimeType: "text/plain",
      },
    ],
  })
);

server.registerResource(
  "cstring-guide",
  "cstring://guide",
  {
    title: "ULRE C String Policy Guide",
    description: "Canonical C-string wrapper usage guide for ULRE.",
    mimeType: "text/markdown",
  },
  async () => ({
    contents: [{ uri: "cstring://guide", text: CSTRING_GUIDE, mimeType: "text/markdown" }],
  })
);

server.registerResource(
  "cstring-forbidden-patterns",
  "cstring://forbidden-patterns",
  {
    title: "Forbidden C String Patterns",
    description: "Patterns that should not appear in feature code C-string handling.",
    mimeType: "text/plain",
  },
  async () => ({
    contents: [
      {
        uri: "cstring://forbidden-patterns",
        text: [
          "- include <cstring>/<string.h> in feature code",
          "- direct C string function calls (strlen/strcmp/strcpy/...)",
          "- bypassing ULRE StrChar/Str.* wrappers",
        ].join("\n"),
        mimeType: "text/plain",
      },
    ],
  })
);

server.registerResource(
  "hgl-string-guide",
  "hglstring://guide",
  {
    title: "ULRE String Stack Guide",
    description: "Canonical String/StringView/StringList usage guide for ULRE.",
    mimeType: "text/markdown",
  },
  async () => ({
    contents: [{ uri: "hglstring://guide", text: HGL_STRING_GUIDE, mimeType: "text/markdown" }],
  })
);

server.registerResource(
  "hgl-string-forbidden-patterns",
  "hglstring://forbidden-patterns",
  {
    title: "Forbidden std::string Patterns",
    description: "Patterns that should not appear in feature code for ULRE string policy.",
    mimeType: "text/plain",
  },
  async () => ({
    contents: [
      {
        uri: "hglstring://forbidden-patterns",
        text: [
          "- std::string/std::wstring/std::string_view direct type usage",
          "- direct include of <string>/<string_view> in feature code",
          "- std::hash<std::string> style paths instead of ULRE string+hash stack",
        ].join("\n"),
        mimeType: "text/plain",
      },
    ],
  })
);

server.registerPrompt(
  "logger_usage_review",
  {
    title: "Logger Usage Review",
    description: "Review code against ULRE logger policy.",
    argsSchema: { source: z.string() },
  },
  async ({ source }) => ({
    messages: [
      {
        role: "user",
        content: {
          type: "text",
          text:
            "Review the following code against the ULRE logger policy. Reject any use of std::cout, std::cerr, printf, or fprintf(stderr, ...). Prefer the logger macros and explain how the code should be rewritten.\n\n" +
            source,
        },
      },
    ],
  })
);

server.registerPrompt(
  "hash_usage_review",
  {
    title: "Hash Usage Review",
    description: "Review code against ULRE hash policy.",
    argsSchema: { source: z.string() },
  },
  async ({ source }) => ({
    messages: [
      {
        role: "user",
        content: {
          type: "text",
          text:
            "Review the following code against the ULRE hash policy. Reject hardcoded hash literals, ad-hoc hash loops, and direct wyhash/wyhash32 usage outside core hash files. Prefer hgl::ComputeOptimalHash(...) and canonical FNV1a helpers when incremental hashing is needed; explain exact rewrites.\n\n" +
            source,
        },
      },
    ],
  })
);

server.registerPrompt(
  "cstring_usage_review",
  {
    title: "C String Usage Review",
    description: "Review code against ULRE C string policy.",
    argsSchema: { source: z.string() },
  },
  async ({ source }) => ({
    messages: [
      {
        role: "user",
        content: {
          type: "text",
          text:
            "Review the following code against the ULRE C string policy. Reject direct C standard string library usage in feature code and suggest equivalent ULRE StrChar/Str.* wrappers. Emphasize this is for cross-platform behavior consistency, not performance tricks.\n\n" +
            source,
        },
      },
    ],
  })
);

server.registerPrompt(
  "hgl_string_usage_review",
  {
    title: "ULRE String Usage Review",
    description: "Review code against ULRE String/StringView/StringList policy.",
    argsSchema: { source: z.string() },
  },
  async ({ source }) => ({
    messages: [
      {
        role: "user",
        content: {
          type: "text",
          text:
            "Review the following code against ULRE string policy. Prefer hgl::String/hgl::StringView/hgl::StringList and reject direct std::string/std::string_view usage in feature code, except dedicated bridge/core files.\n\n" +
            source,
        },
      },
    ],
  })
);

server.registerTool(
  "get_logger_guide",
  {
    title: "Get Logger Guide",
    description: "Return the concise logger usage guide.",
    inputSchema: {},
  },
  async () => ({
    content: [{ type: "text", text: LOGGER_GUIDE }],
    structuredContent: { guide: LOGGER_GUIDE },
  })
);

server.registerTool(
  "get_hash_guide",
  {
    title: "Get Hash Guide",
    description: "Return the concise hash usage guide.",
    inputSchema: {},
  },
  async () => ({
    content: [{ type: "text", text: HASH_GUIDE }],
    structuredContent: { guide: HASH_GUIDE },
  })
);

server.registerTool(
  "get_cstring_guide",
  {
    title: "Get C String Guide",
    description: "Return the concise ULRE C-string policy guide.",
    inputSchema: {},
  },
  async () => ({
    content: [{ type: "text", text: CSTRING_GUIDE }],
    structuredContent: { guide: CSTRING_GUIDE },
  })
);

server.registerTool(
  "get_hgl_string_guide",
  {
    title: "Get ULRE String Guide",
    description: "Return the concise ULRE String/StringView/StringList guide.",
    inputSchema: {},
  },
  async () => ({
    content: [{ type: "text", text: HGL_STRING_GUIDE }],
    structuredContent: { guide: HGL_STRING_GUIDE },
  })
);

server.registerTool(
  "scan_workspace_for_forbidden_logging",
  {
    title: "Scan Workspace for Forbidden Logging",
    description: "Scan the workspace for direct logging anti-patterns.",
    inputSchema: { root: z.string().optional() },
  },
  async ({ root }) => {
    const scanRoot = root ? path.resolve(root) : ROOT_DIR;
    const matches: Finding[] = [];
    let scannedFiles = 0;

    for (const filePath of iterWorkspaceFiles(scanRoot)) {
      try {
        const text = readFileSync(filePath, "utf8");
        scannedFiles += 1;
        matches.push(...scanText(filePath, scanRoot, text));
      } catch {
        continue;
      }
    }

    const summary = summarize(matches);
    const result = {
      root: scanRoot,
      scanned_files: scannedFiles,
      summary,
      matches,
      guidance:
        "Use the logger macros for diagnostics. If you need a quick replacement, map stdout/stderr/printf usage to the nearest logger severity and keep the message on the logger path. Prefer one-for-one replacements so the call site can be rewritten mechanically.",
      recommended_next_step:
        "Replace each finding with the listed logger entry point, then rerun the scan.",
    };

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      structuredContent: result,
    };
  }
);

server.registerTool(
  "check_file_for_forbidden_logging",
  {
    title: "Check File for Forbidden Logging",
    description: "Check a single file for forbidden logging patterns.",
    inputSchema: { path: z.string() },
  },
  async ({ path: inputPath }) => {
    const filePath = path.resolve(inputPath);

    if (!existsSync(filePath)) {
      const missing = {
        path: filePath,
        exists: false,
        summary: { total_matches: 0, by_pattern: {} },
        matches: [],
        message: "File does not exist.",
      };
      return {
        content: [{ type: "text", text: JSON.stringify(missing, null, 2) }],
        structuredContent: missing,
      };
    }

    try {
      const text = readFileSync(filePath, "utf8");
      const matches = scanText(filePath, displayRootForFile(filePath), text);
      const byPattern: Record<string, number> = {};
      for (const m of matches) {
        byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
      }

      const result = {
        path: filePath,
        exists: true,
        summary: {
          total_matches: matches.length,
          by_pattern: byPattern,
        },
        matches,
        message: matches.length === 0 ? "No forbidden logging patterns found." : "Forbidden logging patterns found.",
      };

      return {
        content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
        structuredContent: result,
      };
    } catch (err) {
      const failed = {
        path: filePath,
        exists: true,
        summary: { total_matches: 0, by_pattern: {} },
        matches: [],
        message: `Failed to read file: ${String(err)}`,
      };
      return {
        content: [{ type: "text", text: JSON.stringify(failed, null, 2) }],
        structuredContent: failed,
      };
    }
  }
);

server.registerTool(
  "scan_workspace_for_forbidden_hashing",
  {
    title: "Scan Workspace for Forbidden Hashing",
    description: "Scan the workspace for hardcoded hash and ad-hoc hashing anti-patterns.",
    inputSchema: { root: z.string().optional() },
  },
  async ({ root }) => {
    const scanRoot = root ? path.resolve(root) : ROOT_DIR;
    const matches: HashFinding[] = [];
    let scannedFiles = 0;

    for (const filePath of iterWorkspaceFiles(scanRoot)) {
      try {
        const text = readFileSync(filePath, "utf8");
        scannedFiles += 1;
        matches.push(...scanHashText(filePath, scanRoot, text));
      } catch {
        continue;
      }
    }

    const byPattern: Record<string, number> = {};
    for (const m of matches) {
      byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
    }

    const result = {
      root: scanRoot,
      scanned_files: scannedFiles,
      summary: {
        total_matches: matches.length,
        by_pattern: byPattern,
        files_with_matches: new Set(matches.map((m) => m.path)).size,
      },
      matches,
      guidance:
        "Use hgl::ComputeOptimalHash wrappers in feature code; use canonical FNV1a helpers only when incremental hashing is required. Reserve direct wyhash internals to core hash files only.",
      recommended_next_step:
        "Replace each finding with ComputeOptimalHash/FNV1a canonical code, then rerun the hash scan.",
    };

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      structuredContent: result,
    };
  }
);

server.registerTool(
  "check_file_for_forbidden_hashing",
  {
    title: "Check File for Forbidden Hashing",
    description: "Check a single file for hardcoded hash and ad-hoc hashing patterns.",
    inputSchema: { path: z.string() },
  },
  async ({ path: inputPath }) => {
    const filePath = path.resolve(inputPath);

    if (!existsSync(filePath)) {
      const missing = {
        path: filePath,
        exists: false,
        summary: { total_matches: 0, by_pattern: {} },
        matches: [],
        message: "File does not exist.",
      };
      return {
        content: [{ type: "text", text: JSON.stringify(missing, null, 2) }],
        structuredContent: missing,
      };
    }

    try {
      const text = readFileSync(filePath, "utf8");
      const matches = scanHashText(filePath, displayRootForFile(filePath), text);
      const byPattern: Record<string, number> = {};
      for (const m of matches) {
        byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
      }

      const result = {
        path: filePath,
        exists: true,
        summary: {
          total_matches: matches.length,
          by_pattern: byPattern,
        },
        matches,
        message: matches.length === 0 ? "No forbidden hash patterns found." : "Forbidden hash patterns found.",
      };

      return {
        content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
        structuredContent: result,
      };
    } catch (err) {
      const failed = {
        path: filePath,
        exists: true,
        summary: { total_matches: 0, by_pattern: {} },
        matches: [],
        message: `Failed to read file: ${String(err)}`,
      };
      return {
        content: [{ type: "text", text: JSON.stringify(failed, null, 2) }],
        structuredContent: failed,
      };
    }
  }
);

server.registerTool(
  "scan_workspace_for_forbidden_cstring_usage",
  {
    title: "Scan Workspace for Forbidden C String Usage",
    description: "Scan the workspace for direct C standard string API usage in feature code.",
    inputSchema: { root: z.string().optional() },
  },
  async ({ root }) => {
    const scanRoot = root ? path.resolve(root) : ROOT_DIR;
    const matches: CStringFinding[] = [];
    let scannedFiles = 0;

    for (const filePath of iterWorkspaceFiles(scanRoot)) {
      try {
        const text = readFileSync(filePath, "utf8");
        scannedFiles += 1;
        matches.push(...scanCStringText(filePath, scanRoot, text));
      } catch {
        continue;
      }
    }

    const byPattern: Record<string, number> = {};
    for (const m of matches) {
      byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
    }

    const result = {
      root: scanRoot,
      scanned_files: scannedFiles,
      summary: {
        total_matches: matches.length,
        by_pattern: byPattern,
        files_with_matches: new Set(matches.map((m) => m.path)).size,
      },
      matches,
      guidance:
        "Use ULRE string wrappers from StrChar/Str.* for cross-platform consistency in edge cases.",
      recommended_next_step:
        "Replace each direct C string call/include with the matching ULRE wrapper, then rerun this scan.",
    };

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      structuredContent: result,
    };
  }
);

server.registerTool(
  "check_file_for_forbidden_cstring_usage",
  {
    title: "Check File for Forbidden C String Usage",
    description: "Check a single file for direct C standard string API usage.",
    inputSchema: { path: z.string() },
  },
  async ({ path: inputPath }) => {
    const filePath = path.resolve(inputPath);

    if (!existsSync(filePath)) {
      const missing = {
        path: filePath,
        exists: false,
        summary: { total_matches: 0, by_pattern: {} },
        matches: [],
        message: "File does not exist.",
      };
      return {
        content: [{ type: "text", text: JSON.stringify(missing, null, 2) }],
        structuredContent: missing,
      };
    }

    try {
      const text = readFileSync(filePath, "utf8");
      const matches = scanCStringText(filePath, displayRootForFile(filePath), text);
      const byPattern: Record<string, number> = {};
      for (const m of matches) {
        byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
      }

      const result = {
        path: filePath,
        exists: true,
        summary: {
          total_matches: matches.length,
          by_pattern: byPattern,
        },
        matches,
        message: matches.length === 0 ? "No forbidden C string patterns found." : "Forbidden C string patterns found.",
      };

      return {
        content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
        structuredContent: result,
      };
    } catch (err) {
      const failed = {
        path: filePath,
        exists: true,
        summary: { total_matches: 0, by_pattern: {} },
        matches: [],
        message: `Failed to read file: ${String(err)}`,
      };
      return {
        content: [{ type: "text", text: JSON.stringify(failed, null, 2) }],
        structuredContent: failed,
      };
    }
  }
);

server.registerTool(
  "scan_workspace_for_forbidden_std_string_usage",
  {
    title: "Scan Workspace for Forbidden std::string Usage",
    description: "Scan the workspace for direct std::string/std::string_view usage against ULRE string policy.",
    inputSchema: { root: z.string().optional() },
  },
  async ({ root }) => {
    const scanRoot = root ? path.resolve(root) : ROOT_DIR;
    const matches: HglStringFinding[] = [];
    let scannedFiles = 0;

    for (const filePath of iterWorkspaceFiles(scanRoot)) {
      try {
        const text = readFileSync(filePath, "utf8");
        scannedFiles += 1;
        matches.push(...scanHglStringText(filePath, scanRoot, text));
      } catch {
        continue;
      }
    }

    const byPattern: Record<string, number> = {};
    for (const m of matches) {
      byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
    }

    const result = {
      root: scanRoot,
      scanned_files: scannedFiles,
      summary: {
        total_matches: matches.length,
        by_pattern: byPattern,
        files_with_matches: new Set(matches.map((m) => m.path)).size,
      },
      matches,
      guidance:
        "Use ULRE String stack in feature code and reserve std-string interop to bridge/core files.",
      recommended_next_step:
        "Replace direct std::string/std::string_view usages with ULRE String wrappers, then rerun this scan.",
    };

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      structuredContent: result,
    };
  }
);

server.registerTool(
  "check_file_for_forbidden_std_string_usage",
  {
    title: "Check File for Forbidden std::string Usage",
    description: "Check a single file for direct std::string/std::string_view usage.",
    inputSchema: { path: z.string() },
  },
  async ({ path: inputPath }) => {
    const filePath = path.resolve(inputPath);

    if (!existsSync(filePath)) {
      const missing = {
        path: filePath,
        exists: false,
        summary: { total_matches: 0, by_pattern: {} },
        matches: [],
        message: "File does not exist.",
      };
      return {
        content: [{ type: "text", text: JSON.stringify(missing, null, 2) }],
        structuredContent: missing,
      };
    }

    try {
      const text = readFileSync(filePath, "utf8");
      const matches = scanHglStringText(filePath, displayRootForFile(filePath), text);
      const byPattern: Record<string, number> = {};
      for (const m of matches) {
        byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
      }

      const result = {
        path: filePath,
        exists: true,
        summary: {
          total_matches: matches.length,
          by_pattern: byPattern,
        },
        matches,
        message: matches.length === 0 ? "No forbidden std::string patterns found." : "Forbidden std::string patterns found.",
      };

      return {
        content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
        structuredContent: result,
      };
    } catch (err) {
      const failed = {
        path: filePath,
        exists: true,
        summary: { total_matches: 0, by_pattern: {} },
        matches: [],
        message: `Failed to read file: ${String(err)}`,
      };
      return {
        content: [{ type: "text", text: JSON.stringify(failed, null, 2) }],
        structuredContent: failed,
      };
    }
  }
);

server.registerTool(
  "review_code_for_logger_misuse",
  {
    title: "Review Code for Logger Misuse",
    description: "Review a code snippet for forbidden logging patterns.",
    inputSchema: { code: z.string() },
  },
  async ({ code }) => {
    const syntheticPath = path.join(ROOT_DIR, "<snippet>");
    const matches = scanText(syntheticPath, ROOT_DIR, code);
    const summary = summarize(matches);

    const result = {
      verdict: matches.length > 0 ? "fail" : "pass",
      summary,
      matches,
      guidance: "Rewrite the snippet to use the logger macros and keep diagnostics out of stdout/stderr.",
      recommended_next_step:
        "Replace each forbidden print call with the nearest logger macro, then rerun this review.",
    };

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      structuredContent: result,
    };
  }
);

server.registerTool(
  "review_code_for_hash_misuse",
  {
    title: "Review Code for Hash Misuse",
    description: "Review a code snippet for forbidden hash patterns.",
    inputSchema: { code: z.string() },
  },
  async ({ code }) => {
    const syntheticPath = path.join(ROOT_DIR, "<snippet>");
    const matches = scanHashText(syntheticPath, ROOT_DIR, code);

    const byPattern: Record<string, number> = {};
    for (const m of matches) {
      byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
    }

    const result = {
      verdict: matches.length > 0 ? "fail" : "pass",
      summary: {
        total_matches: matches.length,
        by_pattern: byPattern,
        files_with_matches: matches.length > 0 ? 1 : 0,
      },
      matches,
      guidance:
        "Avoid hardcoded hash values and ad-hoc hash implementations. Use hgl::ComputeOptimalHash wrappers or canonical FNV1a helpers.",
      recommended_next_step:
        "Rewrite hash logic with ComputeOptimalHash/FNV1a canonical calls, then rerun this review.",
    };

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      structuredContent: result,
    };
  }
);

server.registerTool(
  "review_code_for_cstring_misuse",
  {
    title: "Review Code for C String Misuse",
    description: "Review a code snippet for forbidden direct C-string API usage.",
    inputSchema: { code: z.string() },
  },
  async ({ code }) => {
    const syntheticPath = path.join(ROOT_DIR, "<snippet>");
    const matches = scanCStringText(syntheticPath, ROOT_DIR, code);

    const byPattern: Record<string, number> = {};
    for (const m of matches) {
      byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
    }

    const result = {
      verdict: matches.length > 0 ? "fail" : "pass",
      summary: {
        total_matches: matches.length,
        by_pattern: byPattern,
        files_with_matches: matches.length > 0 ? 1 : 0,
      },
      matches,
      guidance:
        "Use ULRE StrChar/Str.* wrappers instead of direct C standard string APIs in feature code.",
      recommended_next_step:
        "Rewrite direct C-string calls via ULRE wrappers, then rerun this review.",
    };

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      structuredContent: result,
    };
  }
);

server.registerTool(
  "review_code_for_std_string_misuse",
  {
    title: "Review Code for std::string Misuse",
    description: "Review a code snippet for direct std::string/std::string_view usage against ULRE policy.",
    inputSchema: { code: z.string() },
  },
  async ({ code }) => {
    const syntheticPath = path.join(ROOT_DIR, "<snippet>");
    const matches = scanHglStringText(syntheticPath, ROOT_DIR, code);

    const byPattern: Record<string, number> = {};
    for (const m of matches) {
      byPattern[m.pattern] = (byPattern[m.pattern] ?? 0) + 1;
    }

    const result = {
      verdict: matches.length > 0 ? "fail" : "pass",
      summary: {
        total_matches: matches.length,
        by_pattern: byPattern,
        files_with_matches: matches.length > 0 ? 1 : 0,
      },
      matches,
      guidance:
        "Prefer hgl::String/hgl::StringView/hgl::StringList in feature code and keep std-string interop in dedicated bridge files.",
      recommended_next_step:
        "Rewrite std-string usage with ULRE string stack APIs, then rerun this review.",
    };

    return {
      content: [{ type: "text", text: JSON.stringify(result, null, 2) }],
      structuredContent: result,
    };
  }
);

async function main(): Promise<void> {
  const transport = new StdioServerTransport();
  await server.connect(transport);
}

main().catch((error) => {
  console.error("Failed to start ULRE Policy Guard MCP server:", error);
  process.exit(1);
});
