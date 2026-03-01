# TransformSystem 更新策略技术文档

> 适用版本：ULRE ECS（基于 Vulkan，`HGL_L2W_RING_FRAMES` 多帧环形缓冲方案）

---

## 1. 总体架构

```
ECSContext
  ├── worldGetStaticTransforms()   → TransformComponent list (Mobility::Static)
  └── GetMovableTransforms()       → TransformComponent list (Mobility::Movable)
                                             │
                                    TransformSystem::SubmitTransformUpdates()
                                             │
                                    TransformAssignmentBuffer
                                             │
                               ┌─────────────┴─────────────┐
                        Static Zone                   Dynamic Ring Zone
                    (直接写入，dirty 触发)         (每帧全量写当前帧槽位)
                               │                           │
                       VkBuffer (UBO/SSBO):
              [Static[0..N-1] | Frame0[0..M-1] | Frame1[0..M-1] | ... | FrameK[0..M-1]]
```

**两类 Transform 对象完全分开存储与更新**，是设计的核心原则：

| 分类 | `Mobility` | CPU 矩阵更新 | GPU 上传策略 | GPU 缓冲区位置 |
|------|-----------|------------|-------------|--------------|
| 静态 | `Mobility::Static`  | 仅在 dirty 时 | 增量写脏项 | Static Zone（固定偏移） |
| 动态 | `Mobility::Movable` | 每帧 tick | **每帧全量写当前 ring 帧槽位** | Dynamic Ring Zone |

---

## 2. 数据层：TransformDataStorage（SOA 布局）

`TransformDataStorage` 以 **SOA（Structure of Arrays）** 方式存储所有 Transform 数据，而非传统的 AOS：

```cpp
// SOA 布局，各数组内存连续，批量访问 cache 友好
std::vector<glm::vec3> positions;      // 12 bytes × N
std::vector<glm::quat> rotations;      // 16 bytes × N
std::vector<glm::vec3> scales;         // 12 bytes × N
std::vector<glm::mat4> worldMatrices;  // 64 bytes × N  ← 最终上传到 GPU 的数据
std::vector<uint8_t>   mobility;       // 1 byte × N  (0=Static, 1=Movable)
std::vector<bool>      matrixDirty;    // 1 bit × N
```

每个 `TransformComponent` 构造时调用 `Allocate()` 获得一个 `HandleID`（uint32_t 数组下标）。
`SetMobility(handle, 0/1)` 记录 mobility 属性，后续 TransformSystem 据此路由到 static/dynamic 两条路径。

---

## 3. GPU 缓冲区布局

### 3.1 物理内存结构

单块 CPU-visible 缓冲（UBO 或 SSBO，取决于编译宏 `HGL_L2W_USE_SSBO`）内部按以下布局分配：

```
[ Static Zone (N个矩阵) | Dynamic Frame0 (M个矩阵) | Dynamic Frame1 | ... | Dynamic FrameK ]
  索引 0 .. N-1          索引 N .. N+M-1            索引 N+M .. N+2M-1
```

- **N** = `static_count`（静态对象数量）
- **M** = `dynamic_count`（动态对象数量）
- **K** = `HGL_L2W_RING_FRAMES - 1`（环形帧总数减一）
- 总元素数 = `N + M × HGL_L2W_RING_FRAMES`

由 `DeviceBufferRingWriter::GetTotalCount()` 计算：

```cpp
uint32_t GetTotalCount(uint32_t static_count, uint32_t dynamic_count) const {
    return static_count + dynamic_count * ring_frames;  // N + M×K
}
```

### 3.2 动态区当前帧槽位的起始索引

由 `DeviceBufferRingWriter::GetBaseIndex()` 计算：

```cpp
uint32_t GetBaseIndex(uint32_t static_count, uint32_t dynamic_count) const {
    return static_count + frame_index * dynamic_count;  // N + frameIdx×M
}
```

每帧结束后调用 `AdvanceFrame()` 递增 `frame_index`（对 `ring_frames` 取模），
GPU读取时使用当前帧的 base index 索引到正确的槽位。

### 3.3 示意图（ring_frames = 3, N=2 静态, M=2 动态）

```
Buffer 内存:
  [Static0][Static1] | [F0_Dyn0][F0_Dyn1] | [F1_Dyn0][F1_Dyn1] | [F2_Dyn0][F2_Dyn1]
    idx=0   idx=1       idx=2    idx=3        idx=4    idx=5        idx=6    idx=7

frame_index=0 时: baseIndex = 2 + 0×2 = 2  → 写入 [F0_Dyn0][F0_Dyn1]
frame_index=1 时: baseIndex = 2 + 1×2 = 4  → 写入 [F1_Dyn0][F1_Dyn1]
frame_index=2 时: baseIndex = 2 + 2×2 = 6  → 写入 [F2_Dyn0][F2_Dyn1]
```

---

## 4. CPU 矩阵更新流程

### 4.1 Update()（每帧 tick 阶段）

