# Meshlet 几何数据管道与压缩顶点格式实现规范

- **文档日期**：2026-09-18
- **状态**：已落地实现 (Production Implementation)
- **涵盖范围**：顶点压缩、离线 Meshlet 剖分、MiniPack 存储规范、运行时自适应加载与 BDA 调度双轨分流

---

## 1. 架构总览与设计目标

随着 ULRE 引擎全面采用 Mesh Shader 作为唯一顶点处理阶段并引入 GPU-Driven BDA 架构，原有的未压缩顶点流和传统 `VertexPassthrough` 跨步循环暴露出两个核心问题：
1. **顶点数据带宽过大**：传统 float3 法线（12B）与 float2 UV（8B）以及无用切线（16B）造成显存和带宽浪费。
2. **缺乏局域顶点复用**：`VertexPassthrough` 仿真旧顶点着色器导致相同顶点在共享三角形中被重复变换 4~6 次（约 60%~75% 顶点计算冗余）。

为此，引擎完成了两大核心能力的落地：
1. **标准压缩顶点格式规范**（`VF_V3F` + `VF_V2HF` + `VF_V2UN8` / `VF_V2HF` + `U32 索引`）。
2. **可选 Meshlet 剖分与自适应双轨渲染管道**（离线 `meshoptimizer` 剖分 $\to$ MiniPack 增量存储 $\to$ `LoadGeometry` 自动识别 $\to$ `PrimitiveBatchPipeline` 双轨分流）。

---

## 2. 顶点格式规范与编码算法

### 2.1 引擎标准顶点格式（`CreateStandardGeometryVertexFormat`）

| 属性语义 | 格式宏 | 物理 Vulkan 格式 | 步长 | 编码/量化方式 | 状态 |
|---|---|---|---|---|---|
| `Position` | `VF_V3F` | `VK_FORMAT_R32G32B32_SFLOAT` | 12B | 原始 float3 空间坐标 | 强制基础流 |
| `TexCoord` | `VF_V2HF` | `VK_FORMAT_R16G16_SFLOAT` | 4B | IEEE 754 半精度 half2 (`FloatToHalf`) | 强制基础流 |
| `Normal` | `VF_V2UN8` | `VK_FORMAT_R8G8_UNORM` | 2B | 八面体编码 + uint8 量化 ([0, 255]) | 当前默认 |
| `Normal` | `VF_V2HF` | `VK_FORMAT_R16G16_SFLOAT` | 4B | 八面体编码 + half2 (高精度) | 预留可选支持 |
| `Tangent` | - | - | 0B | 默认剔除，着色器由法线贴图/屏幕空间求导重建 | 可选保留 |
| `Indices` | `IndexType::U32` | `uint32_t` | 4B | 统一展开为 32 位无符号整数 | 强制统一 |

### 2.2 核心数学算法（`GeometryCreater.h` & `VertexCompression.h`）

#### 1) 八面体法线编码 (`EncodeOctahedralNormal`)
将单位球面法线向量 $(n_x, n_y, n_z)$ 投影并折叠映射到平面八面体坐标 $(p, q) \in [-1, 1]$：
```cpp
inline void EncodeOctahedralNormal(float nx, float ny, float nz, float &out_p, float &out_q)
{
    const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if(len < 0.0001f) { out_p = 0.0f; out_q = 0.0f; return; }
    nx /= len; ny /= len; nz /= len;

    const float ax = std::fabs(nx);
    const float ay = std::fabs(ny);
    const float az = std::fabs(nz);
    const float l1 = ax + ay + az;

    out_p = nx / l1;
    out_q = ny / l1;

    if(nz < 0.0f)
    {
        if(out_p * out_p + out_q * out_q < 1e-6f)
        {
            out_p = 1.0f; out_q = 1.0f;
        }
        else
        {
            out_p = (1.0f - ay / l1) * (nx >= 0.0f ? 1.0f : -1.0f);
            out_q = (1.0f - ax / l1) * (ny >= 0.0f ? 1.0f : -1.0f);
        }
    }
}
```

