# Asset Node Motion Override Data Draft

## 1. Purpose
Define a minimal and practical data model for partially movable assets (for example, house door/window), under the greenfield architecture:
- no duplicated entities
- per-instance node motion overrides
- deterministic serialization and rebuild

This document focuses on data structures and persistence contracts.

## 2. Scope
Applies to:
- `AssetWorldDef` with mixed static and movable nodes
- `AssetInstanceComponent` instances in scene
- runtime bridge that applies node-level overrides

Does not cover:
- animation graph authoring pipeline
- advanced IK/physics constraints

## 3. ID And Key Model
```cpp
using AssetWorldId = uint64_t;
using AssetNodeId  = uint64_t;
using InstanceId   = uint64_t;
```

Composite key for node motion state:
```cpp
struct AssetInstanceNodeKey
{
    InstanceId instance_id;
    AssetNodeId node_id;
};
```

Rules:
1. `AssetNodeId` must be stable in one asset definition version.
2. Runtime allocator handles are not part of persistence keys.
3. Motion override lookup must not depend on scene entity order.

## 4. Definition-Side Node Metadata
`AssetWorldDef` needs explicit movable metadata.

```cpp
enum class AssetNodeMobility : uint8_t
{
    Static = 0,
    Movable = 1
};

enum class AssetNodeMotionType : uint8_t
{
    None = 0,
    Hinge = 1,
    Slider = 2,
    FreeTRS = 3
};

struct AssetNodeMotionDef
{
    AssetNodeMobility mobility = AssetNodeMobility::Static;
    AssetNodeMotionType motion_type = AssetNodeMotionType::None;

    // Optional authored limits in local space.
    float limit_min[3] = {0, 0, 0};
    float limit_max[3] = {0, 0, 0};

    // Optional pivot/axis for hinge-like motions.
    float pivot[3] = {0, 0, 0};
    float axis[3] = {0, 0, 1};
};
```

Guideline:
- only nodes marked `Movable` can accept runtime motion overrides.

## 5. Instance-Side Motion State
### 5.1 Minimal ECS Component
```cpp
class AssetNodeMotionComponent final : public Component
{
public:
    InstanceId GetInstanceId() const;
    void SetInstanceId(InstanceId id);

    uint32_t GetStateRevision() const;
    void BumpStateRevision();

    // Packed override table reference, storage owned elsewhere.
    uint64_t GetOverrideTableRef() const;
    void SetOverrideTableRef(uint64_t ref);
};
```

Rationale:
- keep ECS component small
- place heavy per-node table in pooled storage

### 5.2 Override Table Entry
```cpp
enum class NodeMotionStateFlags : uint32_t
{
    None = 0,
    DirtyTransform = 1 << 0,
    DirtyVisibility = 1 << 1,
    Active = 1 << 2
};

struct NodeMotionState
{
    AssetNodeId node_id = 0;

    // Local delta against definition pose.
    float delta_pos[3] = {0, 0, 0};
    float delta_rot[4] = {0, 0, 0, 1};
    float delta_scale[3] = {1, 1, 1};

    // Generic scalar channel (door open ratio, slider value, etc.).
    float param = 0.0f;

    uint32_t flags = 0;
    uint32_t revision = 0;
};
```

Table shape:
```cpp
struct AssetNodeMotionOverrideTable
{
    InstanceId instance_id = 0;
    uint32_t table_revision = 0;
    std::vector<NodeMotionState> nodes;
};
```

## 6. Serialization Contract
Persist only stable state.

### 6.1 Serialized Record
```cpp
struct NodeMotionStateRecord
{
    uint64_t node_id = 0;

    float delta_pos[3] = {0, 0, 0};
    float delta_rot[4] = {0, 0, 0, 1};
    float delta_scale[3] = {1, 1, 1};

    float param = 0.0f;
    uint32_t flags = 0;
    uint32_t revision = 0;
};

struct AssetNodeMotionRecord
{
    uint64_t instance_id = 0;
    uint32_t table_revision = 0;
    std::vector<NodeMotionStateRecord> node_states;
};
```

### 6.2 Do Not Serialize
- bridge proxy handles
- node-to-proxy runtime indices
- temporary dirty queues

### 6.3 Load Flow
1. load `AssetInstanceComponent`
2. load `AssetNodeMotionRecord`
3. validate every `node_id` against `AssetWorldDef`
4. drop invalid node records with warning
5. mark instance dirty and rebuild bridge incrementally

## 7. Runtime Update Rules
1. Scene logic modifies node motion states through API, not direct memory writes.
2. Every write bumps node `revision` and table `table_revision`.
3. Bridge applies only changed nodes (`revision` diff or dirty flags).
4. Nodes marked `Static` in definition reject runtime overrides.
5. Missing nodes in override table mean "use authored default".

## 8. Public API Draft
```cpp
class IAssetNodeMotionService
{
public:
    virtual bool SetNodeParam(InstanceId instance_id, AssetNodeId node_id, float value) = 0;
    virtual bool SetNodeDeltaTRS(InstanceId instance_id,
                                 AssetNodeId node_id,
                                 const float pos[3],
                                 const float rot[4],
                                 const float scale[3]) = 0;

    virtual bool ResetNodeState(InstanceId instance_id, AssetNodeId node_id) = 0;
    virtual bool ResetAllNodeStates(InstanceId instance_id) = 0;
};
```

Expected behavior:
- API validates node mobility and motion type before writing.
- API returns false when node is static or not found.

## 9. Bridge Integration
Bridge responsibilities for node motion:
1. resolve definition node mobility/motion metadata
2. fetch override table by `instance_id`
3. apply changed node states to runtime proxy transform palette
4. submit minimal changed ranges to render backend

Optimization:
- maintain per-instance changed node list
- pack changed nodes into contiguous upload ranges

## 10. Authoring Rules
1. Node naming does not define identity; `AssetNodeId` does.
2. Door/window-like nodes must be explicitly marked `Movable`.
3. Motion limits must be authored in definition metadata.
4. Asset version bump is required when node ids or hierarchy change.

## 11. Error Handling Policy
1. Unknown `node_id` on load: warning + skip record.
2. Motion type mismatch (for example writing hinge param to static node): warning + reject write.
3. Missing override table: create empty table lazily.
4. Registry miss for asset definition: keep instance pending, no crash.

## 12. Minimal Rollout Steps
1. Add node mobility/motion metadata to `AssetWorldDef`.
2. Add `AssetNodeMotionComponent` and `AssetNodeMotionRecord` serializer.
3. Implement `IAssetNodeMotionService` with validation.
4. Extend bridge to apply node-level overrides with dirty tracking.
5. Add one sample: house with openable door replicated to many instances.

## 13. Acceptance Checks
1. Door open ratio changes only affect target node and instance.
2. 1000+ house instances with sparse door interactions remain stable in frame time.
3. save/load preserves per-instance door state exactly.
4. full runtime cache drop and rebuild reproduces same node poses.
