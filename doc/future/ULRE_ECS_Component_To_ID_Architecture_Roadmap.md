# ULRE 引擎架构演进：从 OOP Component 到全 ID + Accessor 与二进制场景的重构技术规范 (Technical Specification & Implementation Roadmap)

## 一、 背景与核心设计目标 (Background & Goals)

### 1.1 现状与痛点
当前 ULRE (Universal Lightweight Rendering Engine) 的 ECS 模块仍存在较重的面向对象组件封装（如 `TransformComponent`, `RenderableComponent`, `MaterialComponent` 等）。
* **内存膨胀与碎片**：每个组件独立堆分配，携带虚表指针（vptr）、父子容器指针（`std::vector`）及继承元数据，导致 48 字节的 TRS 变换膨胀至 120~160 字节，在数万实体规模下造成严重的内存浪费与 CPU Cache Miss。
* **数据二次搬运**：逻辑层更新组件后，需由各 System 遍历组件并拉平同步到底层 Storage（如 `TransformDataStorage`），再打包上传 GPU。

### 1.2 演进目标
1. **贯彻 4-ID GPU-Driven 闭环**：以 `(TransformID, MaterialID, GeometryID, PipelineID)` 为核心，CPU 实体与 GPU SSBO 物理槽位一一对应。
2. **消灭 OOP Component 堆实体**：将所有组件重构成“**连续平铺存储（Storage Array）+ 4 字节整型 ID + 轻量瞬态访问器（Flyweight Accessor）**”。
3. **64 字节 Cache-Line 实体与二进制原位直载（Blit Loading / mmap）**：最终将 `Entity` 固化为严格对齐 64 字节的定长键值对数组 `KeyValuePair<uint32_t, uint32_t> ID_LIST[8]`，支持无指针、零解析的整块文件直接映射加载。

---

## 二、 分阶段实施路线图 (Evolution Roadmap)

为确保项目稳定过渡，重构必须按严格的时序依赖推进：

```text
[阶段零：前置基准]
  Transform 局部数据 Matrix4f -> TRS 统一与 GLTF 仿射极分解器实现 (DecomposeMatrixToTRS)
        │
        ▼
[阶段一：核心基石] 
  Transform 体系 ID 化与 Accessor 重构 (TransformComponent -> TransformID + TransformAccessor)
        │
        ▼
[阶段二：全面推广]
  全子系统 Component 的 ID 化与访问器迁移 (Geometry, Material, Camera, Visibility, Light 等)
        │
        ▼
[阶段三：终极收敛]
  64 字节 Entity 结构升级与二进制场景 Blit-Loading / mmap 落地
```

---

## 三、 阶段零：Transform 局部数据由 Matrix4f 迁移至 TRS 详细规范

### 3.1 GLTF 导入期仿射矩阵极分解 (DecomposeMatrixToTRS)
在 GLTF 导入阶段，针对包含局部 `Matrix4f` 的节点，必须通过极分解将其转换为统一的 `TransformTRS` 结构。分解算法步骤如下：
1. **平移提取**：直接提取矩阵的第四列/平移分量。
2. **缩放提取与镜像检测**：提取前三列基向量的模长作为缩放分量。检测矩阵行列式，若 `det < 0` 则说明存在镜像/负缩放，需翻转相应缩放分量与基向量。
3. **旋转四元数转换**：将归一化后的正交旋转矩阵转换为四元数。需注意避免 GLM `(w, x, y, z)` 构造顺序与 glTF `[x, y, z, w]` 数组顺序不一致导致的陷阱。

```cpp
bool DecomposeMatrixToTRS(const Matrix4f& mat, TransformTRS& out_trs) {
    // 1. 平移提取
    out_trs.translation = Vector4f(mat[3].xyz(), 0.0f);

    // 2. 提取基向量并计算缩放模长
    Vector3f col0 = mat[0].xyz();
    Vector3f col1 = mat[1].xyz();
    Vector3f col2 = mat[2].xyz();

    Vector3f scale(col0.Length(), col1.Length(), col2.Length());

    // 负缩放 / 镜像检测 (det < 0)
    if (mat.Determinant() < 0.0f) {
        scale.x = -scale.x;
        col0 = -col0;
    }
    out_trs.scale = Vector4f(scale, 1.0f);

    // 3. 归一化正交基并提取旋转四元数
    Matrix3f rot_mat(col0 / scale.x, col1 / scale.y, col2 / scale.z);
    Quaternionf q(rot_mat); // 注意: 保证内存存储格式严格匹配 [x, y, z, w]
    out_trs.rotation = Vector4f(q.x, q.y, q.z, q.w);
    return true;
}
```

