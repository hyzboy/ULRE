# SubWorld AssetWorld Greenfield Design (No Legacy)

## 1. Premise
This design assumes:
- `SubWorldComponent` is removed.
- `SubSceneMembershipComponent` is removed.
- no compatibility path for old scenes or old runtime behavior.
- new architecture is optimized for large-scale instance references.

Primary target:
- each `StaticMesh` / `Gizmo` is authored as an `AssetWorld` definition.
- one scene can hold thousands to hundreds of thousands of `AssetInstance` entries.

## 2. Hard Requirements
1. No duplicated ECS entity import for asset instances.
2. Stable identity for definitions and instances.
3. Runtime caches are rebuildable and non-serialized.
4. Scheduling is budgeted and scalable.
5. Serialization stores references and overrides only.

## 3. New Architecture
## 3.1 Definition Domain
`AssetWorld` is a pure definition graph.

- `AssetWorldId` uniquely identifies an asset definition.
- `AssetNodeId` identifies nodes inside one definition.
- definition contains static hierarchy, mesh refs, material defaults, bounds.
- definition can be versioned (`AssetVersion`).

No runtime `EntityID` exists in this domain.

### 3.2 Scene Domain
Scene uses `AssetInstanceComponent` as the only binding.

```cpp
class AssetInstanceComponent final : public Component
{
    AssetWorldId asset_world_id;
    InstanceId instance_id;
    AssetVersion expected_version;

    uint64_t visibility_mask;
    uint32_t flags;

    AssetOverrideRef override_ref;
};
```

Scene entity also carries generic scene transform and scene-level logic components as needed.

### 3.3 Runtime Bridge Domain
`AssetInstanceBridgeSystem` transforms scene references into runtime proxies.

Responsibilities:
- resolve `AssetWorldId` in registry.
- maintain proxy handles per `InstanceId`.
- apply instance transform and override deltas.
- emit draw packets and optional physics proxies.

Bridge state is internal, transient, and deterministic rebuildable.

### 3.4 Scheduler Domain
`WorldScheduler` (or `InstanceScheduler`) drives staged execution:
1. collect dirty instances
2. rebuild proxies with frame budget
3. cull and bucket
4. emit draw commands

Buckets are grouped by `(AssetWorldId, material class, render pass)`.

## 4. Data Model
## 4.1 IDs
```cpp
using AssetWorldId = uint64_t;
using AssetNodeId  = uint64_t;
using InstanceId   = uint64_t;
using AssetVersion = uint32_t;
```

Rules:
- IDs must be stable across save/load.
- runtime `EntityID` is local allocator index only, never persisted identity.

### 4.2 Definition Records
```cpp
struct AssetNodeDef
{
    AssetNodeId node_id;
    AssetNodeId parent_node_id;

    uint64_t mesh_ref;
    uint64_t material_ref;

    float pos[3];
    float rot[4];
    float scale[3];
};

struct AssetWorldDef
{
    AssetWorldId id;
    AssetVersion version;
    std::string name;
    std::vector<AssetNodeDef> nodes;

    float aabb_min[3];
    float aabb_max[3];
};
```

### 4.3 Instance Records
```cpp
struct AssetInstanceRecord
{
    AssetWorldId asset_world_id;
    InstanceId instance_id;
    AssetVersion expected_version;

    uint64_t override_payload_ref;
    uint32_t override_revision;

    uint64_t visibility_mask;
    uint32_t flags;
};
```

## 5. Serialization Contract
Serialize only source of truth:
- `AssetInstanceRecord`
- scene transforms
- override references

Do not serialize:
- runtime proxy handles
- bridge hash tables
- renderer/physics backend caches

Load sequence:
1. load scene entities and `AssetInstanceRecord`.
2. resolve `AssetWorldDef` from registry.
3. mark unresolved ones as pending.
4. bridge rebuilds proxies gradually with budget.

## 6. Core Services
### 6.1 AssetWorldRegistry
```cpp
class IAssetWorldRegistry
{
public:
    virtual bool Register(const AssetWorldDef&) = 0;
    virtual bool Unregister(AssetWorldId) = 0;
    virtual const AssetWorldDef* Get(AssetWorldId) const = 0;
    virtual bool Exists(AssetWorldId) const = 0;
};
```

### 6.2 AssetInstanceBridgeSystem
```cpp
class AssetInstanceBridgeSystem : public System
{
public:
    bool Initialize(ECSContext*, IAssetWorldRegistry*);

    void Collect(float dt);
    void Rebuild(uint32_t max_instances_per_frame);
    void SyncRender(float dt);

    void OnAssetWorldUpdated(AssetWorldId id, AssetVersion v);
    void OnAssetWorldEvicted(AssetWorldId id);
};
```

### 6.3 Optional Systems
- `AssetStreamingSystem`: residency and memory pressure control.
- `AssetInstanceDebugSystem`: mapping inspection and diagnostics.

