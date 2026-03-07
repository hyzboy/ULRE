# SubWorld AssetWorld Instance Separation Draft

## 1. Background
Current `SubWorldComponent` mixes three responsibilities:
1. sub-world runtime isolation and scheduling
2. asset import and entity duplication into parent world (`created_ids` path)
3. per-subscene lifecycle and serialization glue

This is workable for small scale, but becomes hard to maintain for thousands of world instances.

Target scenario:
- Each `StaticMesh` or `Gizmo` is authored as an independent world definition.
- A scene can contain thousands of instances of those definitions.
- Runtime must remain deterministic, streamable, and serialization-safe.

## 2. Design Goals
1. Fully separate world definition from world instancing.
2. Remove duplicated runtime entities from the mainline path.
3. Keep `SubWorld` for true isolated runtime worlds only.
4. Make serialization based on stable references, not runtime `EntityID` caches.
5. Scale to 1k-100k instances with predictable memory and update cost.

## 3. Core Model
### 3.1 Definition Layer: AssetWorld
`AssetWorld` is a reusable template world.

Properties:
- stable `AssetWorldId`
- stable node ids inside definition (`AssetNodeId`)
- immutable or versioned data (`asset_version`)
- no dependency on runtime entity allocation order

Managed by `AssetWorldRegistry`.

### 3.2 Instance Layer: AssetInstance
Scene entity stores `AssetInstanceComponent` only.

Suggested fields:
- `AssetWorldId asset_world_id`
- `uint64_t instance_id`
- `uint32_t asset_version_expected`
- `Transform local_to_parent`
- `OverrideRef material_override_ref`
- `uint64_t visibility_mask`
- `uint32_t flags`

This layer describes references and per-instance overrides, not duplicated entities.

### 3.3 Runtime Bridge Layer
`AssetInstanceBridgeSystem` builds runtime proxies from instance data.

Responsibilities:
- resolve `asset_world_id` from registry
- create and maintain runtime proxy handles (render, optional physics)
- apply transform and override deltas
- support incremental rebuild and streaming budget

Important:
- bridge cache is rebuildable
- bridge cache is not serialized

### 3.4 Execution Layer
`WorldScheduler` handles update ordering and budgets.

Responsibilities:
- bucket instances by `AssetWorldId`
- update only dirty or visible buckets
- support frame budget limits for rebuild and streaming
- isolate heavy work into jobs

## 4. Separation From Existing SubWorld
### 4.1 Keep SubWorld For
- truly isolated local gameplay simulation
- nested world ownership and lifecycle
- contexts that must tick independently

### 4.2 Move Away From SubWorld
- `InstantiateAssetToParent()` based duplication
- `created_ids` ownership as primary instance model
- root id semantics coupled to import side effects

### 4.3 Compatibility
Keep current import path as `LegacyAssetImportMode` during migration.
New content defaults to `AssetInstanceComponent` path.

## 5. Data Contracts
## 5.1 Stable IDs
- `AssetWorldId`: globally stable asset key
- `AssetNodeId`: stable node key inside definition
- `InstanceId`: scene-local stable instance key

No runtime `EntityID` should be persisted as canonical identity.

### 5.2 Serialization Rules
Serialize only source-of-truth data:
- `AssetInstanceComponent`
- override references or override blobs
- instance-level state flags

Do not serialize:
- bridge caches
- runtime proxy handles
- temporary lookup maps

Load flow:
1. deserialize instances
2. resolve registry references
3. rebuild bridge caches incrementally

## 6. ECS API Draft
### 6.1 New Components
- `AssetInstanceComponent`
- optional `AssetInstanceStateComponent` (debug/inspection only)

### 6.2 New Systems
- `AssetWorldRegistrySystem` or global service
- `AssetInstanceBridgeSystem`
- `AssetInstanceStreamingSystem` (optional phase 2)

### 6.3 Updated SubWorld API Policy
`SubWorldComponent` should no longer be the default API for mesh/gizmo instancing.

Deprecation candidates:
- `SetAssetPath`
- `InstantiateAssetToParent`
- `GetInstancedEntityIDs`

## 7. Performance Plan
1. SoA storage for instance runtime state.
2. bucket by `AssetWorldId` and material key.
3. visibility first, then per-asset detail pass.
4. frame-budgeted rebuild (`N` instances per frame).
5. multithread jobs for transform, visibility, and draw packet build.

## 8. Migration Plan
### Phase A: Foundations
- introduce id types and registry interfaces
- add `AssetInstanceComponent`
- keep old path untouched

### Phase B: Dual Path
- add bridge system
- route selected static mesh and gizmo flows to instance path
- keep old import path as fallback

### Phase C: Serialization Flip
- new scenes serialize only instance references
- load old scenes with compatibility adapter

### Phase D: Cleanup
- mark old import APIs deprecated
- remove dependency on `created_ids` in core workflows

## 9. Risks And Mitigations
1. Risk: bridge rebuild spikes
- mitigation: frame budgets and dirty partitioning

2. Risk: asset version mismatch
- mitigation: explicit `asset_version_expected` and compatibility handlers

3. Risk: debugging complexity
- mitigation: inspection views for instance-to-asset mapping and bridge stats

4. Risk: temporary dual-path maintenance cost
- mitigation: strict sunset milestones for legacy path

## 10. Immediate Next Actions
1. Add interfaces and type definitions without behavior change.
2. Implement `AssetInstanceComponent` serializer.
3. Build minimal bridge for render-only static meshes.
4. Add perf counters: instance count, bucket count, rebuild cost, cache hit ratio.
5. Convert one example from `SubWorld` import mode to instance reference mode.
