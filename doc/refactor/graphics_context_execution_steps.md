# Graphics Context Refactor: Executable Steps

Date: 2026-02-16

This is a step-by-step execution plan that can be split into small PRs or commits.

## Step 1: RenderContext API Shrink

### Files
- inc/hgl/graph/render/RenderContext.h
- src/SceneGraph/render/RenderContext.cpp

### Tasks
- Remove manager members and getters from `RenderContext`.
- Remove `VulkanDevice*` member if it is unused by render-state APIs.
- Reduce constructor parameters to only what the render-state needs.
- Keep `IGraphicsContext*` as an injected pointer for optional resource lookups.
- Keep render-state members only: current RT, current command buffer.
- Keep `CreatePipeline(...)` only if it is RT-dependent. If it needs managers, route via `IGraphicsContext`.
- Update includes to remove unused manager headers.

### Validation
- Build to surface call-site breaks after the signature changes.

## Step 2: RenderFramework Wiring Update

### Files
- inc/hgl/graph/render/RenderFramework.h
- src/SceneGraph/render/RenderFramework.cpp

### Tasks
- Update `RenderContext` construction with the new minimal signature.
- Inject `IGraphicsContext*` into `RenderContext` after `GraphicsModule` creation.
- Remove any `RenderContext` construction arguments that are managers.
- Keep `RenderFramework` as the only place that wires `GraphicsModule` + `RenderContext`.

### Validation
- Build to catch any incorrect constructor calls.

## Step 3: IGraphicsContext Boundary Cleanup

### Files
- inc/hgl/graph/core/GraphicsContext.h

### Tasks
- Verify no render-target or frame state appears in the interface.
- If `GetDefaultRenderPass()` is RT-dependent, move it out of `IGraphicsContext`.
- Ensure only resource creation + manager access + device queries remain.

### Validation
- Build to surface missing method references.

## Step 4: GraphicsModule Simplification

### Files
- inc/hgl/graph/core/GraphicsModule.h
- src/SceneGraph/core/GraphicsModule.cpp

### Tasks
- Remove `SetLegacyRenderFramework()` and `legacy_rf` unless strictly required.
- Remove `SetDefaultRenderPass()` and `default_render_pass` if default pass is RT-derived.
- Ensure all methods are a 1:1 implementation of `IGraphicsContext`.

### Validation
- Build and update call sites if those setters were referenced.

## Step 5: Call-site Migration Cleanup

### Scope
- Systems/examples still using `RenderContext::Get*Manager()` or `RenderFramework::Get*Manager()`.

### Tasks
- Replace manager access with `IGraphicsContext` access.
- Replace default render pass access with `RenderFramework::GetDefaultRenderPass()` or `RenderContext::GetCurrentRenderTarget()->GetRenderPass()`.

### Validation
- Build, then run a representative sample app if available.

## Step 6: Final Verification

### Tasks
- Full build (Debug/Release as needed).
- Run existing samples or smoke tests.
- Confirm no functional overlap remains in the four core classes.

## Suggested PR/Commit Split

1) RenderContext signature + wiring changes (Steps 1-2)
2) IGraphicsContext boundary cleanup (Step 3)
3) GraphicsModule simplification (Step 4)
4) Call-site migration sweep (Step 5)
5) Final verification (Step 6)
