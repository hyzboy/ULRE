# DescriptorSemantic Refactor Plan

## Handoff Status (2026-03-23, updated)

Phases 0–5 are fully complete. Phase 6 remains optional.

### Completed in code

1. Phase 0: `DescriptorSemanticMeta` definition and metadata access were stabilized.
2. Phase 1: `MaterialCreateInfo` now exposes semantic-based public UBO/SSBO APIs; old string-based variants are private implementation details.
3. `MaterialCompiler` builtin UBO/SSBO path now calls semantic APIs.
4. Phase 2: semantic fields were propagated into descriptor/source data models:
   - `ShaderDescriptor` now stores `DescriptorSemantic`.
   - `ShaderBufferSource` now stores `DescriptorSemantic`.
   - UBO/SSBO descriptor creation paths now preserve semantic.
5. Runtime layout path hardening:
   - `MaterialManager` now builds `MaterialDescriptorManager` from `ShaderDescriptorSetArray` directly (removed legacy flatten/copy path).
   - `VKMaterialDescriptorManager` now filters null descriptor pointers before computing `bindingCount` and emitting Vulkan layout bindings.
6. Phase 3: Semantic-indexed storage added to `MaterialDescriptorInfo`:
   - `ubo_by_semantic[]` / `ssbo_by_semantic[]` arrays (size `DescriptorSemanticCount`).
   - `AddUBO`/`AddSSBO` now populate semantic slots for builtin semantics.
   - `GetUBO(DescriptorSemantic)` / `GetSSBO(DescriptorSemantic)` O(1) getters added.
7. Phase 4: `ResolveUBODescriptor` / `ResolveSSBODescriptor` in `MaterialCreateInfo.cpp` are now semantic-only; non-builtin semantic returns `nullptr` immediately with no name-based fallback.
   - `SetLocalToWorld` uses `GetSSBO(DescriptorSemantic::LocalToWorld)`.
8. Phase 5: Removed commented legacy `struct_name` fallback blocks from `MaterialCompiler.cpp` (UBO and SSBO cases).
   - Inline `!= Unknown && != Custom` guards replaced with `IsBuiltinDescriptorSemantic()`.
9. Additional cleanups:
   - `IsBuiltinDescriptorSemantic` strengthened to range check (`> Unknown && < Custom`).
   - Redundant `ResolveDescriptorSemanticMetaForKind` pre-flight calls removed from the four `AddUBO`/`AddSSBO` overloads that immediately delegate to `AddUBOStruct`/`AddSSBOStruct`.

### Last observed runtime issue chain

1. Earlier error: duplicate descriptor binding indices in `vkCreateDescriptorSetLayout`.
2. After layout hardening, issue progressed to stage flag mismatch (`VUID-VkGraphicsPipelineCreateInfo-layout-07988`) in a later run.
3. Additional null-filter/count alignment fix was applied in `VKMaterialDescriptorManager` to avoid stale/partially written `VkDescriptorSetLayoutBinding` entries.
4. Final revalidation on a clean machine/rebuild is still required.

### Files touched

- `inc/hgl/mtl/DescriptorBindingContract.h`
- `inc/hgl/shadergen/MaterialCreateInfo.h`
- `src/ShaderGen/MaterialCreateInfo.cpp`
- `src/ShaderGen/MaterialCompiler.cpp`
- `inc/hgl/shadergen/MaterialDescriptorInfo.h`
- `src/ShaderGen/MaterialDescriptorInfo.cpp`
- `inc/hgl/common/ShaderDescriptorDef.h`
- `inc/hgl/mtl/ShaderBufferSource.h`
- `inc/hgl/mtl/UBOCommon.h`
- `src/ShaderGen/common/UBOCommon.cpp`
- `src/SceneGraph/module/MaterialManager.cpp`
- `src/SceneGraph/VKMaterialDescriptorManager.cpp`

## Goal

Tighten the material/shader descriptor path around `DescriptorSemantic` instead of string-based lookup, while keeping the code compiling at each step.

## Rules

1. First add semantic-driven public APIs.
2. Keep old string-based implementations as private compatibility helpers.
3. Delay internal container replacement until the semantic API is stable.
4. UBO/SSBO first, texture paths later.
5. Keep `LocalToWorld` and `MaterialInstance` as explicit special cases.

## Phase 0 (Done)

Target:

- `inc/hgl/mtl/DescriptorBindingContract.h`

Work:

