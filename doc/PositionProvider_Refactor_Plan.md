# PositionProvider 重构方案 — 从 enum 路由到 `.glsl + @sfm` 表驱动

> 分支: `Decoupling_RecipePresetGeometry`
> 状态: 设计草案 (Draft v2 — 已合并 review 反馈)
> 关联文件:
> - `inc/hgl/common/PositionProvider.h`
> - `inc/hgl/shadergen/PositionProviderRegistry.h`
> - `src/ShaderGen/PositionProviderRegistry.cpp`
> - `src/ShaderGen/RecipeToKey.cpp`
> - `src/ShaderGen/CompositorAssembler.cpp`
> - `src/ShaderGen/ShaderLayoutEmitter.cpp`
> - `src/ShaderGen/VariantRegistry.cpp`
> - `src/ShaderGen/BuiltinVariantEntry.h`
> - `inc/hgl/mtl/MaterialVariantKey.h`
> - `inc/hgl/mtl/MaterialVariantRow.h`
> - `inc/hgl/mtl/ShaderProgramKey.h`
> - `inc/hgl/mtl/MaterialLibrary.h`
> - `inc/hgl/mtl/VertexProgramTemplate.h`
> - `inc/hgl/shadergen/CompositorFeatureFlags.h`
> - `src/ShaderGen/3d/M_VertexLum3D.cpp`
> - `src/ShaderGen/registry/VertexProgramTemplates.cpp`

---

## 1. 背景与动机

### 1.1 现状问题

当前 `PositionProviderId` 是一个 `enum class : uint16`，承载了三件事：

1. **缓存键** — 嵌入 `MaterialVariantKey`，参与 hash。
2. **文件定位** — `PositionProviderRegistry` 通过 ID 查到 `.glsl` 路径。
3. **路由判定语义** — C++ 上层用 `IsPCGPositionProvider()` 之类的开关决定是否允许 dim/policy 覆盖。

这三件事被 enum 强行绑在一起，带来以下缺陷:

- **新增 PCG 必须改 C++ 三处**: enum、`kBuiltinProviders[]`、`IsPCGPositionProvider()` switch。
- **builtin 与 user PCG 不对等**: 用户自定义 PCG 没有等价 builtin 的入口，需要一条单独的 `UserCustom_Begin = 0x8000` ID 空间但没有真正可用。
- **C++ 隐式表达 GLSL 输入契约**: `vab_count`/`needs_ssbo` 等字段在 C++ 中维护，但真正决定输入的是 `.glsl` 顶部的声明。两份信息容易漂移。
- **路由 hack 不收敛**: `RecipeToKey.cpp` Step 1 的 dim → VAB_Vec2/DirectVec3 分支、`if (!IsPCGPositionProvider) override` 块，全部是因为 enum 不携带「是否消费 VAB」信息而存在。
- **回归易发**: `FullscreenTriangleFragCoord` 渲染失败的根因就是 dim-based override 把 `PCG_FullscreenTriangle` 覆写成 `DirectVec3`。

### 1.2 设计目标

用户提出的核心理念:

> 内置和 PCG、User 自定义 PCG 等价处理，都只是指定不同的 `.glsl`，并以 `@sfm` 标记自己的输入要求和输出要求。GLSL vertex input stream 常用类型有限，全展开生成一堆 `.glsl` 也无所谓。这样让 C++ 层变薄。

落实为本方案的设计目标:

| # | 目标 |
|---|---|
| G1 | `PositionProviderId` 的语义从「ID」收敛为「输入流形状的占位枚举」，仅作为表查键。 |
| G2 | Builtin 与 User 路径在 ShaderGen 下层 **完全不区分**: 都只是 `(glsl_path, @sfm metadata)` 对。 |
| G3 | C++ 层不再表达「PCG vs VAB」判定逻辑，所有路由信息从 `@sfm` 表派生。 |
| G4 | 新增 builtin provider = 加一个 `.glsl` + 一行注册; 新增 user PCG = recipe 里直接指定 `.glsl` 路径。 |
| G5 | 完全表驱动, 满足 `copilot-instructions` 中 "ShaderGen 全表驱动 / JSON-ready" 的既定方针。 |

---

## 2. 新 `PositionProviderId` 编号方案

### 2.1 编号区间

