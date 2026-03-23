# DescriptorSemantic Refactor Plan

## Goal

Tighten the material/shader descriptor path around `DescriptorSemantic` instead of string-based lookup, while keeping the code compiling at each step.

## Rules

1. First add semantic-driven public APIs.
2. Keep old string-based implementations as private compatibility helpers.
3. Delay internal container replacement until the semantic API is stable.
4. UBO/SSBO first, texture paths later.
5. Keep `LocalToWorld` and `MaterialInstance` as explicit special cases.

## Phase 0

Target:

- `inc/hgl/mtl/DescriptorBindingContract.h`

Work:

1. Restore and stabilize `DescriptorSemanticMeta` type definition.
2. Ensure `DescriptorSemanticMetaList` and `GetDescriptorSemanticMeta(...)` are valid for all builtin semantics.
3. Optionally add small builtin helper predicates if needed by later phases.

Exit criteria:

1. `DescriptorSemanticMeta` is a named type again.
2. `GetDescriptorSemanticMeta(...)` can be used as the single semantic metadata entry point.

## Phase 1

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

## Phase 2

Target:

- `inc/hgl/common/ShaderDescriptorDef.h`
- `inc/hgl/mtl/ShaderBufferSource.h`
- `inc/hgl/mtl/UBOCommon.h`
- `src/ShaderGen/common/UBOCommon.cpp`

Work:

1. Add `DescriptorSemantic` field to descriptor data objects.
2. Add `DescriptorSemantic` to `ShaderBufferSource`.
3. Ensure descriptor factory paths populate semantic directly.

## Phase 3

Target:

- `inc/hgl/shadergen/MaterialDescriptorInfo.h`
- `src/ShaderGen/MaterialDescriptorInfo.cpp`

Work:

1. Add semantic-indexed storage for builtin UBO/SSBO descriptors.
2. Keep string maps temporarily for compatibility and custom descriptors.
3. Add semantic getters/setters alongside string getters.

## Phase 4

Target:

- `src/ShaderGen/MaterialCreateInfo.cpp`

Work:

1. Change UBO/SSBO resolve paths to use semantic lookup first.
2. Restrict string lookup to `Unknown`/`Custom` compatibility cases.
3. Replace name-based builtin fetches such as `GetSSBO(SBS_LocalToWorld.name)` with semantic versions.

## Phase 5

Target:

- `src/ShaderGen/MaterialCompiler.cpp`

Work:

1. Remove commented legacy struct-name fallback code.
2. Keep compiler logic semantic-only for builtin descriptors.

## Phase 6

Target:

- `inc/hgl/shadergen/MaterialDescriptorInfo.h`
- `src/ShaderGen/MaterialDescriptorInfo.cpp`

Work:

1. Consider replacing string-sort-driven UBO/SSBO ordering with fixed semantic queues.
2. Keep descriptor set assignment stable and explicit.
