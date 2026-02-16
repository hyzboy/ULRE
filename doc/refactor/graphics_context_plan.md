# Graphics Context Refactor Checklist and Plan

Date: 2026-02-16

## Goals
- Eliminate functional overlap across `RenderFramework`, `RenderContext`, `GraphicsModule`, and `IGraphicsContext`.
- Make dependencies single-direction: `RenderFramework` -> `GraphicsModule` -> `IGraphicsContext`, and `RenderContext` uses `IGraphicsContext` only for resource queries/bindings.
- Keep frame/pass state isolated to `RenderContext`.

## Functional Checklist (No Overlap)

### IGraphicsContext (Resource Domain Only)
- [ ] Create resource APIs only: buffers, textures, shaders, materials, pipeline layouts, descriptor sets.
- [ ] Expose resource managers (Texture/Material/Shader/etc.).
- [ ] Provide read-only device capability info (optional).
- [ ] No frame or render-target state access.
- [ ] No command buffer or pass state.

### GraphicsModule (IGraphicsContext Implementation)
- [ ] Own resource manager lifetimes and initialization.
- [ ] Implement all `IGraphicsContext` methods only.
- [ ] Provide module-level default resources (default textures, materials).
- [ ] No `RenderContext` ownership or pass scheduling.

### RenderContext (Frame/Pass Domain Only)
- [ ] Hold current render target, command buffer, and dynamic pass state.
- [ ] Provide state setters: viewport, scissor, blend, depth, stencil.
- [ ] Provide draw/dispatch commands and binding APIs.
- [ ] Optional: pipeline creation helpers that depend on current RT, but resource creation uses `IGraphicsContext`.
- [ ] No resource creation APIs or resource managers.

### RenderFramework (Composition + Orchestration)
- [ ] Create and wire `GraphicsModule` and `RenderContext`.
- [ ] Inject `IGraphicsContext` into systems and render loop.
- [ ] Handle window, ECS, and frame lifecycle.
- [ ] No resource creation logic beyond delegating to `IGraphicsContext`.

## Audit Checklist
- [ ] Search for `Create*` calls on `RenderContext` and migrate to `IGraphicsContext`.
- [ ] Remove duplicated overloads across `IGraphicsContext` and `GraphicsModule`.
- [ ] Ensure `RenderContext` methods never expose managers.
- [ ] Verify `RenderFramework` only wires dependencies and drives frame loop.
- [ ] Update examples and systems to call `IGraphicsContext` for resources.

## Implementation Plan

### Phase 1: Interface Cleanup
- [ ] Remove any resource creation APIs from `RenderContext`.
- [ ] Ensure `IGraphicsContext` contains all resource creation and manager access.
- [ ] Align `GraphicsModule` implementation strictly with `IGraphicsContext`.

### Phase 2: Call Site Migration
- [ ] Update systems and examples to use `IGraphicsContext` for resources.
- [ ] Keep `RenderContext` usage to rendering state and draw calls only.

### Phase 3: Rendering State Isolation
- [ ] Move any RT-dependent helper functions into `RenderContext`.
- [ ] Make pipeline helpers explicitly depend on current RT.

### Phase 4: Validation
- [ ] Build and resolve remaining compile errors.
- [ ] Run unit or sample app checks if available.
- [ ] Confirm no duplicate responsibilities remain.

## Rough Timeline Estimate
- Phase 1: 0.5-1 day
- Phase 2: 1-2 days
- Phase 3: 0.5-1 day
- Phase 4: 0.5 day

Notes:
- The timeline is a rough estimate and depends on the number of call sites and compilation issues.