1. Restore and stabilize `DescriptorSemanticMeta` type definition.
2. Ensure `DescriptorSemanticMetaList` and `GetDescriptorSemanticMeta(...)` are valid for all builtin semantics.
3. Optionally add small builtin helper predicates if needed by later phases.

Exit criteria:

1. `DescriptorSemanticMeta` is a named type again.
2. `GetDescriptorSemanticMeta(...)` can be used as the single semantic metadata entry point.

## Phase 1 (Done)

Target:

- `inc/hgl/shadergen/MaterialCreateInfo.h`
- `src/ShaderGen/MaterialCreateInfo.cpp`
- `src/ShaderGen/MaterialCompiler.cpp`

Work:

1. Move string-based UBO/SSBO APIs to `private` implementation scope.
2. Add public semantic-based overloads:
   - `AddUBO(stage_bits, DescriptorSemantic)`
   - `AddSSBO(stage_bits, DescriptorSemantic)`
   - `AddUBOStruct(stage_bits, DescriptorSemantic)`
   - `AddSSBOStruct(stage_bits, DescriptorSemantic)`
3. Semantic public APIs internally translate through `GetDescriptorSemanticMeta(...)`.
4. Semantic public struct variants perform `AddStruct(...)` first, then call the private string-based implementation.
5. Update `MaterialCompiler` to call the semantic public APIs only.

Exit criteria:

1. External code no longer needs `struct_name` and `name` for builtin UBO/SSBO descriptors.
2. Old string APIs still exist but are implementation details.
3. The codebase still compiles with minimal behavior change.

## Phase 2 (Done)

Target:

- `inc/hgl/common/ShaderDescriptorDef.h`
- `inc/hgl/mtl/ShaderBufferSource.h`
- `inc/hgl/mtl/UBOCommon.h`
- `src/ShaderGen/common/UBOCommon.cpp`

Work:

1. Add `DescriptorSemantic` field to descriptor data objects.
2. Add `DescriptorSemantic` to `ShaderBufferSource`.
3. Ensure descriptor factory paths populate semantic directly.

## Phase 3 (Done)

Target:

- `inc/hgl/shadergen/MaterialDescriptorInfo.h`
- `src/ShaderGen/MaterialDescriptorInfo.cpp`

Work:

1. Add semantic-indexed storage for builtin UBO/SSBO descriptors.
2. Keep string maps temporarily for compatibility and custom descriptors.
3. Add semantic getters/setters alongside string getters.

Suggested concrete tasks:

1. Add fixed semantic slots in `MaterialDescriptorInfo` for builtin UBO/SSBO (array indexed by `DescriptorSemantic`).
2. Populate these slots in `AddUBO/AddSSBO` when semantic is builtin.
3. Add `GetUBO(DescriptorSemantic)` / `GetSSBO(DescriptorSemantic)` APIs while keeping name-based lookups for compatibility.

## Phase 4 (Done)

Target:

- `src/ShaderGen/MaterialCreateInfo.cpp`

Work:

1. Change UBO/SSBO resolve paths to use semantic lookup first.
2. Restrict string lookup to `Unknown`/`Custom` compatibility cases.
3. Replace name-based builtin fetches such as `GetSSBO(SBS_LocalToWorld.name)` with semantic versions.

## Phase 5 (Done)

Target:

- `src/ShaderGen/MaterialCompiler.cpp`

Work:

1. Remove commented legacy struct-name fallback code.
2. Keep compiler logic semantic-only for builtin descriptors.

## Phase 6 (Done)

Target:

- `inc/hgl/shadergen/MaterialDescriptorInfo.h`
- `src/ShaderGen/MaterialDescriptorInfo.cpp`

Work:

1. Consider replacing string-sort-driven UBO/SSBO ordering with fixed semantic queues.
2. Keep descriptor set assignment stable and explicit.

## Machine Switch Checklist

1. Clean rebuild the project (`REBUILD ALL`).
2. Run example: `example/Basic/06b_BasicLitMeshesECS.cpp`.
3. Inspect `run.log` for:
   - `VUID-VkDescriptorSetLayoutCreateInfo-binding-00279`
   - `VUID-VkGraphicsPipelineCreateInfo-layout-07988`
4. If any Vulkan validation remains, dump per-set `name/binding/stageFlags` in `VKMaterialDescriptorManager` and compare against emitted GLSL layout macros.
5. Continue Phase 3 only after runtime validation is clean.
