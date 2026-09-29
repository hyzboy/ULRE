# ULRE 架构重构 - 阶段三：64 字节 Entity 与全场景二进制直载 (Blit-Loading) 技术规范 (Phase 3 Specification)

## 一、 阶段目标与背景 (Goals & Background)

在前两阶段完成全子系统的 ID 化与 Accessor 化后，系统已具备了终极收敛的前提条件：
* **阶段目标**：
  1. 彻底固化 `Entity` 为严格 64 字节（对齐 1 个 CPU L1 Cache Line）的紧凑结构体，包含代数（Generation）安全防线与 SIMD 并行匹配；
  2. 确立无指针、零解析的场景文件格式标准（`.ulrescene`）；
  3. 实现全场景内存镜像直载（Blit-Loading via `mmap` / 一次性 `fread`），达到 10 毫秒级瞬时场景还原并直推 GPU。

---

## 二、 64 字节 Entity 物理结构设计

### 2.1 内存布局规范（严格对齐单个 Cache Line）
在 `inc/hgl/ecs/core/Entity.h` 中彻底重构 `Entity`：
* **16 字节元数据区**：
  * `uint32_t entity_index`：全局下标；
  * `uint16_t generation`：句柄代数版本（防止实体被销毁复用后产生僵尸句柄/悬垂引用）；
  * `uint16_t flags`：Active, Static, PendingDestroy 等标志位；
  * `uint32_t layer_mask`：渲染图层、物理层碰撞掩码；
  * `uint32_t string_name_offset`：指向文件末尾 `StringPool` 的实体名称偏移；
* **48 字节属性表区**：
  * `uint8_t keys[8]`：8 个属性类型（占用 64 位整数，支持 SIMD 单周期并发匹配）；
  * `uint32_t values[8]`：8 个属性 ID；
  * `uint8_t reserved[8]`：保留填充，确保结构严格等于 64 字节。

### 2.2 C++ 核心代码实现
```cpp
#pragma once
#include <cstdint>
#include <hgl/ecs/accessors/TransformAccessor.h>
#include <hgl/ecs/accessors/GeometryAccessor.h>
#include <hgl/ecs/accessors/MaterialAccessor.h>

namespace hgl::ecs {
    enum class ComponentType : uint8_t {
        Empty       = 0,
        Transform   = 1,
        Geometry    = 2,
        Material    = 3,
        Camera      = 4,
        Light       = 5,
        Physics     = 6,
        Audio       = 7,
        OverflowPtr = 0xFF // 极少数超限实体挂接二级扩展表
    };

    struct alignas(64) Entity {
        // 元数据区 (16 字节)
        uint32_t entity_index;
        uint16_t generation;         // 代数防僵尸句柄
        uint16_t flags;
        uint32_t layer_mask;
        uint32_t string_name_offset; // 指向 StringPool 的名称偏移

        // 属性表区 (48 字节)
        uint8_t  keys[8];            // 支持 SIMD 并发匹配 (8 字节刚好为一个 uint64)
        uint32_t values[8];          // 对应的 8 个属性 ID
        uint8_t  reserved[8];        // 保留对齐

        inline uint32_t GetID(ComponentType type) const {
            const uint8_t target_key = static_cast<uint8_t>(type);
            for (int i = 0; i < 8; ++i) {
                if (keys[i] == target_key) return values[i];
                if (keys[i] == 0) break; // 遇空槽提前退出
            }
            return ~0u;
        }

        inline void SetID(ComponentType type, uint32_t value) {
            const uint8_t target_key = static_cast<uint8_t>(type);
            for (int i = 0; i < 8; ++i) {
                if (keys[i] == target_key || keys[i] == 0) {
                    keys[i] = target_key;
                    values[i] = value;
                    return;
                }
            }
        }

        // 零开销访问器调用接口
        TransformAccessor GetTransform() const {
            return TransformAccessor(GetID(ComponentType::Transform));
        }
        GeometryAccessor GetGeometry() const {
            return GeometryAccessor(GetID(ComponentType::Geometry));
        }
        MaterialAccessor GetMaterial() const {
            return MaterialAccessor(GetID(ComponentType::Material));
        }
    };
    static_assert(sizeof(Entity) == 64, "Entity must be exactly 64 bytes (1 Cache Line)");
}
```

---