```
0x0000              Unknown                       // 哨兵, 0 = 未指定/无效
0x0001 .. 0x00FF    VAB 流形状占位 (VAB_*)         // 256 个槽位
0x0100 .. 0x0FFF    Builtin PCG (PCG_*)            // 内置过程化生成器
0x1000              UserPCG                        // 用户 PCG 入口, 单一哨兵
0x1001 .. 0xFFFE    保留未来扩展
0xFFFF              Invalid                        // 反向哨兵, 可选
```

### 2.2 VAB 流形状占位 (0x0001 - 0x00FF)

> 「占位即可, 不一定要有」。这一区间穷举所有理论上合法的 vertex attribute 流形状，
> 即使某些组合当前没有对应的 `.glsl`，也保留 ID。这样未来 `.glsl` 增删不会触发编号回收/重排。

| ID    | 名称              | GLSL input 形状                | 用途示例                |
|-------|-------------------|--------------------------------|-------------------------|
| 0x01  | `VAB_Float`       | `layout(location=0) in float`  | 1D 标量轴               |
| 0x02  | `VAB_Vec2`        | `in vec2`                      | 2D ortho/UI            |
| 0x03  | `VAB_Vec3`        | `in vec3`                      | 普通 3D mesh           |
| 0x04  | `VAB_Vec4`        | `in vec4`                      | 带 w 的预乘 clip       |
| 0x05  | `VAB_IFloat`      | `in int`                       | 整数索引                |
| 0x06  | `VAB_IVec2`       | `in ivec2`                     | Text2D pixel coord     |
| 0x07  | `VAB_IVec3`       | `in ivec3`                     | 体素                    |
| 0x08  | `VAB_IVec4`       | `in ivec4`                     | 保留                    |
| 0x09  | `VAB_UFloat`      | `in uint`                      | 保留                    |
| 0x0A  | `VAB_UVec2`       | `in uvec2`                     | 保留                    |
| 0x0B  | `VAB_UVec3`       | `in uvec3`                     | 保留                    |
| 0x0C  | `VAB_UVec4`       | `in uvec4`                     | 保留                    |
| 0x0D  | `VAB_BVec2`       | `in bvec2`                     | 保留 (实际不进 VS)     |
| 0x0E  | `VAB_BVec3`       | `in bvec3`                     | 保留                    |
| 0x0F  | `VAB_BVec4`       | `in bvec4`                     | 保留                    |
| 0x10  | `VAB_DVec2`       | `in dvec2`                     | 双精度坐标 (CAD)        |
| 0x11  | `VAB_DVec3`       | `in dvec3`                     | 双精度坐标              |
| 0x12  | `VAB_DVec4`       | `in dvec4`                     | 双精度坐标              |
| 0x13  | `VAB_Packed_RGB10A2` | `uint` → 解包 vec3          | 紧凑位置存储           |
| 0x14  | `VAB_Packed_R16G16` | `uint` → 解包 vec2            | 紧凑 2D 位置           |
| 0x15  | `VAB_Packed_RGBA16F` | `uvec2` → 解包 vec4          | 半精度位置             |

> 占位策略: 即使 `dvec2/3/4` `bvec*` 当前不产生 `.glsl`, 也保留 ID。
> `Packed_*` 是「同一 `vec3 GetPositionLocal()` 输出 + 不同输入解包」的代表, 演示
> 「输入格式无限可能, 但都收敛到统一 output 契约」。

### 2.3 Builtin PCG (0x0100 - 0x0FFF)

| ID     | 名称                          | 说明                                              |
|--------|-------------------------------|---------------------------------------------------|
| 0x0100 | `PCG_FullscreenTriangle`      | `gl_VertexIndex ∈ {0,1,2}` → 全屏三角形 NDC      |
| 0x0101 | `PCG_FullscreenQuad`          | 6 顶点全屏 quad                                   |
| 0x0102 | `PCG_UnitCube`                | 36 顶点单位立方体                                  |
| 0x0103 | `PCG_UnitSphereIcosahedron`   | 程序化单位球 (icosahedron 细分)                   |
| 0x0104 | `PCG_GridXZ`                  | gl_VertexIndex → grid (x,z) 顶点                |
| 0x0105 | `PCG_DebugAxes`               | 3 条 axis line                                    |
| ...    | (保留)                        |                                                   |

