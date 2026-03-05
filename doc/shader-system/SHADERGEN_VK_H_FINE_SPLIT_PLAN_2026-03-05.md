# ShaderGen Decoupling: VK.h Fine-Grained Split Plan (2026-03-05)

## 1. Goal and Scope

This document defines a focused split plan for `inc/hgl/vk/VK.h` to support:

1. ShaderGen independent compilation.
2. Preserving Vulkan SDK dependency (`vulkan/vulkan.h`) as allowed.
3. Removing ShaderGen dependency on engine renderer umbrella headers.

This is a refinement plan for type/enum/struct extraction only. It does not include runtime behavior rewrites.

## 2. Why VK.h Must Be Split First

`VK.h` is currently a mixed umbrella header that contains:

1. Vulkan SDK wrappers and utility overloads.
2. Engine-level forward declarations (device, swapchain, render target, etc.).
3. Core enum/type definitions used by ShaderGen (`ShaderStage`, descriptor-related enums).
4. Unrelated render pipeline definitions.

As long as ShaderGen includes any header that drags in `VK.h`, compile-time coupling to renderer internals remains high.

## 3. Current Dependency Facts (for prioritization)

From current ShaderGen usage scan:

1. High-frequency symbols:
   - `DescriptorSetType` (very high)
   - `ShaderStage` (very high)
   - `DescriptorKind` (high)
   - `VertexInputRate` and `VertexInputGroup` (high)
2. Highest include pressure:
   - `VKRenderAssign.h`
   - `VertexAttrib.h`
   - `VKVertexInputAttribute.h`
   - `VKDescriptorSetType.h`
   - `VK.h`

Conclusion: split should start from definitions consumed by ShaderGen public headers, then replace umbrella dependencies transitively.

## 4. Target Split Architecture

Create a shared lightweight layer for pure definitions:

1. `inc/hgl/graph/shared/ShaderStageDef.h`
2. `inc/hgl/graph/shared/DescriptorSetTypeDef.h`
3. `inc/hgl/graph/shared/DescriptorTypeDef.h`
4. `inc/hgl/graph/shared/VertexAttribDef.h`
5. `inc/hgl/graph/shared/VertexInputDef.h`
6. `inc/hgl/graph/shared/InterpolationDef.h`
7. `inc/hgl/graph/shared/TextureSamplerTypeDef.h`
8. `inc/hgl/graph/shared/PrimitiveTypeDef.h`
9. `inc/hgl/graph/shared/ShaderDescriptorDef.h`

Rules:

1. Shared `*Def.h` files may include only:
   - C/C++ standard headers
   - `vulkan/vulkan.h` through `VKNamespace.h` if needed
   - generic utility headers (`EnumUtil`, string helpers, POD helpers)
2. Shared files must not include renderer object headers or forward declare renderer runtime classes unless strictly needed for POD signatures.
3. No device/swapchain/renderpass/pipeline forward declarations in shared type headers.

## 5. VK.h Split Mapping (Precise)

### 5.1 Move out of VK.h into shared defs

1. `enum class ShaderStage` -> `ShaderStageDef.h`
2. `enum class DescriptorType` -> `DescriptorTypeDef.h`
3. Descriptor range constants currently tied to descriptor categories -> `DescriptorTypeDef.h`
4. Any shader-stage helper constants that are pure values -> `ShaderStageDef.h`

### 5.2 Keep in VK.h (or move to renderer-only headers)

1. Renderer runtime forward declarations:
   - `VulkanInstance`, `VulkanPhyDevice`, `VulkanDevice`, `Swapchain`, `RenderTarget`, etc.
2. Device/pipeline/render-target specific declarations.
3. Utility operators not needed by ShaderGen boundary.

### 5.3 Optional follow-up extraction

1. `DynamicState` can be moved to `DynamicStateDef.h` only if ShaderGen actually consumes it.
2. If not consumed by ShaderGen, keep it renderer-side.

