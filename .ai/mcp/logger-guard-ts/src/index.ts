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
    name: "ULRE Logger Guard",
    version: "0.1.0",
  },
  {
    instructions:
      "Teach and enforce the ULRE logger usage rules. Prefer the logger macros, flag direct stdout/stderr logging, and provide the canonical logger guide.",
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

async function main(): Promise<void> {
  const transport = new StdioServerTransport();
  await server.connect(transport);
}

main().catch((error) => {
  console.error("Failed to start ULRE Logger Guard MCP server:", error);
  process.exit(1);
});
