# Copilot Instructions

## Project Guidelines
- When adding/running ShaderGen smoke test executables, set their Visual Studio debugger working directory to ${ULRE_RUNTIME_PATH} (repo root), otherwise GLSLCompiler.dll cannot be found.
- During iterative ShaderGen/F1 work, add more end-to-end logging in smoke tests and relevant paths to make failures easier to analyze.
- Prioritize the migration of all legacy material paths/candidates before doing further internal cleanup or structural refactoring work.
- Address the material system's structural duplication by separating VS and FS concerns instead of keeping them tightly coupled as a single VS+FS material unit. For ShaderGen planning, treat VS/FS separation as a future architectural direction only; current refactoring should create groundwork and hard boundaries without implementing VS/FS splitting until the renderer-side management supports it.
- This repo uses a CMake out-of-source build. If the workspace root is `...\build`, treat that as the generated build directory only; edit source files in the repo root (`..\src`, `..\inc`, `..\.github`, etc.) and never modify generated files under `build\src`.
