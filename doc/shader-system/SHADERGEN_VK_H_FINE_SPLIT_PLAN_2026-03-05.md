# ShaderGen Decoupling: VK.h Fine-Grained Split Plan (2026-03-05)

## 1. Goal and Scope

This document defines a focused split plan for `inc/hgl/vk/VK.h` to support:

1. ShaderGen independent compilation.
2. Preserving Vulkan SDK dependency (`vulkan/vulkan.h`) as allowed.
3. Removing ShaderGen dependency on engine renderer umbrella headers.

This is a refinement plan for type/enum/struct extraction only. It does not include runtime behavior rewrites.

### 1.1 Progress Update (runtime sync)

已落地（当前机器最新代码）：

1. 已创建首批 shared 定义头：
   - `ShaderStageDef.h`
   - `DescriptorSetTypeDef.h`
   - `VertexAttribDef.h`
   - `VertexInputDef.h`
   - `InterpolationDef.h`
   - `TextureSamplerTypeDef.h`
   - `PrimitiveTypeDef.h`
   - `ShaderDescriptorDef.h`
2. 已完成 Batch-1 replacement matrix 的核心公共头替换：
   - `ShaderCreateInfo.h`
   - `ShaderDescriptorInfo.h`
   - `MaterialCreateInfo.h`
   - `MaterialDescriptorInfo.h`
   - `FixedMaterialDef.h`
   - `ShaderComposition.h`
3. 已完成旧 vk 头兼容转发（wrapper）以避免双定义冲突：
   - `VKDescriptorSetType.h`
   - `VertexAttrib.h`
   - `VKVertexInputAttribute.h`
   - `VKInterpolation.h`
   - `VKPrimitiveType.h`
   - `VKTextureType.h`
   - `VKSamplerType.h`
   - `VKShaderDescriptor.h`
   - `VKShaderDescriptorSet.h`
4. `VK.h` 中 `ShaderStage` 已改为复用 shared 定义，避免重复定义。
5. 边界结果：`inc/hgl/shadergen/**` 中已无 `#include <hgl/vk/VK.h>`，当前仅剩 `ShaderComposition_Examples.h` 对 `VKRenderAssign.h` 的直连（属于 Batch-2 非目标）。

2026-03-05 追加进展：

1. 已新增 `inc/hgl/graph/shared/RenderAssignDef.h`，承载 `Assign::TransformID/MaterialInstanceID` 纯定义。
2. `inc/hgl/vk/VKRenderAssign.h` 已改为兼容转发 wrapper（仅 include shared def）。
3. ShaderGen 侧（`inc/hgl/shadergen` + `src/ShaderGen/**`）已完成 `VKRenderAssign.h -> graph/shared/RenderAssignDef.h` include 替换。
4. `tools/shadergen_boundary_allowlist.txt` 已清空，边界守卫在 0 allowlist 例外下通过。

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

推荐使用自动化边界守卫脚本（与上面 grep 互补）：

```powershell
python tools/check_shadergen_boundary.py --repo-root .
```

白名单已外置（2026-03-05）：

1. 文件：`tools/shadergen_boundary_allowlist.txt`
2. 语义：仅允许“精确 include 路径”过渡例外（当前为 `hgl/vk/VKRenderAssign.h`）
3. 后续 Batch-2 移除该依赖时，只需更新该白名单并确保检查通过。

已接入 CMake（2026-03-05）：

```powershell
cmake --build --preset windows-msvc-debug --config Debug --target check_shadergen_boundary
ctest --preset windows-msvc-debug -C Debug -R check_shadergen_boundary
```

说明：若环境缺少 Python3 解释器，CMake 会给出 warning 并跳过该 target。

当前规则（2026-03-05）：

1. 扫描范围：`inc/hgl/shadergen/**` 公共头。
2. 禁止依赖：`hgl/vk/VK.h`、`hgl/graph/module/*`、`hgl/graph/core/*`、以及 `VKDevice/VKInstance/Swapchain/RenderTarget/RenderPass/pipeline` 等运行时头。
3. 允许过渡白名单：`hgl/vk/VKRenderAssign.h`（Batch-2 目标，后续应移除）。

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

