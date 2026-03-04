# Shader Package Layout Specification (Task B Draft)

## 1. Purpose

Define the canonical on-disk layout for ShaderGen package artifacts before MiniPack aggregation.

This spec is shared by:
- local `shadergen-cli`
- cloud packaging pipeline
- runtime `ShaderPackageLoader`

## 2. Canonical Directory Layout

A package directory MUST follow this shape:

- `manifest.json`
- `materials/<material_key>/layout.json`
- `materials/<material_key>/vertex.json`
- `materials/<material_key>/result.json` (optional but recommended)
- `materials/<material_key>/diagnostics.json` (optional)
- `materials/<material_key>/spv_stage_0x*.spv`

Example:

- `manifest.json`
- `materials/basiclit/layout.json`
- `materials/basiclit/vertex.json`
- `materials/basiclit/spv_stage_0x1.spv`
- `materials/basiclit/spv_stage_0x10.spv`

## 3. Required Files

Runtime minimum for a material:
- `layout.json`
- `vertex.json`
- at least one `spv_stage_0x*.spv`

Package minimum:
- `manifest.json`
- one or more valid materials as above

## 4. Naming Rules

- `material_key`: lowercase `[a-z0-9_\-]+`
- stage files: `spv_stage_0x<HEX>.spv` (uppercase/lowercase hex allowed)
- all manifest paths use `/` separators and are relative to package root

## 5. Manifest Contract

Manifest MUST validate against:
- `doc/shader-system/schema/shader-package-manifest.schema.json`

Reference example:
- `doc/shader-system/schema/examples/shader-package.manifest.example.json`

## 6. MiniPack Aggregation

When packaging into MiniPack (`*.pack`):
- preserve all relative paths exactly
- preserve byte contents of all listed artifacts
- include every artifact listed in `manifest.json`

The pack consumer MUST:
- read `manifest.json` first
- validate `content_hash` and per-artifact hash/size before resource creation

## 7. Compatibility Rules

- `manifest_version` major bump indicates breaking layout/schema changes
- runtime SHOULD reject newer major versions it cannot parse
- additive fields are allowed if schema marks them as optional

## 8. Error Handling (Machine-readable)

Loader errors SHOULD include:
- `code` (e.g., `manifest.invalid`, `artifact.missing`, `hash.mismatch`)
- `path`
- `expected`
- `actual`

This enables deterministic local/cloud debugging and CI triage.
