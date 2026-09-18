# Mesh Shader 局域网格顶点复用与 Meshlet 架构演进方案

- 日期：2026-09-18
- 状态：方案设计 / 架构提案
- 关联模块：`inc/hgl/mtl/MeshShaderMode.h`、`src/ShaderGen/meshgen/`、`ShaderLibrary/mesh/`、`src/ecs/support/PrimitiveBatchPipeline.cpp`

---

## 1. 背景与现状瓶颈

当前 ULRE 引擎已将 Mesh Shader 作为**唯一顶点处理阶段**，彻底废弃了传统 VS + VBO 管线。在通用网格渲染中，主要依赖 `MeshShaderMode::VertexPassthrough` 模式。

### 1.1 核心痛点：缺乏局域顶点复用

- **“伪 Mesh”模型**：`VertexPassthrough` 本质上是针对旧 Vertex Shader 语义的仿真——每个线程处理 1 个顶点，无论该顶点是否在三角形间被共享，均被无差别读取、执行 `LoadVertexData()` 与变换。
- **顶点重复变换与带宽浪费**：在带索引的闭合 3D 网格中，根据欧拉示性数推导，平均每个空间顶点被 **4 ~ 6 个三角形** 共享。在 `VertexPassthrough` 下，相同索引的顶点在各个线程中被重复读取、计算世界变换矩阵和投影变换，导致 **60% ~ 75% 的顶点计算与带宽完全冗余**。
- **无法发挥硬件 Meshlet 局部缓存优势**：现代 GPU（NVIDIA Turing+、AMD RDNA2+、Intel Arc）针对 32~64 顶点的局部 Meshlet 提供了优化的片上共享内存和图元分配器。

---

## 2. Meshlet 核心架构设计

Meshlet 方案的核心思想是：**将大网格离线/加载期预先剖分为包含 ≤64 独立顶点、≤124 三角形的局部微网格（Meshlet）**。Mesh Shader 的 1 个 WorkGroup 负责处理 1 个完整的 Meshlet。

```
[离线预处理 / 资产导入: meshoptimizer]
   原始 Vertex/Index Buffers
            │
            ▼ (meshopt_buildMeshlets)
   拆解为多个 Meshlet (≤64 唯一顶点, ≤124 局域三角形)
            │
            ▼
[GPU 存储结构: BDA 紧凑线性排列]
   1. MeshletDescriptor 表 : { vertex_offset, triangle_offset, vertex_count, triangle_count }
   2. MeshletVertexIndices : uint32 全局顶点索引流 (0..N)
   3. MeshletMicroTriangles: uint8 局域微索引三元组 (u8vec3, 0..63)
            │
            ▼
[运行时 GPU 执行: vkCmdDrawMeshTasksIndirectEXT]
   gl_WorkGroupID.x (1 Meshlet)
   ├── 64 线程协作变换 64 个唯一顶点 (每人 1 个，绝无重复计算) ──► gl_MeshVerticesEXT
   └── 64 线程协作解包 124 个微三角形索引 ────────────────────────► gl_PrimitiveTriangleIndicesEXT
```

---

## 3. 数据结构契约（BDA 数据模型）

### 3.1 CPU 侧描述符（`inc/hgl/graph/ShaderBufferSources.h`）

每个 Meshlet 的头部信息紧凑设计为 16 字节，保证自然对齐并适配 BDA：

```cpp
struct MeshletDescriptor
{
    uint32_t vertex_offset;    // 在全局 MeshletVertexIndices 中的起始偏移 (单位: uint32)
    uint32_t triangle_offset;  // 在全局 MeshletMicroTriangles 中的起始字节偏移 (单位: byte/u8vec3)
    uint8_t  vertex_count;     // 局域唯一顶点数 (≤ 64)
    uint8_t  triangle_count;   // 局域三角形数 (≤ 124)
    uint16_t reserved;         // 对齐填充 / 可扩展标志位
};
static_assert(sizeof(MeshletDescriptor) == 16, "MeshletDescriptor must be 16 bytes");
```

### 3.2 GLSL 侧 BDA Buffer Reference 声明

通过 `MeshShaderVertexAdapter` 扩展注入 BDA 声明：

```glsl
// Meshlet 描述符表
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MeshletDescriptorRef
{
    MeshletDescriptor data[];
};

// 全局唯一顶点索引重定向表 (uint32)
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MeshletVertexRef
{
    uint data[];
};

// 局域 8-bit 微三角形索引表 (每图元 3 字节 uint8)
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MeshletTriangleRef
{
    u8vec3 data[];
};
```

