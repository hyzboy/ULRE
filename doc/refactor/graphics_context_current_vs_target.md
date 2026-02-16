# Graphics Context Refactor: Current vs Target

Date: 2026-02-16

## Scope
Classes: `RenderFramework`, `RenderContext`, `GraphicsModule`, `IGraphicsContext`.

## IGraphicsContext

### Current (Observed)
- Resource creation: materials, material instances, buffers (UBO/SSBO/INBO), index buffers, geometry, primitives, pipelines, textures, samplers, text rendering.
- Manager access: render pass, texture, material, buffer, sampler, geometry, primitive.
- Device access: `VulkanDevice`, `VulkanDevAttr`, `VulkanPhyDevice`, `VkDevice`.
- Helper templates: `CreateUBOAccessor`, `CreateMaterialInstance` (templated data overloads).

### Target (No Overlap)
- Keep resource creation and resource manager access only.
- Keep device capability access (read-only) if needed.
- No frame/pass state, no render target, no command buffer state.
- Avoid API growth that depends on current render target or frame state.

### Gap / Actions
- Ensure any RT-dependent pipeline helpers move to `RenderContext`.
- Keep only resource-centric helpers (e.g., `CreateUBOAccessor`).

## GraphicsModule

### Current (Observed)
- Implements all `IGraphicsContext` resource creation methods.
- Exposes module manager accessors for texture/material/buffer/sampler/geometry/primitive/render pass.
- Holds `default_render_pass` and a `legacy_rf` pointer.

### Target (No Overlap)
- Pure `IGraphicsContext` implementation.
- Own and initialize resource managers and default resources.
- No dependency on `RenderContext` or frame state.

### Gap / Actions
- Consider isolating `default_render_pass` behind `IGraphicsContext` only if it is treated as resource-level default.
- Eliminate or quarantine `legacy_rf` usage to avoid back-reference to framework.

## RenderContext

### Current (Observed)
- Holds device and manager pointers (texture, buffer, material, sampler, render pass, geometry, primitive).
- Holds current render target and command buffer.
- Provides `CreatePipeline` (depends on current RT + pipeline data).
- Provides manager accessors and `GetGraphicsContext`.

### Target (No Overlap)
- Hold only frame/pass state: current RT, command buffer, dynamic state (viewport/scissor, bind state, etc.).
- Provide render commands: bind, draw, dispatch.
- Optional: RT-dependent pipeline helper should be clearly tied to current RT.
- No direct resource manager accessors.

### Gap / Actions
- Remove manager pointers and manager getters from `RenderContext`.
- Keep only `IGraphicsContext` pointer for resource access when needed.
- Consider moving device pointer into `IGraphicsContext` or a narrow `RenderDeviceContext` if needed.

## RenderFramework

### Current (Observed)
- Owns window, Vulkan instance/device.
- Owns all major managers (render pass, texture, render target, material, buffer, sampler, geometry, primitive).
- Owns `SwapchainModule` and `RenderContext`.
- Exposes manager getters and device getters.
- Provides default render pass via swapchain target.

### Target (No Overlap)
- Composition root only: wires `GraphicsModule`, `RenderContext`, `SwapchainModule`, ECS, and window.
- No direct resource creation APIs; resource creation goes through `IGraphicsContext`.
- Provide access to `IGraphicsContext` and `RenderContext` only as needed.

### Gap / Actions
- Minimize manager getters on `RenderFramework` to avoid bypassing `IGraphicsContext`.
- Treat `RenderFramework` as wiring + lifecycle controller.

## Dependency Direction (Target)
- `RenderFramework` -> `GraphicsModule` -> `IGraphicsContext`
- `RenderFramework` -> `RenderContext` (inject `IGraphicsContext`)
- Systems/examples -> `IGraphicsContext` for resources; -> `RenderContext` for rendering.

## Quick Audit Targets
- Search for `RenderFramework` manager getters in systems/examples and migrate to `IGraphicsContext`.
- Remove `RenderContext` manager getters and migrate usage.
- Confirm `RenderContext` only handles render state, not resource creation.