#### 2) [-1, 1] 到 [0, 255] 量化 (`QuantizeU8`)
```cpp
inline uint8_t QuantizeU8(float v)
{
    const int32_t q = static_cast<int32_t>(std::roundf(v * 127.5f + 127.5f));
    return static_cast<uint8_t>(q < 0 ? 0 : (q > 255 ? 255 : q));
}
```

#### 3) 单精度 float 转半精度 half (`FloatToHalf`)
```cpp
inline uint16_t FloatToHalf(float f)
{
    uint32_t x;
    std::memcpy(&x, &f, sizeof(x));
    const uint32_t sign = (x >> 16) & 0x8000u;
    const int32_t exp   = static_cast<int32_t>((x >> 23) & 0xFF) - 127 + 15;
    uint32_t mant = x & 0x7FFFFFu;

    if(exp <= 0)
    {
        if(exp < -10) return static_cast<uint16_t>(sign);
        mant |= 0x800000u;
        return static_cast<uint16_t>(sign | (mant >> static_cast<uint32_t>(14 - exp)));
    }
    if(exp >= 31) return static_cast<uint16_t>(sign | 0x7C00u);
    return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exp) << 10) | (mant >> 13));
}
```

---

## 3. GLTFConvert 工具与 Meshlet 离线剖分管道

工具源码位于 `src/Tools/GLTFConvert`，通过 CMake 集成了 `meshoptimizer` 子模块。

### 3.1 命令行选项规范
```bash
GLTFConvert [选项] <input.gltf|.glb> [output_dir]
```
- `--meshlet`（默认开启）：启用 Meshlet 微网格剖分与导出。
- `--no-meshlet`：禁用 Meshlet，仅导出常规基础网格流。
- `--normal-v2un8`（默认）：法线导出为八面体量化 `VF_V2UN8`（2B/顶点）。
- `--normal-v2hf`：法线导出为八面体半精度 `VF_V2HF`（4B/顶点）。
- `--normal-v3f`：法线保持原始未压缩 `float3`。
- `--with-tangent`：强制导出切线（默认剔除）。

### 3.2 Meshlet 数据结构契约（16 字节自然对齐）
在 `pure/Meshlet.h` 及引擎 `VKGeometry.h` 中严格统一：
```cpp
#pragma pack(push, 1)
struct MeshletDescriptor
{
    uint32_t vertex_offset;    // 在全局 meshlet_vertices 中的起始偏移 (单位: uint32)
    uint32_t triangle_offset;  // 在全局 meshlet_triangles 中的起始偏移 (单位: u8vec3 / 3 bytes)
    uint8_t  vertex_count;     // 局域唯一顶点数 (<= 64)
    uint8_t  triangle_count;   // 局域微三角形数 (<= 124)
    uint16_t reserved16;       // 对齐填充 / 标志位
    uint32_t reserved32;       // 补齐至 16 字节自然对齐 (适配 BDA buffer_reference_align=16)
};
static_assert(sizeof(MeshletDescriptor) == 16, "MeshletDescriptor must be 16 bytes");

struct MeshletBounds
{
    float center[3];
    float radius;
    float cone_apex[3];
    float cone_axis[3];
    float cone_cutoff;
    int8_t cone_axis_s8[3];
    int8_t cone_cutoff_s8;
};
#pragma pack(pop)
```

### 3.3 剖分构建逻辑 (`CreateGeometry.cpp`)
使用 `meshoptimizer` 执行前置索引与顶点优化后剖分：
1. `max_vertices = 64`，`max_triangles = 124`，`cone_weight = 0.5f`。
2. 调用 `meshopt_buildMeshletsBound` 预估缓冲区容量。
3. 调用 `meshopt_buildMeshlets` 完成聚类与微三角形生成。
4. 调用 `meshopt_computeMeshletBounds` 为每个 Meshlet 计算包围球与剔除锥体。
5. 微三角形偏移存为三角形单位（`triangle_offset = om.triangle_offset / 3`），与 GLSL `u8vec3 data[]` 寻址对齐。

---

## 4. `.geometry` MiniPack 存储规范

MiniPack 是自研的多条目紧凑归档格式。几何文件采用双轨兼容打包方案：

