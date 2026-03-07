# SubWorld Shared-Context Refactor Plan

## Goal

Replace the current "one SubWorld = one ECSContext + one system set" design with a shared-context subscene design, so system count does not grow with SubWorld count.

This addresses the scaling issue where many `StaticMesh` and `SkeletonMesh` instances are organized as SubWorlds.

## Problem Summary

Current behavior:

- `SubWorldComponent` creates a child `World` and child `ECSContext`.
- Child context registers tick/render systems again.
- With many SubWorld instances, system instances scale linearly and become a bottleneck.

Target behavior:

- Keep one main `ECSContext` as the only scheduler/system container.
- Treat SubWorld as a logical subscene group (data partition), not a system container.

## Architecture Direction

### Mode Strategy

Introduce explicit runtime modes for compatibility:

- `SharedContext` (new default):
  - No child `ECSContext` creation.
  - All entities live in parent context.
  - All systems are shared and run once per frame.
- `IsolatedContext` (legacy compatibility):
  - Preserve old behavior only for rare strict-isolation scenarios.

### SubWorld New Semantics

`SubWorldComponent` becomes a subscene controller with:

- `mode` (`SharedContext` / `IsolatedContext`)
- `subscene_id`
- `root_entity_id`
- state flags: `paused`, `tick_enabled`, `render_enabled`

In `SharedContext` mode, do not create `sub_world` by default.

## Execution Plan

### 1. Define Boundaries and Compatibility

- Finalize mode defaults (`SharedContext` as default).
- Restrict `IsolatedContext` to explicit opt-in use cases.
- Document that system count must be independent of subscene count.

Deliverable:

- Design note and migration constraints.

### 2. Refactor `SubWorldComponent` Data Model

- Add mode + subscene identity fields.
- Keep existing asset path/import fields.
- In shared mode, stop constructing child `World`/`ECSContext`.

Deliverable:

- Compilable updates in `SubWorldComponent.h/.cpp`.

### 3. Lifecycle and Asset Instantiation Changes

- `OnAttach()` in shared mode:
  - Import entities into parent context.
  - Assign `subscene_id` for imported entities.
  - Track and set `root_entity_id`.
- `OnDetach()` in shared mode:
  - Clear imported entities by tracked IDs/subscene.
  - No child-context shutdown path.

Deliverable:

- Stable attach/detach behavior without implicit system creation.

### 4. Unify System Installation (Single Registration)

- Remove subworld-local registrations such as:
  - `RegisterTickSystem<CameraSystem>()`
  - `RegisterTickSystem<BoundingBoxUpdateSystem>()`
  - `RegisterRenderSystem<RenderPrimitiveCollectSystem>()`
- Ensure systems are installed once on parent context using existing group mechanisms:
  - `IsSystemGroupInstalled`
  - `MarkSystemGroupInstalled`

Deliverable:

- One global system set per context.

### 5. Add Runtime Subscene Filtering

- Add a lightweight `SubSceneStateRegistry` in `ECSContext`:
  - `subscene_id -> paused/tick_enabled/render_enabled`
- Add per-entity subscene membership (`subscene_id` tag/component).
- Systems filter entities by subscene state:
  - Tick systems skip paused/tick-disabled subscenes.
  - Render systems skip paused/render-disabled subscenes.

Deliverable:

- Preserved subscene control with no extra system copies.

### 6. Clean Recursive Driving Paths

- In shared mode, avoid recursive `SubWorld` tick/render dispatch.
- Keep one deterministic per-frame path for each entity/system.
- Leave recursion only for legacy isolated mode.

Deliverable:

- No duplicated scheduling.

### 7. Serialization and Editor Compatibility

- Serialize/deserialize:
  - `mode`
  - `subscene_id`
  - `root_entity_id`
  - runtime flags
- Backward compatibility:
  - missing `mode` defaults to `SharedContext` during migration.

Deliverable:

- Old scenes remain loadable, new scenes storable.

### 8. Observability and Validation

Add diagnostics:

- `ContextSystemCount`
- `ActiveSubSceneCount`
- `FilteredEntityCountPerFrame`

Run scaling tests:

- 100 / 1000 / 5000 subscenes with mesh-heavy content.

Deliverable:

- Performance and behavior report.

### 9. Phased Rollout

- Phase 1 (stop the explosion):
  - Default shared mode.
  - Disable child-context system registration path.
- Phase 2:
  - Subscene state registry + system filtering.
- Phase 3:
  - Cleanup old recursive paths + full regression.

Deliverable:

- Low-risk incremental merges.

## Acceptance Criteria

1. System instance count remains constant (or near constant) as subscene count grows.
2. `paused`, `tick_enabled`, and `render_enabled` work per subscene.
3. Legacy scenes still load with migration defaults.
4. No obvious regressions in mixed `Gizmo + StaticMesh + SkeletonMesh` scenes.

## Suggested First Implementation Scope (Phase 1)

- Update `SubWorldComponent` default mode to `SharedContext`.
- Bypass child world/context creation unless `IsolatedContext` is explicitly requested.
- Remove/guard child-context `Register*System(...)` calls.
- Keep import-to-parent path active.

This phase is intended to immediately stop system multiplication before deeper filtering work.

## Detailed Task Breakdown

### Phase 1: Stop System Explosion (Must Do First)

