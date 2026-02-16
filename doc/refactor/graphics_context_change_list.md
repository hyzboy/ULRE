# Graphics Context Refactor: Specific Change List

Date: 2026-02-16

This list translates the current vs target design into concrete edits by file and interface.

## 1) RenderContext: remove manager ownership and getters

### Files
- inc/hgl/graph/render/RenderContext.h
- src/SceneGraph/render/RenderContext.cpp

### Changes
- Remove manager members: `TextureManager*`, `BufferManager*`, `MaterialManager*`, `SamplerManager*`, `RenderPassManager*`, `GeometryManager*`, `PrimitiveManager*`.
- Remove manager getters: `GetTextureManager()`, `GetBufferManager()`, `GetMaterialManager()`, `GetSamplerManager()`, `GetRenderPassManager()`, `GetGeometryManager()`, `GetPrimitiveManager()`.
- Remove `VulkanDevice* device` member and `GetDevice()` if not used by render-state APIs.
- Shrink constructor signature to only what the render state needs. Recommended:
  - `RenderContext(IGraphicsContext* graphics_ctx)` or
  - `RenderContext()` with setter-only injection of `IGraphicsContext*`.
- Keep only render-state fields: `current_render_cmd_buf`, `current_render_target`, and `graphics_context`.
- Keep `CreatePipeline(...)` if it is strictly tied to the current render target. If it needs access to resource managers, route those through `graphics_context` instead of direct manager members.
- Remove unused includes for managers and device types after member removal.

## 2) IGraphicsContext: remove frame/pass state overlap

### File
- inc/hgl/graph/core/GraphicsContext.h

### Changes
- Ensure no render-target or frame state appears in the interface.
- If `GetDefaultRenderPass()` is based on swapchain or current RT, move it out of `IGraphicsContext`.
  - Preferred replacement: `RenderFramework::GetDefaultRenderPass()` or `RenderContext::GetCurrentRenderTarget()->GetRenderPass()`.
- Keep only resource creation + manager access + device capability queries.

## 3) GraphicsModule: keep it as pure IGraphicsContext impl

### Files
- inc/hgl/graph/core/GraphicsModule.h
- src/SceneGraph/core/GraphicsModule.cpp

### Changes
- Remove `SetDefaultRenderPass()` and `default_render_pass` if default pass is frame/RT-derived.
- Remove `SetLegacyRenderFramework()` and `legacy_rf` to prevent back-references.
- If default resources are needed, initialize them inside `GraphicsModule` without referencing `RenderFramework`.
- Ensure all methods map 1:1 to `IGraphicsContext` with no extra render-state logic.

## 4) RenderFramework: reduce public surface and wire contexts

### Files
- inc/hgl/graph/render/RenderFramework.h
- src/SceneGraph/render/RenderFramework.cpp

### Changes
- Minimize public manager getters (`GetTextureManager()` etc.) to avoid bypassing `IGraphicsContext`.
- Expose `IGraphicsContext*` explicitly (from `GraphicsModule`) as the primary resource access.
- Construct `RenderContext` with only the minimal dependencies (inject `IGraphicsContext*`).
- Remove any direct calls that set default render pass on `GraphicsModule` if that API is removed.
- Ensure ECS systems get `IGraphicsContext` for resources and `RenderContext` for rendering state.

## 5) Call-site cleanup (remaining usages)

### Scope
- Systems, examples, and utilities that still call `RenderFramework::Get*Manager()` or `RenderContext::Get*Manager()`.

### Changes
- Replace manager access with `IGraphicsContext` calls.
- Replace any `GetDefaultRenderPass()` usage with `RenderFramework::GetDefaultRenderPass()` or the current RT’s render pass from `RenderContext`.

## 6) Build verification

- Rebuild after interface cleanup.
- Fix any compile errors by updating remaining call sites to the new boundaries.

## Output Expectations
- `RenderContext` is purely a render-state object.
- `IGraphicsContext` is the only resource creation entry point.
- `GraphicsModule` is a clean implementation of `IGraphicsContext`.
- `RenderFramework` is a composition root, not a resource API.