### 2.4 UserPCG 入口 (0x1000)

```cpp
UserPCG = 0x1000
```

**单一哨兵, 不再编号到每个用户 `.glsl`**。当 `position_provider == UserPCG` 时,
ShaderGen 不查 `kBuiltinProviders[]`, 而是从 recipe / variant key 携带的
`user_provider_glsl_path` 字段读取 `.glsl` 路径。

### 2.5 区段判定 (constexpr helper)

```cpp
constexpr bool IsVABPositionProvider(PositionProviderId id) noexcept
{
    return uint16(id) >= 0x0001 && uint16(id) <= 0x00FF;
}
constexpr bool IsBuiltinPCGPositionProvider(PositionProviderId id) noexcept
{
    return uint16(id) >= 0x0100 && uint16(id) <= 0x0FFF;
}
constexpr bool IsUserPCGPositionProvider(PositionProviderId id) noexcept
{
    return uint16(id) == 0x1000;
}
constexpr bool IsPCGPositionProvider(PositionProviderId id) noexcept
{
    return IsBuiltinPCGPositionProvider(id) || IsUserPCGPositionProvider(id);
}
constexpr bool ConsumesVAB(PositionProviderId id) noexcept
{
    return IsVABPositionProvider(id);
}
```

> 这些 helper 是**区段判定**, 不是 switch 列表; 新增 ID 不需要修改 helper。
> 这正好回答了上一轮 "未来加格式还得一个个 != " 的反馈。

---

## 3. `@sfm` 元数据规范

`@sfm` (Shader Fragment Manifest) 是 `.glsl` 文件顶部的结构化注释头。
ShaderGen 在启动或离线打包时解析, 写入 `ProviderManifest` 表。

### 3.1 语法

**重要约定 (v2)**: 所有 provider `.glsl` 必须实现统一输出契约 `vec4 GetPosition()`,
其语义由 `output_space` 决定 (`Local` → 模型空间 vec4, w=1; `World` → 世界空间; `ClipNDC` → 已在 NDC 的 vec4)。
这样上层 vertex policy 只看 `output_space` 一个字段决定是否再做 MVP/Passthrough, **不再按函数签名分支**。

```glsl
// providers/vab_vec3.glsl
//@sfm version: 1
//@sfm provider_kind: VAB
//@sfm input: VAB Position vec3
//@sfm output: vec4 GetPosition()
//@sfm output_space: Local
//@sfm consumes_vab: 1
//@sfm needs_ssbo: 0
//@sfm needs_ubo: 0
//@sfm needs_sampler: 0
//@sfm allow_dim_override: 1

#version 450
layout(location = 0) in vec3 in_position;
vec4 GetPosition() { return vec4(in_position, 1.0); }
```

```glsl
// providers/pcg_fullscreen_triangle.glsl
//@sfm version: 1
//@sfm provider_kind: PCG
//@sfm input: (none)
//@sfm output: vec4 GetPosition()
//@sfm output_space: ClipNDC
//@sfm consumes_vab: 0
//@sfm allow_dim_override: 0

vec4 GetPosition()
{
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    return vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
```

```glsl
// providers/ssbo_packed_rgb10a2.glsl    (示例, 未来扩展)
//@sfm version: 1
//@sfm provider_kind: PCG
//@sfm input: SSBO Position RGB10A2
//@sfm output: vec4 GetPosition()
//@sfm output_space: Local
//@sfm consumes_vab: 0
//@sfm needs_ssbo: 1
//@sfm allow_dim_override: 0
```

> **DECISION (v2)**: 移除 `matches_pos_id` 字段。`.glsl` **不自报 ID**,
> 由 builtin 注册侧 (C++ `kBuiltinProviders[]`) 单向声明 `ID → glsl_path` 映射,
> 避免「ID 与文件互相声明」造成不一致风险。

### 3.2 字段语义

