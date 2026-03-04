# ShaderGen CLI Specification (Task C Draft)

## 1. Scope

Define command-line contract for `shadergen-cli` used by local toolchain and cloud workers.

The CLI has three primary subcommands:
- `generate`: request -> material artifact directory
- `pack`: artifact directory -> package (`.pack`)
- `verify`: validate package integrity and manifest consistency

## 2. Command Contract

## 2.1 generate

```bash
shadergen-cli generate --request <request.json> --out <result_dir> [options]
```

Required:
- `--request <file>`: input file validated by `shadergen-request.schema.json`
- `--out <dir>`: output directory for generated artifacts

Optional:
- `--result-json <file>`: explicit output path for serialized result metadata (default: `<out>/result.json`)
- `--manifest <file>`: explicit output path for manifest (default: `<out>/manifest.json`)
- `--log-format <jsonl|kv>` (default: `jsonl`)
- `--strict`: fail on warnings
- `--deterministic`: enforce deterministic build mode (stable order/hash)

Output (success):
- material artifact files (layout/vertex/spv)
- manifest.json
- result.json (or equivalent)

## 2.2 pack

```bash
shadergen-cli pack --in <result_dir> --out <shader_package.pack> [options]
```

Required:
- `--in <dir>`: input artifact directory (must contain `manifest.json`)
- `--out <file>`: output package file

Optional:
- `--compression <none|zstd|lz4>` (default: `none`)
- `--sign-key <key-id-or-path>`
- `--log-format <jsonl|kv>` (default: `jsonl`)

Behavior:
- preserve relative paths defined in package layout spec
- package all artifacts listed in manifest
- write/refresh package-level `content_hash` where applicable

## 2.3 verify

```bash
shadergen-cli verify --package <shader_package.pack> [options]
```

Required:
- `--package <file>`

Optional:
- `--manifest-only`: validate index/manifest only, skip full SPV payload checksum
- `--strict-signature`: require valid signature and reject unsigned package
- `--log-format <jsonl|kv>` (default: `jsonl`)

Behavior:
- validate pack structure and manifest schema
- verify required artifact presence
- verify hash/size consistency

## 3. Exit Codes

- `0` success
- `2` input protocol/argument error
- `3` shader compilation/generation failed
- `4` package validation failed
- `5` I/O error (read/write/permission)
- `6` internal/unknown error

## 4. Machine-readable Logging

Default output is JSONL (one JSON object per line).

Schema:
- `doc/shader-system/schema/shadergen-cli-log.schema.json`

Suggested fields:
- `ts_utc`
- `level` (`debug|info|warn|error|fatal`)
- `code` (`request.invalid`, `spv.compile_failed`, `pack.hash_mismatch`, ...)
- `command` (`generate|pack|verify`)
- `message`
- `context` (object)

## 5. Determinism Requirements

When `--deterministic` is enabled, CLI MUST:
- produce stable artifact ordering
- produce stable content hash for same request+toolchain inputs
- record deterministic mode in manifest metadata

## 6. Compatibility

- CLI must accept only supported manifest/schema major versions.
- For newer major versions, return exit code `2` with code `schema.unsupported_version`.
- Additive optional fields are tolerated across minor versions.

## 7. References

- `doc/shader-system/schema/shadergen-request.schema.json`
- `doc/shader-system/schema/shadergen-result.schema.json`
- `doc/shader-system/schema/shader-package-manifest.schema.json`
- `doc/shader-system/SHADER_PACKAGE_LAYOUT_SPEC.md`
