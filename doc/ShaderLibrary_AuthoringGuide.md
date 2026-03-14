# ULRE ShaderLibrary — Shader 编写技术文档

> **适用范围**：本文档面向 ULRE 渲染引擎的 Shader 开发者，描述 ShaderLibrary 的目录结构、
> Surface Function 编写规范、Compositor Template 结构、宏注入机制、Descriptor Set 绑定约定等。

---

## 目录

- [1. 概述](#1-概述)
- [2. 目录结构](#2-目录结构)
- [3. 核心概念](#3-核心概念)
- [4. 内存布局规范](#4-内存布局规范)
- [5. Descriptor Set 绑定布局](#5-descriptor-set-绑定布局)
- [6. Surface Function 编写指南](#6-surface-function-编写指南)
- [7. Compositor Template 结构](#7-compositor-template-结构)
- [8. 宏注入机制](#8-宏注入机制)
- [9. Include 规范](#9-include-规范)
- [10. 已有 Shader 参考](#10-已有-shader-参考)
- [11. 常见问题](#11-常见问题)

---

## 1. 概述

ULRE 采用 **Surface Function + Compositor Template** 架构来组织 GLSL Shader 源码：

- **Surface Function**：定义材质表面属性的计算（颜色、法线、金属度、粗糙度等）。
  每种材质类型（Standard PBR、Skin、Hair、Foliage 等）对应一个 Surface Function 文件。
- **Compositor Template**：定义渲染通道的完整 VS/FS 框架（顶点变换、光照计算、输出组装等）。
  每种渲染通道（Forward Opaque、Forward Transparent、Shadow、VBuffer 等）对应一组 Compositor 模板。

`CompositorAssembler`（C++ 侧）负责将 Surface Function 嵌入 Compositor Template，
注入编译期宏，生成完整的可编译 GLSL 源码，再由 `GLSLCompiler` 编译为 SPIR-V。

---

## 2. 目录结构

```
ShaderLibrary/
├── common/                          # 公共模块（所有 Shader 共享）
│   ├── surface_interface.glsl       # SurfaceInput / SurfaceOutput / SurfaceOutputExt 结构体定义
│   ├── lighting.glsl                # 分级光照计算函数 EvalLighting()
│   ├── depth_utils.glsl             # Reversed-Z 深度工具（LinearizeDepth, ReconstructWorldPos）
│   └── vertex_fetch_ssbo.glsl       # SSBO 顶点获取模块（FetchPosition/Normal/UV0）
│
├── surface/                         # Surface Function 文件（每种材质一个文件）
│   └── standard_surface.glsl        # Standard PBR Surface Function
│
├── compositor/                      # Compositor Template（每种渲染通道一组 VS+FS）
│   ├── main_forward_opaque.vert.glsl
│   └── main_forward_opaque.frag.glsl
│
├── pass/                            # Pass 专用 Shader（Shadow pass 等，后续添加）
├── postprocess/                     # 后处理 Shader（Bloom、Tonemap、FXAA 等，后续添加）
├── debug/                           # 调试可视化 Shader
├── modules/                         # 可复用的功能模块
├── templates/                       # 旧模板（待废弃）
├── recipes/                         # 旧 recipes（待废弃）
├── Std2D/                           # 2D 渲染 Shader
└── Std3D/                           # 3D 渲染 Shader（旧体系）
```

**命名约定**：
- Surface Function: `<name>_surface.glsl`（如 `standard_surface.glsl`、`skin_surface.glsl`）
- Compositor VS: `main_<pass>_<blend>.vert.glsl`（如 `main_forward_opaque.vert.glsl`）
- Compositor FS: `main_<pass>_<blend>.frag.glsl`（如 `main_forward_opaque.frag.glsl`）
- 公共模块: `<功能名>.glsl`（如 `lighting.glsl`、`depth_utils.glsl`）

---

## 3. 核心概念

### 3.1 SurfaceInput / SurfaceOutput

定义在 `common/surface_interface.glsl`，是 Surface Function 与 Compositor 之间的通信接口。

```glsl
struct SurfaceInput
{
    vec3 worldPos;       // Camera-Relative 世界坐标（非绝对世界坐标！）
    vec3 worldNormal;    // 世界空间法线
    vec2 uv0;            // 第一套 UV
    vec2 uv1;            // 第二套 UV
    vec4 vertexColor;    // 顶点色
    vec3 viewDir;        // 视线方向 = normalize(-worldPos)
    vec2 screenPos;      // 屏幕空间坐标
};

struct SurfaceOutput
{
    vec3  baseColor;     // 基础色
    vec3  normal;        // 世界空间法线（可由法线贴图修改）
    float metallic;      // 金属度 [0, 1]
    float roughness;     // 粗糙度 [0, 1]
    float ao;            // 环境遮蔽 [0, 1]
    vec3  emissive;      // 自发光
    float alpha;         // 透明度
};

struct SurfaceOutputExt           // 特殊材质扩展（Skin, Hair, ClearCoat 等）
{
    vec3  subsurfaceColor;        // 次表面散射颜色
    float subsurfacePower;        // 次表面散射强度
    float thickness;              // 厚度（SSS / Translucent）
    vec3  sheenColor;             // Sheen 颜色（Cloth）
    float sheenRoughness;         // Sheen 粗糙度
    float clearCoat;              // 清漆强度
    float clearCoatRoughness;     // 清漆粗糙度
    vec3  clearCoatNormal;        // 清漆法线
    float anisotropy;             // 各向异性强度
    vec3  anisotropyDirection;    // 各向异性方向
};
```

### 3.2 Camera-Relative Rendering

ULRE 全管线使用 **Camera-Relative Rendering**：

- `SurfaceInput.worldPos` 是 Camera-Relative 坐标（相机位置恒为原点）
- `CameraUBO.cameraPos` 恒为 `vec3(0)`
- 视线方向：`viewDir = normalize(-worldPos)`
- 若需要绝对世界坐标（如 fog / terrain）：`absoluteWorldPos = worldPos + CameraUBO.cameraPosWorld`

### 3.3 QualityTier 分级

通过编译期宏 `QUALITY_TIER` 控制 Shader 复杂度：

| 值 | 等级 | 含义 |
|----|------|------|
| 0 | Lowest | 最低画质，移动端低端 |
| 1 | Low | 低画质 |
| 2 | Medium | 中等画质（默认 PC） |
| 3 | High | 高画质 |
| 4 | Ultra | 超高画质 |
| 5 | Cinematic | 电影级 |

在 Surface Function 和 Lighting 中使用 `#if QUALITY_TIER >= N` 守卫高级特性：

```glsl
#if QUALITY_TIER >= 1
    so.baseColor *= texture(TexAlbedo, si.uv0).rgb;  // Low 及以上采样 Albedo
#endif

#if QUALITY_TIER >= 2
    // Medium 及以上采样法线 + MR 贴图
#endif
```

---

## 4. 内存布局规范

### 4.1 统一使用 scalar 布局

为确保 C++ 与 GLSL 结构体 **1:1 内存映射**，ULRE **强制所有 buffer 使用 `scalar` 布局**（`GL_EXT_scalar_block_layout`）：

```glsl
#extension GL_EXT_scalar_block_layout : require

// SSBO
layout(scalar, set=1, binding=0) readonly buffer L2W_SSBO { mat4 transforms[]; };

// UBO
layout(scalar, set=0, binding=1) uniform CameraUBO { ... };
```

**前提条件**：设备初始化时必须启用以下 Vulkan 1.2 特性：
```cpp
VkPhysicalDeviceVulkan12Features features12{};
features12.scalarBlockLayout = VK_TRUE;           // GL_EXT_scalar_block_layout
features12.uniformBufferStandardLayout = VK_TRUE;  // UBO 非 std140 布局支持
```

> **兼容性**：`scalarBlockLayout` 在 Vulkan 1.2 core 中为可选特性，
> 桌面端（NVIDIA Maxwell+、AMD GCN 2+、Intel Skylake+）及 Android（Adreno 6xx+、Mali-G7x+）
> 均已广泛支持。

### 4.2 scalar 布局的核心优势

`scalar` 布局下，所有类型按其**标量分量大小**对齐，与 C/C++ 的 `#pragma pack` 规则等价：

| 类型 | std140 alignment | std430 alignment | **scalar alignment** |
|------|-----------------|-----------------|---------------------|
| `float` | 4 | 4 | **4** |
| `vec2` | 8 | 8 | **4** ← 仅需 float 对齐 |
| `vec3` | **16** | **16** | **4** ← 仅占 12 字节！ |
| `vec4` | 16 | 16 | **4** |
| `mat4` | 每列 16 对齐 | 每列 16 对齐 | **4** ← 64 字节紧密排列 |
| `float[]` 数组元素 | **16** | **4** | **4** |
| `vec3[]` 数组元素 | **16** | **16** | **4** ← 每元素仅 12 字节 |
| struct | 最大成员对齐 | 最大成员对齐 | **最大标量对齐** |

**关键优势**：
- `vec3` 真正占 12 字节，不浪费 4 字节 padding
- 数组元素紧密排列，`float[]` 每元素 4 字节而非 std140 的 16 字节
- C++ struct 与 GLSL struct 内存布局完全一致，无需手动 padding

### 4.3 GLSL 扩展声明

所有使用 `scalar` 布局的 GLSL 文件（或被 CompositorAssembler 注入的 preamble）中必须声明：

```glsl
#extension GL_EXT_scalar_block_layout : require
```

> CompositorAssembler 会在宏注入区域自动添加此扩展声明，Surface Function 作者无需手动添加。

### 4.4 C++ 侧结构体对齐规则

`scalar` 布局下，GLSL 与 C++ 结构体按完全相同的标量对齐规则排列。
C++ 侧正常定义 struct 即可（无需额外 `alignas` 或 padding）：

```cpp
// C++ 侧 — 与 GLSL 侧完全 1:1 映射
struct MI_Standard
{
    Vector4f base_color_factor;   // vec4  — 16 bytes, offset 0
    float    metallic_factor;     // float — 4 bytes,  offset 16
    float    roughness_factor;    // float — 4 bytes,  offset 20
    float    ao_strength;         // float — 4 bytes,  offset 24
    float    emissive_strength;   // float — 4 bytes,  offset 28
    // Total: 32 bytes
};
```

```glsl
// GLSL 侧 — scalar 布局，偏移完全对应
struct MI_Standard
{
    vec4  base_color_factor;      // offset 0
    float metallic_factor;        // offset 16
    float roughness_factor;       // offset 20
    float ao_strength;            // offset 24
    float emissive_strength;      // offset 28
};
```

**包含 vec3 的示例**（scalar 布局独有优势）：
```cpp
// C++ 侧
struct LightData
{
    Vector3f position;    // 12 bytes, offset 0
    float    radius;      // 4 bytes,  offset 12
    Vector3f color;       // 12 bytes, offset 16
    float    intensity;   // 4 bytes,  offset 28
    // Total: 32 bytes — vec3 紧密排列，无浪费
};
```

```glsl
// GLSL 侧 — vec3 仅占 12 字节
struct LightData
{
    vec3  position;       // offset 0  (12 bytes)
    float radius;         // offset 12
    vec3  color;          // offset 16 (12 bytes)
    float intensity;      // offset 28
};
```

**验证手段**：C++ 侧使用 `static_assert(sizeof(T) == N)` 和 `static_assert(offsetof(T, field) == M)` 确认。
建议新增 buffer 结构体时，始终在 C++ 侧加 `static_assert` 校验。

### 4.5 布局声明清单

| Buffer 类型 | 声明格式 |
|-------------|---------|
| UBO | `layout(scalar, set=N, binding=M) uniform Name { ... };` |
| SSBO (readonly) | `layout(scalar, set=N, binding=M) readonly buffer Name { ... };` |
| SSBO (读写) | `layout(scalar, set=N, binding=M) buffer Name { ... };` |

### 4.6 注意事项

1. **不要混用布局**：同一项目中所有 buffer 统一使用 `scalar`，禁止混用 `std140` / `std430` / `scalar`
2. **mat3 陷阱**：`scalar` 下 `mat3` 占 36 字节（3×3×4），C++ 侧需确认 `sizeof(Matrix3f) == 36`；若 C++ 存储为 48 字节（列主序每列 16 字节对齐），则需特殊处理
3. **SPIR-V 编译**：glslang 需要 `--target-env vulkan1.2`（或更高）才能正确处理 `GL_EXT_scalar_block_layout`
4. **RenderDoc 调试**：RenderDoc 完整支持 scalar 布局的 buffer 查看

---

## 5. Descriptor Set 绑定布局

ULRE 使用 4 个 Descriptor Set，绑定号约定如下：

### Set 0 — Scene（场景全局数据）

| Binding | 类型 | 名称 | 内容 |
|---------|------|------|------|
| 0 | UBO | `ViewportUBO` | `vec4 viewport; float time; ...` |
| 1 | UBO | `CameraUBO` | `mat4 view, proj, viewProj; vec3 cameraPos, cameraPosWorld;` |
| 2 | UBO | `SkyUBO` | `vec3 sunDirection, sunColor, ambientColor;` |

### Set 1 — Transform（变换数据）

| Binding | 类型 | 名称 | 内容 |
|---------|------|------|------|
| 0 | SSBO (readonly) | `L2W_SSBO` | `mat4 transforms[];` — Camera-Relative L2W 矩阵数组 |

### Set 2 — Material（材质数据 + 纹理）

| Binding | 类型 | 名称 | 内容 |
|---------|------|------|------|
| 0 | SSBO (readonly) | `MI_Buffer` | `MI_Standard mi_data[];` — Material Instance 数据数组 |
| 1 | sampler2D | `TexAlbedo` | Albedo 纹理（`QUALITY_TIER >= 1`） |
| 2 | sampler2D | `TexNormal` | 法线纹理（`QUALITY_TIER >= 2`） |
| 3 | sampler2D | `TexMR` | Metallic(G) + Roughness(B)（`QUALITY_TIER >= 2`） |

> **注意**：纹理 binding 在 Surface Function 中声明，受 `QUALITY_TIER` 宏守卫。
> 不同 Surface Function 的纹理 binding 可能不同。

### Set 3 — VertexData（SSBO 顶点数据，仅 SSBO 获取路径使用）

| Binding | 类型 | 名称 | 内容 |
|---------|------|------|------|
| 18 | SSBO (readonly) | `VertexDataBuffer` | `VertexData vertices[];` |
| 19 | SSBO (readonly) | `IndexDataBuffer` | `uint indices[];` |

---

## 6. Surface Function 编写指南

### 6.1 文件位置

新 Surface Function 放在 `ShaderLibrary/surface/<name>_surface.glsl`。

### 6.2 必须实现的函数

每个 Surface Function **必须**提供以下两个函数：

```glsl
// 计算完整表面属性
SurfaceOutput EvalSurface(SurfaceInput si, MI_<Type> mi);

// 仅计算 alpha 值（用于 Early-Z / Alpha Test pass）
float EvalAlpha(SurfaceInput si, MI_<Type> mi);
```

其中 `MI_<Type>` 是该材质类型的 Material Instance 结构体，在 Surface Function 文件中定义。

### 6.3 模板

```glsl
// <Name> Surface Function — <简要描述>

#include "common/surface_interface.glsl"

// --- 纹理声明（受 QUALITY_TIER 守卫） ---
#if QUALITY_TIER >= 1
layout(set=2, binding=1) uniform sampler2D TexAlbedo;
#endif
#if QUALITY_TIER >= 2
layout(set=2, binding=2) uniform sampler2D TexNormal;
layout(set=2, binding=3) uniform sampler2D TexMR;
#endif

// --- Material Instance 结构体 ---
struct MI_<Name>
{
    vec4  base_color_factor;
    float metallic_factor;
    float roughness_factor;
    float ao_strength;
    float emissive_strength;
    // 根据需要添加更多字段...
};

// --- 主入口 ---
SurfaceOutput EvalSurface(SurfaceInput si, MI_<Name> mi)
{
    SurfaceOutput so;
    so.baseColor = mi.base_color_factor.rgb;
    so.alpha     = mi.base_color_factor.a;
    so.metallic  = mi.metallic_factor;
    so.roughness = mi.roughness_factor;
    so.ao        = 1.0;
    so.emissive  = vec3(0.0);
    so.normal    = si.worldNormal;

#if QUALITY_TIER >= 1
    so.baseColor *= texture(TexAlbedo, si.uv0).rgb;
#endif

#if QUALITY_TIER >= 2
    // 法线贴图、MR 贴图采样...
#endif

    return so;
}

float EvalAlpha(SurfaceInput si, MI_<Name> mi)
{
    return mi.base_color_factor.a;
}
```

### 6.4 关键规则

1. **必须 Include surface_interface.glsl**：`#include "common/surface_interface.glsl"`
2. **MI 结构体必须与 C++ 侧一致**：GPU 端的 `MI_<Name>` 布局必须与 C++ `MaterialInstance` 结构体严格匹配（字段顺序、大小、对齐）
3. **纹理绑定从 set=2, binding=1 开始**：binding=0 保留给 MI SSBO
4. **用 QUALITY_TIER 守卫高级特性**：避免低端设备编译不必要的采样指令
5. **不要在 Surface Function 中写光照**：光照计算由 `common/lighting.glsl` 统一处理
6. **Camera-Relative 坐标**：`si.worldPos` 是 Camera-Relative，勿当作绝对世界坐标使用

### 6.5 注册新 Surface Function

在 C++ 侧的 `CompositorAssembler::GetSurfaceFunctionPath()` 中添加映射：

```cpp
case SurfaceType::YourNew:  return shader_lib_path_ + "/surface/yournew_surface.glsl";
```

同时在 `SurfaceType.h` 枚举中添加新值。

---

## 7. Compositor Template 结构

### 7.1 VS 模板结构

```
┌─ #version 450
├─ [CompositorAssembler 注入 #define 区域]
├─ Scene UBO 绑定（CameraUBO）
├─ Transform SSBO 绑定（L2W_SSBO）
├─ 顶点获取（VBO / SSBO 双路径，由 GEOMETRY_FETCH_SSBO 宏控制）
├─ Instance ID 获取
├─ Output varying 声明
└─ void main()
      ├─ 获取 L2W 矩阵
      ├─ 获取顶点数据
      ├─ 世界空间变换
      └─ gl_Position = viewProj * worldPos
```

### 7.2 FS 模板结构

```
┌─ #version 450
├─ [CompositorAssembler 注入 #define 区域]
├─ Input varying 声明
├─ #include SURFACE_FUNCTION_FILE     ← 由 CompositorAssembler 替换为实际路径
├─ #include "common/lighting.glsl"
├─ Scene UBO 绑定（ViewportUBO, CameraUBO, SkyUBO）
├─ MI SSBO 绑定
└─ void main()
      ├─ 从 MI SSBO 读取材质实例数据
      ├─ 构造 SurfaceInput
      ├─ SurfaceOutput so = EvalSurface(si, mi)     ← 调用 Surface Function
      ├─ vec3 litColor = EvalLighting(so, ...)       ← 调用 Lighting
      ├─ 添加 ambient + emissive
      └─ outColor = vec4(litColor, so.alpha)
```

### 7.3 SURFACE_FUNCTION_FILE 替换机制

FS 模板中写：
```glsl
#include SURFACE_FUNCTION_FILE
```

`CompositorAssembler` 在组装时将其替换为具体路径：
```glsl
#include "surface/standard_surface.glsl"
```

---

## 8. 宏注入机制

`CompositorAssembler` 在 `#version 450` 之后注入以下 `#define`：

| 宏名 | 类型 | 说明 | 示例值 |
|------|------|------|--------|
| `SURFACE_TYPE` | int | 材质类型枚举值 | `0`（Standard） |
| `QUALITY_TIER` | int | 画质等级 | `2`（Medium） |
| `SHADOW_MODE` | int | 阴影模式 | `0`（None） |
| `PLATFORM_BACKEND` | int | 平台后端 | `0`（PC） |
| `GEOMETRY_FETCH_SSBO` | int | 是否使用 SSBO 顶点获取 | `0` 或 `1` |

注入位置示例：
```glsl
#version 450
#define SURFACE_TYPE 0
#define QUALITY_TIER 2
#define SHADOW_MODE 0
#define PLATFORM_BACKEND 0
#define GEOMETRY_FETCH_SSBO 0
// === Compositor Template: Forward Opaque FS ===
...
```

在 Shader 中直接使用这些宏做条件编译：
```glsl
#if QUALITY_TIER >= 2
    // 高级特性
#endif

#if GEOMETRY_FETCH_SSBO
    vec3 pos = FetchPosition(gl_VertexIndex);
#else
    vec3 pos = inPosition;
#endif
```

---

## 9. Include 规范

### 9.1 GLSLCompiler #include 支持

ULRE 的 GLSL 编译器（基于 glslang）支持 `#include`，通过 `GL_GOOGLE_include_directive` 扩展实现。

- Include 路径基于 `ShaderLibrary/` 根目录
- 引擎侧通过 `AddShaderIncludePath()` 设置搜索路径
- 使用双引号形式：`#include "common/lighting.glsl"`

### 9.2 Include Guard

所有可能被多个文件 include 的公共模块**必须**使用 include guard：

```glsl
#ifndef SURFACE_INTERFACE_GLSL
#define SURFACE_INTERFACE_GLSL

// ... 内容 ...

#endif // SURFACE_INTERFACE_GLSL
```

Guard 宏命名约定：将文件名转为大写，`.` 替换为 `_`（如 `SURFACE_INTERFACE_GLSL`）。

### 9.3 Include 路径约定

所有 `#include` 路径**相对于 `ShaderLibrary/` 根目录**：

```glsl
#include "common/surface_interface.glsl"   // ✅ 正确
#include "common/lighting.glsl"            // ✅ 正确
#include "surface_interface.glsl"          // ❌ 错误（缺少 common/ 前缀）
```

---

## 10. 已有 Shader 参考

### `common/surface_interface.glsl`
SurfaceInput / SurfaceOutput / SurfaceOutputExt 结构体定义。所有 Surface Function 必须 include。

### `surface/standard_surface.glsl`
Standard PBR Surface Function。参考实现：
- 定义 `MI_Standard` 结构体
- 实现 `EvalSurface()` + `EvalAlpha()`
- 使用 `QUALITY_TIER` 守卫纹理采样
- 纹理绑定：set=2, binding=1 (Albedo), binding=2 (Normal), binding=3 (MR)

### `common/lighting.glsl`
分级光照计算：
- `QUALITY_TIER 0~1`：Lambert
- `QUALITY_TIER 2~3`：BlinnPhong
- `QUALITY_TIER 4+`：PBR Cook-Torrance（暂 fallback BlinnPhong）

### `common/depth_utils.glsl`
Reversed-Z 深度工具：
- `LinearizeDepth(d, near_z)`：线性化 Reversed-Z 深度值
- `ReconstructWorldPos(ndc, depth, inv_view_proj)`：从 NDC+深度重建 Camera-Relative 世界坐标

### `common/vertex_fetch_ssbo.glsl`
SSBO 顶点获取模块：
- `VertexData` 结构体（position, normal, uv0）
- `VertexDataBuffer` (set=3, binding=18) + `IndexDataBuffer` (set=3, binding=19)
- `FetchPosition()` / `FetchNormal()` / `FetchUV0()` 函数

### `compositor/main_forward_opaque.vert.glsl`
Forward Opaque VS 模板：支持 VBO / SSBO 双路径顶点获取。

### `compositor/main_forward_opaque.frag.glsl`
Forward Opaque FS 模板：`#include SURFACE_FUNCTION_FILE` + `EvalLighting()`。

---

## 11. 常见问题

### Q: 如何添加一种新材质类型？

1. 在 `SurfaceType.h` 枚举中添加新值
2. 创建 `ShaderLibrary/surface/<name>_surface.glsl`，实现 `EvalSurface()` + `EvalAlpha()`
3. 在 `CompositorAssembler::GetSurfaceFunctionPath()` 中添加映射
4. MI 结构体须 C++ / GLSL 两侧同步定义

### Q: 为什么 worldPos 不是绝对世界坐标？

ULRE 使用 Camera-Relative Rendering 消除大世界浮点精度问题。所有顶点坐标在提交 GPU 前已减去相机世界坐标。若需要绝对坐标，使用 `worldPos + CameraUBO.cameraPosWorld`。

### Q: GLSL 编译报 "undeclared identifier" 纹理变量？

纹理 `sampler2D` 声明必须在 Surface Function 文件中显式写出，且受 `QUALITY_TIER` 守卫。
如果目标 QualityTier 低于纹理声明的守卫等级，该纹理不会被声明，引用它会报错。
确保代码中对纹理的引用也在相同的 `#if QUALITY_TIER` 守卫内。

### Q: 编译报 struct 重定义？

确保共享头文件（如 `surface_interface.glsl`）使用了 `#ifndef` include guard。
多个文件 include 同一头文件时，没有 guard 会导致重定义。

### Q: ViewportUBO / CameraUBO 成员与 C++ 不匹配？

GLSL UBO 定义必须与 C++ 结构体**字段顺序、大小、padding 完全一致**。
使用 `std140` 布局规则对齐。出现全零或错误数据时，首先检查是否存在结构体字段不匹配。