| 字段                  | 类型     | 说明                                                                                  |
|-----------------------|----------|---------------------------------------------------------------------------------------|
| `version`             | int      | **必填**, `@sfm` 协议版本号。当前为 `1`。解析器拒绝未知版本, 防止格式漂移。            |
| `provider_kind`       | enum     | `VAB` / `PCG`。下层不依赖此字段, 仅用于诊断/工具链。                                 |
| `input`               | spec     | `(none)` 或 `<source> <attrib_name> <format>`。`source ∈ {VAB, SSBO, UBO, Push}`。    |
| `output`              | function | **固定为 `vec4 GetPosition()`**。所有 provider 必须实现此签名 (v2 统一契约)。         |
| `consumes_vab`        | 0/1      | 等价于 `IsVABPositionProvider`, 但来自 `.glsl` 真实声明。                              |
| `needs_ssbo`          | 0/1      | 是否需要 SSBO 绑定。                                                                  |
| `needs_ubo`           | 0/1      | 是否需要 UBO 绑定。                                                                   |
| `needs_sampler`       | 0/1      | 是否需要 sampler 绑定。                                                               |
| `allow_dim_override`  | 0/1      | 上层 `dim/vertex_policy` 是否可以替换它。VAB=1, PCG=0。                                |
| `output_space`        | enum     | `Local` / `World` / `ClipNDC`。决定后续 vertex policy 是否需要再做 MVP/Passthrough。   |

> **范围限制 (v2)**: 本方案严格限定为「**位置提供器**」refactor。Normal / Tangent / UV / Color
> 等其他顶点输入流不在本期 `@sfm` 表内, 后续可复用同一机制独立推进, 但不与本计划耦合。

### 3.3 解析时机

- **离线方案 (推荐)**: 构建期扫描 `ShaderLibrary/position_provider/*.glsl`, 生成
  `ProviderManifest.json` 或 `.inc` 文件, 编进二进制。
- **运行时方案 (回退)**: 启动时一次性扫描目录。需在新模块中完成,
  不能污染 `RecipeToKey.cpp` (该文件明确禁止 glslang/file-I/O 依赖)。

---

## 4. ProviderManifest 数据结构

新增 `src/ShaderGen/ProviderManifest.h/.cpp`:

```cpp
struct ProviderManifest
{
    PositionProviderId  pos_id;             // UserPCG 时统一为 PositionProviderId::UserPCG
    std::string         glsl_path;          // ShaderLibrary 相对路径; 占位条目可为空
    uint32_t            glsl_path_hash;     // FNV-1a 32 of glsl_path (UserPCG 用作 key)
    ProviderKind        kind;               // VAB / PCG (诊断用)
    uint16_t            sfm_version;        // @sfm version, 当前=1

    // 来自 @sfm 的元数据 (output 固定为 vec4 GetPosition(), 不再存签名字符串)
    bool                consumes_vab        = false;
    bool                needs_ssbo          = false;
    bool                needs_ubo           = false;
    bool                needs_sampler       = false;
    bool                allow_dim_override  = false;
    OutputSpace         output_space        = OutputSpace::Local;

    // 输入流描述 (允许有限多种, 全展开)
    struct InputSpec {
        InputSource source;     // VAB / SSBO / UBO / Push / None
        std::string attrib_name;
        std::string format;     // "vec3", "RGB10A2", ...
    };
    std::vector<InputSpec> inputs;
};

class ProviderManifestRegistry
{
public:
    static const ProviderManifest* FindByPosId(PositionProviderId id) noexcept;
    static const ProviderManifest* FindByGlslPath(std::string_view path) noexcept;
    static const ProviderManifest* FindByPathHash(uint32_t hash) noexcept;

    // UserPCG: 注册一个新的用户 .glsl, 返回常驻 manifest
    static const ProviderManifest* AcquireUserProvider(std::string_view glsl_path);

    // 启动期自检: 遍历全部 manifest, 校验 @sfm version, 必填字段,
    // glsl_path 存在性, output_space 合法性, consumes_vab 与 inputs 一致性。
    // Debug 构建下断言失败; Release 下记录 error 并降级。
    static bool RunSelfCheck();
};
```

> **DECISION (v2)**: UserPCG 用 `uint32_t glsl_path_hash` 作为 `MaterialVariantKey` 的旁路字段,
> 不再用 `string_view`。理由: 避免生命周期/线程安全风险, 让 key 完全 POD 化, hash/equality 成本恒定。
> 路径冲突由 `RunSelfCheck()` 在启动期检测 (FNV-1a 32-bit 碰撞概率极低, 且全部路径已知)。

---

## 5. C++ 路由层简化