```
TransformSystem::Update(deltaTime)
  └── 遍历 movable_transforms（Movable 对象）
        └── ShouldUpdateTransform() → comp->UpdateIfDirty()
              ↓ 更新 local TRS → 计算 worldMatrix（含父子链传播）
              └── MarkTransformSeen() 记录已处理版本号
```

**注意**：`Update()` 只负责 CPU 侧矩阵计算，**不写 GPU buffer**。
Static 对象不在此路径（除非显式调用 `UpdateStaticDirty()`）。

### 4.2 UpdateStaticDirty()（懒惰触发）

```
TransformSystem::UpdateStaticDirty()
  └── 遍历 static_transforms
        └── UpdateStaticTransformRecursive() 递归处理父子链
              └── comp->UpdateIfDirty()  （仅 dirty 时执行）
```

触发时机：`SubmitTransformUpdates()` 检测到有 static 对象脏位置位时自动调用。

---

## 5. GPU 上传流程：SubmitTransformUpdates()

这是 TransformSystem 的核心，每帧渲染前调用一次。

### 5.1 完整流程图

```
SubmitTransformUpdates()
  │
  ├─[1] 检测 static 脏对象 → 有则触发 UpdateStaticDirty()
  │
  ├─[2] EnsureTransformBuffer()  创建或复用 TransformAssignmentBuffer
  │
  ├─[3] RefreshHandleOrder()     重建 handle→index 映射表
  │      ├── static_handles[]  (有序 HandleID 列表)
  │      └── dynamic_handles[] (有序 HandleID 列表)
  │
  ├─[4] EnsureCapacity(static_count, dynamic_count)  扩容检查
  │
  ├─[5] 计算 dirty_static_indices[]
  │      ├── static_dirty==true  → 全量（0..N-1 全部）
  │      └── static_dirty==false → version 比对，仅变化项
  │
  ├─[6] 计算 dirty_dynamic_indices[]
  │      └── dynamic_force_full==true → 全量（0..M-1 全部）  ← 永远为 true
  │
  ├─[7] WriteStaticDirtyIndices()   按脏索引范围增量写 Static Zone
  │
  └─[8] WriteDynamicDirtyIndices()  按槽位索引写当前帧 Dynamic Ring 槽位
```

### 5.2 Static 上传策略：版本号增量比对

```cpp
// 有脏对象时，全量刷新；否则按版本号逐项判断
if (static_dirty)
{
    for (uint32_t i = 0; i < static_count; ++i)
        dirty_static_indices.push_back(i);   // 全量
}
else
{
    for (auto& weak_comp : static_transforms)
    {
        const uint64_t version = comp->GetVersion();
        const uint64_t *last_uploaded = last_uploaded_version.GetValuePointer(handle);
        if (!last_uploaded || *last_uploaded != version)
            dirty_static_indices.push_back(*idx);  // 增量
    }
}
```

Static 对象写入 buffer 物理偏移为 `index × sizeof(Matrix4f)`（Static Zone 起点）。

### 5.3 Dynamic 上传策略：每帧强制全量写

```cpp
// Dynamic transforms are written into a per-frame ring segment.
// Even when transform values are unchanged, current frame segment must be populated.
const bool dynamic_force_full = (dynamic_count > 0);  // 永远全量
```

动态对象写入的物理偏移为：

```
byte_offset = (base_index + local_index) × sizeof(Matrix4f)
            = (N + frame_index × M + i) × 64 bytes
```

---

## 6. WriteStaticDirtyIndices / WriteDynamicDirtyIndices：范围合并写入

为减少 `VkBuffer::Write()` 调用次数，脏索引先做**连续段合并（run-length merge）**：

```cpp
// 例: dirty_indices = {0, 1, 2, 5, 6, 10}
// 合并后: [{0,2}, {5,6}, {10,10}] → 3次 Write 而非 6次
BuildMergedRangesFromIndices(dirty_indices, handle_count)
```

每段调用 `buffer->Write(data, byte_offset, byte_size)` + 记录 `DirtyRange`，
最后统一 `FlushRanges()` 通知 Vulkan 刷新 host cache（非 coherent 内存需要此步骤）。

---

## 7. 内存模型：CPU-visible 直接写入

```
CPU 进程内存
    │ glm::mat4 worldMatrices[]   (TransformDataStorage SOA)
    │        ↓
    │ Matrix4f temp[]             (SubmitTransformUpdates 临时数组)
    │        ↓ buffer->Write() / buffer->Map()
GPU 可见内存 (CPUOnly 或 ReBAR)
    └── VkBuffer "ECS:LocalToWorld"
          ├── Static Zone  [0 .. N-1]
          └── Ring Zone    [N .. N + M×Frames - 1]
```

**无 staging buffer，无 vkCmdCopyBuffer**，Write/Map 直接写 CPU-visible 显存。
因此 `RingBufferWrapper::CommitInternal()` 是 no-op，数据在 `Write()`/`Unmap()` 后立即对 GPU 可见。

---

## 8. 关键约束：为何 Dynamic 不能用 dirty-only 策略

**错误做法**（已在 bug 修复中移除）：

```cpp
// ❌ 危险：仅在 count/layout 变化时才全量，其余帧只写脏项
const bool dynamic_force_full = (dynamic_count != last_dynamic_count) || dynamic_layout_changed;
```

