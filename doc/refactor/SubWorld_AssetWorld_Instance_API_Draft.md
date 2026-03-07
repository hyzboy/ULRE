# SubWorld AssetWorld Instance API Draft

## 1. Scope
This document defines implementation-oriented APIs for the separation plan:
- `AssetWorld` as reusable world definitions
- `AssetInstance` as scene-side references
- bridge systems for runtime proxy build and updates

This is a draft for coding phase and can evolve with profiling results.

## 2. Naming And ID Types
```cpp
using AssetWorldId = uint64_t;
using AssetNodeId  = uint64_t;
using InstanceId   = uint64_t;
using AssetVersion = uint32_t;
```

Guidelines:
- `AssetWorldId`: stable key for one asset definition world.
- `AssetNodeId`: stable key for one node inside an asset definition.
- `InstanceId`: stable key for one instance in one scene.
- runtime `EntityID` is never canonical persistence identity.

## 3. Core Data Contracts
### 3.1 Asset Definition
```cpp
struct AssetNodeDef
{
    AssetNodeId node_id = 0;
    AssetNodeId parent_node_id = 0; // 0 means root
    uint32_t flags = 0;

    // Definition-level data references.
    uint64_t mesh_ref = 0;
    uint64_t material_ref = 0;

    // Authoring transform in asset-local space.
    float pos[3] = {0,0,0};
    float rot[4] = {0,0,0,1};
    float scale[3] = {1,1,1};
};

struct AssetWorldDef
{
    AssetWorldId id = 0;
    AssetVersion version = 0;
    std::string name;
    std::vector<AssetNodeDef> nodes;

    // Optional coarse bounds for streaming and culling.
    float aabb_min[3] = {0,0,0};
    float aabb_max[3] = {0,0,0};
};
```

### 3.2 Scene Instance Component
```cpp
struct AssetOverrideRef
{
    // external table key or blob id
    uint64_t payload_ref = 0;
    uint32_t revision = 0;
};

class AssetInstanceComponent : public Component
{
public:
    static const char* GetSerializationType();

    AssetWorldId GetAssetWorldId() const;
    void SetAssetWorldId(AssetWorldId id);

    InstanceId GetInstanceId() const;
    void SetInstanceId(InstanceId id);

    AssetVersion GetExpectedVersion() const;
    void SetExpectedVersion(AssetVersion v);

    const AssetOverrideRef& GetOverrideRef() const;
    void SetOverrideRef(const AssetOverrideRef& ref);

    uint64_t GetVisibilityMask() const;
    void SetVisibilityMask(uint64_t mask);

    uint32_t GetFlags() const;
    void SetFlags(uint32_t flags);

    static bool SerializeToRecord(const std::shared_ptr<Component>&,
                                  const hgl::UnorderedMap<EntityID, int32_t>&,
                                  ComponentRecord& out_record);

    static void DeserializeFromRecord(const ComponentRecord&,
                                      Entity*,
                                      std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&);
};
```

### 3.3 Runtime Bridge Cache (Non-Serialized)
```cpp
struct AssetInstanceRuntimeState
{
    InstanceId instance_id = 0;
    AssetWorldId asset_world_id = 0;
    AssetVersion resolved_version = 0;

    bool resident = false;
    bool dirty_transform = true;
    bool dirty_override = true;
    bool dirty_visibility = true;

    // Proxy handles for render/physics/etc.
    uint64_t render_proxy_handle = 0;
    uint64_t physics_proxy_handle = 0;
};
```

## 4. Registry APIs
```cpp
class IAssetWorldRegistry
{
public:
    virtual ~IAssetWorldRegistry() = default;

    virtual bool RegisterAssetWorld(const AssetWorldDef& def) = 0;
    virtual bool UnregisterAssetWorld(AssetWorldId id) = 0;

    virtual const AssetWorldDef* GetAssetWorld(AssetWorldId id) const = 0;
    virtual AssetVersion GetVersion(AssetWorldId id) const = 0;

    virtual bool Exists(AssetWorldId id) const = 0;
};
```

Guidelines:
- thread-safe reads in runtime phase
- writes constrained to loading/editor phase or protected by world barriers