### 5.1 `MaterialVariantKey` 字段调整

```cpp
// MaterialVariantKey.h
struct MaterialVariantKey
{
    ...
    PositionProviderId position_provider = PositionProviderId::Unknown;

    // 仅当 position_provider == UserPCG 时非零; 否则为 0。
    // FNV-1a 32 of glsl_path, 由 ProviderManifestRegistry::AcquireUserProvider 计算。
    uint32_t user_provider_path_hash = 0;

    ...
};
```

> **DECISION (v2)**: 使用 `uint32_t` 而非 `string_view`, 让 `MaterialVariantKey` 保持 POD,
> hash/equality 完全 trivial, 不依赖外部字符串生命周期。

### 5.2 `RecipeToKey.cpp` 移除的代码

**完全删除** Step 1 的以下代码段:

```cpp
// 删除: dim/vertex_policy → VAB_Vec2/DirectVec3 的硬编码
if (!IsPCGPositionProvider(k.position_provider))
{
    if (effective_vertex_policy == VertexTransformPolicy::Quad2D)
        k.position_provider = PositionProviderId::VAB_Vec2;
    else if (r.dim == MaterialRecipe::Dim::D2)
        k.position_provider = PositionProviderId::VAB_Vec2;
    else
        k.position_provider = PositionProviderId::DirectVec3;
}
```

**替换为** 表驱动的覆盖判定:

```cpp
const ProviderManifest* pm = ProviderManifestRegistry::FindByPosId(k.position_provider);
if (pm && pm->allow_dim_override)
{
    // 从 demand table 查这个 (preset, dim, policy) 对应的 builtin pos_id
    k.position_provider = ResolveProviderFromDemand(r.preset, r.dim, effective_vertex_policy);
}
// 否则保留 builtin 行原始 position_provider (例如 PCG_FullscreenTriangle)
```

`ResolveProviderFromDemand` 也是表查, 不带 switch。

### 5.3 `CompositorAssembler.cpp` 简化

当前需要根据 `position_provider` 决定 include 哪个文件。改造后:

```cpp
const ProviderManifest* pm = (key.position_provider == PositionProviderId::UserPCG)
    ? ProviderManifestRegistry::FindByPathHash(key.user_provider_path_hash)
    : ProviderManifestRegistry::FindByPosId(key.position_provider);

// Route-time fallback: 占位 ID 或缺失 .glsl 时退回 VAB_Vec3 + warning,
// 保证管线一定可构建, 错误对用户可见但不致 crash。
if (!pm || pm->glsl_path.empty())
{
    LOG_WARN("provider missing/empty glsl, fallback to VAB_Vec3, id=%u hash=%u",
             (uint32_t)key.position_provider, key.user_provider_path_hash);
    pm = ProviderManifestRegistry::FindByPosId(PositionProviderId::VAB_Vec3);
}

emitter.Include(pm->glsl_path);
// output_space == ClipNDC 时跳过 passthrough/MVP
if (pm->output_space != OutputSpace::ClipNDC)
    emitter.IncludeVertexPolicy(key.vertex_policy);
```

**完全没有** `if (id == PCG_FullscreenTriangle) include "pcg_fullscreen_triangle.glsl";` 之类的分支, 也没有按 output 函数名分支。

### 5.4 `ShaderLayoutEmitter.cpp` 资源声明

当前通过 `PositionProvider::vab_count` / `needs_ssbo` 决定声明哪些 binding。
改造后从 `pm->consumes_vab` / `pm->needs_ssbo` / `pm->inputs[]` 读取, 逻辑不变, 数据源改了。

### 5.5 `BuiltinVariantEntry.h` / `VariantRegistry.cpp`

builtin 行原本就声明 `position_provider`。本方案不改变该字段的含义,
只是新增了 `UserPCG` 的合法路径。

### 5.6 `M_VertexLum3D.cpp` / `VertexProgramTemplates.cpp`

这些文件目前直接引用 `PositionProviderId::DirectVec3` 等枚举值, 改造后保持引用,
因为新方案保留了 enum (只是重新编号 + 分区段)。**不需要改业务逻辑**。

---

## 6. Recipe 层用户接口

### 6.1 Builtin 使用 (不变)