**原因分析**：

```
Frame N:   frame_index=0 → 写入槽位[F0]: Obj0={1,0,0}, Obj1={0,2,0}  ✓
Frame N+1: frame_index=1 → 只写脏项（Obj0 变了）→ 仅写槽位[F1]: Obj0={1.1,0,0}
                            槽位[F1] 的 Obj1 从未被写入 → 读到未初始化数据或上次其他用途的残留值
Frame N+2: frame_index=2 → 同 F1 问题
Frame N+3: frame_index=0 (回绕) → F0 已经是 N 帧的旧数据
```

每帧切换 `frame_index`，意味着当前帧对应的槽位在 `ring_frames` 帧前最后一次被写入，
若中间未全量覆盖，GPU 读到的是"ring_frames 帧前的旧矩阵"，表现为**闪烁或抖动**。

**正确做法**：只要有动态对象，就全量写当前帧槽位：

```cpp
// ✓ 正确：每帧都把当前 ring 帧槽位完整填写
const bool dynamic_force_full = (dynamic_count > 0);
```

代价：每帧 `dynamic_count × 64` 字节写入。这是 ring buffer 方案的**固有成本**，
无法通过 dirty 优化绕开，因为 ring buffer 的价值不在于减少写入量，
而在于**让 CPU 写入与 GPU 读取并行（不同帧槽位），避免 pipeline stall**。

---

## 9. TransformSystem 主要成员与职责速查

| 成员 | 类型 | 用途 |
|------|------|------|
| `static_handles` | `vector<HandleID>` | Static 对象 handle 有序列表（与 GPU Static Zone 下标一一对应） |
| `dynamic_handles` | `vector<HandleID>` | Dynamic 对象 handle 有序列表（与 Dynamic Ring 槽位下标一一对应） |
| `static_index_map` | `UnorderedMap<HandleID, uint32_t>` | handle → Static Zone 下标 |
| `dynamic_index_map` | `UnorderedMap<HandleID, uint32_t>` | handle → Dynamic Ring 本地下标 |
| `last_seen_version` | `UnorderedMap<HandleID, uint64_t>` | CPU 矩阵更新的版本号（Update 阶段用） |
| `last_uploaded_version` | `UnorderedMap<HandleID, uint64_t>` | GPU 上传的版本号（SubmitTransformUpdates 用，静态增量比对） |
| `static_dirty` | `bool` | 标记 Static Zone 需要全量刷一次 |
| `transform_buffer` | `TransformAssignmentBuffer*` | GPU 缓冲区管理器（懒创建） |

---

## 10. 每帧调用时序

```
[Frame Begin]
    TransformAssignmentBuffer::AdvanceFrame()   ← frame_index 推进到下一槽位
        ↓
[Tick Phase]
    TransformSystem::Update(dt)                 ← 更新 Movable 的 CPU 矩阵
        ↓
[Pre-Render Phase]
    TransformSystem::SubmitTransformUpdates()   ← 上传矩阵到 GPU buffer
        ├── Static: dirty 增量写
        └── Dynamic: 全量写 ring[frame_index] 槽位
        ↓
[Render Phase]
    GPU 读取 ring[frame_index] 槽位中的矩阵     ← 总是读到当帧写入的新鲜数据
```

---

## 11. 参数说明

| 宏/常量 | 含义 |
|---------|------|
| `HGL_L2W_RING_FRAMES` | Dynamic Ring 帧数（典型值 2 或 3）。值越大 CPU/GPU 并行度越高，内存占用也成倍增加 |
| `HGL_L2W_USE_SSBO` | 定义后用 SSBO 存矩阵（支持更大的 transform count），否则用 UBO（硬件兼容性更好） |

---

## 12. 常见问题 FAQ

**Q: 为什么 Static 对象使用 dirty 增量写，而不也放进 ring buffer？**

A: Static 对象矩阵一旦计算完成就不再变化（除非 scene 结构改变），ring 多帧复制没有意义，
浪费内存。Static 写一次即永久有效，GPU 每帧读同一物理地址。

**Q: 增加一个新的 Movable 对象后是否有什么需要注意的？**

A: `RefreshHandleOrder()` 会在 `SubmitTransformUpdates()` 开头自动重建 `dynamic_handles`，
新对象会获得新的本地下标，`dynamic_force_full = true` 保证当帧即完整写入，无需额外操作。

**Q: `MigrateStorage(Mobility)` 的作用是什么？**

A: 当调用 `SetMobility()` 改变对象的 mobility 属性时，需要同时将其从
`world->GetStaticTransforms()` 迁移到 `world->GetMovableTransforms()`（或反向）。
`MigrateStorage()` 负责 `TransformDataStorage` 侧的 `mobility` 字段更新，
下一帧 `RefreshHandleOrder()` 会自动将其纳入正确的 handle 列表。
同时会将 `static_dirty` 置为 `true`，触发 Static Zone 的全量刷新，
避免旧索引残留在 Static Zone 末尾显示为错误矩阵。