### 3.2 局部存储与世界输出的职责拆分
* **局部变换 (Local Transform)**：全面统一为 48 字节的 `TransformTRS` 结构，作为内存存储与编辑的基本单元，彻底废除局部 `Matrix4f`。
* **世界变换 (World Transform)**：仅在层级求值与最终渲染输出阶段，将 `TransformTRS` 转化为 `Matrix4x3f` 全局世界矩阵，供 GPU 渲染与视锥剔除使用。

### 3.3 阶段零验证标准
1. 所有 GLTF 模型（无论原始数据使用 TRS 还是 `matrix` 矩阵定义）在导入后，局部数据均能无损转换为 `TransformTRS`。
2. 包含负缩放与镜像变换的模型，分解后的四元数与渲染效果与原生 `Matrix4f` 保持完全一致。

---

## 四、 阶段一：Transform 体系 ID 与 Accessor 重构详细规范

### 4.1 物理数据结构（C++ 与 GPU std430 严格对齐）
在 `inc/hgl/ecs/support/TransformTypes.h` 中定义原生 48 字节 TRS 结构：

```cpp
#pragma once
#include <hgl/math/Vector.h>
#include <hgl/math/Quaternion.h>
#include <hgl/math/Matrix.h>

namespace hgl::ecs {
    // 严格 48 字节对齐，跨平台无歧义 (3 个 vec4)
    struct alignas(16) TransformTRS {
        Vector4f translation; // xyz: 平移, w: 保留
        Vector4f rotation;    // xyzw: 旋转四元数 (qx, qy, qz, qw)
        Vector4f scale;       // xyz: 缩放, w: 保留
    };
    static_assert(sizeof(TransformTRS) == 48, "TransformTRS must be exactly 48 bytes");

    // 变换节点父子拓扑元数据 (与变换数据解耦平铺)
    struct TransformHierarchyData {
        uint32_t parent_id;   // 父节点 TransformID (根节点为 ~0u)
        uint32_t depth;       // 树深度
    };
}
```

### 4.2 TransformDataStorage 的动静物理分区与连续存储设计
修改 `inc/hgl/ecs/support/TransformDataStorage.h`：
* **动静严格分区**：
  * $[0, \text{static\_count})$：静态物体（房屋、地形、植被），初始化/关卡加载时计算一次，运行时免刷新。
  * $[\text{static\_count}, \text{static\_count} + \text{dynamic\_count})$：动态/骨骼物体，运行时每帧刷新。
* **AoS 与 SoA 选型**：底层同时提供 SoA 平行数组（`Vector4f* translations`, `Vector4f* rotations`, `Vector4f* scales`），供未来的 Compute Shader 实现 100% 显存合并访存（Memory Coalescing）。
* **无数据竞争脏标记**：`dirty_bits` 必须采用 `uint8_t` 数组（每个实体 1 字节独立写入），避免多线程并发修改邻近 bit 发生竞争。
* **运行时禁止动态扩容**：关卡加载或 Editor 构建期分配固定最大容量，消除显存重分配与 GPU BDA 指针失效。
```cpp
namespace hgl::ecs {
    class TransformDataStorage {
    public:
        uint32_t static_count = 0;
        uint32_t dynamic_count = 0;
        uint32_t total_capacity = 0;

        // 扁平连续物理数组 (供 CPU/GPU 直推)
        TransformTRS*           raw_trs_array = nullptr;
        Matrix4x3f*             global_world_matrices = nullptr;
        TransformHierarchyData* hierarchy_array = nullptr;
        uint8_t*                dirty_bits = nullptr; // 每实体 1 字节，保证多线程并发安全

        // SoA 平行数组指针 (与 raw_trs_array 映射或独立提供，优化 GPU 访存)
        Vector4f*               soa_translations = nullptr;
        Vector4f*               soa_rotations = nullptr;
        Vector4f*               soa_scales = nullptr;

        void Initialize(uint32_t max_statics, uint32_t max_dynamics);
        void MarkDirty(uint32_t id) {
            dirty_bits[id] = 1;
        }
        
        // 单次线性循环无递归刷新动态区间
        void UpdateDynamicTransforms();
    };

    // 全局/当前活动场景的 Storage 访问中枢
    extern TransformDataStorage* g_CurrentTransformStorage;
}
```