## 5. Bridge System APIs
```cpp
class AssetInstanceBridgeSystem : public System
{
public:
    struct Stats
    {
        uint32_t instance_count = 0;
        uint32_t resident_count = 0;
        uint32_t dirty_count = 0;
        uint32_t rebuild_count_frame = 0;
        double rebuild_ms = 0.0;
    };

public:
    bool Initialize(ECSContext* context, IAssetWorldRegistry* registry);

    // Collect changed AssetInstanceComponents and stage updates.
    void Collect(float dt);

    // Build or patch runtime proxies with budget caps.
    void Rebuild(float dt, uint32_t max_instances_per_frame);

    // Submit render-domain updates for this frame.
    void SyncRender(float dt);

    // Optional hooks for streaming lifecycle.
    void OnAssetLoaded(AssetWorldId id, AssetVersion version);
    void OnAssetEvicted(AssetWorldId id);

    const Stats& GetStats() const;
};
```

Execution order proposal:
1. `Collect`
2. `Rebuild` (budgeted)
3. `SyncRender`

## 6. Scheduler Integration
`WorldScheduler` keeps ownership of frame order.

Minimal integration points:
- `WorldScheduler::CollectInstanceBridges()`
- `WorldScheduler::RebuildInstanceBridges(uint32_t budget)`
- `WorldScheduler::SyncInstanceBridgesForRender()`

Policy:
- no heavy bridge work in render pass
- do incremental rebuild in pre-render phase

## 7. Serialization Record Draft
```cpp
struct AssetInstanceRecord
{
    uint64_t asset_world_id = 0;
    uint64_t instance_id = 0;
    uint32_t expected_version = 0;

    uint64_t override_payload_ref = 0;
    uint32_t override_revision = 0;

    uint64_t visibility_mask = ~0ull;
    uint32_t flags = 0;

    template<class Archive>
    void serialize(Archive& ar)
    {
        ar(asset_world_id,
           instance_id,
           expected_version,
           override_payload_ref,
           override_revision,
           visibility_mask,
           flags);
    }
};
```

Context serialization rule:
- save `AssetInstanceRecord` only
- never save runtime proxy or bridge caches

## 8. Lifecycle Sequences
### 8.1 Scene Load
1. Load scene entities and `AssetInstanceComponent`.
2. Resolve each `asset_world_id` in registry.
3. Mark unresolved instances as pending.
4. Bridge `Collect` and enqueue build jobs.
5. Bridge `Rebuild` with frame budget until all required instances are resident.

### 8.2 Runtime Update
1. Transform/override changes mark instance dirty bits.
2. Bridge patches only dirty proxies.
3. Scheduler submits render synchronization.

### 8.3 Asset Hot Reload
1. Registry bumps version for target `asset_world_id`.
2. Bridge marks affected instances dirty.
3. Budgeted rebuild patches all affected runtime proxies.

### 8.4 Scene Unload
1. Destroy scene entities.
2. Bridge releases proxy handles for removed `instance_id`.
3. Registry keeps definitions if referenced by other scenes.

## 9. Legacy Compatibility Strategy
Legacy path in `SubWorldComponent`:
- `SetAssetPath`
- `InstantiateAssetToParent`
- `GetInstancedEntityIDs`

Compatibility mode proposal:
- keep for migration only
- add warning on first use in runtime logs
- convert editor-generated new content to `AssetInstanceComponent`

## 10. Suggested Milestones
### M1: Type And Serializer Foundations
- add ids and `AssetInstanceComponent`
- add `AssetInstanceRecord` serialization
- no bridge yet

### M2: Minimal Render Bridge
- implement `IAssetWorldRegistry`
- implement bridge collect/rebuild/sync for static mesh render proxies
- convert one static mesh example

### M3: Performance Stabilization
- bucket by `AssetWorldId`
- add budgets and metrics
- multithread rebuild jobs

### M4: Legacy Sunset
- deprecate legacy import APIs
- remove `created_ids` dependence from mainline flows

## 11. KPI And Acceptance Criteria
Functional:
- scene save/load round-trip preserves all instance references
- hot reload updates all related instances
- unresolved asset references do not crash world updates

Performance baseline (initial targets):
- 10k instances loaded without full-frame spikes over configured budget
- dirty-only update scales with number of changed instances, not total instances

Stability:
- no persistence dependency on runtime `EntityID`
- bridge cache rebuild after full cache drop is deterministic

## 12. Open Questions
1. Should `AssetWorldRegistry` live in ECS context, engine singleton, or resource module?
2. Do we need per-instance script hooks at bridge level, or keep scripts at scene entity level only?
3. Should physics proxies be built from the same bridge or a separate physics bridge?
4. What is the final ownership model for override payload storage?