```cpp
mtl::MaterialRecipe r {
    .preset = MaterialPreset::FullscreenTriangle,
    .dim    = MaterialRecipe::Dim::D3,
};
// preset 决定 builtin 行, 自动得到 PCG_FullscreenTriangle
```

### 6.2 User PCG 使用 (新)

```cpp
mtl::MaterialRecipe r {
    .preset = MaterialPreset::Custom,                    // 必须为 Custom
    .vertex_provider_glsl = "myproj/voxel_unpack.glsl",  // 新字段
    .dim    = MaterialRecipe::Dim::D3,
};
```

`RecipeToKey.cpp` 检测到 `vertex_provider_glsl` 非空时:

```cpp
if (!r.vertex_provider_glsl.empty())
{
    // 合法性约束: 仅 MaterialPreset::Custom 允许指定 user provider,
    // 防止与 builtin preset 行的 position_provider 冲突。
    HGL_ASSERT(r.preset == MaterialPreset::Custom);
    auto* pm = ProviderManifestRegistry::AcquireUserProvider(r.vertex_provider_glsl);
    k.position_provider       = PositionProviderId::UserPCG;
    k.user_provider_path_hash = pm->glsl_path_hash;
}
```

> **DECISION (v2)**: `vertex_provider_glsl` 仅在 `preset == Custom` 时合法。
> 其他 preset 由 builtin 表唯一决定 provider, 上层不能旁路覆盖, 避免「preset 与 user glsl 互相覆盖」二义性。

---

## 7. 阶段化实施步骤

> 必须保持每个阶段可单独构建通过, 避免大爆炸式重构。

### Phase 0 — 基线 (准备)
- [ ] 提交当前 `IsPCGPositionProvider()` 修复, 作为 baseline。
- [ ] 在 `doc/` 下保存本计划 (本文档)。

### Phase 1 — 枚举重编号 (兼容窗口)
- [ ] 修改 `PositionProvider.h`:
    - `Unknown = 0x0000` (替换原 `0x7FFF`)
    - 重新分配 VAB 区段 `0x01..0xFF`, 写入全部占位 (vec/ivec/uvec/bvec/dvec + packed)
    - PCG 区段 `0x0100..0x0FFF`, 至少含 `PCG_FullscreenTriangle = 0x0100`
    - `UserPCG = 0x1000`
- [ ] 添加 `IsVAB/IsBuiltinPCG/IsUserPCG/IsPCG/ConsumesVAB` 区段 helper。
- [ ] 删除旧的 `UserCustom_Begin = 0x8000`。
- [ ] **缓存失效**: 同时 bump `kMaterialKeyGLSLVersion` (在 `MaterialKeyToolchainVersion.h`),
      使所有旧 SPIR-V 缓存自动作废。
- [ ] **VkPipelineCache 隔离**: 修改 pipeline cache 磁盘文件名 (后缀附 GLSLVersion),
      确保旧 driver-side pipeline blob 不会因 layout 不变而被错误复用。

### Phase 2 — Registry 重构
- [ ] 重写 `PositionProviderRegistry.cpp`: 表里所有 ID 都有占位条目;
      没有 `.glsl` 的占位条目 `glsl_path = ""` (调用方需检查)。
- [ ] 真正具备 `.glsl` 的 ID: 当前 `VAB_Vec3` / `VAB_Vec2` / `PCG_FullscreenTriangle`,
      其他保留为空字符串。
- [ ] 更新所有 `PositionProviderId::DirectVec3` 引用 → `PositionProviderId::VAB_Vec3`
      (语义相同, 名字归一化)。

### Phase 3 — `@sfm` 解析器与 `ProviderManifest`
- [ ] 新增 `src/ShaderGen/ProviderManifest.h/.cpp`。
- [ ] 新增 `@sfm` 解析器 (独立模块, 不污染 `RecipeToKey.cpp`):
    - 默认运行时, 启动期一次扫描 `ShaderLibrary/position_provider/`。
    - 解析器强制要求 `@sfm version: 1`, 未知/缺失版本拒绝加载。
    - 离线方案留口子, 后续切换不影响 API。
- [ ] 给现有 3 个 `.glsl` 文件补 `@sfm` 头, 并把 output 统一为 `vec4 GetPosition()`。
- [ ] 实现 `ProviderManifestRegistry::RunSelfCheck()`, 在 `ShaderGen` 初始化末尾调用一次,
      Debug 构建下断言失败 (字段缺失 / glsl 文件不存在 / hash 冲突 / consumes_vab 与 inputs 不一致)。