### 4.3 零成本 TransformAccessor 句柄实现
新增 `inc/hgl/ecs/accessors/TransformAccessor.h`：
* 内部**仅存储 4 字节的 `uint32_t id`**。
* 传参零成本（单寄存器传递），所有函数内联直写 `TransformDataStorage`。

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
}
```

### 4.4 阶段一迁移任务
1. 在原有 `Entity` 中暂时保留 `TransformAccessor transform` 字段。
2. 彻底删除 `TransformComponent.h` 及其对 `Component` 虚类的继承。
3. 将现有所有系统的 `entity->GetComponent<TransformComponent>()` 替换为 `entity->GetTransform()`。

---

## 五、 阶段二：全子系统 Component 的 ID 化与访问器迁移详细规范

在 Transform 跑通后，将其余所有子组件按相同范式彻底平铺化与 ID 化：

### 5.1 Geometry 体系（Mesh / Primitive）
* **废弃**：`PrimitiveComponent`, `RenderableComponent`。
* **数据收敛**：`GeometryManager` 维护紧凑的 `GeometryDescriptor` 数组。
* **定义**：
  ```cpp
  struct GeometryAccessor {
      uint32_t id = ~0u;
      // 读取顶点布局、Meshlet 起始偏移、IndexBuffer 偏移、Local AABB
      const AABB& GetLocalBounds() const;
      uint32_t GetIndexCount() const;
  };
  ```

### 5.2 Material 体系
* **废弃**：`MaterialComponent`。
* **数据收敛**：`MaterialSSBO` 连续内存块。实体的 MaterialID 直接对应 GPU 材质数据表的行号。
* **定义**：
  ```cpp
  struct MaterialAccessor {
      uint32_t id = ~0u;
      void SetBaseColor(const Vector4f& color);
      void SetRoughness(float r);
      void SetMetallic(float m);
  };
  ```

### 5.3 渲染与可见性体系
* **废弃**：`BoundingBoxComponent`, `VisibilityComponent`。
* **数据收敛**：
  * 在 `BoundingBoxDataStorage` 中存放连续的 `WorldAABB[TotalCount]`（甚至可做 SoA 向量化）；
  * 视锥剔除 Compute/CPU 系统只基于连续数组计算，输出紧凑的 `DrawItem` 列表，完全不访问 Entity。

---

## 六、 阶段三：64 字节 Entity 结构升级与二进制场景直载 (Blit Loading)

### 6.1 实体物理定义（严格对齐 1 个 Cache Line）
在 `inc/hgl/ecs/core/Entity.h` 中彻底重构 `Entity`：
* **元数据区 (16 字节)**：`uint32_t entity_index`、`uint16_t generation` (代数防僵尸句柄)、`uint16_t flags`、`uint32_t layer_mask`、`uint32_t string_name_offset`。
* **保持对齐**：`alignas(64)` 和 `static_assert(sizeof(Entity) == 64)`。
* **极速匹配**：`keys[8]` 占用 64 位整数，可使用 SIMD 指令（如 `_mm_cmpeq_epi8`）在 1 个时钟周期内完成并行匹配。
```cpp
#pragma once
#include <cstdint>
#include <hgl/ecs/accessors/TransformAccessor.h>
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
        OverflowPtr = 0xFF
    };
    // 严格 64 字节，对齐单个 CPU L1 Cache Line
    struct alignas(64) Entity {
        // 元数据区 (16 字节)
        uint32_t entity_index;
        uint16_t generation;         // 代数防僵尸句柄
        uint16_t flags;
        uint32_t layer_mask;
        uint32_t string_name_offset; // 指向 StringPool Block 的名称偏移
        uint8_t  keys[8];            // SIMD 极速并发匹配
        uint32_t values[8];
        uint8_t  reserved[8];
        inline uint32_t GetID(ComponentType type) const {
            const uint8_t target_key = static_cast<uint8_t>(type);
            for (int i = 0; i < 8; ++i) {
                if (keys[i] == target_key) return values[i];
                if (keys[i] == 0) break;
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
### 6.2 全场景二进制文件规范（`.ulrescene`）
* **Header 硬对齐**：`SceneHeader` 必须按 64 字节或 128 字节严格对齐补齐（Padding），确保文件 `mmap` 时紧随其后的 `Entity[0]` 不发生内存错位，100% 契合 `alignas(64)`。
* **StringPool 外置**：实体调试名称和资产相对路径统一放入末尾的 `[StringPool Block]`。

```text
[文件头 Header] (64 Bytes 或 128 Bytes 严格对齐补齐 Padding)
  - Magic: "ULRESCN\0" (8 bytes)
  - Version: uint32_t
  - EntityCount: uint32_t
  - StaticTransformCount: uint32_t
  - DynamicTransformCount: uint32_t
  - GeometryCount: uint32_t
  - MaterialCount: uint32_t
  - Reserved / Padding: 保证 Header 严格对齐 64B/128B

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
### 6.3 运行时零反序列化加载器 (Blit Loading / mmap)
```cpp
void LoadSceneBlit(const char* filepath) {
    int fd = open(filepath, O_RDONLY);
    struct stat sb;
    fstat(fd, &sb);
    
    // 1. 内存直接映射
    void* mapped = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    const auto* header = reinterpret_cast<const SceneHeader*>(mapped);
    const char* ptr = reinterpret_cast<const char*>(mapped) + sizeof(SceneHeader);
    
    // 2. 零反序列化：直接绑定指针
    g_Entities = reinterpret_cast<Entity*>(const_cast<char*>(ptr));
    ptr += header->entity_count * sizeof(Entity);
    
    // 3. 将 Transform Block 原位移入或直推 GPU SSBO
    g_CurrentTransformStorage->raw_trs_array = reinterpret_cast<TransformTRS*>(const_cast<char*>(ptr));
    // ...
    
    // 4. 将预烘焙的静态矩阵与材质数据一次性 DMA 直推 GPU (vkCmdCopyBuffer)
}
```

---

## 七、 AI Coding Agent 落地实施任务清单 (Actionable Task Checklist)

给后续执行开发的 AI Agent 规划的具体操作文件与验收标准：

### Task 0: 实现 DecomposeMatrixToTRS 与 GLTF 局部 Matrix4f 清理
* [ ] 实现 `DecomposeMatrixToTRS` 仿射极分解函数，支持平移、模长提取、`det < 0` 镜像检测与四元数转换。
* [ ] 在 GLTF 导入流程中全面应用该分解器，彻底消灭局部 `Matrix4f` 存储。

### Task 1: Transform 基础类型与 Accessor 建立
* [ ] 创建 `inc/hgl/ecs/support/TransformTypes.h`，定义 48 字节 `TransformTRS` 并加入 `static_assert`。
* [ ] 创建 `inc/hgl/ecs/accessors/TransformAccessor.h`，实现只包含 `uint32_t id` 的访问器代理。
* [ ] 重构 `inc/hgl/ecs/support/TransformDataStorage.h`，废弃 `ActiveRowLease`，改为连续 `TransformTRS[]` 与动静分区分界点（`static_count`），同时提供 SoA 平行数组并使用 `uint8_t` 数组实现并发安全的 `dirty_bits`。
### Task 2: 废除 TransformComponent 并做业务平替
* [ ] 搜索工程中所有 `TransformComponent.h` 的引用。
* [ ] 修改 `example/Geometry/` 中的用例，将变换创建与获取改为调用 `TransformAccessor`。
* [ ] 编译通过并运行，确保单材质与复合网格的矩阵运算与此前完全一致。

### Task 3: 其余组件 ID 化推进
* [ ] 创建 `GeometryAccessor.h` 与 `MaterialAccessor.h`。
* [ ] 剥离 `RenderableComponent` 与 `MaterialComponent` 中的面向对象多态，建立对应的 Accessor 访问机制。

### Task 4: 64 字节 Entity 与二进制导出导入验证
* [ ] 修改 `Entity.h`，实现 16B 元数据区（含代数防僵尸句柄与 string_name_offset）与 48B 属性表区（`uint8_t keys[8]`, `uint32_t values[8]`, `reserved[8]`），确保 `alignas(64)` 及 `static_assert(sizeof(Entity) == 64)`，并引入 SIMD 匹配优化。
* [ ] 编写离线序列化工具，把测试场景导出为 `.ulrescene` 二进制文件，确保 SceneHeader 按 64B/128B 对齐补齐，并包含末尾的 `[StringPool Block]`。
* [ ] 编写 `mmap` / `fread` 直载函数，验证场景在 10 毫秒内瞬时还原并在 Vulkan 中正常绘制。