## 7. Performance Strategy
1. SoA layout for runtime instance state.
2. Dirty-bit updates, never full rebuild unless forced.
3. Budgeted rebuild per frame.
4. Multi-thread jobs for transform propagation and packet build.
5. Per-asset buckets for cache locality.
6. Coarse instance culling before node-level work.

## 8. New Workflow
### Authoring
- author one `AssetWorld` for one static mesh/gizmo family.
- publish to registry with stable ids.

### Placement
- create scene entity.
- attach `AssetInstanceComponent` with `asset_world_id`.
- set transform and overrides.

### Runtime
- scheduler runs bridge stages.
- renderer consumes draw packets from bridge output.

## 9. What Is Explicitly Removed
1. `SubWorldComponent` lifecycle and local context ownership.
2. `SubSceneMembershipComponent` tagging.
3. asset import duplication path (`created_ids`).
4. root-id semantics coupled to import side effects.

## 10. Risks In Greenfield Mode
1. Existing content cannot load without converter.
2. Tooling must regenerate scene data in new format.
3. Initial bridge implementation needs robust debugging tools.

Given the stated requirement, these are accepted trade-offs.

## 11. Implementation Plan (Greenfield)
### G1: Core Types
- add id typedefs and records
- add `AssetInstanceComponent` and serializer

### G2: Registry + Bridge MVP
- implement registry service
- implement bridge for render-only static mesh path
- integrate scheduler stages

### G3: Scale Features
- add budgeting and threading
- add instance stats and diagnostics
- add streaming/residency control

### G4: Production Hardening
- hot reload support
- deterministic rebuild validation
- stress tests with 10k+ instances

## 12. Acceptance Criteria
1. 10k instances load and run with bounded frame spikes.
2. save/load round trip preserves all instance references.
3. full cache drop and rebuild produce deterministic equivalent output.
4. no code path depends on `SubWorldComponent` or `SubSceneMembershipComponent`.

## 13. Simplified Implementation Principles By Content Type
This section defines practical rules for three major categories in this project.

### 13.1 Pure Static StaticMesh
Definition characteristics:
- no per-node runtime motion after placement
- geometry and material structure fixed by `AssetWorldDef`

Implementation principles:
1. Use one `AssetWorldDef` as immutable template, many `AssetInstanceComponent` as references.
2. Runtime bridge should generate static render proxies once, then only update instance transform and visibility flags.
3. Instance-level overrides are allowed only for material parameters and visibility masks, not topology.
4. Culling is instance-level first (coarse AABB), node-level culling only when needed for very large assets.
5. Serialization stores only `asset_world_id`, `instance_id`, transform, and override reference.

Performance notes:
- prioritize aggressive per-asset bucketing and hardware instancing.
- avoid rebuilding proxy structures unless `asset_version` or override revision changes.

### 13.2 Partially Movable Asset (Example: House With Openable Door)
Definition characteristics:
- most nodes are static
- a small subset of nodes are runtime-driven (door, window, lever)

Implementation principles:
1. Keep one `AssetWorldDef`; mark movable nodes with explicit node flags (`MovableNode`).
2. Scene stores motion state in a dedicated component, for example `AssetNodeMotionComponent`, keyed by `(InstanceId, AssetNodeId)`.
3. Bridge applies motion deltas only to flagged movable nodes; static nodes stay in shared static proxy data.
4. Simulation logic (door open/close) is scene-level logic on instance components, not in definition domain.
5. Per-instance motion state is serialized as compact node-state overrides, not duplicated entities.

Recommended data split:
- static part: `AssetWorldDef` + shared proxy cache
- dynamic part: node-state override table per instance

### 13.3 Gizmo Tools
Definition characteristics:
- high interaction frequency
- mostly editor/runtime-tool visuals with transient states

Implementation principles:
1. Treat gizmos as `AssetWorldDef` families (translate/rotate/scale variants) referenced by instances.
2. Keep interaction state in tool components (selected axis, hover state, active mode), not in `AssetWorldDef`.
3. Bridge supports fast per-frame updates for color/highlight/scale without full proxy rebuild.
4. Gizmo rendering uses dedicated render layer or pass with clear priority and visibility rules.
5. Gizmo instances are non-persistent by default; persistence is opt-in for tool replay scenarios.

Tooling notes:
- provide debug counters for gizmo rebuild frequency and highlight updates.
- isolate gizmo update budget from world static-mesh budget to prevent editor interaction lag.

### 13.4 Shared Rules Across All Three
1. Definition and instance ownership must never be mixed.
2. Runtime cache must always be reconstructable from serialized source-of-truth data.
3. Runtime `EntityID` must not be used as persistence identity.
4. Dirty-bit and budgeted updates are mandatory for scalability.
5. Any feature requiring entity duplication should be treated as an exception path, not default architecture.