## 6. Concrete Refactor Sequence

### Phase A: Introduce shared definition headers

1. Add all `inc/hgl/graph/shared/*Def.h` skeletons.
2. Move pure enums/constexpr/structs from vk headers to shared headers.
3. Keep temporary compatibility includes in old vk headers:
   - old header includes new shared header
   - no behavior change in first commit

Deliverable:

1. Project still builds unchanged.
2. Existing includes remain source-compatible.

### Phase B: Replace ShaderGen public header includes

Primary targets:

1. `inc/hgl/shadergen/ShaderCreateInfo.h`
2. `inc/hgl/shadergen/ShaderDescriptorInfo.h`
3. `inc/hgl/shadergen/MaterialCreateInfo.h`
4. `inc/hgl/shadergen/FixedMaterialDef.h`
5. `inc/hgl/shadergen/ShaderComposition.h`

Action:

1. Replace `#include <hgl/vk/...>` with `#include <hgl/graph/shared/...Def.h>` where possible.
2. Ensure no include path from ShaderGen public headers reaches `VK.h`.

Deliverable:

1. `inc/hgl/shadergen/**` has no direct include of `VK.h`.
2. ShaderGen public API remains equivalent.

### Phase C: Split `VKRenderAssign.h` dependency

Problem:

1. This header is heavily included by ShaderGen material code.
2. It mixes semantic names and Vulkan format constants.

Action:

1. Extract pure naming/type constants used by ShaderGen into shared defs.
2. Keep Vulkan format mapping in renderer-side `VKRenderAssign.h`.
3. ShaderGen side includes only shared assign definitions.

Deliverable:

1. ShaderGen no longer needs renderer-side assign header.

### Phase D: Cleanup and hardening

1. Remove now-unused includes from ShaderGen sources.
2. Add include-boundary checks:
   - Fail CI if `inc/hgl/shadergen/**` includes `<hgl/vk/VK.h>`
3. Keep temporary compatibility wrappers for one transition window.

## 7. Acceptance Criteria

All must pass:

1. Build pass: `windows-msvc-debug` and `windows-msvc-release`.
2. ShaderGen public headers do not include renderer umbrella headers.
3. ShaderGen public headers do not expose renderer runtime classes.
4. Material preset compile tests pass.
5. Contract request/result parity tests pass.

## 8. Suggested Validation Commands (new machine)

Run from repo root:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --config Debug --parallel 16
```

Boundary checks:

```powershell
Get-ChildItem inc/hgl/shadergen -Recurse -File -Include *.h,*.hpp |
  Select-String '#include\s*<hgl/vk/VK.h>'

Get-ChildItem inc/hgl/shadergen -Recurse -File -Include *.h,*.hpp |
  Select-String '#include\s*<hgl/vk/'
```

Expected:

1. First command returns no result.
2. Second command only returns explicitly approved transitional includes (if any).

## 9. Workload Estimate for This Variant

For "share pure definitions first, no complex behavior migration":

1. Implementation: 4 to 8 person-days.
2. Build and stabilization: 1 to 3 person-days.
3. Documentation and migration notes: 0.5 to 1 person-day.

Total: approximately 1 to 2 weeks for one engineer.

## 10. PR Breakdown Recommendation

1. PR-1: Add shared `*Def.h` and compatibility includes.
2. PR-2: Replace ShaderGen public header includes.
3. PR-3: Split assign definitions and remove `VKRenderAssign.h` from ShaderGen path.
4. PR-4: Cleanup, CI checks, and transitional include removal.

This breakdown keeps each PR reviewable and reduces regression risk.

## 11. Notes for Cross-Machine Execution

1. Use this file as the source of truth for sequence and acceptance.
2. Do not mix runtime refactor tasks into this split batch.
3. If build failures occur, fix include boundary first, then symbol mapping, then behavior.
4. Keep a temporary compatibility window, but enforce boundary checks early.
