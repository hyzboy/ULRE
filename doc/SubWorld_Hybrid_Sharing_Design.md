# SubWorld Hybrid Sharing Design

## Objective

Implement a hybrid SubWorld execution model:

- Shared rendering systems across the whole scene graph
- Isolated gameplay/tick systems for selected sub-worlds

This extends the current `SharedContext` / `IsolatedContext` split to support mixed ownership by system category.

## Why This Design

The project has two different requirements:

1. Resource-heavy content (`StaticMesh`, `SkeletonMesh`) must avoid system multiplication.
2. Some true sub-worlds (e.g., many houses) still need local gameplay logic and local simulation cadence.

A single global mode cannot satisfy both at scale.

## Design Principles

1. Render is global-first.
2. Gameplay is local-optional.
3. Registration rules must be explicit and enforceable.
4. Runtime behavior must be observable with diagnostics.

## High-Level Architecture

### Context Roles

- Root ECSContext:
  - Owns all shared render systems
  - Owns global render pipelines and GPU submission path
- SubWorld local ECSContext (only when needed):
  - Owns local gameplay/tick systems
  - Does not own shared render systems

### System Ownership Classes

Introduce a system ownership class used during registration:

- `GlobalShared`
- `LocalIsolated`
- `HybridForward` (optional, for bridge systems)

`GlobalShared` means one instance globally.
`LocalIsolated` means per local sub-world context.
`HybridForward` means local update, global render consumption.

## Registration Rules

### Rule A: Render System Rule

Render systems are `GlobalShared` by default.

- They must only be registered in root context.
- Sub-world local contexts must not register global render systems.
- Attempted local registration should be ignored with a debug warning.

### Rule B: Gameplay System Rule

Gameplay/tick systems are `LocalIsolated` by default for true sub-worlds.

- Local contexts can register gameplay systems independently.
- Tick lifecycle (pause/resume/time scale) is local to each sub-world.

### Rule C: Bridge Rule

If local gameplay affects rendering:

- local context updates data/components
- root render systems consume resulting state in global pass

No local render submission pipeline should be created for shared render mode.

## SubWorld Policy Model

Add per-subworld execution policy:

- `render_shared = true` (default)
- `logic_isolated = false` (default)

Valid combinations:

1. `render_shared=true`, `logic_isolated=false`
- Pure asset/scene partition (most subworlds)

2. `render_shared=true`, `logic_isolated=true`
- Preferred hybrid mode for gameplay-heavy houses/rooms

3. `render_shared=false`, `logic_isolated=true`
- Full isolated world (legacy/special only)

4. `render_shared=false`, `logic_isolated=false`
- Invalid (should be rejected)

## Minimal API Sketch

### SubWorldComponent

Add policy APIs:

- `void SetRenderShared(bool v);`
- `bool IsRenderShared() const;`
- `void SetLogicIsolated(bool v);`
- `bool IsLogicIsolated() const;`

Keep existing mode for compatibility, but map it internally:

- `SharedContext` -> render shared + logic not isolated
- `IsolatedContext` -> render not shared + logic isolated

### ECSContext

Add registration gate helpers:

- `bool CanRegisterRenderSystemInThisContext() const;`
- `bool CanRegisterGameplaySystemInThisContext() const;`

Add optional ownership-aware registration entry points:

- `RegisterTickSystemWithScope<T>(SystemOwnership scope, ...)`
- `RegisterRenderSystemWithScope<T>(SystemOwnership scope, ...)`

### Diagnostics

Expose counters:

- `GlobalRenderSystemCount`
- `LocalGameplaySystemCount`
- `RejectedLocalRenderRegistrationCount`

## Execution Flow

### Tick

1. Root tick runs global shared tick systems.
2. For subworlds with `logic_isolated=true`, local tick contexts run local gameplay systems.
3. Local component state is synchronized/visible to shared render input.

### Render

1. Root render executes all render phases once.
2. No duplicated render system execution in local contexts when `render_shared=true`.
3. Local isolated full-render path is only active for explicit full-isolated mode.

## Migration Plan

### Phase H1: Policy Scaffolding

- Add policy flags and API
- Keep old behavior as fallback
- Add debug diagnostics only

### Phase H2: Registration Gating

- Block local shared-render system registration
- Keep gameplay local registration active

### Phase H3: Hybrid Runtime

- Enable `render_shared=true + logic_isolated=true`
- Verify mixed scenarios (many houses)

### Phase H4: Cleanup

- Deprecate direct reliance on old binary mode checks
- Keep compatibility adapter for legacy data

## Acceptance Criteria

1. Render system count remains near-constant as house count grows.
2. Local gameplay systems can run independently per house.
3. No duplicated global render submission pipeline.
4. Legacy isolated behavior still available when explicitly requested.

## Suggested First Implementation Slice

1. Add `render_shared` and `logic_isolated` policy flags.
2. Add registration gate for local render systems.
3. Add diagnostics and logs for rejected local render registration.
4. Run stress case:
- many houses with local gameplay enabled
- one global render path

## Risks and Mitigations

- Risk: hidden dependencies in legacy systems expecting local render registration.
  - Mitigation: add temporary warnings with system names and context names.

- Risk: data sync mismatch between local logic and global render inputs.
  - Mitigation: define explicit sync point before render collect phase.

- Risk: mixed-mode complexity.
  - Mitigation: enforce strict policy matrix and reject invalid combinations early.
