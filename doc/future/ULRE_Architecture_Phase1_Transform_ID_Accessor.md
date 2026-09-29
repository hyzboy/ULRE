# ULRE 架构重构 - 阶段一：Transform 体系 ID 化与 Accessor 重构技术规范 (Phase 1 Specification)

## 一、 阶段目标与背景 (Goals & Background)

* **现状痛点**：当前 `TransformComponent` 继承自 `Component` 虚基类，包含大量指针和动态容器开销，逻辑层修改后还需要经过复杂的 System 收集同步，存在明显的冗余内存税与数据二次搬运。
* **阶段目标**：
  1. 彻底废除 `TransformComponent` 堆实体；
  2. 实现轻量零开销的 `TransformAccessor` 访问器（仅包含 4 字节 `uint32_t id`）；
  3. 重构 `TransformDataStorage`，实现“静态在前、动态在后”的物理分区与固定容量机制；
  4. 引入并发安全的一实体一字节脏标记（`uint8_t dirty_bits`），并提供 SoA 数组以面向未来 GPU Compute Shader 合并访存。

---

## 二、 TransformDataStorage 物理存储层设计

### 2.1 动静物理分区与固定容量机制
修改 `inc/hgl/ecs/support/TransformDataStorage.h`：
* **内存分区**：
  * `[0, static_count)`：静态实体，仅初始化与烘焙期计算一次，运行时免刷新；
  * `[static_count, static_count + dynamic_count)`：动态实体，运行时每帧刷新；
* **固定容量（禁止运行时动态扩容）**：
  * 在 Editor 期或关卡加载期一次性分配最大容量，运行时绝对不进行 `realloc`，彻底保证 GPU BDA 64 位物理地址永久有效，杜绝野指针崩溃；
* **废弃 `ActiveRowLease`**：移除繁重的租约对象，采用纯整型行槽位管理。

### 2.2 AoS 与 SoA 混合存储模型（GPU 合并访存保障）
在 Storage 内部除维持连续的 `TransformTRS* raw_trs_array` 外，底层提供 SoA 平行数组以达到未来 Compute Shader 100% 显存合并访存（Memory Coalescing）：
```cpp
Vector4f* soa_translations; // [TotalCapacity] (16 字节对齐)
Vector4f* soa_rotations;    // [TotalCapacity]
Vector4f* soa_scales;       // [TotalCapacity]
```

### 2.3 无数据竞争脏标记（Thread-Safe Dirty Tracking）
* 脏标记采用 `uint8_t* dirty_bits`，**严禁使用 `bitset` 或按位压缩**；
* 每个实体独占 1 个独立字节，多线程并发调用 `SetPosition` 时即使槽位相邻，也不会产生 False Sharing 与数据竞争。

### 2.4 C++ 核心类定义规范
```cpp
namespace hgl::ecs {
    class TransformDataStorage {
    public:
        uint32_t static_count = 0;
        uint32_t dynamic_count = 0;
        uint32_t total_capacity = 0;

        // 扁平连续物理数组
        TransformTRS*           raw_trs_array = nullptr;
        Matrix4x3f*             global_world_matrices = nullptr;
        TransformHierarchyData* hierarchy_array = nullptr;
        uint8_t*                dirty_bits = nullptr; // 每实体 1 字节独立写入

        // SoA 平行数组 (优化 GPU 显存吞吐)
        Vector4f*               soa_translations = nullptr;
        Vector4f*               soa_rotations = nullptr;
        Vector4f*               soa_scales = nullptr;

        void Initialize(uint32_t max_statics, uint32_t max_dynamics);
        
        inline void MarkDirty(uint32_t id) {
            dirty_bits[id] = 1;
        }

        // 单次拓扑展开线性循环 (仅更新动态区间)
        void UpdateDynamicTransforms();
    };

    extern TransformDataStorage* g_CurrentTransformStorage;
}
```

---

## 三、 零成本 TransformAccessor 句柄设计

在 `inc/hgl/ecs/accessors/TransformAccessor.h` 中实现：
* **尺寸严格等于 4 字节**（仅包含 `uint32_t id`），x86-64 ABI 单寄存器零成本传递；
* 内部所有读写方法完全内联直写 `TransformDataStorage`。

```cpp
#pragma once
#include <hgl/ecs/support/TransformDataStorage.h>

namespace hgl::ecs {
    class TransformAccessor {
    public:
        uint32_t id = ~0u;

        constexpr TransformAccessor() = default;
        constexpr explicit TransformAccessor(uint32_t in_id) : id(in_id) {}

        bool IsValid() const { return id != ~0u; }

        Vector3f GetPosition() const {
            return g_CurrentTransformStorage->raw_trs_array[id].translation.xyz();
        }

        Quaternionf GetRotation() const {
            const auto& q = g_CurrentTransformStorage->raw_trs_array[id].rotation;
            return Quaternionf(q.x, q.y, q.z, q.w);
        }

        Vector3f GetScale() const {
            return g_CurrentTransformStorage->raw_trs_array[id].scale.xyz();
        }

        void SetPosition(const Vector3f& pos) {
            g_CurrentTransformStorage->raw_trs_array[id].translation = Vector4f(pos, 0.0f);
            g_CurrentTransformStorage->MarkDirty(id);
        }

        void SetRotation(const Quaternionf& rot) {
            g_CurrentTransformStorage->raw_trs_array[id].rotation = Vector4f(rot.x, rot.y, rot.z, rot.w);
            g_CurrentTransformStorage->MarkDirty(id);
        }

        void SetScale(const Vector3f& scale) {
            g_CurrentTransformStorage->raw_trs_array[id].scale = Vector4f(scale, 1.0f);
            g_CurrentTransformStorage->MarkDirty(id);
        }

        const Matrix4x3f& GetWorldMatrix() const {
            return g_CurrentTransformStorage->global_world_matrices[id];
        }
    };
    static_assert(sizeof(TransformAccessor) == 4, "TransformAccessor must be exactly 4 bytes");
}
```

---

## 四、 迁移平替与废除 TransformComponent

1. 在 `Entity` 中暂时保留 `TransformAccessor transform` 字段；
2. 全局搜索并删除 `TransformComponent.h` 及其虚基类依赖；
3. 将现有的 `entity->GetComponent<TransformComponent>()` 全面平替为 `entity->GetTransform()`；
4. 渲染收集阶段直接取出 `transform.id` 作为渲染项 4-ID 中的 `TransformID`。

---

## 五、 AI Coding Agent 落地任务清单 (Task Checklist)

* [ ] **Task 1.1**：重构 `TransformDataStorage.h/.cpp`，实现 `[0, static_count)` 与 `[static_count, total)` 分区，移除 `ActiveRowLease`。
* [ ] **Task 1.2**：在 Storage 中实现 `uint8_t* dirty_bits` 独立字节标记，并添加 SoA 平行数组支持。
* [ ] **Task 1.3**：实现 `inc/hgl/ecs/accessors/TransformAccessor.h`，确保 `sizeof(TransformAccessor) == 4` 且所有读写函数内联。
* [ ] **Task 1.4**：废除 `TransformComponent.h`，将全工程业务层统一改为通过 `TransformAccessor` 访问。
* [ ] **Task 1.5**：运行变换系统压力测试，验证 10 万个实体的动态平移与旋转在无堆分配下正常刷新且渲染正常。