## 三、 全场景二进制文件规范 (`.ulrescene`)

因为全系统完全消灭了内部指针，场景文件可以以内存镜像格式存储：

### 3.1 内存对齐硬约束
* **Header 硬对齐**：`SceneHeader` 必须通过保留字段补齐为 **64 字节或 128 字节** 的整数倍，确保内存映射（`mmap`）时紧随其后的 `Entity[0]` 起始地址绝对 64 字节对齐，100% 契合 `alignas(64)` 硬件加速。
* **StringPool 外置**：实体调试名称与资产相对路径统一放入末尾的 `[StringPool Block]`，实体内仅保存 `string_name_offset`。

### 3.2 文件块组织结构
```text
[文件头 Header] (128 Bytes，严格对齐补齐 Padding)
  - Magic: "ULRESCN\0" (8 bytes)
  - Version: uint32_t
  - EntityCount: uint32_t
  - StaticTransformCount: uint32_t
  - DynamicTransformCount: uint32_t
  - GeometryCount: uint32_t
  - MaterialCount: uint32_t
  - Reserved / Padding: 保证 Header 严格对齐 128B

[数据块 1: Entity Block]
  - 连续平铺的 Entity[EntityCount] (尺寸 = EntityCount * 64 字节)

[数据块 2: Transform Block]
  - 连续平铺的 TransformTRS[StaticCount + DynamicCount] (尺寸 = N * 48 字节)
  - 连续平铺的 TransformHierarchyData[N] (尺寸 = N * 8 字节)

[数据块 3: Geometry Meta Block]
  - 连续平铺的 GeometryDescriptor[GeometryCount]

[数据块 4: Material Block]
  - 连续平铺的 MaterialData[MaterialCount] (直接匹配 GPU std430 布局)

[数据块 5: StringPool Block]
  - 存放实体调试名称与资产相对路径，通过 string_name_offset 寻址
```

---

## 四、 运行时零反序列化直载器 (Blit Loading / mmap)

### 4.1 核心加载实现
```cpp
void LoadSceneBlit(const char* filepath) {
    int fd = open(filepath, O_RDONLY);
    struct stat sb;
    fstat(fd, &sb);

    // 1. 内存直接映射 (Zero-Copy)
    void* mapped = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    const auto* header = reinterpret_cast<const SceneHeader*>(mapped);
    const char* ptr = reinterpret_cast<const char*>(mapped) + sizeof(SceneHeader);

    // 2. 零反序列化：直接绑定实体平铺数组指针
    g_Entities = reinterpret_cast<Entity*>(const_cast<char*>(ptr));
    ptr += header->entity_count * sizeof(Entity);

    // 3. 绑定 Transform 物理存储
    g_CurrentTransformStorage->raw_trs_array = reinterpret_cast<TransformTRS*>(const_cast<char*>(ptr));
    ptr += (header->static_transform_count + header->dynamic_transform_count) * sizeof(TransformTRS);
    
    g_CurrentTransformStorage->hierarchy_array = reinterpret_cast<TransformHierarchyData*>(const_cast<char*>(ptr));
    ptr += (header->static_transform_count + header->dynamic_transform_count) * sizeof(TransformHierarchyData);

    // 4. 将预烘焙的静态矩阵与材质数据块通过 DMA (vkCmdCopyBuffer) 一巴掌直推 GPU SSBO
    // ...
}
```

---

## 五、 AI Coding Agent 落地任务清单 (Task Checklist)

* [ ] **Task 3.1**：在 `inc/hgl/ecs/core/Entity.h` 中实现 16B 元数据 + 48B 属性表的 64 字节结构，添加 `alignas(64)` 及 `static_assert`。
* [ ] **Task 3.2**：编写离线场景导出工具（Cooker），将场景扁平化并序列化为 `.ulrescene` 二进制镜像，确保 Header 严格 128B 对齐，并生成 `StringPool Block`。
* [ ] **Task 3.3**：实现 `LoadSceneBlit`（基于 `mmap` / 一次性 `fread`），验证 5 万个实体的超大场景在 10 毫秒内瞬时恢复。
* [ ] **Task 3.4**：实现全场景数据一次性 `vkCmdCopyBuffer` 直推 GPU 的管线链路，验证 Vulkan 4-ID 渲染流程零卡顿正常运行。