```
.geometry (MiniPack Container)
├── [基础流 - 必选, 双轨共享]
│   ├── GeometryHeader       (14 字节头部：版本、图元类型、顶点数、索引步长、索引数)
│   ├── AttributeMeta        (属性格式数组、属性名长数组、属性名字面量字符串)
│   ├── BoundingVolumes      (AABB/球体/OBB 包围体)
│   ├── POSITION             (R32G32B32_SFLOAT, 12B/顶点)
│   ├── NORMAL               (R8G8_UNORM 2B 或 R16G16_SFLOAT 4B)
│   ├── TEXCOORD_0           (R16G16_SFLOAT 4B)
│   └── indices              (统一为 uint32_t, 供碰撞、光追或 VertexPassthrough 备用)
└── [Meshlet 流 - 可选, 启用 --meshlet 时增量追加]
    ├── meshlets             (MeshletDescriptor 数组, 16 字节/个)
    ├── meshlet_vertices     (局域到全局重定向数组, uint32_t[])
    ├── meshlet_triangles    (8-bit 局域微三角形三元组, u8vec3[])
    └── meshlet_bounds       (MeshletBounds 数组)
```
- **开启 Meshlet 时**：文件包含 11 个子条目。
- **禁用 Meshlet 时**：干净回退为 7 个基础子条目。

---

## 5. 运行时自适应加载与转码 (`LoadGeometry.cpp`)

源码位于 `example/Geometry/LoadGeometry/LoadGeometry.cpp` 与 `example/Geometry/LoadScene/LoadGeometry.cpp`。

### 5.1 语义映射（Semantic-based Lookup）
彻底废弃依赖流索引位置的脆弱遍历，解析 `AttributeMeta` 建立语义索引表：
- `POSITION` / `Position` $\to$ `VertexSemantic::Position`
- `NORMAL` / `Normal` $\to$ `VertexSemantic::Normal`
- `TEXCOORD*` $\to$ `VertexSemantic::TexCoord`

### 5.2 自适应格式匹配与 Fallback 转码
当加载几何体时，根据请求的 `GeometryVertexFormat` 与文件内格式自动分流：
1. **直接匹配**：文件格式 == 请求格式，直接流式读入 VAB。
2. **法线 Fallback 转码**：
   - 文件为旧版 `float3`，目标为 `VF_V2UN8` $\to$ 内存调用 `EncodeNormalsToRG8` 转码为 2B 写入。
   - 文件为旧版 `float3`，目标为 `VF_V2HF` $\to$ 内存调用 `EncodeOctahedralNormal` + `FloatToHalf` 转码写入。
3. **UV Fallback 转码**：
   - 文件为旧版 `float2`，目标为 `VF_V2HF` $\to$ 内存调用 `FloatToHalf` 转码写入。
4. **索引统一展开**：
   - 源文件 `indexStride` 为 1/2 时自动展开为 `uint32_t` 写入 IBO。

### 5.3 Meshlet 自适应探测与 GPU 缓冲挂载
探测 MiniPack 中是否存在 `"meshlets"`：
- 若存在：调用 `device->CreateSSBO(...)` 为 `meshlets`、`meshlet_vertices`、`meshlet_triangles`、`meshlet_bounds` 分配带 BDA 属性的 GPU Storage Buffer，并调用 `geometry->SetMeshlets(...)`。
- 若不存在：`geometry->HasMeshlets()` 返回 `false`，保持纯常规网格。

---

## 6. BDA 数据契约与管线双轨分流调度

