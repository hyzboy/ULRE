# ULRE 渲染管线增量重构计划

> **配套文档**：
> - `SimplifiedMaterialSystem_Design.md` — 材质系统设计（目标架构）
> - `RenderingPipeline_Design.md` — 渲染管线设计（目标架构）
>
> **原则**：每一步都是最小化变动，完成后**必须能编译通过并可由用户手动测试**。
> 每步标注 `[测试]` 说明验证方法。若某步打破了已有功能，必须在同一步内修复。

---

## 目录

- [第一阶段：准备工作与代码清理](#第一阶段准备工作与代码清理)
- [第二阶段：核心类型定义](#第二阶段核心类型定义)
- [第三阶段：Descriptor Set 布局迁移](#第三阶段descriptor-set-布局迁移)
- [第四阶段：Reversed-Z 深度管线 + Camera-Relative Rendering](#第四阶段reversed-z-深度管线--camera-relative-rendering)
- [第五阶段：Surface Function 与 Compositor 基础](#第五阶段surface-function-与-compositor-基础)
- [第六阶段：SSBO 顶点获取路径](#第六阶段ssbo-顶点获取路径)
- [第七阶段：Forward 材质迁移](#第七阶段forward-材质迁移)
- [第八阶段：阴影系统](#第八阶段阴影系统)
- [第九阶段：HZB 与遮挡剔除](#第九阶段hzb-与遮挡剔除)
- [第十阶段：VBuffer 渲染路径](#第十阶段vbuffer-渲染路径)
- [第十一阶段：Meshlet 几何管线](#第十一阶段meshlet-几何管线)
- [第十二阶段：后处理管线](#第十二阶段后处理管线)
- [第十三阶段：Material LOD 与 Special Surface](#第十三阶段material-lod-与-special-surface)
- [第十四阶段：Terrain 系统](#第十四阶段terrain-系统)
- [第十五阶段：Clustered Shading 与高级光照](#第十五阶段clustered-shading-与高级光照)
- [第十六阶段：旧代码清理](#第十六阶段旧代码清理)
- [附录 A：文件变更追踪表](#附录-a文件变更追踪表)
- [附录 B：阶段依赖图](#附录-b阶段依赖图)

---

## 术语约定

| 缩写 | 含义 |
|------|------|
| **SF** | Surface Function |
| **CT** | Compositor Template |
| **SPK** | ShaderPermutationKey |
| **DS** | Descriptor Set |
| **MI** | MaterialInstance |
| **L2W** | Local-to-World 矩阵 |

---

## 第一阶段：准备工作与代码清理

> **目标**：在不破坏任何现有功能的前提下，清理与新设计冲突的旧枚举/结构；建立新目录结构。
> 此阶段**只删除未被引用的定义**或**添加新文件/目录**，不修改任何活跃代码路径。

---

### Step 1.1 — 建立回归测试基线

**变动**：无代码变动。

**操作**：
1. 编译全部 example（Debug + Release）
2. 运行每个 example，截图保存至 `doc/baseline/` 目录
3. 编写一份 `doc/baseline/README.md`，列出每个 example 的截图文件名和预期行为

**[测试]**：所有 example 编译通过、运行正常。截图作为后续步骤的回归参考。

---

### Step 1.2 — 创建新目录结构

**变动**：仅创建空目录（加占位 `.gitkeep`），不修改任何已有文件。

**新增目录**：
```
ShaderLibrary/surface/           ← Surface Function 存放目录
ShaderLibrary/compositor/        ← Compositor Template 存放目录
ShaderLibrary/common/            ← 公共 GLSL include（如已存在则跳过）
ShaderLibrary/pass/              ← 独立 Pass shader（HZB/Shadow/VBuffer etc.）
ShaderLibrary/postprocess/       ← 后处理 shader
ShaderLibrary/debug/             ← 调试 shader
inc/hgl/mtl/new/                 ← 新类型定义（临时目录，后续合并）
```

**[测试]**：编译通过（新目录不影响编译）。

---

### Step 1.3 — 标记 GBuffer 相关代码为 deprecated

**变动**：在 `inc/hgl/common/RenderFlowDef.h` 中给即将删除的枚举值添加 `[[deprecated]]` 属性。

**具体修改**：

`RenderFlowDef.h` 中：
```cpp
// PipelineRenderPath — 标记即将删除的值
enum class PipelineRenderPath : uint8
{
    Forward,
    [[deprecated("将在 Step 1.5 删除，设计文档不含 GBuffer 延迟路径")]]
    GBufferDeferred,
    VBufferDeferred,
    [[deprecated("将在 Step 1.5 删除")]]
    MobileSubpassGBufferDeferred,
    PostProcess
};
```

对以下也添加 `[[deprecated]]`：
- `GBufferFormatLevel` 全部值
- `GBufferQualityPreset` 全部值
- `GBufferConfiguration` 结构体
- `NormalEncodingMode`（如仅用于 GBuffer）
- `NormalCompressionPolicy`（如仅用于 GBuffer）
- `RenderStage` 中 `GBuffer_*` 和 `Deferred_Lighting*` 相关值

**[测试]**：编译通过。存在 deprecated 警告是预期行为——记录有哪些引用点，为后续删除做准备。

---

### Step 1.4 — 消除 GBuffer deprecated 引用

**变动**：逐个修复上一步产生的 deprecated 警告。对每个引用 GBuffer 的代码：
- 如果是 `switch` 中未使用的 `case`：直接删除该 `case` 分支
- 如果是工厂函数中的配置路径：将 GBuffer 路径改为 `Forward`（等价行为）
- 如果是注释中的引用：更新注释

**原则**：每修复一个文件就编译一次——不积累。

**[测试]**：编译零 deprecated 警告（仅 GBuffer 相关），所有 example 运行不变。

---

### Step 1.5 — 删除 GBuffer 枚举与结构体

**变动**：从 `RenderFlowDef.h` 中删除以下定义：

```
删除枚举值:
  PipelineRenderPath::GBufferDeferred
  PipelineRenderPath::MobileSubpassGBufferDeferred
  GBufferFormatLevel (整个枚举)
  GBufferQualityPreset (整个枚举)
  NormalEncodingMode (如仅用于 GBuffer)

删除结构体:
  GBufferFormatSpec
  NormalCompressionPolicy (如仅用于 GBuffer)
  GBufferConfiguration

删除 RenderStage 中:
  GBuffer_BaseColor, GBuffer_Normal, GBuffer_Emissive, GBuffer_Specular
  Deferred_LightingAccum, Deferred_LightingResolve, Deferred_LightingTiled, Deferred_LightingClustered
```

**保留**：`RenderStage` 中 `EarlyZ_*`, `ShadowMap_*`, `VisibilityBuffer_Fill`, `Forward_*`, `HZB_*`, `PostProcess_*`, `Debug_Visualization`。

**[测试]**：编译通过，所有 example 运行正常。

---

### Step 1.6 — 精简 RenderFlowPreset

**变动**：`RenderFlowDef.h` 中精简 `RenderFlowPreset`：

```
保留:
  Forward_Basic
  Forward_WithEarlyZ
  ForwardPlus_SingleHZB
  VisibilityBuffer_Deferred
  Mobile_Forward

删除:
  Deferred_Traditional
  Deferred_TiledLighting
  Deferred_ClusteredLighting
  Mobile_SubpassDeferred
  其他 GBuffer 相关
```

同时删除或 stub 掉 `RenderFlowDefinition` 中引用已删除 Preset 的初始化代码。

**[测试]**：编译通过，现有 example 不使用 Deferred 路径，运行不受影响。

---

### Step 1.7 — 精简 RenderStage 枚举

**变动**：将 `RenderStage` 从 26 个精简到实际需要的集合：

```cpp
enum class RenderStage : uint8
{
    EarlyZ_Solid,
    EarlyZ_Masked,
    ShadowMap_Dynamic,
    ShadowMap_Cached,
    ShadowMap_Cascade,
    VisibilityBuffer_Fill,
    Forward_Opaque,
    Forward_Masked,
    Forward_Transparent,
    Forward_Sky,
    HZB_Generation,
    HZB_Culling,
    PostProcess_TAA,
    PostProcess_Bloom,
    PostProcess_ToneMap,
    PostProcess_FXAA,
    PostProcess_MotionBlur,
    PostProcess_DOF,
    PostProcess_SSR,
    PostProcess_SSAO,
    Debug_Visualization,
    COUNT
};
```

**[测试]**：编译通过，所有 example 运行正常。

---

## 第二阶段：核心类型定义

> **目标**：定义新设计要求的核心枚举和结构体。此阶段**只新增头文件**，不修改任何已有代码。
> 新类型先放在 `inc/hgl/mtl/new/` 临时目录下，后续合并。

---

### Step 2.1 — 定义 SurfaceType 枚举

**新增文件**：`inc/hgl/mtl/new/SurfaceType.h`

```cpp
#pragma once
#include<hgl/type/DataType.h>

namespace hgl::graph
{
    enum class SurfaceType : uint8
    {
        Unlit = 0,
        Standard,
        Skin,
        Hair,
        Cloth,
        Eye,
        Foliage,
        ClearCoat,
        Water,
        Terrain,
        Sky,

        ENUM_CLASS_RANGE(Unlit, Sky)
    };

    constexpr const char* SurfaceTypeNames[] = {
        "Unlit", "Standard", "Skin", "Hair", "Cloth",
        "Eye", "Foliage", "ClearCoat", "Water", "Terrain", "Sky"
    };

    inline const char* GetSurfaceTypeName(SurfaceType st)
    {
        const uint8 idx = static_cast<uint8>(st);
        if (idx > static_cast<uint8>(SurfaceType::Sky)) return "Unknown";
        return SurfaceTypeNames[idx];
    }
}
```

**[测试]**：编译通过（新文件未被任何人引用，不影响现有代码）。写一个简单的 static_assert 测试编译。

---

### Step 2.2 — 定义 QualityTier 枚举

**新增文件**：`inc/hgl/mtl/new/QualityTier.h`

```cpp
#pragma once
#include<hgl/type/DataType.h>

namespace hgl::graph
{
    enum class QualityTier : uint8
    {
        Lowest = 0,
        Low,
        Medium,
        High,
        Ultra,
        Cinematic,

        ENUM_CLASS_RANGE(Lowest, Cinematic)
    };

    constexpr const char* QualityTierNames[] = {
        "Lowest", "Low", "Medium", "High", "Ultra", "Cinematic"
    };
}
```

**[测试]**：编译通过。

---

### Step 2.3 — 定义 BlendMode 枚举

**新增文件**：`inc/hgl/mtl/new/BlendMode.h`

```cpp
#pragma once
#include<hgl/type/DataType.h>

namespace hgl::graph
{
    enum class BlendMode : uint8
    {
        Opaque = 0,
        Masked,
        Transparent,
        Dither,
        AlphaToCoverage,

        ENUM_CLASS_RANGE(Opaque, AlphaToCoverage)
    };
}
```

**[测试]**：编译通过。

---

### Step 2.4 — 定义 PassType 枚举

**新增文件**：`inc/hgl/mtl/new/PassType.h`

```cpp
#pragma once
#include<hgl/type/DataType.h>

namespace hgl::graph
{
    enum class PassType : uint8
    {
        ForwardOpaque = 0,
        ForwardMasked,
        ForwardTransparent,
        ForwardDither,
        ForwardA2C,
        ShadowOpaque,
        ShadowMasked,
        EarlyZSolid,
        EarlyZMasked,
        VBufferID,

        ENUM_CLASS_RANGE(ForwardOpaque, VBufferID)
    };
}
```

**[测试]**：编译通过。

---

### Step 2.5 — 定义 PlatformBackend 与 GeometryFetchMode 枚举

**新增文件**：`inc/hgl/mtl/new/PlatformBackend.h`

```cpp
#pragma once
#include<hgl/type/DataType.h>

namespace hgl::graph
{
    enum class PlatformBackend : uint8
    {
        PC = 0,
        Apple,
        Android,

        ENUM_CLASS_RANGE(PC, Android)
    };

    enum class GeometryFetchMode : uint8
    {
        SSBO = 0,
        VBO,

        ENUM_CLASS_RANGE(SSBO, VBO)
    };
}
```

**[测试]**：编译通过。

---

### Step 2.6 — 定义 MaterialCategory 分类

**新增文件**：`inc/hgl/mtl/new/MaterialCategory.h`

```cpp
#pragma once
#include<hgl/type/DataType.h>

namespace hgl::graph
{
    // 引擎内建材质分类 (0-3)
    // 项目扩展材质分类从 64 开始
    enum class MaterialCategory : uint8
    {
        // 引擎内建
        Unlit2D     = 0,    // 2D 无光照
        Unlit3D     = 1,    // 3D 无光照
        Standard3D  = 2,    // 3D 标准光照 (Standard Surface)
        Special3D   = 3,    // 3D 特殊表面 (Skin/Hair/Cloth/Eye/Foliage/ClearCoat/Water)

        // 项目扩展从此开始
        ProjectBase = 64,
    };

    constexpr bool IsBuiltinCategory(MaterialCategory cat)
    {
        return static_cast<uint8>(cat) < static_cast<uint8>(MaterialCategory::ProjectBase);
    }
}
```

**[测试]**：编译通过。

---

### Step 2.7 — 定义新 ShaderPermutationKey（草案，不替换旧的）

**新增文件**：`inc/hgl/mtl/new/NewShaderPermutationKey.h`

```cpp
#pragma once
#include"SurfaceType.h"
#include"QualityTier.h"
#include"PlatformBackend.h"
#include<hgl/type/DataType.h>

namespace hgl::graph
{
    // 16-bit packed key:
    //   [15:12] SurfaceType  (4 bit, max 16)
    //   [11:9]  QualityTier  (3 bit, max 8)
    //   [8:7]   ShadowMode   (2 bit: None/PCF/PCSS)
    //   [6:4]   Flags        (3 bit: fog/skinning/wind etc.)
    //   [3:2]   Platform     (2 bit: PC/Apple/Android)
    //   [1:0]   Reserved     (2 bit)
    struct NewShaderPermutationKey
    {
        uint16 packed;

        NewShaderPermutationKey() : packed(0) {}

        void SetSurfaceType(SurfaceType st)     { packed = (packed & 0x0FFF) | (static_cast<uint16>(st) << 12); }
        void SetQualityTier(QualityTier qt)      { packed = (packed & 0xF1FF) | (static_cast<uint16>(qt) << 9); }
        void SetShadowMode(uint8 sm)             { packed = (packed & 0xFE7F) | ((sm & 0x3) << 7); }
        void SetFlags(uint8 flags)               { packed = (packed & 0xFF8F) | ((flags & 0x7) << 4); }
        void SetPlatform(PlatformBackend pb)     { packed = (packed & 0xFFF3) | (static_cast<uint16>(pb) << 2); }

        SurfaceType     GetSurfaceType() const   { return static_cast<SurfaceType>((packed >> 12) & 0xF); }
        QualityTier     GetQualityTier() const   { return static_cast<QualityTier>((packed >> 9) & 0x7); }
        uint8           GetShadowMode()  const   { return (packed >> 7) & 0x3; }
        uint8           GetFlags()       const   { return (packed >> 4) & 0x7; }
        PlatformBackend GetPlatform()    const   { return static_cast<PlatformBackend>((packed >> 2) & 0x3); }

        bool operator==(const NewShaderPermutationKey& o) const { return packed == o.packed; }
        bool operator<(const NewShaderPermutationKey& o) const { return packed < o.packed; }

        // 生成 GLSL #define 字符串
        void AppendGLSLDefines(AnsiString& out) const;
    };
}
```

**[测试]**：编译通过。新文件不影响旧代码。

---

### Step 2.8 — 定义 MaterialPresetDef 结构体

**新增文件**：`inc/hgl/mtl/new/MaterialPresetDef.h`

```cpp
#pragma once
#include"SurfaceType.h"
#include"QualityTier.h"
#include"MaterialCategory.h"
#include<hgl/type/DataType.h>
#include<hgl/type/String.h>

namespace hgl::graph
{
    struct TextureSlotDef
    {
        AnsiString name;            // "albedo", "normal", "metallic_roughness", ...
        QualityTier min_tier;       // 低于此档位不绑定此槽（使用默认纹理）
        bool required;              // 是否必须
    };

    struct MaterialPresetDef
    {
        uint16          preset_id;
        SurfaceType     surface_type;
        MaterialCategory category;
        AnsiString      name;               // "StandardTexture", "PureColor2D", ...
        AnsiString      mi_struct_name;     // GLSL 中 MI 结构体名
        uint32          mi_struct_size;     // MI 数据字节数
        TextureSlotDef  texture_slots[8];   // 最多 8 个纹理槽
        uint8           texture_slot_count;
        SurfaceType     fallback_surface_type;      // Material LOD 降级目标
        QualityTier     unique_feature_min_tier;    // 低于此档位 fallback 到 fallback_surface_type
    };
}
```

**[测试]**：编译通过。

---

### Step 2.9 — 定义 DeviceQualityProfile 结构体（仅数据，无检测逻辑）

**新增文件**：`inc/hgl/mtl/new/DeviceQualityProfile.h`

```cpp
#pragma once
#include"QualityTier.h"
#include"PlatformBackend.h"
#include<hgl/type/DataType.h>

namespace hgl::graph
{
    struct DeviceQualityProfile
    {
        QualityTier         quality_tier;
        PlatformBackend     platform;
        GeometryFetchMode   geometry_fetch;

        // 特性掩码
        bool support_ssbo_vertex;       // SSBO 顶点获取
        bool support_meshlet;           // Meshlet 管线
        bool support_hzb;               // HZB 生成和遮挡剔除
        bool support_clustered;         // Clustered Shading
        bool support_vbuffer;           // VBuffer 路径
        bool support_compute;           // Compute Shader
        bool support_indirect_draw;     // Indirect Draw
        bool support_d32_sfloat;        // D32_SFLOAT 深度格式

        uint32 max_texture_size;        // 最大纹理尺寸
        float  render_scale;            // 渲染分辨率缩放 (0.5~1.0)
        uint8  max_shadow_cascade;      // 最大阴影级联数
        uint8  max_point_lights;        // 最大点光源数

        // 默认构造 — PC High
        DeviceQualityProfile()
            : quality_tier(QualityTier::High)
            , platform(PlatformBackend::PC)
            , geometry_fetch(GeometryFetchMode::SSBO)
            , support_ssbo_vertex(true)
            , support_meshlet(false)
            , support_hzb(true)
            , support_clustered(true)
            , support_vbuffer(true)
            , support_compute(true)
            , support_indirect_draw(true)
            , support_d32_sfloat(true)
            , max_texture_size(4096)
            , render_scale(1.0f)
            , max_shadow_cascade(4)
            , max_point_lights(64)
        {}
    };
}
```

**[测试]**：编译通过。

---

### Step 2.10 — 实现 DeviceQualityProfile 自动检测

**新增文件**：`src/ShaderGen/DeviceQualityProfile.cpp`

**操作**：
1. 实现 `DeviceQualityProfile DetectDeviceQuality(const VkPhysicalDeviceProperties& props, const VkPhysicalDeviceFeatures& features)` 函数
2. 根据 `vendorID` 和 `deviceType` 判断平台
3. 根据 `limits.maxStorageBufferRange`、`limits.maxComputeWorkGroupCount` 等判断能力
4. 设置对应的 `QualityTier` 和特性掩码

**修改文件**：在 `DeviceQualityProfile.h` 中声明该函数。

**[测试]**：编译通过。可写一个简单的测试 example 打印检测结果。

---

### Step 2.11 — 定义 NewShaderPermutationKey::AppendGLSLDefines() 实现

**新增文件**：`src/ShaderGen/NewShaderPermutationKey.cpp`

```cpp
void NewShaderPermutationKey::AppendGLSLDefines(AnsiString& out) const
{
    out += "#define SURFACE_TYPE ";
    out += AnsiString::valueOf(static_cast<int>(GetSurfaceType()));
    out += "\n";

    out += "#define QUALITY_TIER ";
    out += AnsiString::valueOf(static_cast<int>(GetQualityTier()));
    out += "\n";

    out += "#define SHADOW_MODE ";
    out += AnsiString::valueOf(GetShadowMode());
    out += "\n";

    out += "#define PLATFORM_PC ";
    out += (GetPlatform() == PlatformBackend::PC ? "1" : "0");
    out += "\n";

    out += "#define PLATFORM_APPLE ";
    out += (GetPlatform() == PlatformBackend::Apple ? "1" : "0");
    out += "\n";

    out += "#define PLATFORM_ANDROID ";
    out += (GetPlatform() == PlatformBackend::Android ? "1" : "0");
    out += "\n";

    // geometry fetch mode 由 platform 隐含
    const bool ssbo = (GetPlatform() != PlatformBackend::Android)
                    || (GetQualityTier() >= QualityTier::High);
    out += "#define GEOMETRY_FETCH_SSBO ";
    out += (ssbo ? "1" : "0");
    out += "\n";
}
```

**[测试]**：编译通过。可写一个简单测试验证输出字符串。

---

## 第三阶段：Descriptor Set 布局迁移

> **目标**：将当前 7 个 Descriptor Set 收敛为 4 个。
> **关键约束**：现有 `TransformAssignmentBuffer`、`MaterialInstanceAssignmentBuffer`、Texture2DArray 纹理池**完全保留不动**。
> 此阶段采用**并行双轨**策略——先新增 4-Set 接口，保留旧 7-Set 接口，逐步迁移。

---

### Step 3.1 — 定义新 DescriptorSetType 枚举（并行）

**新增文件**：`inc/hgl/mtl/new/NewDescriptorSetType.h`

```cpp
#pragma once
#include<hgl/type/DataType.h>

namespace hgl::graph
{
    // 新 4-Set 布局：与旧 DescriptorSetType 并存，迁移完成后替换
    enum class NewDescriptorSetType : uint8
    {
        PerScene    = 0,    // Set 0: 全局/每帧数据 (ViewportInfo, CameraInfo, SkyInfo, LightBuffer)
        PerView     = 1,    // Set 1: 每视图数据 (L2W SSBO)
        PerMaterial = 2,    // Set 2: 每材质数据 (MI SSBO, 纹理槽)
        PerDraw     = 3,    // Set 3: 环境/管线 RT (ShadowMap, SSAO, IBL, HZB, Cluster, Fog, ...)

        COUNT = 4
    };
}
```

**[测试]**：编译通过。

---

### Step 3.2 — 定义新 Set 的 Binding 布局常量

**新增文件**：`inc/hgl/mtl/new/DescriptorSetBindings.h`

定义每个 Set 每个 Binding 的用途常量：

```cpp
#pragma once

namespace hgl::graph::DSBinding
{
    // Set 0 — PerScene
    namespace PerScene
    {
        constexpr uint32 ViewportInfo   = 0;
        constexpr uint32 CameraInfo     = 1;
        constexpr uint32 SkyInfo        = 2;
        constexpr uint32 LightBuffer    = 3;
    }

    // Set 1 — PerView
    namespace PerView
    {
        constexpr uint32 LocalToWorld   = 0;    // SSBO: mat4 池
    }

    // Set 2 — PerMaterial
    namespace PerMaterial
    {
        constexpr uint32 MI_SSBO        = 0;
        constexpr uint32 TexAlbedo      = 1;
        constexpr uint32 TexNormal      = 2;
        constexpr uint32 TexMR          = 3;    // Metallic-Roughness
        constexpr uint32 TexAO          = 4;
        constexpr uint32 TexEmissive    = 5;
        constexpr uint32 TexDetail      = 6;    // Detail Normal
        // 7-12: Special Surface 扩展纹理
        constexpr uint32 TexSpecial0    = 7;
        constexpr uint32 TexSpecial1    = 8;
        constexpr uint32 TexSpecial2    = 9;
        constexpr uint32 TexSpecial3    = 10;
        constexpr uint32 TexSpecial4    = 11;
        constexpr uint32 TexSpecial5    = 12;
    }

    // Set 3 — PerDraw (Environment / Pipeline RT)
    namespace PerDraw
    {
        constexpr uint32 ColorPalette       = 0;
        constexpr uint32 ShadowMapNear      = 1;
        constexpr uint32 ShadowMask         = 2;
        constexpr uint32 SSAO_RT            = 3;
        constexpr uint32 IBL_Irradiance     = 4;
        constexpr uint32 IBL_Prefiltered    = 5;
        constexpr uint32 IBL_BRDF_LUT      = 6;
        constexpr uint32 SSS_LUT           = 7;
        constexpr uint32 DebugLightingCfg   = 8;
        constexpr uint32 HZB_Pyramid       = 9;
        constexpr uint32 ClusterLightList   = 10;
        constexpr uint32 ClusterAABB        = 11;
        constexpr uint32 FogParams          = 12;
        constexpr uint32 SSR_RT             = 13;
        constexpr uint32 ExposureData       = 14;
        constexpr uint32 MeshletBuffer      = 15;
        constexpr uint32 InstanceBuffer     = 16;
        constexpr uint32 TerrainHeightMap   = 17;
        constexpr uint32 VertexDataBuffer   = 18;
        constexpr uint32 IndexDataBuffer    = 19;
        constexpr uint32 ShadowMapCached    = 20;
        constexpr uint32 CapsuleShadowData  = 21;
    }
}
```

**[测试]**：编译通过。

---

### Step 3.3 — 创建 NewDescriptorSetLayoutFactory

**新增文件**：`inc/hgl/mtl/new/NewDescriptorSetLayoutFactory.h` + `src/ShaderGen/NewDescriptorSetLayoutFactory.cpp`

**功能**：
- `CreatePerSceneLayout(VkDevice)` → `VkDescriptorSetLayout`
- `CreatePerViewLayout(VkDevice)` → `VkDescriptorSetLayout`
- `CreatePerMaterialLayout(VkDevice, SurfaceType)` → `VkDescriptorSetLayout`（Terrain 走专用布局）
- `CreatePerDrawLayout(VkDevice, bool ssbo_platform)` → `VkDescriptorSetLayout`
- `CreateNewPipelineLayout(VkDevice, SurfaceType, bool ssbo_platform)` → `VkPipelineLayout`

**不修改任何已有代码**，仅新增文件。

**[测试]**：编译通过。可选：写一个独立测试创建 Layout 验证不报错。

---

### Step 3.4 — 在 RenderContext 中注册新 Layout（双轨共存）

**修改文件**：`inc/hgl/graph/render/RenderContext.h`（或等价的全局管理类）

**变动**：
1. 新增成员变量 `VkPipelineLayout new_pipeline_layout_` 和 4 个 `VkDescriptorSetLayout`
2. 在初始化时调用 `NewDescriptorSetLayoutFactory` 创建
3. 新增 `GetNewPipelineLayout()` 方法
4. **不修改**现有 `GetPipelineLayout()` 方法——旧路径不变

**[测试]**：编译通过，所有 example 走旧路径，运行不变。

---

### Step 3.5 — 创建新 DescriptorBinding 适配器

**新增文件**：`inc/hgl/mtl/new/NewDescriptorBinding.h` + `src/ShaderGen/NewDescriptorBinding.cpp`

**功能**：实现一个新的 `NewDescriptorBinding` 类，对标旧 `DescriptorBinding`，但使用新的 4-Set 布局。

接口：
```cpp
class NewDescriptorBinding
{
public:
    void BindPerScene(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set);
    void BindPerView(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set);
    void BindPerMaterial(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set);
    void BindPerDraw(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet set);
};
```

**[测试]**：编译通过。

---

### Step 3.6 — (预留) 逐个 example 切换到新 Layout

> 此步骤将在第七阶段（材质迁移）中逐个执行。
> 这里只是预留标记——当某个材质完成 Surface Function 迁移后，
> 同时切换其 Descriptor Set 到新 4-Set 布局。

---

## 第四阶段：Reversed-Z 深度管线 + Camera-Relative Rendering

> **目标**：全管线切换 Reversed-Z + D32_SFLOAT + Infinite Far Plane；
> 同时实现 Camera-Relative Rendering 消除大世界 float32 精度拖动。
> 此阶段可与第五、六阶段并行进行。

---

### Step 4.1 — 实现 Reversed-Z 投影矩阵函数

**新增文件**：`inc/hgl/graph/camera/ReversedZProj.h`

```cpp
// MakeInfiniteReversedZProj(fov_y_radians, aspect, near_plane)
// 返回 Reversed-Z + Infinite Far 投影矩阵
Matrix4f MakeInfiniteReversedZProj(float fov_y, float aspect, float near_z);
```

**新增实现**：`src/SceneGraph/camera/ReversedZProj.cpp`

**不修改现有 Camera 代码**。

> **⚠️ 实现备忘（Z 轴向上坐标系 + 引擎投影约定）**：
> 本引擎使用 **Z 轴向上右手坐标系**，`PerspectiveMatrix()` 返回的投影矩阵中
> `m[0][0]` 和 `m[1][1]` 均为 **负值**（`-f/aspect` 和 `-f`），用以翻转 X/Y
> 到 Vulkan NDC。`MakeInfiniteReversedZProj()` **必须保持相同的负号约定**，
> 否则画面上下左右镜像翻转。正确写法：
> ```cpp
> m[0][0] = -f / aspect;
> m[1][1] = -f;
> m[2][3] = -1.0f;
> m[3][2] = near_z;
> ```

**[测试]**：编译通过。可选：写单元测试验证矩阵值（near 映射到 1.0，无穷远映射到 0.0）。

---

### Step 4.2 — Camera 增加 Reversed-Z 模式开关

**修改文件**：Camera 相关头文件（如 `inc/hgl/graph/camera/Camera.h`）

**变动**：
1. 新增 `bool use_reversed_z = false;` 成员
2. 在投影矩阵计算处增加条件分支：`if (use_reversed_z) return MakeInfiniteReversedZProj(...);`
3. 默认值 `false`——不影响现有行为

**[测试]**：编译通过，所有 example 运行不变（默认未启用 Reversed-Z）。

---

### Step 4.3 — 在一个 example 中测试 Reversed-Z

**修改**：选择一个简单 example（如 `SimpleCube`），在其初始化中设置 `camera->use_reversed_z = true;`。

**同时修改**：
1. 该 example 创建 RenderPass 时使用 `VK_COMPARE_OP_GREATER`
2. 该 example 的 depth clear 值改为 `0.0f`

**[测试]**：该 example 渲染正确（立方体可见、深度测试正常）。其他 example 不受影响。

---

### Step 4.4 — 全局默认切换 Reversed-Z

**变动**：
1. `Camera` 默认 `use_reversed_z = true`
2. `VKPipelineData` 默认 `depthCompareOp = VK_COMPARE_OP_GREATER`
3. `VKRenderPass` 默认 `depthClearValue = 0.0f`
4. 逐个修复因此产生的渲染错误（主要是天空 Pass 需要输出 depth=0.0）

**[测试]**：所有 example 渲染正确，与 Step 1.1 的基线截图对比。

---

### Step 4.5 — D32_SFLOAT 深度格式选择

**修改文件**：RenderTarget 或 RenderPass 创建逻辑

**变动**：
1. 检测 `VK_FORMAT_D32_SFLOAT` 支持
2. 支持则使用 D32_SFLOAT，不支持则降级 D24_UNORM_S8_UINT
3. 在 `DeviceQualityProfile` 中记录 `support_d32_sfloat`

**[测试]**：编译通过，深度精度提升（远景 Z-fighting 减少或消除）。

---

### Step 4.6 — 新增 depth_utils.glsl

**新增文件**：`ShaderLibrary/common/depth_utils.glsl`

```glsl
// Reversed-Z depth utilities
float LinearizeDepth(float d, float near_z)
{
    return near_z / d;  // Reversed-Z: d=1 at near, d→0 at far
}

vec3 ReconstructWorldPos(vec2 ndc, float depth, mat4 inv_view_proj)
{
    vec4 clip = vec4(ndc * 2.0 - 1.0, depth, 1.0);
    vec4 world = inv_view_proj * clip;
    return world.xyz / world.w;   // 返回 camera-relative world position
}

// 若需要绝对世界坐标：worldPosAbsolute = ReconstructWorldPos(...) + cameraPosWorld
```

**[测试]**：GLSL 文件语法正确（可通过 glslangValidator 验证）。

---

### Step 4.7 — Camera-Relative Rendering 实现

**目标**：消除大世界场景中远离世界原点时的 float32 顶点拖动。将相机世界坐标从 L2W 矩阵中减去，使 GPU 侧始终在相机相对坐标系下运算。

#### 4.7.1 — Camera 类增加 double 精度位置存储

**修改文件**：Camera 相关类

**变动**：
1. Camera 内部位置存储从 `vec3` 升级为 `dvec3`（double 精度）
2. 提供 `dvec3 GetWorldPositionDouble()` 接口

```cpp
class Camera {
    dvec3 worldPosition_double;  // double 精度世界坐标
public:
    dvec3 GetWorldPositionDouble() const { return worldPosition_double; }
};
```

**[测试]**：编译通过，现有 Camera API 不受影响。

---

#### 4.7.2 — L2W SSBO 上传时减去相机位置

**修改文件**：Transform 上传 / Scene Upload 逻辑

**变动**：
1. 每帧上传 L2W SSBO 前，将每个 `mat4` 的平移列减去 `cameraWorldPosition`
2. 减法用 double 执行，结果转 float 上传

```cpp
void UploadTransforms(const Camera& cam, span<mat4> l2wPool)
{
    dvec3 camPos = cam.GetWorldPositionDouble();

    for (auto& l2w : l2wPool) {
        l2w[3][0] -= float(camPos.x);
        l2w[3][1] -= float(camPos.y);
        l2w[3][2] -= float(camPos.z);
    }

    ssboL2W.Upload(l2wPool);
}
```

> **⚠️ 实现备忘**：
> 1. `camera_offset_`（Vector3d）成员变量**必须显式初始化为 `{0,0,0}`**，
>    否则未初始化的 double 强转 float 会产生 INF，导致 L2W 矩阵全为 INF。
> 2. 此步骤新增的 `SetCameraOffset()` **必须在 CameraSystem 中被实际调用**
>    才能生效。由于 ECS 执行顺序中 `TickTransform`（TransformSystem）
>    **先于** `TickCamera`（CameraSystem），需要使用**上一帧**的相机位置，
>    或者在帧首单独同步一次相机偏移。
> 3. 4.7.2 和 4.7.3 必须**原子性地**同时启用：L2W 减偏移 + View 矩阵归零
>    缺一不可，否则相机距离和物体位置会错乱。

**[测试]**：对象在世界原点附近与距离 100km 处显示一致，无拖动。

---

#### 4.7.3 — CameraUBO 调整

**修改文件**：CameraUBO 填充逻辑 + GLSL UBO 定义

**变动**：
1. `view` 矩阵的平移部分归零（相机始终在原点）
2. `cameraPos` 字段恒为 `vec3(0,0,0)` — 保留字段以便兼容，但值始终为 0
3. 新增 `vec3 cameraPosWorld`（低精度，供需要绝对世界坐标的 shader 使用，如 fog / terrain）

```glsl
layout(set=0, binding=1) uniform CameraUBO {
    mat4 view;             // camera-relative: 平移归零
    mat4 proj;
    mat4 viewProj;
    vec3 cameraPos;        // 恒为 vec3(0) (兼容保留)
    vec3 cameraPosWorld;   // 绝对世界坐标 (用于 fog/terrain 等)
};
```

> **⚠️ 实现备忘**：
> 1. `SBS_CameraInfo`（GLSL UBO 定义）必须与 C++ `CameraInfo` 结构体严格同步。
>    新增的 `use_reversed_z` / `_pad_ci0` / `camera_world_pos` 字段
>    需要在 `UBOCommon.h` 的 GLSL 定义中同步追加，否则 GPU 端读取偏移错误。
> 2. `StructuredBufferAccessor<T>::MarkDirty()` 只设置 accessor 内部 dirty，
>    **不会**自动传播到底层 `StagedBuffer`。必须同时调用 `gpu_buf->MarkDirty()`
>    才能让 `RenderBufferUploadSystem` 识别并执行 staging→device 拷贝。
>    （此 bug 会导致 Camera UBO 在 GPU 端全为零。）

**[测试]**：编译通过，现有 shader 不受影响（`cameraPos` 字段仍存在）。

---

#### 4.7.4 — Shadow Map 光源矩阵适配

**修改文件**：Shadow 矩阵构建逻辑

**变动**：
1. 光源 View 矩阵的平移列同样减去 `cameraWorldPosition`，保持与 L2W 相同的 camera-relative 坐标系
2. `lightViewProj = lightProj * lightView_relative`

**[测试]**：阴影位置正确，远处物体阴影不偏移。

---

#### 4.7.5 — Debug 渲染适配

**修改文件**：Debug Line / Gizmo 渲染逻辑

**变动**：
1. Debug line 顶点坐标在上传前同样减去 `cameraWorldPosition`

**[测试]**：Gizmo 位置正确，远离原点时无拖动。

---

## 第五阶段：Surface Function 与 Compositor 基础

> **目标**：建立 Surface Function + Compositor Template 架构的基础框架。
> 此阶段先实现最简单的 Standard Surface → Forward Opaque Compositor 全链路，
> 验证 CT 架构可行后再扩展。

---

### Step 5.1 — 定义 SurfaceInput/SurfaceOutput GLSL 接口

**新增文件**：`ShaderLibrary/common/surface_interface.glsl`

```glsl
struct SurfaceInput
{
    vec3 worldPos;       // camera-relative world position（非绝对世界坐标！）
    vec3 worldNormal;
    vec2 uv0;
    vec2 uv1;
    vec4 vertexColor;
    vec3 viewDir;        // normalize(-worldPos)，因为 cameraPos 恒为 0
    vec2 screenPos;
};

struct SurfaceOutput
{
    vec3  baseColor;
    vec3  normal;
    float metallic;
    float roughness;
    float ao;
    vec3  emissive;
    float alpha;
};
```

**[测试]**：GLSL 语法正确。

---

### Step 5.2 — 定义 SurfaceOutputExt（Special Surface 扩展）

**追加到**：`ShaderLibrary/common/surface_interface.glsl`

```glsl
struct SurfaceOutputExt
{
    vec3  subsurfaceColor;
    float subsurfacePower;
    float thickness;
    vec3  sheenColor;
    float sheenRoughness;
    float clearCoat;
    float clearCoatRoughness;
    vec3  clearCoatNormal;
    float anisotropy;
    vec3  anisotropyDirection;
};
```

**[测试]**：GLSL 语法正确。

---

### Step 5.3 — 编写最简 Standard Surface Function

**新增文件**：`ShaderLibrary/surface/standard_surface.glsl`

第一版只实现最基础的 PBR 采样逻辑：

```glsl
#include "common/surface_interface.glsl"

// 材质实例数据 — 从 MI SSBO 读取
struct MI_Standard
{
    vec4 base_color_factor;
    float metallic_factor;
    float roughness_factor;
    float ao_strength;
    float emissive_strength;
    // ...
};

SurfaceOutput EvalSurface(SurfaceInput si, MI_Standard mi)
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
    // 采样 Albedo 纹理
    so.baseColor *= texture(TexAlbedo, si.uv0).rgb;
#endif

#if QUALITY_TIER >= 2
    // 采样法线贴图
    // so.normal = ...
    // 采样 MR 贴图
    // vec2 mr = texture(TexMR, si.uv0).gb;
    // so.metallic *= mr.r;
    // so.roughness *= mr.g;
#endif

    return so;
}

float EvalAlpha(SurfaceInput si, MI_Standard mi)
{
    return mi.base_color_factor.a;
}
```

**[测试]**：GLSL 语法正确（glslangValidator 验证）。

---

### Step 5.4 — 编写最简 EvalLighting() 函数

**新增文件**：`ShaderLibrary/common/lighting.glsl`

```glsl
#include "common/surface_interface.glsl"

vec3 EvalLighting(SurfaceOutput surface, vec3 viewDir, vec3 lightDir, vec3 lightColor)
{
#if QUALITY_TIER <= 1
    // Simple Lambert
    float NdotL = max(dot(surface.normal, lightDir), 0.0);
    return surface.baseColor * lightColor * NdotL;

#elif QUALITY_TIER <= 3
    // BlinnPhong
    float NdotL = max(dot(surface.normal, lightDir), 0.0);
    vec3 H = normalize(viewDir + lightDir);
    float NdotH = max(dot(surface.normal, H), 0.0);
    float spec = pow(NdotH, mix(8.0, 128.0, 1.0 - surface.roughness));
    vec3 diffuse = surface.baseColor * lightColor * NdotL;
    vec3 specular = lightColor * spec * surface.metallic;
    return diffuse + specular;

#else
    // PBR (Cook-Torrance) — 后续实现
    // 暂时 fallback BlinnPhong
    float NdotL = max(dot(surface.normal, lightDir), 0.0);
    vec3 H = normalize(viewDir + lightDir);
    float NdotH = max(dot(surface.normal, H), 0.0);
    float spec = pow(NdotH, mix(8.0, 128.0, 1.0 - surface.roughness));
    vec3 diffuse = surface.baseColor * lightColor * NdotL;
    vec3 specular = lightColor * spec * surface.metallic;
    return diffuse + specular;
#endif
}
```

**[测试]**：GLSL 语法正确。

---

### Step 5.5 — 编写 Forward Opaque Compositor VS 模板

**新增文件**：`ShaderLibrary/compositor/main_forward_opaque.vert.glsl`

```glsl
#version 450

// === Compositor Template: Forward Opaque VS ===
// 自动生成 — 不要手动编辑此文件

// Descriptor bindings (uses #define injected by CompositorAssembler)
layout(set=0, binding=1) uniform CameraUBO { mat4 view; mat4 proj; mat4 viewProj; vec3 cameraPos; vec3 cameraPosWorld; };
layout(set=1, binding=0) readonly buffer L2W_SSBO { mat4 transforms[]; };  // camera-relative L2W

#if GEOMETRY_FETCH_SSBO
    // SSBO 顶点获取
    #include "common/vertex_fetch_ssbo.glsl"
#else
    // VBO 顶点获取
    layout(location=0) in vec3 inPosition;
    layout(location=1) in vec3 inNormal;
    layout(location=2) in vec2 inUV0;
#endif

// Instance ID 获取
#if GEOMETRY_FETCH_SSBO
    // SSBO 平台: gl_InstanceIndex
    #define GET_INSTANCE_ID() gl_InstanceIndex
#else
    // VBO 平台: instance-rate attribute
    layout(location=10) in uint inInstanceID;
    #define GET_INSTANCE_ID() inInstanceID
#endif

layout(location=0) out vec3 fragWorldPos;
layout(location=1) out vec3 fragWorldNormal;
layout(location=2) out vec2 fragUV0;
layout(location=3) flat out uint fragInstanceID;

void main()
{
    uint instanceID = GET_INSTANCE_ID();
    mat4 l2w = transforms[instanceID];

#if GEOMETRY_FETCH_SSBO
    vec3 pos = FetchPosition(gl_VertexIndex);
    vec3 normal = FetchNormal(gl_VertexIndex);
    vec2 uv0 = FetchUV0(gl_VertexIndex);
#else
    vec3 pos = inPosition;
    vec3 normal = inNormal;
    vec2 uv0 = inUV0;
#endif

    vec4 worldPos = l2w * vec4(pos, 1.0);   // camera-relative world position
    fragWorldPos = worldPos.xyz;
    fragWorldNormal = mat3(l2w) * normal;
    fragUV0 = uv0;
    fragInstanceID = instanceID;

    gl_Position = viewProj * worldPos;   // viewProj 已是 camera-relative
}
```

**[测试]**：GLSL 语法验证通过。

---

### Step 5.6 — 编写 Forward Opaque Compositor FS 模板

**新增文件**：`ShaderLibrary/compositor/main_forward_opaque.frag.glsl`

```glsl
#version 450

// === Compositor Template: Forward Opaque FS ===

layout(location=0) in vec3 fragWorldPos;
layout(location=1) in vec3 fragWorldNormal;
layout(location=2) in vec2 fragUV0;
layout(location=3) flat in uint fragInstanceID;

layout(location=0) out vec4 outColor;

// --- Surface Function include (由 CompositorAssembler 注入) ---
#include SURFACE_FUNCTION_FILE
// 展开后类似: #include "surface/standard_surface.glsl"

// --- Lighting ---
#include "common/lighting.glsl"

// --- Scene Data ---
layout(set=0, binding=0) uniform ViewportUBO { /* ... */ };
layout(set=0, binding=1) uniform CameraUBO { mat4 view; mat4 proj; mat4 viewProj; vec3 cameraPos; vec3 cameraPosWorld; };
layout(set=0, binding=2) uniform SkyUBO { vec3 sunDirection; vec3 sunColor; vec3 ambientColor; };

// --- MI SSBO ---
layout(set=2, binding=0) readonly buffer MI_Buffer { MI_Standard mi_data[]; };

void main()
{
    MI_Standard mi = mi_data[fragInstanceID];    // 简化——实际需要 TransformID → MI_ID 映射

    SurfaceInput si;
    si.worldPos    = fragWorldPos;
    si.worldNormal = normalize(fragWorldNormal);
    si.uv0         = fragUV0;
    si.viewDir     = normalize(-fragWorldPos);  // cameraPos 恒为 0，故 viewDir = -worldPos

    SurfaceOutput so = EvalSurface(si, mi);

    vec3 litColor = EvalLighting(so, si.viewDir, sunDirection, sunColor);
    litColor += so.baseColor * ambientColor * so.ao;
    litColor += so.emissive;

    outColor = vec4(litColor, so.alpha);
}
```

> **Camera-Relative 注意**：`fragWorldPos` 是 camera-relative 坐标。若 fog/terrain 需要绝对世界坐标，
> 使用 `fragWorldPos + cameraPosWorld`。

**[测试]**：GLSL 语法验证通过。

---

### Step 5.7 — 实现 CompositorAssembler 核心类（C++ 侧）

**新增文件**：`inc/hgl/shadergen/CompositorAssembler.h` + `src/ShaderGen/CompositorAssembler.cpp`

**功能**（第一版最小实现）：
1. 输入：`SurfaceType`, `BlendMode`, `PassType`, `QualityTier`, `PlatformBackend`
2. 查表选择 VS/FS Compositor Template 文件路径
3. 读取模板文件内容
4. 注入 `#define` 宏（`NewShaderPermutationKey::AppendGLSLDefines()`）
5. 替换 `#include SURFACE_FUNCTION_FILE` 为实际路径
6. 返回完整 GLSL 字符串

```cpp
class CompositorAssembler
{
public:
    struct AssembleResult
    {
        AnsiString vertex_glsl;
        AnsiString fragment_glsl;
        bool success;
        AnsiString error_message;
    };

    AssembleResult Assemble(
        SurfaceType surface,
        BlendMode blend,
        PassType pass,
        QualityTier tier,
        PlatformBackend platform
    );

private:
    AnsiString GetCompositorVSPath(PassType pass) const;
    AnsiString GetCompositorFSPath(BlendMode blend, PassType pass) const;
    AnsiString GetSurfaceFunctionPath(SurfaceType surface) const;
    AnsiString InjectDefines(const AnsiString& source, const NewShaderPermutationKey& key) const;
};
```

**[测试]**：编译通过。可选：写测试调用 Assemble() 检查输出字符串是否包含正确的 #define。

---

### Step 5.8 — 实现 PresetShaderCompiler（离线 SPV 编译）

**新增文件**：`inc/hgl/shadergen/PresetShaderCompiler.h` + `src/ShaderGen/PresetShaderCompiler.cpp`

**功能**（第一版）：
1. 遍历指定的 `MaterialPresetDef` 列表
2. 对每个 Preset × 有效的 `NewShaderPermutationKey` 组合
3. 调用 `CompositorAssembler::Assemble()` 生成 GLSL
4. 调用现有的 `GLSLCompiler` 编译为 SPV
5. 输出 `{preset_id, key} → SPV binary` 映射

**[测试]**：编译通过。可选：编译 Standard Surface × Forward Opaque × Medium × PC 验证产出 SPV。

---

### Step 5.9 — 实现 SPVCache（构建期缓存）

**新增文件**：`inc/hgl/shadergen/SPVCache.h` + `src/ShaderGen/SPVCache.cpp`

**功能**（第一版）：
1. 内存中的 `Map<{preset_id, packed_key, pass_type}, SPVData>` 查表
2. `Store(key, spv)` / `Lookup(key) → SPVData*`
3. `SaveToFile(path)` / `LoadFromFile(path)` — 序列化支持

**[测试]**：编译通过。可选：写/读测试。

---

### Step 5.10 — 端到端验证：Compositor 生成的 SPV 渲染一个三角形

**操作**：
1. 新建 example `example/Basic/CompositorTest.cpp`
2. 使用 `CompositorAssembler` 生成 Standard × ForwardOpaque × Medium × PC 的 GLSL
3. 编译为 SPV
4. 用该 SPV 创建 Pipeline
5. 渲染一个简单三角形或立方体

**[测试]**：三角形/立方体正确渲染（与旧材质系统的渲染结果视觉一致）。

---

## 第六阶段：SSBO 顶点获取路径

> **目标**：激活已有但未激活的 SSBO 顶点获取路径。

---

### Step 6.1 — 实现 vertex_fetch_ssbo.glsl

**新增文件**：`ShaderLibrary/common/vertex_fetch_ssbo.glsl`

```glsl
// SSBO 顶点获取模块
struct VertexData
{
    vec3 position;
    vec3 normal;
    vec2 uv0;
    // 按需扩展 (tangent, color, etc.)
};

layout(set=3, binding=18) readonly buffer VertexDataBuffer { VertexData vertices[]; };
layout(set=3, binding=19) readonly buffer IndexDataBuffer { uint indices[]; };

vec3 FetchPosition(uint vertexIndex) { return vertices[vertexIndex].position; }
vec3 FetchNormal(uint vertexIndex) { return vertices[vertexIndex].normal; }
vec2 FetchUV0(uint vertexIndex) { return vertices[vertexIndex].uv0; }
```

**[测试]**：GLSL 语法验证通过。

---

### Step 6.2 — 实现 vertex_fetch_vbo.glsl

**新增文件**：`ShaderLibrary/common/vertex_fetch_vbo.glsl`

```glsl
// VBO 顶点获取模块 — 使用传统 vertex attribute
// 注意：layout(location=N) 在 Compositor VS 中声明，此文件仅提供函数别名
vec3 FetchPosition(uint vertexIndex) { return inPosition; }
vec3 FetchNormal(uint vertexIndex) { return inNormal; }
vec2 FetchUV0(uint vertexIndex) { return inUV0; }
```

**[测试]**：GLSL 语法验证通过。

---

### Step 6.3 — 实现 VertexDataBufferManager（全局 SSBO 分配）

**新增文件**：`inc/hgl/graph/VertexDataBufferManager.h` + `src/SceneGraph/VertexDataBufferManager.cpp`

**功能**：
1. 管理一个大的 VkBuffer（SSBO），存储所有 mesh 的顶点数据
2. `AllocateVertexBlock(vertex_count) → {offset, size}` — 分配一段
3. `UploadVertices(offset, data, size)` — 上传顶点数据（利用现有 `VKStagedBuffer`）
4. `FreeVertexBlock(offset, size)` — 释放（简单标记，可后续实现 compact）

同样为 `IndexDataBuffer` 实现类似管理。

**[测试]**：编译通过。

---

### Step 6.4 — Mesh 上传时分配 SSBO 空间

**修改文件**：Mesh/Geometry 加载相关代码（如 `GeometryManager` 或 `PrimitiveManager`）

**变动**：
1. Mesh 加载时，同时在 `VertexDataBufferManager` 分配 SSBO 空间
2. 将顶点/索引数据上传到 SSBO
3. 记录 `{vertexOffset, indexOffset}` 到 Mesh 元数据中
4. 保持现有 VBO 路径不变——SSBO 分配是**额外的**

**[测试]**：编译通过，所有 example 运行不变（仍走 VBO 路径）。

---

### Step 6.5 — Pipeline 创建分支（SSBO 空 VertexInput）

**修改文件**：Pipeline 创建逻辑

**变动**：
1. 当 `DeviceQualityProfile.geometry_fetch == SSBO` 时，创建 Pipeline 的 `VkPipelineVertexInputStateCreateInfo` 为空（无 binding、无 attribute）
2. 当 `VBO` 时，保持现有标准 VertexInput
3. 确保两条路径各自创建正确的 Pipeline

**[测试]**：编译通过,根据 `geometry_fetch` 模式创建不同 Pipeline。

---

### Step 6.6 — 端到端测试 SSBO 顶点获取

**操作**：修改 Step 5.10 的 `CompositorTest.cpp` example，增加 SSBO 路径：
1. 上传顶点到 `VertexDataBufferManager`
2. 创建 Pipeline（空 VertexInput）
3. 绑定 VertexData SSBO 到 Set 3
4. Draw 使用 SSBO 路径

**[测试]**：三角形/立方体正确渲染。与 VBO 路径渲染结果一致。

---

## 第七阶段：Forward 材质迁移

> **目标**：将现有材质逐个迁移到新 Compositor 系统。
> **原则**：每次迁移一个材质，确保渲染结果与旧系统一致。
> 旧材质代码暂不删除——双轨共存。

---

### Step 7.1 — 迁移 PureColor3D（最简 Unlit Material）

**操作**：
1. 在 `ShaderLibrary/surface/` 新增 `unlit_color3d_surface.glsl`（极简：`EvalSurface()` 直接输出纯色）
2. 在 `ShaderLibrary/compositor/` 新增 `main_forward_unlit.frag.glsl`（不调用 `EvalLighting()`，直接输出 baseColor）
3. 在 `CompositorAssembler` 中注册 SurfaceType::Unlit × BlendMode::Opaque → 使用 `main_forward_unlit.frag.glsl`
4. 在 `MaterialManager::CreateMaterial()` 中增加分支：当使用新系统时走 `CompositorAssembler` 路径
5. 修改 `PureColor3D` 的 example 使用新路径

**[测试]**：PureColor3D example 渲染正确，颜色与旧版一致。

---

### Step 7.2 — 迁移 VertexColor3D

**操作**：
1. 新增 `unlit_vertexcolor3d_surface.glsl`（从 vertexColor 读取颜色）
2. 修改 Compositor VS 模板支持 vertexColor 输出
3. 更新 `CompositorAssembler` 注册

**[测试]**：VertexColor3D example 渲染正确。

---

### Step 7.3 — 迁移 PureColor2D

**操作**：
1. PureColor2D 保留独立 main()（不走 Compositor）
2. 仅调整 Descriptor Set 映射到新 4-Set 布局

**[测试]**：PureColor2D 相关 example 渲染正确。

---

### Step 7.4 — 迁移 Texture2D

**操作**：
1. 合并 `PureTexture2D` + `RectTexture2D` + `RectTexture2DArray`
2. 保留独立 main()，调整 Descriptor Set

**[测试]**：纹理 example 渲染正确。

---

### Step 7.5 — 迁移 Text2D

**操作**：保留独立 main()，调整 Descriptor Set。

**[测试]**：文本渲染正确。

---

### Step 7.6 — 迁移 PaletteColor3D

**操作**：新增 Surface Function 或保留独立 main()（视复杂度决定）。

**[测试]**：调色板渲染正确。

---

### Step 7.7 — 迁移 Gizmo3D

**操作**：保留独立 main()，调整 Descriptor Set。

**[测试]**：Gizmo example 渲染正确（轴、网格等）。

---

### Step 7.8 — 迁移 Billboard

**操作**：合并 Dynamic/Fixed 两种 Billboard 模式为参数变体。

**[测试]**：Billboard example 渲染正确。

---

### Step 7.9 — 迁移 Sky

**操作**：
1. Sky 保留独立 main()
2. 适配 Reversed-Z（输出 depth = 0.0）
3. 调整 Descriptor Set

**[测试]**：天空渲染正确，位于场景最远处。

---

### Step 7.10 — 迁移 BasicLit → Standard Surface

**操作**：（**核心步骤**）
1. 完善 `standard_surface.glsl` — 加入法线贴图采样、MR 纹理采样
2. 为 Forward Opaque FS Compositor 完善光照调用
3. 将 `BasicLit` 的 example 切换到 Compositor 路径
4. 验证 Low (Lambert) / Medium (BlinnPhong) / High (PBR) 三档光照

**[测试]**：BasicLit example 在各档位下渲染结果与旧版视觉等价。

---

### Step 7.11 — 迁移 TextureBlinnPhong → Standard Surface

**操作**：
1. 完善 `standard_surface.glsl` 的纹理采样逻辑
2. 将 `TextureBlinnPhongMeshesECS` 的 example 切换到 Compositor 路径
3. 验证纹理 + 光照正确

**[测试]**：06c_TextureBlinnPhongMeshesECS 渲染结果与旧版一致。

---

### Step 7.12 — 迁移 PBRColor3D → Standard Surface

**操作**：
1. 完善 `standard_surface.glsl` PBR 采样（metallic/roughness 纹理）
2. 复用现有 `pbr_functions.glsl`, `ggx.glsl`
3. 将 PBRSpheresECS 切换到 Compositor 路径

**[测试]**：PBRSpheresECS 渲染结果与旧版一致。变化 metallic/roughness 参数时视觉连续。

---

### Step 7.13 — 实现 Forward Masked / Transparent / Dither Compositor

**操作**：
1. `main_forward_masked.frag.glsl` — 同 6.6 但加 `if (alpha < alpha_threshold) discard;`
2. `main_forward_transparent.frag.glsl` — 启用 alpha blend 输出
3. `main_forward_dither.frag.glsl` — 基于屏幕空间 dither pattern
4. `main_forward_a2c.frag.glsl` — alpha-to-coverage

**[测试]**：每种 BlendMode 都可正确渲染。Masked 的边缘无误切，Transparent 的混合正确。

---

### Step 7.14 — 实现 BlendMode → PassType[] 自动变体映射

**修改**：`CompositorAssembler`

**变动**：
1. `Opaque` → `[ForwardOpaque, ShadowOpaque, EarlyZSolid]`
2. `Masked` → `[ForwardMasked, ShadowMasked, EarlyZMasked]`
3. `Transparent` → `[ForwardTransparent]`（无阴影、无 EarlyZ）
4. `Dither` → `[ForwardDither, ShadowOpaque]`
5. `A2C` → `[ForwardA2C, ShadowMasked]`

**[测试]**：编译通过。每种 BlendMode 自动生成正确数量的 Pass 变体 SPV。

---

### Step 7.15 — 验证 Texture2DArray 纹理池 + 新 Compositor 配合

**操作**：
1. 确认 `standard_surface.glsl` 中可选 `TEXTURE_ARRAY` 模式
2. 使用 `sampler2DArray` + MI 中的 `tex_albedo` 索引采样
3. 使用 PBRColor3D example（已经使用 Texture2DArray 模式）验证

**[测试]**：Texture2DArray 模式下的 Standard Surface 渲染正确。

---

## 第八阶段：阴影系统

> **目标**：实现双层 ShadowMap 架构（Near Dynamic + Far Cached Toroidal）。

---

### Step 8.1 — Shadow Pass Compositor 模板

**新增文件**：
- `ShaderLibrary/compositor/main_shadow_opaque.vert.glsl` — 仅 MVP 变换
- `ShaderLibrary/compositor/main_shadow_masked.frag.glsl` — alpha test + discard

**[测试]**：GLSL 编译通过。

---

### Step 8.2 — ShadowMap RenderTarget 创建

**新增**：
1. 创建 N 张 Depth-Only RenderTarget（Near Cascade × M 级 + Far Cached）
2. 定义 `ShadowCascadeUBO` 结构体（每级联的 view-proj 矩阵 + 分界深度）
3. 绑定到 Set 0

**[测试]**：编译通过，RenderTarget 创建成功（可通过 debug marker 确认）。

---

### Step 8.3 — Near Dynamic Cascade 渲染

**操作**：
1. 实现 Cascade 划分算法（视锥按对数/线性混合划分为 2 级）
2. 每帧对所有 opaque + masked 几何体渲染到 Near SM
3. 使用 Shadow Pass Compositor SPV

**[测试]**：ShadowMap RT 中可见正确的深度信息（使用 RenderDoc 或调试可视化）。

---

### Step 8.4 — ShadowMask Compose Pass

**新增**：Compute shader 将 Near SM sample 结果写入 ShadowMask RT (R channel)。

**[测试]**：ShadowMask RT 中可见阴影遮挡模式。

---

### Step 8.5 — Forward 光照集成 ShadowMask

**修改**：`lighting.glsl` 中采样 ShadowMask RT，乘以光照结果。

**[测试]**：Forward 路径下可见正确的级联阴影。

---

### Step 8.6 — PCF 软阴影

**新增**：`ShaderLibrary/common/shadow_sampling.glsl` — NxN PCF 采样。

**[测试]**：阴影边缘柔软过渡。

---

### Step 8.7 — Far Cached Cascade（Toroidal Scrolling）

**操作**：
1. 实现环形滚动 SM（30m~200m）
2. 只更新 dirty tile（相机移动超过一个 tile 时标记 dirty）
3. fract() UV 采样

**[测试]**：远处阴影存在，且相机移动时不闪烁。

---

### Step 8.8 — Near + Far 距离混合

**修改**：ShadowMask Compose Pass — 按深度混合 Near 和 Far SM 结果。

**[测试]**：Near→Far 过渡平滑，无明显接缝。

---

### Step 8.9 — (可选) Capsule Shadow

**操作**：
1. 定义 `CapsuleShadowData` SSBO（角色胶囊体参数）
2. 在 ShadowMask Compose 中写入 G channel

**[测试]**：角色底部有柔和阴影。

---

### Step 8.10 — (可选) Contact Shadow

**操作**：屏幕空间 ray-march，High+ 专属。

**[测试]**：小尺度接触处有阴影细节。

---

## 第九阶段：HZB 与遮挡剔除

> **目标**：实现 HZB 降采样和 GPU 遮挡剔除。

---

### Step 9.1 — HZB Downsample Compute Shader

**新增文件**：`ShaderLibrary/pass/hzb_downsample.comp.glsl`

从 Depth RT 生成 HZB Pyramid（逐级 min downsample，Reversed-Z 下 min = 最远）。

**[测试]**：HZB Pyramid 正确生成（通过调试可视化确认每级 mip 内容）。

---

### Step 9.2 — HZB RT 管理

**操作**：创建 R32F 格式的 HZB Texture，log2(max(w,h)) 级 mip。

**[测试]**：RT 创建成功。

---

### Step 9.3 — GPU Instance Culling Compute Shader

**新增文件**：`ShaderLibrary/pass/instance_cull.comp.glsl`

输入：Instance AABB SSBO + HZB
输出：可见 Instance 列表（过滤后的 IndirectDraw Buffer）

**[测试]**：GPU 剔除后 Draw Call 数减少，渲染结果无遗漏物体。

---

### Step 9.4 — 集成到渲染帧序列

**操作**：在 Forward 渲染之前插入 HZB 生成 + Instance Cull Compute Pass。

**[测试]**：帧率提升（大场景中），渲染正确。

---

## 第十阶段：VBuffer 渲染路径

> **目标**：实现 Visibility Buffer 路径（SSBO 平台专属）。

---

### Step 10.1 — VBuffer ID Pass

**新增**：
- `ShaderLibrary/compositor/main_vbuffer_id.vert.glsl` — 仅 MVP
- `ShaderLibrary/compositor/main_vbuffer_id.frag.glsl` — 输出 `{instanceId(16), materialPresetId(8), triangleId(8)}` 到 R32G32UI RT

**[测试]**：VBuffer RT 中有正确的 ID 编码。

---

### Step 10.2 — VBuffer RT 创建

**操作**：创建 VBuffer RT（R32G32_UINT）+ 关联 Depth RT。

**[测试]**：RT 格式正确，可 clear 到 0xFFFFFFFF。

---

### Step 10.3 — Tile Classification Compute Shader

**新增文件**：`ShaderLibrary/pass/tile_classify.comp.glsl`

16×16 tile 扫描 VBuffer → 输出:
- TileSurfaceMask (R32UI) — 每 tile 包含哪些 SurfaceType
- TileMaterialCount (R8UI) — 单一材质/混合材质
- TileList + DispatchArgs — Indirect dispatch 参数

**[测试]**：Tile 分类结果正确（调试可视化：空 tile 黑色，单一材质 tile 彩色，混合 tile 白色）。

---

### Step 10.4 — VBuffer Resolve — 单一材质 Tile

**新增文件**：`ShaderLibrary/pass/vbuffer_resolve_single.comp.glsl`

Compute shader：
1. 从 VBuffer 解包 → 获取 vertex indices
2. 重心坐标插值 UV/Normal（从全局 VertexData SSBO 读取）
3. `#include` Surface Function → `EvalSurface()`
4. `#include` Lighting → `EvalLighting()`
5. 输出到 LitColor RT

**[测试]**：单一材质区域渲染正确。

---

### Step 10.5 — VBuffer Resolve — 多材质通用路径

**新增**：`ShaderLibrary/pass/vbuffer_resolve_multi.comp.glsl`

对混合 tile 中的每个像素分支处理不同 SurfaceType。

**[测试]**：混合区域渲染正确。

---

### Step 10.6 — Forward/VBuffer 路径自动切换

**操作**：
1. 不透明 3D 几何 → VBuffer（SSBO 平台）
2. 透明/粒子/2D/Sky → Forward
3. VBO 平台强制全 Forward

**[测试]**：VBuffer 路径渲染结果 == Forward 路径（像素级对比或视觉对比）。

---

### Step 10.7 — (可选) Fused Material Tile 路径

**操作**：预编译 2-4 种材质内联在一个 Compute Shader 中，减少分支。

**[测试]**：Fused 路径渲染正确，性能优于通用路径。

---

### Step 10.8 — Tile Classification Debug 可视化

**新增**：热力图 Compute Shader — 用不同颜色显示 tile 类型（empty / single / fused / multi）。

**[测试]**：Debug 模式下可见 tile 类型分布。

---

## 第十一阶段：Meshlet 几何管线

> **目标**：实现 GPU-Driven Meshlet 渲染管线。

---

### Step 11.1 — 集成 meshoptimizer 库

**操作**：
1. 将 meshoptimizer 添加为 3rdpty 依赖
2. 在 CMakeLists.txt 中 `add_subdirectory(3rdpty/meshoptimizer)`
3. 编译通过

**[测试]**：meshoptimizer 库编译成功。

---

### Step 11.2 — 定义 MeshletGPU 结构体

**新增文件**：`inc/hgl/graph/MeshletGPU.h`

```cpp
struct MeshletGPU
{
    uint32 vertex_offset;       // 顶点数据在全局 SSBO 中的偏移
    uint32 index_offset;        // 索引数据偏移
    uint16 vertex_count;
    uint16 triangle_count;
    float  bounding_sphere[4];  // xyz + radius
    float  normal_cone[4];      // xyz + half_angle (cone culling)
    uint8  lod_level;
    uint8  flags;               // ALPHA_TEST, WIND_ANIM, etc.
    uint16 padding;
};
```

**[测试]**：编译通过。

---

### Step 11.3 — 离线 Meshlet 构建工具

**新增**：`src/Tools/MeshletBuilder/` — 读取 mesh → meshoptimizer 构建 meshlet → 输出 .ulm 二进制

**[测试]**：工具能将一个测试 mesh 转换为 meshlet 数据。

---

### Step 11.4 — Meshlet 数据加载

**操作**：引擎加载 .ulm 文件 → 上传 MeshletBuffer SSBO。

**[测试]**：SSBO 中可见 meshlet 数据。

---

### Step 11.5 — Meshlet LOD Select + Cull Compute Shader

**新增文件**：`ShaderLibrary/pass/meshlet_cull.comp.glsl`

DAG 遍历 + Frustum + Cone + HZB → 输出 IndirectDraw 命令。

**[测试]**：只有视锥内的 meshlet 被渲染。

---

### Step 11.6 — vkCmdDrawIndexedIndirectCount 集成

**修改**：渲染命令录制使用 `vkCmdDrawIndexedIndirectCount`（meshlet 粒度）。

**[测试]**：Indirect Draw 正确渲染所有可见 meshlet。

---

### Step 11.7 — Two-Phase Occlusion Culling

**操作**：
1. Phase 1: 上一帧 HZB → cull meshlet
2. EarlyZ/VBuffer ID → 生成当前帧 HZB
3. Phase 2: 当前帧 HZB → 补充 cull 存疑 meshlet → 补充 Draw

**[测试]**：快速移动相机时无明显 pop-in。

---

### Step 11.8 — VBO 平台回退路径

**操作**：离散 LOD mesh 从 DAG 导出 + CPU Frustum Cull + `vkCmdDrawIndexed`。

**[测试]**：VBO 平台正确渲染（虽然无 GPU 剔除）。

---

## 第十二阶段：后处理管线

> **目标**：实现基础后处理链。每个后处理 Pass 独立实现，可逐个开关。

---

### Step 12.1 — 后处理框架

**新增**：PostProcessChain 类 — 管理一系列 Compute/FS Pass 的执行顺序。

**[测试]**：框架编译通过，空 chain 执行不崩。

---

### Step 12.2 — ToneMapping Pass (ACES)

**新增文件**：`ShaderLibrary/postprocess/tonemapping.comp.glsl`

**[测试]**：HDR → LDR 映射正确，亮区不过曝。

---

### Step 12.3 — Bloom Pass

**新增文件**：`ShaderLibrary/postprocess/bloom_downsample.comp.glsl`, `bloom_upsample.comp.glsl`

**[测试]**：亮源周围有辉光效果。

---

### Step 12.4 — FXAA Pass

**新增文件**：`ShaderLibrary/postprocess/fxaa.comp.glsl`

**[测试]**：边缘锯齿减少。

---

### Step 12.5 — TAA Pass

**新增**：
1. Camera 投影矩阵 Jitter
2. 历史帧缓冲区
3. 指数混合 + 邻域裁剪

**[测试]**：静态场景下锯齿大幅减少，动态场景无明显鬼影。

---

### Step 12.6 — SSAO Pass

**新增文件**：`ShaderLibrary/postprocess/ssao.comp.glsl`（GTAO 或 HBAO）

**[测试]**：角落和缝隙处有环境遮蔽效果。

---

### Step 12.7 — Auto Exposure

**新增**：Luminance Histogram Compute + Temporal Smooth。

**[测试]**：从暗到亮环境时自动调节曝光。

---

### Step 12.8 — Fog

**操作**：集成到 `EvalLighting()` 尾部或独立 Pass。

**[测试]**：远处物体正确雾化。

---

## 第十三阶段：Material LOD 与 Special Surface

> **目标**：实现 Material LOD 自动降级和特殊表面材质。

---

### Step 13.1 — CalcObjectLODTier()

**新增**：函数根据物体屏幕空间面积 + importanceBias 计算 `objectLODTier`。

**[测试]**：远处物体 LOD Tier 低，近处高。

---

### Step 13.2 — EffectiveTier 计算 + SPV 选择

**操作**：`EffectiveTier = min(deviceTier, objectLODTier, surfaceLODCap)` → 选择对应 SPV。

**[测试]**：远处 Standard Surface 使用低档 SPV（SimpleLambert），近处使用高档。

---

### Step 13.3 — ResolveSPVFallback()

**操作**：当 `EffectiveTier < unique_feature_min_tier` 时，fallback 到 `fallback_surface_type` 的 SPV。

**[测试]**：Skin@Low == Standard@Low 渲染结果。

---

### Step 13.4 — Skin Surface Function

**新增**：`ShaderLibrary/surface/skin_surface.glsl`
- Ultra: 全 SSS + Detail Normal + 曲率 AO
- High: 简化 SSS
- Medium/Low: fallback Standard

**[测试]**：皮肤材质在各档位下渲染正确。

---

### Step 13.5 — Hair Surface Function

**新增**：`ShaderLibrary/surface/hair_surface.glsl`
- Ultra: Marschner 双高光
- High: Kajiya-Kay
- Low: BlinnPhong

**[测试]**：头发材质渲染正确。

---

### Step 13.6 — Cloth Surface Function

**新增**：`ShaderLibrary/surface/cloth_surface.glsl`
- Sheen + Charlie Model

**[测试]**：布料材质有正确的 sheen 效果。

---

### Step 13.7 — ClearCoat Surface Function

**新增**：`ShaderLibrary/surface/clearcoat_surface.glsl`
- High+: 双层 BRDF

**[测试]**：车漆等清漆材质有双层反射。

---

### Step 13.8 — Foliage Surface Function

**新增**：`ShaderLibrary/surface/foliage_surface.glsl`
- High+: Thin Translucency + Wind
- Low: 静态 AlphaTest

**[测试]**：树叶半透光效果，风动正确。

---

### Step 13.9 — Eye Surface Function

**新增**：`ShaderLibrary/surface/eye_surface.glsl`
- Ultra: Parallax Refraction + 焦散 + 角膜 SSS
- Low: 平面纹理

**[测试]**：眼球折射正确。

---

### Step 13.10 — Water Surface Function

**新增**：`ShaderLibrary/surface/water_surface.glsl`

**[测试]**：水面反射/折射基本正确。

---

## 第十四阶段：Terrain 系统

> **目标**：实现设计文档 §5.5 的 256 层 Terrain 渲染。

---

### Step 14.1 — Terrain Surface Function

**新增**：`ShaderLibrary/surface/terrain_surface.glsl`
- 循环采样 TerrainLayerSSBO + Texture2DArray
- Weight threshold skip
- QualityTier 层数限制

**[测试]**：多层地形渲染正确。

---

### Step 14.2 — TerrainLayerSSBO + MI_Terrain

**操作**：定义 GPU 数据结构 + CPU 侧上传。

**[测试]**：SSBO 数据正确上传。

---

### Step 14.3 — SplatMap 编码

**操作**：RGBA8 Texture2DArray，ceil(N/4) 层。

**[测试]**：SplatMap 权重正确混合。

---

### Step 14.4 — Terrain Forward Path

**操作**：使用 Compositor 路径渲染地形。

**[测试]**：地形在 Forward 路径下正确渲染。

---

### Step 14.5 — Terrain VBuffer Resolve

**操作**：专用 Compute Kernel — per-pixel 多层 Dither 混合。

**[测试]**：VBuffer 路径下地形渲染正确。

---

### Step 14.6 — Terrain Descriptor Set 2 专用布局

**操作**：SurfaceType==Terrain 时替换整个 Set 2。

**[测试]**：Terrain 专用 Descriptor Set 正确绑定。

---

## 第十五阶段：Clustered Shading 与高级光照

> **目标**：多光源支持（High+ 档位）。

---

### Step 15.1 — Cluster 空间划分

**新增**：Compute shader 预计算 cluster AABB。

**[测试]**：Cluster 空间正确划分。

---

### Step 15.2 — Light Assignment Compute

**新增**：每 cluster 分配 light list。

**[测试]**：点光源正确分配到对应 cluster。

---

### Step 15.3 — EvalLighting() 集成 Cluster

**修改**：`lighting.glsl` 中 `QUALITY_TIER >= 3` 时从 cluster light list 遍历。

**[测试]**：多光源 (>8) 场景渲染正确，无遗漏光源。

---

### Step 15.4 — SSR (Screen-Space Reflections)

**新增**：Hi-Z Ray March Compute Shader。

**[测试]**：光滑表面可见周围环境的反射。

---

### Step 15.5 — Decal System

**新增**：Screen-Space Decal — OBB mesh + Depth 反算 + 投影采样。

**[测试]**：弹孔/血迹贴花正确投影到表面。

---

## 第十六阶段：旧代码清理

> **目标**：删除所有已被替代的旧代码。
> **前提**：前面所有阶段完成，所有 example 在新系统下正确运行。

---

### Step 16.1 — 删除旧 Shader 组合层

**删除文件**：
- `inc/hgl/shadergen/ShaderComposition.h`
- `inc/hgl/shadergen/ShaderLogic.h`
- `src/ShaderGen/ShaderCompositionBridge.cpp`
- 所有 `S_*_Logic.h` 文件

**[测试]**：编译通过。

---

### Step 16.2 — 删除旧 StdMaterial 子类

**删除**：所有 `M_*.cpp` 工厂文件（已被 Compositor 替代的）。

**[测试]**：编译通过。

---

### Step 16.3 — 删除旧 ShaderLibrary 模板引擎

**删除**：
- `ShaderLibrary/templates/*.tmpl`
- `ShaderLibrary/recipes/`
- inja 模板引擎相关代码

**[测试]**：编译通过。

---

### Step 16.4 — 删除旧 DescriptorSetType 和 Contract 系统

**删除**：
- 旧 `DescriptorSetType`（7 个 Set 版本）
- `DescriptorBindingContract.h` 中的语义推断逻辑
- `ShaderGenContract*.h/cpp`

**将** `NewDescriptorSetType` 重命名为 `DescriptorSetType`。

**[测试]**：编译通过。

---

### Step 16.5 — 合并新类型到正式目录

**操作**：
1. 将 `inc/hgl/mtl/new/*.h` 移动到 `inc/hgl/mtl/`
2. 将 `NewShaderPermutationKey` 重命名为 `ShaderPermutationKey`
3. 将 `NewDescriptorBinding` 重命名为 `DescriptorBinding`
4. 更新所有 include 路径

**[测试]**：编译通过，全部 example 运行正常。

---

### Step 16.6 — 删除 GBuffer 残留代码

**确认**：`RenderFlowDef.h` 中无 GBuffer 相关定义。全局搜索 "GBuffer" 应零命中。

**[测试]**：全局搜索确认清理完毕。

---

### Step 16.7 — 删除临时 example

**操作**：删除 `CompositorTest.cpp` 等仅用于开发调试的 example。

**[测试]**：编译通过。

---

## 附录 A：文件变更追踪表

| 阶段 | 新增文件 | 修改文件 | 删除文件 |
|------|---------|---------|---------|
| 1.2 | `ShaderLibrary/surface/`, `compositor/`, `common/`, `pass/`, `postprocess/`, `debug/`, `inc/hgl/mtl/new/` (dirs) | — | — |
| 1.3-1.7 | — | `RenderFlowDef.h` | GBuffer 相关枚举/结构体 |
| 2.1-2.9 | `SurfaceType.h`, `QualityTier.h`, `BlendMode.h`, `PassType.h`, `PlatformBackend.h`, `MaterialCategory.h`, `NewShaderPermutationKey.h`, `MaterialPresetDef.h`, `DeviceQualityProfile.h` | — | — |
| 2.10-2.11 | `DeviceQualityProfile.cpp`, `NewShaderPermutationKey.cpp` | `DeviceQualityProfile.h` | — |
| 3.1-3.5 | `NewDescriptorSetType.h`, `DescriptorSetBindings.h`, `NewDescriptorSetLayoutFactory.h/.cpp`, `NewDescriptorBinding.h/.cpp` | `RenderContext.h`（新增成员） | — |
| 4.1-4.6 | `ReversedZProj.h/.cpp`, `depth_utils.glsl` | Camera 头文件, `VKPipelineData.cpp`, `VKRenderPass` 相关 | — |
| 5.1-5.10 | `surface_interface.glsl`, `standard_surface.glsl`, `lighting.glsl`, `main_forward_opaque.vert.glsl`, `main_forward_opaque.frag.glsl`, `CompositorAssembler.h/.cpp`, `PresetShaderCompiler.h/.cpp`, `SPVCache.h/.cpp`, `CompositorTest.cpp` | — | — |
| 6.1-6.6 | `vertex_fetch_ssbo.glsl`, `vertex_fetch_vbo.glsl`, `VertexDataBufferManager.h/.cpp` | Mesh 加载逻辑, Pipeline 创建逻辑 | — |
| 7.1-7.15 | `unlit_color3d_surface.glsl`, `unlit_vertexcolor3d_surface.glsl`, `main_forward_unlit.frag.glsl`, `main_forward_masked.frag.glsl`, `main_forward_transparent.frag.glsl`, `main_forward_dither.frag.glsl`, `main_forward_a2c.frag.glsl` | 各 example, `CompositorAssembler`, `MaterialManager` | (旧材质代码暂保留) |
| 8.1-8.10 | `main_shadow_opaque.vert.glsl`, `main_shadow_masked.frag.glsl`, `shadow_sampling.glsl`, ShadowMap 管理代码 | `lighting.glsl` | — |
| 9.1-9.4 | `hzb_downsample.comp.glsl`, `instance_cull.comp.glsl` | 渲染帧序列 | — |
| 10.1-10.8 | `main_vbuffer_id.vert/frag.glsl`, `tile_classify.comp.glsl`, `vbuffer_resolve_single.comp.glsl`, `vbuffer_resolve_multi.comp.glsl` | 渲染路径调度逻辑 | — |
| 11.1-11.8 | `MeshletGPU.h`, `meshlet_cull.comp.glsl`, MeshletBuilder 工具, .ulm 格式 | CMakeLists.txt, Indirect Draw 逻辑 | — |
| 12.1-12.8 | PostProcessChain, `tonemapping.comp.glsl`, `bloom_*.comp.glsl`, `fxaa.comp.glsl`, `ssao.comp.glsl` | Camera (TAA jitter) | — |
| 13.1-13.10 | `skin_surface.glsl`, `hair_surface.glsl`, `cloth_surface.glsl`, `clearcoat_surface.glsl`, `foliage_surface.glsl`, `eye_surface.glsl`, `water_surface.glsl`, Material LOD 逻辑 | `SPVCache`, `CompositorAssembler` | — |
| 14.1-14.6 | `terrain_surface.glsl`, Terrain GPU 数据管理 | Descriptor Set 布局 (Terrain 专用) | — |
| 15.1-15.5 | Cluster 相关 Compute, SSR Compute, Decal 逻辑 | `lighting.glsl` | — |
| 16.1-16.7 | — | Include 路径, 重命名 | `ShaderComposition.h`, `ShaderLogic.h`, `ShaderCompositionBridge.cpp`, `S_*_Logic.h`, `M_*.cpp` (旧工厂), `templates/*.tmpl`, `recipes/`, `CompositorTest.cpp`, `inc/hgl/mtl/new/` (目录) |

---

## 附录 B：阶段依赖图

```
  第一阶段 (准备清理)
       │
       ├─── 第二阶段 (核心类型) ──────────┐
       │                                   │
       ├─── 第四阶段 (Reversed-Z + CRR) ◄────────┤    ← 可与第二阶段并行
       │                                   │
       └─── 第三阶段 (Descriptor Set) ─┐   │
                                        │   │
                                        ▼   ▼
                               第五阶段 (SF + Compositor)    ★★★ 关键路径
                                        │
                        ┌───────────────┼───────────────┐
                        │               │               │
                        ▼               ▼               ▼
               第六阶段 (SSBO)  第七阶段 (Forward)  第八阶段 (阴影)
                        │               │               │
                        └───────┬───────┘               │
                                │                       │
                                ▼                       │
                       第九阶段 (HZB+Cull) ◄────────────┘
                                │
                        ┌───────┴───────┐
                        ▼               ▼
               第十阶段 (VBuffer)  第十一阶段 (Meshlet)
                        │               │
                        └───────┬───────┘
                                │
                        ┌───────┴───────┐
                        ▼               ▼
               第十二阶段 (后处理)  第十四阶段 (Terrain)
                        │               │
                        └───────┬───────┘
                                ▼
                       第十三阶段 (Material LOD + Special Surface)
                                │
                                ▼
                       第十五阶段 (Clustered + 高级光照)
                                │
                                ▼
                       第十六阶段 (旧代码清理)
```

**关键路径**：第一 → 第二 → 第三 → 第五 → 第七 → 第八 → 第九 → 第十/十一 → 第十三

**可并行**：
- 第二阶段 ∥ 第四阶段
- 第六阶段 ∥ 第七阶段 ∥ 第八阶段
- 第十阶段 ∥ 第十一阶段
- 第十二阶段 ∥ 第十四阶段

---

## 附录 C：每步验证清单模板

用于每一步完成后的验证：

```
□ 代码编译通过 (Debug + Release)
□ 无新增编译警告（允许 deprecated 警告，如是本步计划内的）
□ 所有现有 example 编译通过
□ 本步涉及的 example 运行正常
□ 与基线截图对比，无视觉回退
□ (如涉及新 Shader) glslangValidator 验证通过
□ (如涉及 Compute) RenderDoc 捕获确认 Dispatch 参数正确
```

---

> **使用方式**：从 Step 1.1 开始，严格按顺序执行每一步。
> 每步完成后运行验证清单。如果某步验证失败，在该步内修复后再进入下一步。
> 同一阶段内的步骤按编号顺序执行；不同阶段之间按依赖图选择执行顺序。