- [ ] 单元测试: 解析 → 比对预期字段; SelfCheck pass; hash 唯一性。

### Phase 4 — Recipe 增 `vertex_provider_glsl` 字段
- [ ] `MaterialRecipe.h` 添加 `std::string vertex_provider_glsl;` (默认空)。
- [ ] `MaterialRecipeStore.cpp` hash 计算包含此字段。
- [ ] 增加 `MaterialPreset::Custom` (若不存在), 表示「无 preset, 完全由 recipe 描述」。
- [ ] 合法性约束: `vertex_provider_glsl` 非空 ⇒ `preset == Custom` (Debug 断言)。

### Phase 5 — `MaterialVariantKey` 扩展
- [ ] 添加 `uint32_t user_provider_path_hash = 0;`。
- [ ] hash / equality 包含此字段。
- [ ] key 保持 POD, 不持有任何外部字符串。

### Phase 6 — `RecipeToKey.cpp` 路由替换
- [ ] 删除 Step 1 末尾的 dim-based override block (上文 5.2)。
- [ ] 改为 `ResolveProviderFromDemand()` (新函数, 查 `PresetDemand` 表)。
- [ ] 增加 UserPCG 分支: `r.vertex_provider_glsl` 非空 → AcquireUserProvider + 写 `user_provider_path_hash`。
- [ ] 移除 `IsPCGPositionProvider()` 在本文件的内部使用 (改用 manifest 字段 `allow_dim_override`)。

### Phase 7 — `CompositorAssembler.cpp` 统一 include 路径
- [ ] 替换 provider-id 分支为统一 `pm->glsl_path` emit。
- [ ] `pm->output_space == ClipNDC` 时跳过 `passthrough_ndc.glsl` 与 MVP
      (这正是 FullscreenTriangle 当前会错误叠加 passthrough 的修复点)。
- [ ] 实现 route-time fallback (上文 5.3): 占位 ID / 空 glsl_path 时退回 `VAB_Vec3` + warning,
      保证管线可构建。

### Phase 8 — `ShaderLayoutEmitter.cpp` 资源声明改源
- [ ] 改读 `pm->consumes_vab` 等字段, 不再依赖 `PositionProvider::vab_count`。
- [ ] 旧 `PositionProvider` 结构标记 deprecated, 等所有调用切换后删除。

### Phase 9 — 验证与回归
- [ ] 运行所有 `example/Basic/*` 程序, 逐一确认渲染正确:
    - `draw_triangle` (2D ortho / VAB_Vec2)
    - `auto_instance` (2D NDC / VAB_Vec2)
    - `clock` (2D / VAB_Vec2)
    - `FullscreenTriangleFragCoord` (PCG_FullscreenTriangle, **本次回归的根因案例**)
    - 任一 3D Standard 示例 (VAB_Vec3)
- [ ] 验证日志中 `BuildForwardVertexEntry` 的 include 路径与预期 manifest 一致。
- [ ] 验证 Vulkan validation 无 `Location 0` 之类的 vertex-input 不匹配错误。

### Phase 10 — 清理
- [ ] 删除 `PositionProvider` 旧结构 (如已无使用)。
- [ ] 删除 `RecipeToKey.cpp` 中残留的兼容注释。
- [ ] 更新 `copilot-instructions.md`, 把「位置提供器表驱动」加入既定原则列表。

---

## 8. 风险与对策