1. `inc/hgl/ecs/components/SubWorldComponent.h`
- Add mode enum:
  - `enum class SubWorldMode { SharedContext, IsolatedContext };`
- Add fields:
  - `SubWorldMode mode = SubWorldMode::SharedContext;`
  - `uint64_t subscene_id = 0;`
  - `EntityID root_entity_id;`
- Add APIs:
  - `void SetMode(SubWorldMode m);`
  - `SubWorldMode GetMode() const;`
  - `uint64_t GetSubsceneID() const;`
  - `EntityID GetRootEntityID() const;`

Definition of done:

- Header compiles.
- Default mode is `SharedContext`.

2. `src/ecs/components/SubWorldComponent.cpp`
- Update `OnAttach()` (`src/ecs/components/SubWorldComponent.cpp:389`):
  - `SharedContext`: do not create `sub_world`, do not call `Initialize(parent_context)`.
  - If `asset_path` exists, instantiate to parent only.
  - Capture imported root(s) and set `root_entity_id` (or first valid root).
- Update `Initialize(ECSContext*)` (`src/ecs/components/SubWorldComponent.cpp:175`):
  - If mode is `SharedContext`, return true early.
  - Keep existing child-context initialization only in `IsolatedContext`.
- Guard these registrations under `IsolatedContext` only:
  - `RegisterTickSystem<CameraSystem>()`
  - `RegisterTickSystem<BoundingBoxUpdateSystem>()`
  - `RegisterRenderSystem<RenderPrimitiveCollectSystem>()`
- Update `UpdateSubWorld()` (`src/ecs/components/SubWorldComponent.cpp:220`) and
  `RenderSubWorld()` (`src/ecs/components/SubWorldComponent.cpp:253`):
  - `SharedContext`: no child-context tick/render dispatch.
  - Keep pause/tick/render flags for later filtering phase.
- Update `OnDetach()` (`src/ecs/components/SubWorldComponent.cpp:417`):
  - `SharedContext`: clear imported entities only.
  - `IsolatedContext`: keep shutdown/reset behavior.

Definition of done:

- No child context or child system registration in `SharedContext`.
- Existing `IsolatedContext` behavior still works.

3. `src/ecs/core/World.cpp`
- Keep current recursion path for isolated child worlds.
- Add comments and guards so `SharedContext` subscene components are not recursively driven as child worlds.
- Verify no double schedule path via `TickSubWorldComponents`/`RenderSubWorldComponents`
  (`src/ecs/core/World.cpp:20`, `src/ecs/core/World.cpp:34`, `src/ecs/core/World.cpp:86`, `src/ecs/core/World.cpp:118`).

Definition of done:

- Single-frame execution path is deterministic.
- No duplicate Tick/Render for shared-mode subscenes.

4. `src/ecs/core/Context.cpp`
- Keep `sub_world_auto_update` behavior as-is for now.
- Add temporary debug log counters around subworld auto update sections
  (`src/ecs/core/Context.cpp:296`, `src/ecs/core/Context.cpp:370`, `src/ecs/core/Context.cpp:418`) to verify shared mode does not trigger child-context work.

Definition of done:

- Runtime logs confirm system count is not multiplied by subscene count.

### Phase 2: Subscene State + Filtering

1. `inc/hgl/ecs/core/Context.h`
- Add lightweight subscene state registry:
  - `subscene_id -> { paused, tick_enabled, render_enabled }`.
- Add APIs:
  - `SetSubsceneState(...)`
  - `GetSubsceneState(...)`
  - `RemoveSubsceneState(...)`

2. New component (recommended)
- `inc/hgl/ecs/components/SubSceneMembershipComponent.h`
- `src/ecs/components/SubSceneMembershipComponent.cpp`
- Field: `uint64_t subscene_id`.

3. System filtering updates
- Apply subscene-state filtering in core tick/render query paths used by:
  - mesh collection
  - visibility/camera dependent render collection
  - transform-driven per-frame updates

Definition of done:

- Pause/render/tick flags work per subscene without any extra system instances.

### Phase 3: Serialization + Full Regression

1. Serialization files
- `src/ecs/serialization/ContextSerialization.cpp`
- Add read/write support for:
  - `mode`
  - `subscene_id`
  - `root_entity_id`
  - runtime state flags

2. Backward compatibility
- If missing mode in old data, migrate to `SharedContext`.

3. Regression passes
- Scenes with mixed `Gizmo + StaticMesh + SkeletonMesh`.
- Large subscene counts (100 / 1000 / 5000).

Definition of done:

- Old scene data loads.
- New scene data round-trips.

## Engineering Checklist

1. Add mode and subscene metadata to `SubWorldComponent`.
2. Guard child-context creation/registration by `IsolatedContext` only.
3. Ensure `SharedContext` path imports assets to parent context only.
4. Validate no duplicate per-frame scheduling paths.
5. Add subscene state registry and membership component.
6. Add filtering in tick/render query path.
7. Add serialization and migration fallback.
8. Run scaling tests and log system-count trend.

## Validation Script (Manual)

1. Create 1 parent world + N subscenes in `SharedContext` mode.
2. Instantiate mesh assets into each subscene.
3. Log system registry size before and after instantiation.
4. Confirm system count remains constant while entity count grows.
5. Toggle per-subscene `paused`, `tick_enabled`, `render_enabled` and verify behavior.