### 6.1 `MeshDrawParams` 参数表（112 字节规范）
在 `ShaderBufferSources.h` 中扩展 3 个 64 位 BDA 设备地址字段：
```cpp
#define HGL_MESH_DRAW_PARAMS_FIELD_LIST(M)   \
    M(index_base,             "uint",     uint32_t) \
    M(vertex_base,            "uint",     uint32_t) \
    M(is_indexed,             "uint",     uint32_t) \
    M(total_vertices,         "uint",     uint32_t) \
    M(char_height,            "float",    float)    \
    M(first_instance,         "uint",     uint32_t) \
    M(addr_position,          "uint64_t", uint64_t) \
    M(addr_uv,                "uint64_t", uint64_t) \
    M(addr_ntb,               "uint64_t", uint64_t) \
    M(addr_color,             "uint64_t", uint64_t) \
    M(addr_luminance,         "uint64_t", uint64_t) \
    M(addr_transform_id,      "uint64_t", uint64_t) \
    M(addr_size,              "uint64_t", uint64_t) \
    M(addr_index,             "uint64_t", uint64_t) \
    M(addr_meshlets,          "uint64_t", uint64_t) \
    M(addr_meshlet_vertices,  "uint64_t", uint64_t) \
    M(addr_meshlet_triangles, "uint64_t", uint64_t)
```
- 布局验证：头部 6 字段（24B） + 11 个基址字段（88B） = 112B，经 `static_assert` 编译期校验。

### 6.2 GLSL 适配层 (`MeshShaderVertexAdapter.h`)
在着色器生成器中集中声明 Buffer Reference 及其垫片宏：
```glsl
struct MeshletDescriptor { uint vertex_offset; uint triangle_offset; uint counts; uint reserved; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MeshletDescriptorRef { MeshletDescriptor data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MeshletVertexRef     { uint data[]; };
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MeshletTriangleRef   { u8vec3 data[]; };

#define sbo_meshlets          MeshletDescriptorRef(draw_params.addr_meshlets)
#define sbo_meshlet_vertices  MeshletVertexRef(draw_params.addr_meshlet_vertices)
#define sbo_meshlet_triangles MeshletTriangleRef(draw_params.addr_meshlet_triangles)
```

### 6.3 间接绘制命令双轨分流 (`PrimitiveBatchPipeline.cpp`)
在 `WriteMeshDrawCommands` 中根据几何体特征自动决策工作组数：
```cpp
if (db.geometry && db.geometry->HasMeshlets())
{
    // Meshlet 轨道：1 个 WorkGroup 处理 1 个 Meshlet (64 唯一顶点协作变换)
    mesh_cmd->groupCountX = db.geometry->GetMeshletCount();
}
else
{
    // 通用轨道：按顶点总数计算 64 线程跨步协作组数
    mesh_cmd->groupCountX = CalcMeshGroupCount(is_lines, total_vertices);
}
mesh_cmd->groupCountY = db.instance_count > 1 ? db.instance_count : 1u;
mesh_cmd->groupCountZ = 1u;
```

---

## 7. 跨设备开发与验证指引

在新的开发环境中获取本仓库后，验证与复现步骤如下：

### 7.1 编译转换工具与示例工程
```bash
# 1. 编译引擎及示例（含 LoadGeometry 与 LoadScene）
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug --target LoadGeometry LoadScene

# 2. 编译 GLTFConvert 工具
cmake -B src/Tools/GLTFConvert/build -S src/Tools/GLTFConvert -DCMAKE_TOOLCHAIN_FILE=<path_to_vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build src/Tools/GLTFConvert/build --config Release
```

### 7.2 转换几何模型
```bash
# 默认启用 Meshlet 与 V2UN8 法线压缩
./src/Tools/GLTFConvert/build/Release/GLTFConvert.exe input.glb output_dir

# 禁用 Meshlet（测试传统直通管线）
./src/Tools/GLTFConvert/build/Release/GLTFConvert.exe --no-meshlet input.glb output_dir

# 指定高精度 V2HF 法线压缩
./src/Tools/GLTFConvert/build/Release/GLTFConvert.exe --normal-v2hf input.glb output_dir
```

### 7.3 查看归档结构
```bash
./res/minipack_info.exe output_dir/Model.0.geometry
```
观察输出中是否包含 `meshlets`、`meshlet_vertices`、`meshlet_triangles`、`meshlet_bounds` 条目。

### 7.4 自动化回归验证
运行引擎内置的 ShaderGen 回归测试门禁：
```bash
./build/out/Windows_64_Debug/ShaderDocumentProductionRegression.exe --smoke
./build/out/Windows_64_Debug/ShaderResourceSchemaRegressionGate.exe
```
确保所有契约路径均为 `[PASS]`。