| 风险                                                  | 影响                       | 对策                                                          |
|-------------------------------------------------------|----------------------------|---------------------------------------------------------------|
| `PositionProviderId` 重编号导致旧 SPIR-V 缓存命中错文件 | 渲染异常                   | Phase 1 同步 bump `kMaterialKeyGLSLVersion`, 并隔离 `VkPipelineCache` 磁盘文件名。 |
| `@sfm` 解析放运行时, 启动期 I/O                       | 启动延迟                   | 默认运行时, 离线方案预留; 启动期解析只针对 `position_provider/` 子目录, 文件数 < 30。 |
| User 提供错误 `.glsl` (缺 `@sfm` 头 / 版本不匹配)     | 路由失败                   | 解析失败时 manifest 为空; route-time fallback 为 `VAB_Vec3` + warning; SelfCheck 启动期暴露。 |
| UserPCG path hash 冲突                                | key 误命中                 | FNV-1a 32; `RunSelfCheck()` 启动期遍历检测; 冲突时拒绝注册并报错。 |
| 占位 ID 没有 `.glsl` 时被误用                         | shader 编译失败            | `Registry` 返回空 `glsl_path` 时由 route-time fallback 兜底, 不抛异常。 |
| 既有依赖 `DirectVec3 = 0` 的代码                      | 编号变更破坏假设           | Phase 1 同时改名 `DirectVec3` → `VAB_Vec3`, 全工程 grep & 替换。 |
| `vec4 GetPosition()` 统一契约迁移                     | 既有 `.glsl` 编译破坏      | Phase 3 与 Phase 7 一并完成, 同步改 3 个现存 `.glsl` 与 vertex policy include 端。 |
| 范围蔓延到 normal/uv/tangent 等其他流                 | scope creep                | 本计划严格限定为 position provider; 其他流以后续独立计划复用同一机制。 |

---

## 9. 与既有方针的契合

本方案与 `copilot-instructions.md` 中已确立的原则完全一致:

- ✅ "ShaderGen / material routing 全表驱动, JSON-ready, 即使冗长也优先表"
- ✅ "去除隐藏硬编码分支, 移除特化路径"
- ✅ "扩展性: 使用 RANGE_SIZE / foreach 化, 避免手动更新硬编码"
- ✅ "MaterialInstance 与 ShaderMaterialProgram 解耦" 的同源思路 (这里是 provider 与 material 解耦)
- ✅ "DitherMask 等过渡技术作为表中一等公民" 的同源思路 (这里是 user PCG 与 builtin 平权)

---

## 10. 后续可扩展点

完成本方案后, 自然解锁以下能力:

1. **Fragment / Geometry / Mesh shader provider** 套用同一 `@sfm` 机制。
2. **离线打包**: `@sfm` → JSON → 构建期生成 `.inc` 表, 完全消除运行时 I/O。
3. **工具链可视化**: 列出所有 provider 及其 `.glsl`, 用于编辑器面板。
4. **shader feature pruning**: 低质量 LOD 时根据 `@sfm` 自动选更便宜的 provider。

---

## 11. 决策记录 (ADR 摘要)

- **DECISION**: `PositionProviderId::Unknown = 0x0000` 而非 `0x7FFF`, 使 zero-init 即「未指定」, 更安全。
- **DECISION**: VAB 区段保留全部理论形状占位 (即使无 `.glsl`), 避免未来扩展时编号回收。
- **DECISION**: `UserPCG` 用单一哨兵 ID (`0x1000`), 实际路径走 `uint32_t` hash 旁路, 让 key 保持 POD。
- **DECISION**: `@sfm` 解析默认运行时启动期, 离线打包作为后续优化方向。
- **DECISION**: 路由层完全表驱动, 禁止再引入「是否 PCG」之类的 C++ 判定 switch。
- **DECISION (v2)**: 所有 provider `.glsl` 统一输出 `vec4 GetPosition()`, 由 `output_space` 决定语义,
  避免按函数签名分支。
- **DECISION (v2)**: 移除 `@sfm matches_pos_id` 字段, ID↔glsl 映射由 builtin 注册侧单向持有,
  防止双向声明不一致。
- **DECISION (v2)**: `@sfm version` 必填且强校验, 防止未来格式漂移悄无声息地通过。
- **DECISION (v2)**: 引入 route-time fallback (空 `glsl_path` → `VAB_Vec3` + warning),
  保证管线在 manifest 缺漏时仍可构建, 错误对用户可见但不致 crash。
- **DECISION (v2)**: `vertex_provider_glsl` 仅在 `preset == Custom` 时合法, 消除 preset 与 user glsl 的覆盖二义性。
- **DECISION (v2)**: 启动期 `ProviderManifestRegistry::RunSelfCheck()` 强制执行, Debug 断言、Release 记日志。
- **DECISION (v2)**: 本计划严格限定为 **position provider** refactor; normal/uv/tangent 等其他顶点流由后续独立计划处理。