`MeshDrawParams` 扩展追加三个 BDA 地址字段：
```cpp
uint64_t addr_meshlets;          // MeshletDescriptorRef
uint64_t addr_meshlet_vertices;  // MeshletVertexRef
uint64_t addr_meshlet_triangles; // MeshletTriangleRef
```

---

## 4. ShaderGen 与 GLSL 生成逻辑

### 4.1 新增模式枚举 `MeshShaderMode::Meshlet`

在 `inc/hgl/mtl/MeshShaderMode.h` 中追加模式：
```cpp
enum class MeshShaderMode : uint8_t
{
    VertexPassthrough,   // 通用直通（跨步协作循环）
    LineQuad,            // 屏幕空间线段展开为 Quad
    CharQuad,            // 文字字符展开为 Quad
    Meshlet,             // 局部微网格顶点复用模式 (新增)
};
```

### 4.2 拓扑与容量解析 (`MeshModeDescriptor.h`)

- **工作组线程数**：`max_invocations = 64`（完美贴合 Wave32 与 Wave64）。
- **输出容量声明**：
  - `max_vertices = 64`（每个线程至多加载和变换 1 个唯一顶点）。
  - `max_primitives = 124`（符合大部分硬件 `maxMeshOutputPrimitives` 最佳甜点）。

```cpp
inline bool ResolveMeshletTopology(
    uint32_t max_invocations,
    MeshModeCapacity &out_capacity)
{
    out_capacity.max_vertices   = 64u;
    out_capacity.max_primitives = 124u;
    return true;
}
```

### 4.3 模板设计 (`ShaderLibrary/mesh/meshlet.glsl.tmpl`)

```glsl
    const uint meshlet_idx = gl_WorkGroupID.x;
    const MeshletDescriptor m = sbo_meshlets.data[meshlet_idx];

    SetMeshOutputsEXT(uint(m.vertex_count), uint(m.triangle_count));

    // ── 阶段 1: 局域唯一顶点变换 (64 线程每人负责 ≤ 1 个顶点，绝无冗余计算) ──
    const uint vid = gl_LocalInvocationIndex;
    if (vid < uint(m.vertex_count))
    {
        // 查表取得全局绝对顶点索引
        MeshVertexIndex = sbo_meshlet_vertices.data[m.vertex_offset + vid];
        LoadVertexData();
{{varying_outputs}}
    }

    // ── 阶段 2: 8-bit 微索引解包 (124 三角形，64 线程通过跨步循环展开，平均每线程处理 2 个) ──
    for (uint p = gl_LocalInvocationIndex; p < uint(m.triangle_count); p += 64u)
    {
        const u8vec3 tri = sbo_meshlet_triangles.data[m.triangle_offset + p];
        gl_PrimitiveTriangleIndicesEXT[p] = uvec3(uint(tri.x), uint(tri.y), uint(tri.z));
    }
```

---

## 5. 几何资产与管线调度分流策略

引擎采用**双轨共存**策略，按几何特性自动分流：

| 特性维度 | `MeshShaderMode::Meshlet` | `MeshShaderMode::VertexPassthrough` (优化版) |
|---|---|---|
| **适用网格类型** | 静态复杂网格、高密模型、静态场景道具 | 动态程序化生成网格（如草皮、简易面片、即时调试几何体） |
| **数据前置条件** | 需经 `meshopt_buildMeshlets` 预处理 | 直接使用常规 Vertex/Index Buffer 即可 |
| **顶点计算冗余** | **0%**（唯一顶点只变换一次） | 存在冗余（按面片重复抓取） |
| **GPU 吞吐优势** | 节省 60%~75% 顶点处理功耗与带宽 | 零预处理开销，轻量网格无额外间接寻址成本 |

---

## 6. 后续演进路线（Roadmap）

1. **Phase 1: 离线 Meshlet 工具链接入**
   - 在 `CMAssetsManage` 或模型导入管线引入 `meshoptimizer`，导出 Meshlet 连续二进包。
2. **Phase 2: ShaderGen 与运行时适配**
   - 在 `MeshModeDescriptor` 中注册 `Meshlet` 模式，实现 `meshlet.glsl.tmpl`。
   - `PrimitiveBatchPipeline` 中新增对 Meshlet 几何缓冲的间接命令组数分发（`groupCountX = meshlet_count`）。
3. **Phase 3: Task Shader (Amplification) 联动**
   - 增加可选的 Task Shader 阶段，在 GPU 侧依据 Meshlet 包围盒（Bounding Cone & Sphere）进行剔除与动态 LOD 选择。