## 12. Minimal First-Batch Change List (File-Level)

This section is the executable checklist for the first landing batch. Goal: remove direct `VK.h` path leakage from ShaderGen public headers before touching deep implementation behavior.

### 12.1 Batch-1 Scope

1. Only public headers and pure definition extraction.
2. No runtime logic change.
3. No descriptor binding algorithm change.

### 12.2 New Shared Headers to Create in Batch-1

1. `inc/hgl/graph/shared/ShaderStageDef.h`
2. `inc/hgl/graph/shared/DescriptorSetTypeDef.h`
3. `inc/hgl/graph/shared/VertexAttribDef.h`
4. `inc/hgl/graph/shared/VertexInputDef.h`
5. `inc/hgl/graph/shared/InterpolationDef.h`
6. `inc/hgl/graph/shared/TextureSamplerTypeDef.h`
7. `inc/hgl/graph/shared/PrimitiveTypeDef.h`
8. `inc/hgl/graph/shared/ShaderDescriptorDef.h`

### 12.3 Public Header Replacement Matrix (Must Do)

| File | Current include(s) | Replace to | Notes |
|---|---|---|---|
| `inc/hgl/shadergen/ShaderCreateInfo.h` | `VertexAttrib.h`, `VK.h`, `VKInterpolation.h`, `VKDescriptorSetType.h` | `VertexAttribDef.h`, `ShaderStageDef.h`, `InterpolationDef.h`, `DescriptorSetTypeDef.h` | Highest priority, used transitively everywhere |
| `inc/hgl/shadergen/ShaderDescriptorInfo.h` | `VK.h`, `VKShaderDescriptor.h`, `VKVertexInputAttribute.h`, `VKDescriptorSetType.h` | `ShaderStageDef.h`, `ShaderDescriptorDef.h`, `VertexInputDef.h`, `DescriptorSetTypeDef.h` | Remove all direct vk umbrella coupling |
| `inc/hgl/shadergen/MaterialCreateInfo.h` | `VKTextureType.h`, `VKSamplerType.h` | `TextureSamplerTypeDef.h` | Keep function signatures unchanged |
| `inc/hgl/shadergen/MaterialDescriptorInfo.h` | `VKShaderDescriptorSet.h` | `ShaderDescriptorDef.h` | If `ShaderDescriptorSet` needed, add minimal shared set struct |
| `inc/hgl/shadergen/FixedMaterialDef.h` | `VertexAttrib.h` | `VertexAttribDef.h` | Must remain POD-only |
| `inc/hgl/shadergen/ShaderComposition.h` | `VKPrimitiveType.h` | `PrimitiveTypeDef.h` | No behavior change |

### 12.4 Compatibility Wrapper Updates (Same PR)

Keep old vk headers source-compatible for one transition window:

1. `inc/hgl/vk/VKDescriptorSetType.h` includes `DescriptorSetTypeDef.h`.
2. `inc/hgl/vk/VertexAttrib.h` includes `VertexAttribDef.h` and `VertexInputDef.h`.
3. `inc/hgl/vk/VKVertexInputAttribute.h` includes `VertexInputDef.h` and `InterpolationDef.h`.
4. `inc/hgl/vk/VKTextureType.h` and `inc/hgl/vk/VKSamplerType.h` include `TextureSamplerTypeDef.h`.
5. `inc/hgl/vk/VKPrimitiveType.h` includes `PrimitiveTypeDef.h`.
6. `inc/hgl/vk/VKShaderDescriptor.h` and `inc/hgl/vk/VKShaderDescriptorSet.h` include `ShaderDescriptorDef.h`.

### 12.5 Batch-1 Explicit Non-Goals

1. Do not split `VKRenderAssign.h` in this batch.
2. Do not replace `VK_*` shader stage constants in `.cpp` files yet.
3. Do not migrate contract mirror/request/result conversion logic in this batch.

### 12.6 Build and Boundary Gate for Batch-1

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --config Debug --parallel 16
```

Boundary check:

```powershell
Get-ChildItem inc/hgl/shadergen -Recurse -File -Include *.h,*.hpp |
   Select-String '#include\s*<hgl/vk/VK.h>'
```

Pass condition:

1. No hit from boundary check.
2. Full debug build succeeds.
3. No signature change visible to existing ShaderGen callers.

### 12.7 Batch-2 Preview (for next PR)

1. Split `VKRenderAssign.h` into shared assign names/types and renderer-only Vulkan format map.
2. Replace `VKRenderAssign.h` usage in `src/ShaderGen/3d/*.h` and `src/ShaderGen/3d/*.cpp`.
3. Replace `VKRenderAssign.h` usage in `src/ShaderGen/common/MFCommon.h`.
4. Replace `VKRenderAssign.h` usage in `src/ShaderGen/common/MFSkyLight.h`.
5. Move `VK_DESCRIPTOR_TYPE_*` mapping glue to adapter boundary.

## 13. Symbol-Level Migration Map (PR-1)

This table is the concrete symbol migration baseline for PR-1. Keep names unchanged where possible to minimize caller impact.

### 13.1 `ShaderStageDef.h`

Source symbols now in `VK.h`:

1. `enum class ShaderStage:uint32_t`
2. combined values inside `ShaderStage`:
   - `VertexFragment`
   - `VertexGeometryFragment`
   - `Tessellation`
   - `TaskMesh`
   - `TaskMeshFragment`
   - `AllGraphics`

Target:

1. Move full enum definition and combined values to `inc/hgl/graph/shared/ShaderStageDef.h`.
2. `VK.h` keeps compatibility by including `ShaderStageDef.h`.

### 13.2 `DescriptorSetTypeDef.h`

Source symbols now in `VKDescriptorSetType.h`:

1. `enum class DescriptorSetType`
2. `DESCRIPTOR_SET_TYPE_COUNT`
3. `DescriptSetTypeName[]`
4. `GetDescriptorSetTypeName(...)`
5. `GetDescriptorSetType(...)`

Target:

1. Move these to `inc/hgl/graph/shared/DescriptorSetTypeDef.h`.
2. Keep string compatibility mapping in the shared header for now.

### 13.3 `VertexAttribDef.h`

Source symbols now in `VertexAttrib.h`:

1. `enum class VertexInputGroup`
2. `VertexInputGroupName[]`
3. `GetVertexInputGroupName(...)`
4. `enum class VertexAttribBaseType`
5. `struct VertexAttribType` and alias `VAType`
6. `VAT_*` constexpr constants
7. `VERTEX_ATTRIB_NAME_MAX_LENGTH`
8. `namespace VertexAttribName` and alias `VAN`
9. `ParseVertexAttribType(...)`
10. `GetVertexAttribName(...)`

Target:

1. Move to `inc/hgl/graph/shared/VertexAttribDef.h`.
2. Keep function declarations in shared header; function definitions can remain in existing cpp implementation.

### 13.4 `VertexInputDef.h`

Source symbols now in `VKVertexInputAttribute.h`:

1. `enum class VertexInputRate:uint8_t`
2. `struct VertexInputAttribute` / alias `VIA`
3. `using VIAList`
4. `struct VertexInputAttributeArray` / alias `VIAArray`
5. `GetShaderAttributeTypename(...)`

Target:

1. Move POD types and aliases to `inc/hgl/graph/shared/VertexInputDef.h`.
2. Keep Vulkan-format-specific helper declarations (`GetVulkanFormat`) in vk-side header.

### 13.5 `InterpolationDef.h`

Source symbols now in `VKInterpolation.h`:

1. `enum class Interpolation:uint8`
2. `InterpolationName[]`
3. `GetInterpolationName(...)`

Target:

1. Move to `inc/hgl/graph/shared/InterpolationDef.h`.

### 13.6 `TextureSamplerTypeDef.h`

Source symbols now in `VKTextureType.h` and `VKSamplerType.h`:

1. `enum class TextureType`
2. `TextureTypeName[]`
3. `GetTextureTypeName(...)`
4. `ParseTextureType(...)`
5. `enum class SamplerType`
6. `SamplerTypeName[]`
7. `GetSamplerTypeName(...)`
8. `ParseSamplerType(...)`

Target:

1. Move enums and name/parse helpers to `inc/hgl/graph/shared/TextureSamplerTypeDef.h`.
2. Keep `VkImageViewType` mapping arrays on vk-side for PR-1 (reduce risk).

### 13.7 `PrimitiveTypeDef.h`

Source symbols now in `VKPrimitiveType.h`:

1. `enum class PrimitiveType:uint32`
2. `GetPrimName(...)`
3. `ParsePrimitiveType(...)`
4. `CheckGeometryShaderIn(...)`
5. `CheckGeometryShaderOut(...)`

Target:

1. Move enum and declarations to `inc/hgl/graph/shared/PrimitiveTypeDef.h`.
2. Keep implementations in current cpp for compatibility.

### 13.8 `ShaderDescriptorDef.h`

Source symbols now in `VKShaderDescriptor.h` (+ set wrapper in `VKShaderDescriptorSet.h`):

1. `DESCRIPTOR_NAME_MAX_LENGTH`
2. `struct ShaderDescriptor`
3. `struct UBODescriptor`
4. `struct SSBODescriptor`
5. `struct TextureDescriptor`
6. `struct TextureSamplerDescriptor`
7. `struct ShaderObjectData`
8. `struct ConstValueDescriptor`
9. `struct SubpassInputDescriptor`
10. `struct ShaderPushConstant`

Target:

1. Move descriptor structs to `inc/hgl/graph/shared/ShaderDescriptorDef.h`.
2. Keep `VkDescriptorType` field as-is (allowed Vulkan SDK dependency).
3. Keep `ShaderDescriptorSet` move optional in PR-1; include if compile boundary requires it.

## 14. PR-1 Execution Checklist (Step-by-Step)

### 14.1 Preparation

1. Create a branch dedicated to split-only work.
2. Do not mix unrelated formatting or behavior edits.

### 14.2 Add shared headers

1. Create all headers listed in 12.2.
2. Copy symbols according to section 13.
3. Keep API names stable in PR-1.

### 14.3 Add compatibility includes in old vk headers

1. Old vk headers include new shared headers.
2. Keep old include paths valid for callers outside ShaderGen.

### 14.4 Replace ShaderGen public header includes

1. Apply replacement matrix in 12.3 exactly.
2. Ensure `inc/hgl/shadergen/**` no longer includes `VK.h` directly.

### 14.5 Compile and boundary gate

1. Run build commands from 12.6.
2. Run boundary check from 12.6.
3. If failed, fix include graph before fixing behavior.

### 14.6 Commit policy

1. Commit-1: shared headers + compatibility wrappers.
2. Commit-2: ShaderGen public include replacement.
3. Commit-3: compile fixes only (no extra refactor).

## 15. Fast Triage Rules on New Machine

1. Error `unknown type ShaderStage`:
   - Missing `ShaderStageDef.h` include or include order issue.
2. Error `unknown type DescriptorSetType`:
   - `DescriptorSetTypeDef.h` not included in transitive path.
3. Error in `ShaderDescriptor*` symbols:
   - `ShaderDescriptorDef.h` not visible or incomplete symbol migration.
4. Error in `VAT_*` or `VAType`:
   - `VertexAttribDef.h` not included where old `VertexAttrib.h` was removed.
5. Boundary check still finds `VK.h`:
   - Inspect `ShaderCreateInfo.h` and `ShaderDescriptorInfo.h` first.
