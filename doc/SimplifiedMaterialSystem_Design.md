# 简易材质生成器设计方案 v2

## 1. 设计目标与原则

### 核心目标
- **硬编码预设材质**：按 **表面类型（SurfaceType）** 分类，约 10+ 种硬编码表面材质
- **美术只调参数**：等同于 UE 的 MaterialInstance 模式——选表面类型，填纹理和参数，不可自定义 Shader
- **画质自动降级**：美术配置完整的最高规格纹理集，引擎根据设备档位自动选择 Shader 变体
  - 高端独显 → 完整 PBR（多纹理，IBL，PCSS）
  - 核显/中端 → 标准 PBR（BaseColor + Normal + MetallicRoughness）
  - 移动端/低端 → BlinnPhong FakePBR（BaseColor + Normal，简化光照）
- **两条渲染路径**：Forward / VBuffer（删除传统 GBuffer 延迟渲染）
- **美术只看到一个材质**：BlinnPhong / PBR 不是不同材质，而是同一表面类型的不同画质档位

### 设计原则
1. **零自由度**：不提供节点编辑器、Shader 拼接、Logic 注入等任何自定义 Shader 逻辑入口
2. **零运行时编译**：所有 Shader 变体在构建期全部编译为 SPV，运行时只做查表
3. **最小抽象**：删除 ComposedMaterialDef、MaterialLogicDef、ShaderCompositionBridge 等组合层；
   每个预设材质 = 一份完整的 GLSL 源码模板 + 一组 `#define` 开关
4. **参数即 MaterialInstance**：每个表面类型对应固定的 MaterialInstance 结构体（UBO/SSBO），
   美术通过引擎编辑器填写该结构体的值和绑定纹理
5. **最高规格配置，自动降级**：美术永远面对最高档位的纹理槽和参数列表，引擎在低档位自动忽略不需要的纹理

---

## 2. 渲染路径定义

### 2.1 Forward Pass（前向渲染）

```
┌─────────────┐     ┌──────────────┐
│ Vertex Shader│────▶│Fragment Shader│────▶ Color RT (RGBA16F)
│  (MVP + lit) │     │ (lighting)   │────▶ Depth RT
└─────────────┘     └──────────────┘
```

- 每像素执行完整光照计算（Unlit / Blinn-Phong / PBR）
- 输出：**单个 Color RT**（RGBA16F 或 RGBA8_UNORM）+ **Depth**
- 适用：透明物体、粒子、2D UI、简单场景

### 2.2 VBuffer Pass（Visibility Buffer 渲染）

```
Pass 1 - Visibility Buffer 生成:
┌─────────────┐     ┌──────────────┐
│ Vertex Shader│────▶│Fragment Shader│────▶ VBuffer RT (MaterialID + TriangleID)
│  (MVP only)  │     │ (ID write)   │────▶ Depth RT
└─────────────┘     └──────────────┘

Pass 2 - Material Resolve + Lighting（Compute / Full-screen FS）:
┌───────────────────────┐
│ 读取 VBuffer + Depth   │
│ 反算世界坐标/法线/UV   │
│ 查 Material 参数表     │      ┌──────────────────────┐
│ 执行光照计算           │─────▶│ LitColor RT (RGBA16F) │  ← 小 GBuffer
│ (Blinn-Phong / PBR)   │      └──────────────────────┘
└───────────────────────┘

Pass 3 - Post-Processing（可选）:
┌─────────────┐
│ 读取 LitColor│────▶ Bloom / ToneMapping / FXAA ────▶ SwapChain
└─────────────┘
```

- Pass 1 极轻量（只写 ID + Depth，不做光照）
- Pass 2 用 Compute 或全屏三角形做 Material Resolve + 光照
- **LitColor RT** = 光照计算后的颜色，相当于"小 GBuffer"
  - 与传统 GBuffer 区别：存的是 **已光照的最终颜色**，不存法线/粗糙度等中间量
  - 如果没有后期处理，LitColor 直接就是最终画面
- 适用：不透明几何体为主的复杂场景

### 2.3 路径选择策略

| 对象类型 | 渲染路径 | 原因 |
|----------|----------|------|
| 不透明 3D 几何体 | VBuffer（高密度场景）或 Forward | 按场景复杂度切换 |
| 透明 3D 几何体 | Forward | VBuffer 无法处理透明 |
| 2D UI / HUD | Forward | 无需深度/光照 |
| 粒子 / Billboard | Forward | 半透明需求 |
| 天空 | Forward（特殊） | 单独 Pass |

### 2.4 完整渲染管线 Pass 顺序（程序关注）

> 美术不需要关心以下内容。这一节面向引擎程序员，描述每帧的完整 Pass 流程。

#### Forward 路径帧序列

```
┌────────────────────────────────────────────────────────────────────┐
│ 0. ShadowMap Pass (per light, 离屏)                                │
│    - 对每个投影光源（主平行光 + 可选点光源/聚光源）渲染深度图            │
│    - 使用 Depth-Only VS，不执行 FS（或极简 Alpha-Test FS）           │
│    - 主平行光使用 Cascaded Shadow Map (CSM), 2~4 级                 │
│    - 输出: ShadowMap Depth Texture (per cascade/per light)         │
├────────────────────────────────────────────────────────────────────┤
│ 1. Early-Z Pre-Pass (Depth Pre-Pass)                               │
│    - 仅写 Depth Buffer，不写 Color                                  │
│    - VS 只做 MVP 变换，无 FS 或空 FS                                │
│    - Alpha-Test 物体使用简易 FS（采样 Albedo.a 做 discard）          │
│    - 输出: Depth RT (D32_SFLOAT 或 D24_UNORM_S8_UINT)             │
│    - 目的: 让后续 Forward Pass 受益于 Early-Z Rejection，减少 overdraw │
├────────────────────────────────────────────────────────────────────┤
│ 2. ShadowMask Compose Pass (全屏 Compute / FS)                     │
│    - 输入: Depth RT + ShadowMap Textures + Camera/Light Matrices    │
│    - 将屏幕像素反投影到 Light Space，采样 ShadowMap 做 PCF/PCSS       │
│    - 多级 CSM 混合（级间插值或硬切）                                  │
│    - 多光源的阴影合并到一张 ShadowMask RT (R8/RG8)                   │
│    - 输出: ShadowMask RT（R 通道 = 主平行光遮蔽, G 通道 = 可选）      │
│    - 目的: 避免在 Forward Pass 中每像素每光源重复采样 ShadowMap        │
├────────────────────────────────────────────────────────────────────┤
│ 3. SSAO / SSDO Pass (全屏 Compute / FS)                            │
│    - 输入: Depth RT + (可选) 法线                                   │
│    - SSAO: Screen-Space Ambient Occlusion, 基于 depth-only          │
│    - SSDO: Screen-Space Directional Occlusion (High 档位可选)       │
│    - 输出: SSAO RT (R8 或 RG8, 可选模糊)                            │
│    - Low 档位跳过此 Pass（AO 只用纹理中的 AO Map 或默认 1.0）         │
├────────────────────────────────────────────────────────────────────┤
│ 4. Forward Lit Pass (主渲染)                                        │
│    - Depth Test = Equal（利用 Pre-Pass Depth）                      │
│    - Depth Write = Off（已有 Pre-Pass 写入的深度）                    │
│    - 输入 ShadowMask RT (sampler, 屏幕空间查找)                     │
│    - 输入 SSAO RT (sampler, 屏幕空间查找)                            │
│    - 只渲染通过 Early-Z 的片元 → 零 overdraw                        │
│    - 按材质排序 → 批次化 (SurfaceType, PresetID, Pipeline)           │
│    - 输出: LitColor RT (RGBA16F)                                   │
├────────────────────────────────────────────────────────────────────┤
│ 5. Sky Pass                                                        │
│    - Depth Test = Equal, Depth = 1.0 (远平面)                       │
│    - 程序化天空或天空盒                                              │
│    - 输出: 写入 LitColor RT                                         │
├────────────────────────────────────────────────────────────────────┤
│ 6. Translucent Pass (Forward, 后排序)                               │
│    - 透明物体、粒子、Billboard                                       │
│    - 从后往前排序 (painter's algorithm)                               │
│    - Depth Write = Off, Depth Test = Less, Blend = SrcAlpha         │
│    - 简化光照（通常 Unlit 或简单 BlinnPhong，不接收 SSAO）           │
│    - 输出: 写入 LitColor RT                                         │
├────────────────────────────────────────────────────────────────────┤
│ 7. Debug / Gizmo Overlay Pass (可选)                                │
│    - Gizmo3D 材质: 骨骼可视化、碰撞体线框、坐标轴、选择高亮            │
│    - Depth Test 可选: 穿透/不穿透                                    │
│    - 使用硬编码太阳方向调试光照（见 2.5 节）                           │
│    - 输出: 写入 LitColor RT                                         │
├────────────────────────────────────────────────────────────────────┤
│ 8. Post-Processing Pass Chain                                       │
│    - 输入: LitColor RT (RGBA16F)                                   │
│    - Temporal AA (TAA): 利用 Motion Vector RT + 前帧历史帧混合        │
│    - Bloom: 提取高亮 → 模糊 → 叠加                                  │
│    - ToneMapping: ACES / Filmic / Neutral → LDR                    │
│    - 最终 FXAA / Sharpening → SwapChain                            │
│    - 输出: Backbuffer                                               │
└────────────────────────────────────────────────────────────────────┘
```

#### VBuffer 路径帧序列

```
┌────────────────────────────────────────────────────────────────────┐
│ 0. ShadowMap Pass                         (同 Forward)             │
├────────────────────────────────────────────────────────────────────┤
│ 1. VBuffer ID Pass (替代 Early-Z + Forward Lit)                    │
│    - 写入 VBuffer RT (MaterialID + TriangleID) + Depth RT           │
│    - 极轻量 FS，无光照计算                                          │
├────────────────────────────────────────────────────────────────────┤
│ 2. ShadowMask Compose Pass                (同 Forward)             │
├────────────────────────────────────────────────────────────────────┤
│ 3. SSAO / SSDO Pass                       (同 Forward)             │
├────────────────────────────────────────────────────────────────────┤
│ 4. VBuffer Resolve + Lighting (Compute Shader)                     │
│    - 读取 VBuffer + Depth → 反算世界坐标/UV/法线                     │
│    - 读取 ShadowMask + SSAO                                        │
│    - 查 MaterialInstance 参数 → 执行光照 → 写入 LitColor RT          │
├────────────────────────────────────────────────────────────────────┤
│ 5~8. Sky / Translucent / Debug / Post-Processing  (同 Forward)     │
└────────────────────────────────────────────────────────────────────┘
```

#### TAA (Temporal Anti-Aliasing) 所需额外资源

| 资源 | 格式 | 用途 |
|------|------|------|
| Motion Vector RT | RG16F | 每像素运动向量 (dx, dy) |
| History LitColor | RGBA16F | 上一帧的 LitColor（双缓冲 ping-pong） |
| Jitter Offset | CPU uniform | 每帧 sub-pixel 抖动偏移 (Halton 序列) |

> Forward Lit Pass / VBuffer ID Pass 的 VS 中注入 jitter offset → `gl_Position.xy += jitter * gl_Position.w;`
> Motion Vector 在 Forward Lit Pass 的 VS 中计算：`motion = currClipPos.xy/w - prevClipPos.xy/w;` 写入 Motion Vector RT。
> TAA Resolve: 用 motion vector 采样 history → 邻域 clamp → 指数混合 → 输出新帧。

#### 各 Pass 与档位的关系

| Pass | Low | Medium | High | Ultra |
|------|-----|--------|------|-------|
| ShadowMap | ❌ 或 1 级 CSM | ✅ 2 级 CSM | ✅ 4 级 CSM + PCSS | ✅ 同 High |
| Early-Z Pre-Pass | ✅ | ✅ | ✅ | ✅ |
| ShadowMask | ❌（shadow=None 时跳过）| ✅ PCF | ✅ PCSS | ✅ PCSS |
| SSAO | ❌ | ✅ 半分辨率 | ✅ 全分辨率 SSAO | ✅ SSDO |
| TAA | ❌（fallback FXAA） | ✅ 基础 TAA | ✅ TAA + Sharpening | ✅ 同 High |
| Motion Vector RT | ❌（不分配） | ✅ | ✅ | ✅ |

### 2.5 调试光照模式（Gizmo3D / Debug Utilities）

Gizmo3D 材质及各种调试可视化（骨骼、碰撞体、包围盒等）使用**硬编码固定光照**，
不受场景灯光、ShadowMap、SSAO 等影响，确保调试模型在任何场景下都能清晰可见。

```cpp
// ===== 调试光照配置（全局常量，可在 Debug 面板热修改）=====
struct DebugLightingConfig
{
    vec3  sun_direction     = normalize(vec3(-0.5, -1.0, -0.3));  // 硬编码太阳方向
    float sun_intensity     = 1.5f;                                // 光照强度
    vec3  sun_color         = vec3(1.0, 0.98, 0.95);              // 微暖白光
    float ambient_intensity = 0.25f;                               // 固定环境光强度
    vec3  ambient_color     = vec3(0.6, 0.7, 0.85);               // 天蓝色环境光
};
```

#### 调试可视化模式

| 模式 | 材质 | 渲染方式 | 用途 |
|------|------|---------|------|
| **Wireframe Overlay** | Gizmo3D | 线框，纯色 + 硬编码光 | 网格结构检查 |
| **Skeleton** | Gizmo3D | 骨骼连线 + 关节球，纯色 | 骨骼动画调试 |
| **Physics Collision** | Gizmo3D | 碰撞体线框/半透明 | 物理碰撞模型可视化 |
| **Bounding Box** | Gizmo3D | AABB/OBB 线框 | 包围盒调试 |
| **Normals** | Gizmo3D | 法线方向线段 | 法线方向检查 |
| **UV Checker** | 特殊 Debug | 棋盘格纹理替换 Albedo | UV 展开检查 |
| **Overdraw Heatmap** | 特殊 Debug | 半透明叠加计数 → 热力图 | 性能分析 |
| **Depth Visualization** | 特殊 Debug | 深度线性化 → 灰度 | 深度缓冲检查 |

#### Gizmo3D Fragment Shader 核心逻辑

```glsl
// gizmo3d.frag — 不参与场景光照管线
#include "debug_lighting.glsl"

layout(location = 0) in vec3 v_WorldNormal;
layout(location = 1) in vec4 v_Color;

layout(location = 0) out vec4 o_Color;

void main()
{
    // 硬编码 Half-Lambert 调试光照 — 始终可见，不受场景灯光影响
    float NdotL = dot(normalize(v_WorldNormal), -debug.sun_direction);
    float halfLambert = NdotL * 0.5 + 0.5;  // 暗面不会全黑
    
    vec3 diffuse  = debug.sun_color * debug.sun_intensity * halfLambert;
    vec3 ambient  = debug.ambient_color * debug.ambient_intensity;
    vec3 lighting = diffuse + ambient;
    
    o_Color = vec4(v_Color.rgb * lighting, v_Color.a);
}
```

> **关键设计**：Gizmo3D / Debug 材质在 Pass 7 (Debug Overlay) 中渲染，
> 使用独立的 `DebugLightingConfig` UBO，不绑定 ShadowMask / SSAO / 场景灯光 SSBO。
> 程序员可在 Debug 面板实时调整太阳方向和强度，用于多角度检查模型。

---

## 3. 画质档位与光照模型

### 核心思想：表面类型 × 画质档位

美术看到的是**表面类型**（Standard、Skin、Hair ...），不会看到 BlinnPhong / PBR 这种底层概念。
引擎根据设备能力自动选画质档位（QualityTier），每个档位对应不同的光照算法和纹理需求。

### 3.1 画质档位定义

```cpp
enum class QualityTier : uint8_t
{
    Low     = 0,    // BlinnPhong FakePBR — 移动端/低端 iGPU
    Medium  = 1,    // 标准 PBR (Cook-Torrance) — 核显/中端独显
    High    = 2,    // 完整 PBR + 高级特性 — 高端独显
    Ultra   = 3,    // 保留：未来可能加入 Ray-Tracing 等
};
```

### 3.2 每档位的光照算法

| 档位 | 直接光照 | 环境光 | 阴影 | 纹理需求 |
|------|---------|--------|------|---------|
| **Low** | Half-Lambert + Blinn-Phong Specular | 指数天空色 (Simple) | None 或 PCF | BaseColor, Normal |
| **Medium** | Cook-Torrance BRDF | FakeAtmosphere | PCF | BaseColor, Normal, MetallicRoughness |
| **High** | Cook-Torrance BRDF | IBL (CubeMap) | PCSS | BaseColor, Normal, MetallicRoughness, AO, Emissive |
| **Ultra** | 同 High（预留扩展） | 同 High | 同 High | 同 High + DetailNormal 等 |

### 3.3 纹理降级策略

美术总是配置最高规格纹理集。低档位自动忽略多余纹理，引擎用缺省值替代：

```
Ultra/High:  Albedo + Normal + MetallicRoughness + AO + Emissive + Detail...
                                                    ↓ 低档位忽略
Medium:      Albedo + Normal + MetallicRoughness
                                       ↓ 低档位忽略
Low:         Albedo + Normal
                       ↓ 如果连 Normal 也不要
极低端:       Albedo only (NdotL 用顶点法线)
```

降级规则（编译期 `#define` 控制，不是运行时分支）：

| 纹理 | Low | Medium | High/Ultra |
|------|-----|--------|------------|
| Albedo / BaseColor | ✅ 采样 | ✅ 采样 | ✅ 采样 |
| Normal Map | ✅ 采样 | ✅ 采样 | ✅ 采样 |
| MetallicRoughness | ❌ 用 MI 常量 | ✅ 采样 | ✅ 采样 |
| AO | ❌ 默认 1.0 | ❌ 默认 1.0 | ✅ 采样 |
| Emissive | ❌ 默认 0.0 | ❌ MI 常量色 | ✅ 采样 |
| Detail Normal | ❌ | ❌ | ✅ 可选采样 |

### 3.4 环境光模型（内部实现，美术不可见）

```cpp
enum class AmbientModel : uint8_t
{
    Simple              = 0,    // 指数梯度天空色（Low 档位使用）
    FakeAtmosphere      = 1,    // 地平线暖色调 + 大气散射（Medium 档位使用）
    IBL                 = 2,    // Image-Based Lighting（High/Ultra 档位使用）
};
```

### 3.5 阴影模型（内部实现，美术不可见）

```cpp
enum class ShadowMode : uint8_t
{
    None    = 0,    // 无阴影
    PCF     = 1,    // Percentage Closer Filtering（Low/Medium）
    PCSS    = 2,    // Percentage Closer Soft Shadows（High/Ultra）
};
```

> 美术不直接选择环境光或阴影模式——这些由 QualityTier 自动决定。
> 程序 / TA 可以在项目设置中覆盖每个档位的具体配置。

---

## 4. 表面类型与 Shader Permutation Key

### 4.1 表面类型定义

```cpp
enum class SurfaceType : uint8_t
{
    // ===== Unlit 类（无光照，不受 QualityTier 影响）=====
    Unlit           = 0,    // 纯色、顶点色、2D UI 等

    // ===== Lit 类（受 QualityTier 降级）=====
    Standard        = 1,    // 标准表面（岩石、金属、木头、塑料...覆盖 90% 场景）
    Skin            = 2,    // 皮肤（次表面散射 SSS）
    Hair            = 3,    // 头发（各向异性高光 Kajiya-Kay / Marschner）
    Cloth           = 4,    // 布料（Sheen + Charlie 分布）
    Eye             = 5,    // 眼球（折射 + 焦散 + SSS）
    Foliage         = 6,    // 植被（薄层透光 Thin Translucency）
    ClearCoat       = 7,    // 清漆/车漆（双层 BRDF）
    Water           = 8,    // 水面（FFT 波形 + 折射 + 反射）

    // ===== 特殊类（专用 Shader，不走通用光照管线）=====
    Sky             = 9,    // 天空（程序化大气散射）
    Terrain         = 10,   // 地形（多层 Splat 混合 + 高度图）

    MAX_SURFACE_TYPE
};
```

> 阶段一只实现 `Unlit` + `Standard`，其余类型预留枚举和接口，后续逐步填充。

### 4.2 Shader Permutation Key（修订版）

```cpp
struct ShaderPermutationKey
{
    SurfaceType     surface     : 4;    // 0-15，表面类型
    QualityTier     quality     : 2;    // 0-3，画质档位（引擎自动选）
    ShadowMode      shadow      : 2;    // 0-2，阴影模式（由 quality 决定）
    // ---- byte boundary ----
    uint8_t         flags       : 4;    // bit0: alpha_test
                                        // bit1: double_sided
                                        // bit2: vertex_color_blend
                                        // bit3: reserved
    uint8_t         _reserved   : 4;

    uint16_t ToU16() const;
};
```

### 4.3 有效排列矩阵

对于 **Standard 表面**：

| Quality | 直接光照 | 环境光 | 阴影 | 纹理数 |
|---------|---------|--------|------|--------|
| Low | BlinnPhong | Simple | None/PCF | 2 (Albedo+Normal) |
| Medium | Cook-Torrance | FakeAtm | PCF | 3 (Albedo+Normal+MR) |
| High | Cook-Torrance | IBL | PCSS | 5+ (Albedo+Normal+MR+AO+Emissive) |
| Ultra | 同 High | IBL | PCSS | 6+ (加 DetailNormal 等) |

总变体数 = SurfaceType(实现数) × Quality(4) × Shadow(3) × Flags 子集 ≈ **可控范围**

> Unlit 类不受 Quality 影响，只有 1 个变体（+ flags 组合）

---

## 5. 预设材质清单（按表面类型分类）

### 5.1 总览

**核心理念**：材质按"用来画什么"分类，不按"用什么算法"分类。

#### Unlit 系列（无光照）

| ID | 预设名 | 维度 | 用途 | 美术可调 |
|----|--------|------|------|---------|
| 0 | PureColor2D | 2D | UI 纯色矩形 | Color(vec4) |
| 1 | Texture2D | 2D | UI 贴图 | Texture |
| 2 | Text2D | 2D | 文字 SDF 渲染 | TextColor, FontTexture |
| 3 | PureColor3D | 3D | 纯色网格/线框/调试 | Color(vec4) |
| 4 | VertexColor3D | 3D | 顶点色网格 | — |
| 5 | PaletteColor3D | 3D | 调色板索引色（体素风格） | ColorPalette[256] |
| 6 | Gizmo3D | 3D | 编辑器调试可视化（骨骼/碰撞体/包围盒等） | Color, RenderMode（见 2.5 节） |
| 7 | Emissive3D | 3D | 自发光/霓虹/LED | EmissiveColor, Texture, Intensity |
| 8 | Billboard | 3D | 公告板（粒子/标签） | Size, Texture |

#### Standard Surface（标准表面 — 覆盖 90% 场景对象）

| ID | 预设名 | 纹理输入模式 | 用途 | 美术可调参数 |
|----|--------|-------------|------|-------------|
| 10 | StandardTexture | 全纹理 | **最通用**：岩石/金属/木头等 | Albedo, Normal, MR, AO, Emissive + 参数因子 |
| 11 | StandardColor | 纯参数 | 纯色金属/塑料/调试 | BaseColor, Metallic, Roughness |
| 12 | StandardVertexColor | 顶点色 | 顶点色 + 完整光照 | — |

#### Special Surface（特殊表面 — 阶段一预留接口）

| ID | 预设名 | 用途 | 状态 | 额外纹理/参数 |
|----|--------|------|------|---------------|
| 20 | Skin | 角色皮肤 | 🔮 预留 | SubsurfaceColor, Thickness, CurvatureMap |
| 21 | Hair | 角色头发 | 🔮 预留 | HairDirection, ShiftMap, AnisotropyRotation |
| 22 | Cloth | 布料 | 🔮 预留 | SheenColor, SheenRoughness |
| 23 | Eye | 眼球 | 🔮 预留 | IrisTexture, IrisDepth, RefractionIndex |
| 24 | Foliage | 植被/树叶 | 🔮 预留 | TranslucencyColor, Thickness, WindParams |
| 25 | ClearCoat | 车漆/瓷釉 | 🔮 预留 | ClearCoatRoughness, ClearCoatNormal |
| 26 | Water | 水面 | 🔮 预留 | WaveParams, FoamTexture, RefractionDepth |

#### Scene（场景专用）

| ID | 预设名 | 用途 | 美术可调 |
|----|--------|------|---------|
| 30 | Sky | 天空球 | SkyInfo UBO（程序控制） |
| 31 | Terrain | 地形 | HeightMap, NormalMap, SplatMap, LayerTextures[4] |

### 5.2 Standard Surface 详解（重点）

Standard 是最核心的材质。美术只看到这 **一个** 材质（选"标准表面"），填完所有纹理和参数。
引擎自动根据 QualityTier 降级。

#### 美术面对的完整纹理槽与参数

```
┌─────────────────────────────────────────────────┐
│ Material Editor — Standard Surface               │
├─────────────────────────────────────────────────┤
│                                                  │
│ Surface Type: [Standard Surface     ▼] (不可改)   │
│                                                  │
│ ── Textures ──                                   │
│ Albedo:             [rock_albedo.png     ] [🔍]  │  ← 必填
│ Normal:             [rock_normal.png     ] [🔍]  │  ← 必填
│ MetallicRoughness:  [rock_mr.png         ] [🔍]  │  ← 选填(Medium+)
│ AO:                 [rock_ao.png         ] [🔍]  │  ← 选填(High+)
│ Emissive:           [                    ] [🔍]  │  ← 选填(High+)
│ Detail Normal:      [                    ] [🔍]  │  ← 选填(Ultra)
│                                                  │
│ 🔔 灰色槽位在低画质档位会被自动忽略                    │
│                                                  │
│ ── Parameters ──                                 │
│ Base Color Tint:     [████████] #FFFFFF          │
│ Metallic Factor:     [=====●====] 0.80           │  ← 无MR纹理时用此值
│ Roughness Factor:    [==●=======] 0.30           │  ← 无MR纹理时用此值
│ Normal Strength:     [======●===] 0.70           │
│ AO Strength:         [========●=] 0.90           │
│ Emissive Intensity:  [●=========] 0.00           │
│                                                  │
│ ── Render Options ──                             │
│ [☐] Alpha Test    Threshold: [====●=====] 0.50  │
│ [☐] Double Sided                                 │
│                                                  │
│ [Apply] [Reset to Default]                       │
└─────────────────────────────────────────────────┘
```

#### Standard Surface 的 MaterialInstance（统一结构）

所有 QualityTier 共享同一个 MI 结构体（低档位忽略部分字段但不影响布局）：

```glsl
struct MI_Standard {
    // Base
    uint  base_color;           // 4B, RGBA packed 色调叠加
    float metallic_factor;      // 4B, [0,1] 金属度（无纹理时为直接值，有纹理时为乘数因子）
    float roughness_factor;     // 4B, [0,1] 粗糙度
    float normal_strength;      // 4B, [0,1] 法线贴图强度

    // Extended (Medium+ 使用)
    float ao_strength;          // 4B, [0,1] AO 强度，Low 档位忽略（默认 1.0）
    float emissive_intensity;   // 4B, 自发光强度，Low 档位忽略（默认 0.0）
    uint  emissive_color;       // 4B, RGBA packed 自发光颜色

    // Flags
    uint  flags;                // 4B, bit0: alpha_test, bit1: double_sided
                                //     bit2: has_vertex_color (自动混合)
                                // 总计 32 bytes
};
```

#### Standard Surface Shader 模板（核心逻辑）

```glsl
// standard_surface.frag.glsl
// 编译期 #define:
//   QUALITY_TIER  0=Low, 1=Medium, 2=High, 3=Ultra
//   SHADOW_MODE   0=None, 1=PCF, 2=PCSS
//   HAS_VERTEX_COLOR  0/1

#include "common/structs.glsl"
#include "common/material_instance.glsl"
#include "common/transform.glsl"
#include "common/shadow.glsl"

// ===== 纹理采样（按档位编译期裁剪）=====
vec4  SampleAlbedo(vec2 uv)           { return texture(TextureAlbedo, uv); }
vec3  SampleNormal(vec2 uv)           { return texture(TextureNormal, uv).xyz * 2.0 - 1.0; }

#if QUALITY_TIER >= 1  // Medium+
vec2  SampleMetallicRoughness(vec2 uv) { return texture(TextureMetallicRoughness, uv).bg; }
#endif

#if QUALITY_TIER >= 2  // High+
float SampleAO(vec2 uv)              { return texture(TextureAO, uv).r; }
vec3  SampleEmissive(vec2 uv)        { return texture(TextureEmissive, uv).rgb; }
#endif

void main()
{
    MI_Standard mi = GetMI();
    vec2 uv = Input.TexCoord;

    // Albedo
    vec4 albedo = SampleAlbedo(uv) * unpackUnorm4x8(mi.base_color);
#if HAS_VERTEX_COLOR
    albedo.rgb *= Input.Color.rgb;
#endif

    // Alpha test
    if ((mi.flags & 1u) != 0u && albedo.a < 0.5) discard;

    // Normal
    vec3 N = ApplyNormalMap(SampleNormal(uv), Input.Normal, Input.Tangent, mi.normal_strength);
    vec3 V = normalize(camera.pos - Input.WorldPosition);
    vec3 L = normalize(ULRE_SUN_DIR);

    // ===== 光照计算（编译期选择）=====
#if QUALITY_TIER == 0
    // --- Low: BlinnPhong FakePBR ---
    float NdotL = dot(N, L) * 0.5 + 0.5;  // Half-Lambert
    vec3 diffuse = albedo.rgb * NdotL * ULRE_SUN_COLOR;

    vec3 H = normalize(V + L);
    float spec_power = mix(128.0, 8.0, mi.roughness_factor);
    float spec = pow(max(dot(N, H), 0.0), spec_power) * (1.0 - mi.roughness_factor);
    vec3 specular = vec3(spec) * ULRE_SUN_COLOR * mi.metallic_factor;

    vec3 ambient = GetAmbientSimple(N, albedo.rgb);
    vec3 color = (diffuse + specular) * GetShadowFactor() + ambient;

#elif QUALITY_TIER == 1
    // --- Medium: Standard PBR ---
    vec2 mr = SampleMetallicRoughness(uv);
    float metallic  = mr.x * mi.metallic_factor;
    float roughness = mr.y * mi.roughness_factor;

    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
    vec3 color = EvalCookTorranceBRDF(N, V, L, albedo.rgb, F0, metallic, roughness);
    color *= ULRE_SUN_COLOR * GetShadowFactor();
    color += GetAmbientFakeAtm(N, albedo.rgb);

#else
    // --- High/Ultra: Full PBR + AO + Emissive + IBL ---
    vec2 mr = SampleMetallicRoughness(uv);
    float metallic  = mr.x * mi.metallic_factor;
    float roughness = mr.y * mi.roughness_factor;
    float ao        = SampleAO(uv) * mi.ao_strength;

    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
    vec3 direct = EvalCookTorranceBRDF(N, V, L, albedo.rgb, F0, metallic, roughness);
    direct *= ULRE_SUN_COLOR * GetShadowFactor();

    vec3 ambient = GetAmbientIBL(N, V, albedo.rgb, F0, metallic, roughness) * ao;
    vec3 emissive = SampleEmissive(uv) * unpackUnorm4x8(mi.emissive_color).rgb * mi.emissive_intensity;

    vec3 color = direct + ambient + emissive;
#endif

    FragColor = vec4(color, albedo.a);
}
```

### 5.3 每个预设的 MaterialInstance 结构

#### 2D Unlit 材质

```glsl
// [0] PureColor2D
struct MI_PureColor2D {
    vec4 Color;                 // 16 bytes
};

// [1] Texture2D — 无 MI 结构，只有纹理槽: sampler2D TextureBaseColor

// [2] Text2D
struct MI_Text2D {
    uint TextColor;             // 4 bytes (RGBA packed)
};
```

#### 3D Unlit 材质

```glsl
// [3] PureColor3D
struct MI_PureColor3D {
    vec4 Color;                 // 16 bytes
};

// [4] VertexColor3D — 无 MI 结构

// [5] PaletteColor3D — MI = ColorPalette UBO (vec4 color[256])

// [6] Gizmo3D — 使用硬编码调试光照（不受场景灯光/阴影/SSAO 影响）
//   独立绑定 DebugLightingConfig UBO（见 2.5 节），程序可热修改太阳方向/强度
struct MI_Gizmo3D {
    vec4 Color;                 // 16 bytes, 线框/表面颜色
    uint render_mode;           // 4 bytes: 0=Solid, 1=Wireframe, 2=SolidWireframe, 3=XRay(穿透深度)
    float wire_width;           // 4 bytes: 线宽（像素）
    uint _padding[2];           // 8 bytes
};                              // 32 bytes
// DebugLightingConfig UBO 在 Set 3 单独绑定（仅 Gizmo3D / Debug 材质使用）

// [7] Emissive3D
struct MI_Emissive3D {
    uint  emissive_color;       // 4 bytes, RGBA packed
    float intensity;            // 4 bytes, HDR 强度乘数
    uint  _padding[2];          // 8 bytes
};
// 纹理槽: TextureEmissive（可选）

// [8] Billboard
struct MI_Billboard {
    uvec2 size;                 // 8 bytes, 尺寸
    uint  flags;                // 4 bytes (bit0: fixed_pixel_size)
    uint  _padding;             // 4 bytes
};
// 纹理槽: TextureBaseColor
```

#### Standard Surface（统一 MI，见 5.2 节）

```glsl
// [10] StandardTexture    — 使用 MI_Standard，纹理槽全开
// [11] StandardColor      — 使用 MI_Standard，无纹理（纯参数驱动）
// [12] StandardVertexColor — 使用 MI_Standard，flags.bit2=1
```

#### Special Surface（预留，结构设计供参考）

```glsl
// [20] Skin（预留）
struct MI_Skin {
    // 继承 Standard 全部字段
    uint  base_color;           
    float metallic_factor;      
    float roughness_factor;     
    float normal_strength;      
    float ao_strength;          
    float emissive_intensity;   
    uint  emissive_color;       
    uint  flags;                // 32 bytes（同 MI_Standard）
    // SSS 扩展
    uint  subsurface_color;     // 4B, 次表面散射颜色
    float subsurface_radius;    // 4B, 散射半径
    float curvature_scale;      // 4B, 曲率缩放
    float thickness_scale;      // 4B, 厚度缩放
};                              // 48 bytes
// 额外纹理: TextureSubsurfaceColor, TextureThickness, TextureCurvature

// [21] Hair（预留）
struct MI_Hair {
    uint  base_color;
    float roughness_factor;
    float normal_strength;
    uint  flags;                // 16 bytes
    // 各向异性扩展
    float anisotropy;           // 4B, 各向异性强度 [-1, 1]
    float anisotropy_rotation;  // 4B, 旋转角度 [0, 2π]
    float primary_shift;        // 4B, Kajiya-Kay 主高光偏移
    float secondary_shift;      // 4B, 次高光偏移
    uint  secondary_color;      // 4B
    float scatter;              // 4B
    uint  _padding[2];          // 8 bytes
};                              // 48 bytes
// 额外纹理: TextureHairDirection, TextureShift

// [22] Cloth（预留）
struct MI_Cloth {
    uint  base_color;
    float roughness_factor;
    float normal_strength;
    uint  flags;                // 16 bytes
    // 布料扩展
    uint  sheen_color;          // 4B
    float sheen_roughness;      // 4B
    uint  _padding[2];          // 8 bytes
};                              // 32 bytes

// [25] ClearCoat（预留）
struct MI_ClearCoat {
    // 继承 Standard 全部字段 (32 bytes)
    uint  base_color;
    float metallic_factor;
    float roughness_factor;
    float normal_strength;
    float ao_strength;
    float emissive_intensity;
    uint  emissive_color;
    uint  flags;
    // 清漆扩展
    float clearcoat_intensity;  // 4B [0,1]
    float clearcoat_roughness;  // 4B [0,1]
    uint  _padding[2];          // 8 bytes
};                              // 48 bytes
// 额外纹理: TextureClearCoatNormal
```

#### Scene 特殊材质

```glsl
// [30] Sky — 无 MI，使用 SkyInfo UBO

// [31] Terrain
struct MI_Terrain {
    float tile_scale;           // 4 bytes, 纹理平铺密度
    float height_scale;         // 4 bytes, 高度缩放
    uint  splat_layer_count;    // 4 bytes, 实际使用的 Splat 层数
    uint  _padding;             // 4 bytes
};
// 纹理槽: TextureHeightMap, TextureNormalMap, TextureSplatMap, TextureLayer[4]
```

---

## 6. 系统架构

### 6.1 对比：现有系统 vs 新系统

```
【现有系统】— 过度灵活
FixedMaterialDef ──────┐
ComposedMaterialDef ───┤
MaterialLogicDef ──────┼──▶ ShaderCompositionBridge ──▶ MaterialCompiler ──▶ GLSL
ShaderPermutationKey ──┤                                     ↑
MaterialCreateConfig ──┘                              BuiltinHelpers
                                                      (运行时注入函数)

【新系统】— 硬编码直出
MaterialPreset ──▶ PresetShaderTemplate[preset_id] ──▶ #define 替换 ──▶ GLSL ──▶ SPV
       +
ShaderPermutationKey (lighting × ambient × shadow)
```

### 6.2 核心类图

```
┌──────────────────────────────────────────────────────────┐
│                 MaterialPresetRegistry                    │
│  (单例，持有所有预设的完整定义)                               │
│                                                           │
│  MaterialPresetDef presets[PRESET_COUNT];                 │
│                                                           │
│  + GetPreset(PresetID) → MaterialPresetDef&              │
│  + CompileAll() → SPVCache                               │
│  + GetSPV(PresetID, QualityTier, Shadow) → SPVData      │
└───────────────┬──────────────────────────────────────────┘
                │ 包含
                ▼
┌──────────────────────────────────────────────────────────┐
│              MaterialPresetDef                             │
│  (单个预设的完整定义，编译期常量)                              │
│                                                           │
│  const char*           name                               │
│  PresetID              id                                 │
│  SurfaceType           surface_type                       │
│  QualityTier           min_tier, max_tier                 │ ← 支持的档位范围
│  PrimitiveType         primitive                          │
│  FixedVertexEntry[]    vertex_entries                     │
│  FixedDescriptorEntry[] descriptor_entries                │
│  const char*           mi_glsl_struct                     │
│  uint32_t              mi_struct_bytes                    │
│  const char*           vs_template                        │ ← 完整 GLSL 模板
│  const char*           fs_template                        │ ← 完整 GLSL 模板
│  TextureSlotDef[]      texture_slots                      │ ← 分档位纹理使用表
│  bool                  supports_vbuffer                   │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│                TextureSlotDef                              │
│  (纹理槽声明 + 按档位降级规则)                               │
│                                                           │
│  uint8_t          binding                                 │
│  const char*      name          // "TextureAlbedo"        │
│  const char*      glsl_type     // "sampler2D"            │
│  QualityTier      min_tier_required  // 此槽最低需要的档位   │
│  FallbackMode     fallback           // 低于min_tier时的策略 │
│    → UseDefault(vec4 default_value)                       │
│    → UseMIParam(const char* param_name)                   │
│    → Disabled                                             │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│              MaterialInstance                              │
│  (运行时，美术/程序填写参数)                                  │
│                                                           │
│  PresetID              preset_id                          │
│  void*                 mi_data              → MI_XXX 数据  │
│  uint32_t              mi_data_size                       │
│  VkDescriptorSet       textures[MAX_TIER]   → 每档位可能    │
│                                               不同纹理绑定  │
│  // 注意：permutation_key 不再由美术设置                     │
│  // 而是由引擎根据 DeviceProfile 自动生成                    │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│               DeviceQualityProfile                        │
│  (设备画质档案——启动时检测一次)                               │
│                                                           │
│  QualityTier      tier                                    │ ← 根据 GPU 能力自动判定
│  AmbientModel     ambient                                 │
│  ShadowMode       shadow                                  │
│  bool             supports_bindless                       │
│  bool             supports_compute                        │
│  uint32_t         max_texture_units                       │
│                                                           │
│  + static Detect(VkPhysicalDevice) → DeviceQualityProfile│
│  + ToPermutationKey(SurfaceType) → ShaderPermutationKey  │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│            PresetShaderCompiler                            │
│  (构建期工具，批量编译所有变体)                               │
│                                                           │
│  + CompilePreset(def, tier, shadow) → SPVPair            │
│  + CompileAllVariants(def) → SPVCache                    │
│  + InjectDefinesAndCompile(template, tier, shadow)       │
└──────────────────────────────────────────────────────────┘
```

### 6.3 Descriptor Set 布局（固定统一）

所有材质统一使用以下 4 个 Descriptor Set，**布局全局固定不变**：

```
Set 0 — Global（每帧一次绑定）
  binding 0: ViewportInfo        (UBO)
  binding 1: CameraInfo          (UBO)
  binding 2: SkyInfo             (UBO)
  binding 3: LightBuffer         (SSBO, 场景灯光列表，未来用)

Set 1 — PerObject（每物体/每 DrawCall）
  binding 0: LocalToWorld        (UBO 或 SSBO, 支持 instancing)

Set 2 — PerMaterial（每材质切换时绑定）
  binding 0: MaterialInstance    (UBO 或 SSBO)
  binding 1-12: 纹理槽          (CombinedImageSampler)
    ── Standard Surface 纹理槽 ──
      binding 1: TextureAlbedo            (Low+)
      binding 2: TextureNormal            (Low+)
      binding 3: TextureMetallicRoughness (Medium+)
      binding 4: TextureAO               (High+)
      binding 5: TextureEmissive          (High+)
      binding 6: TextureDetailNormal      (Ultra, 预留)
    ── Special Surface 扩展纹理槽 ──
      binding 7: TextureExtra0   (Skin: SubsurfaceColor / Hair: Direction / Terrain: SplatMap)
      binding 8: TextureExtra1   (Skin: Thickness / Hair: Shift / Terrain: Layer0)
      binding 9: TextureExtra2   (ClearCoat: CoatNormal / Terrain: Layer1)
      binding 10: TextureExtra3  (Terrain: Layer2)
      binding 11: TextureExtra4  (Terrain: Layer3)
      binding 12: TextureExtra5  (预留)

Set 3 — Environment（环境/全局光照资源 + 管线 RT）
  binding 0: ColorPalette        (UBO, PaletteColor3D 专用)
  binding 1: ShadowMap           (sampler2DShadow, CSM Array — 2~4 级)
  binding 2: ShadowMask          (sampler2D, R8/RG8 — ShadowMask Compose Pass 输出)
  binding 3: SSAO_RT             (sampler2D, R8 — SSAO/SSDO Pass 输出)
  binding 4: IBL_Irradiance      (samplerCube)
  binding 5: IBL_Prefiltered     (samplerCube)
  binding 6: IBL_BRDF_LUT        (sampler2D)
  binding 7: SSS_LUT             (sampler2D, Skin 预留)
  binding 8: DebugLightingConfig  (UBO — 仅 Gizmo3D/Debug 材质绑定，见 2.5 节)
```

> **纹理槽设计策略**：binding 1-6 是 Standard Surface 的纹理（按需求频率排列），
> binding 7-12 是 Special Surface 扩展。同一个 binding 不同表面类型复用（靠 SurfaceType 区分语义）。
> 编译器根据 SurfaceType 和 QualityTier 仅声明实际使用的 binding。

---

## 7. Shader 模板机制

### 7.1 模板结构（取代 Logic 注入）

每个预设材质自带完整的 VS/FS GLSL 模板。编译时只做 `#define` 注入。

```glsl
// ===== 模板头部（编译器自动注入）=====
#version 450
#extension GL_ARB_separate_shader_objects : enable

// 由 ShaderPermutationKey 决定的 #define（引擎自动生成，美术无感知）
// SURFACE_TYPE    0=Unlit, 1=Standard, 2=Skin, ...
// QUALITY_TIER    0=Low(BlinnPhong), 1=Medium(PBR), 2=High(PBR+), 3=Ultra
// SHADOW_MODE     0=None, 1=PCF, 2=PCSS
// RENDER_PATH     0=Forward, 1=VBuffer_IDPass, 2=VBuffer_Resolve
// HAS_TEXTURE_MR  0/1 (MetallicRoughness 纹理是否可用)
// HAS_TEXTURE_AO  0/1
// ...

// ===== Descriptor Layout（编译器按预设定义 + 档位生成）=====
layout(set=0, binding=0) uniform UBO_Viewport { ... } viewport;
layout(set=0, binding=1) uniform UBO_Camera   { ... } camera;
// ... 根据预设 + 档位按需生成

// ===== 预设自带的完整 Shader 代码 =====
// 使用 #if QUALITY_TIER 做编译期分支（不是运行时分支）
```

### 7.2 示例：Standard Surface 的统一 Fragment Shader 模板

见 5.2 节中的 `standard_surface.frag.glsl`，此处不重复。
核心是 **同一份模板文件** 通过 `#if QUALITY_TIER` 实现 Low(BlinnPhong) / Medium(PBR) / High(PBR+) 三条路径。

### 7.3 共享 Include 文件

不再使用 C++ 字符串拼接 GLSL，而是使用真正的 GLSL `#include`（glslang 支持）：

```
ShaderLibrary/
  common/
    structs.glsl               // ViewportInfo, CameraInfo, SkyInfo, LocalToWorld 结构体
    material_instance.glsl     // GetMI() 函数
    transform.glsl             // GetLocalToWorld(), GetWorldPosition(), GetNormal() 等
    position_2d.glsl           // GetPosition2D() 各坐标系
    normal_mapping.glsl        // ApplyNormalMap(), TBN 构造
    lighting_blinnphong.glsl   // Half-Lambert + Blinn-Phong（Low 档位）
    lighting_pbr.glsl          // Cook-Torrance BRDF（Medium/High/Ultra 档位）
    ambient.glsl               // 环境光计算（#if QUALITY_TIER 分支选 Simple/FakeAtm/IBL）
    shadow.glsl                // 阴影采样 + PCF/PCSS（#if SHADOW_MODE 分支）
    shadow_mask.glsl           // ShadowMask RT 采样（屏幕空间查找）
    ssao.glsl                  // SSAO RT 采样（屏幕空间查找）
    motion_vector.glsl         // Motion Vector 计算（TAA 用）
    debug_lighting.glsl        // DebugLightingConfig UBO 定义 + Half-Lambert 调试光照
    tone_mapping.glsl          // 色调映射工具函数
  surface/
    standard.vert.glsl         // Standard Surface 顶点 Shader（所有档位共用）
    standard.frag.glsl         // Standard Surface 片元 Shader（#if 分档位）
    standard_color.frag.glsl   // StandardColor 变体（无纹理）
    standard_vtxcolor.frag.glsl// StandardVertexColor 变体
    skin.frag.glsl             // Skin Surface（预留）
    hair.frag.glsl             // Hair Surface（预留）
    cloth.frag.glsl            // Cloth Surface（预留）
    clearcoat.frag.glsl        // ClearCoat Surface（预留）
  unlit/
    pure_color_2d.vert.glsl
    pure_color_2d.frag.glsl
    texture_2d.vert.glsl
    texture_2d.frag.glsl
    text_2d.vert.glsl
    text_2d.frag.glsl
    pure_color_3d.vert.glsl
    pure_color_3d.frag.glsl
    vertex_color_3d.vert.glsl
    vertex_color_3d.frag.glsl
    emissive_3d.vert.glsl
    emissive_3d.frag.glsl
    billboard.vert.glsl
    billboard.frag.glsl
    palette_color_3d.vert.glsl
    palette_color_3d.frag.glsl
  debug/
    gizmo_3d.vert.glsl          // 调试几何体 VS（骨骼/碰撞体/包围盒）
    gizmo_3d.frag.glsl          // 调试 FS: 硬编码光照（#include "debug_lighting.glsl"）
    uv_checker.frag.glsl        // UV 棋盘格检查
    overdraw_heatmap.frag.glsl  // Overdraw 热力图（半透明叠加计数）
    depth_visualize.frag.glsl   // 深度线性化灰度可视化
    normal_visualize.frag.glsl  // 法线方向可视化 (RGB = N*0.5+0.5)
  scene/
    sky.vert.glsl
    sky.frag.glsl
    terrain.vert.glsl
    terrain.frag.glsl
  vbuffer/
    vbuffer_id.vert.glsl         // Pass1: 所有不透明几何体共用
    vbuffer_id.frag.glsl
    vbuffer_resolve.comp.glsl    // Pass2: Material Resolve + Lighting
  postprocess/
    shadowmask_compose.comp.glsl // ShadowMap → ShadowMask RT 转换
    ssao.comp.glsl               // Screen-Space Ambient Occlusion
    ssdo.comp.glsl               // Screen-Space Directional Occlusion（High+ 可选）
    taa_resolve.comp.glsl        // Temporal AA: history + motion vector → 混合
    bloom_extract.comp.glsl      // 高亮提取
    bloom_blur.comp.glsl         // 高斯模糊（水平/垂直）
    tone_mapping.comp.glsl       // ACES / Filmic → LDR
    fxaa.comp.glsl               // FXAA 3.11
```

---

## 8. VBuffer 渲染路径详细设计

### 8.1 VBuffer ID Pass

所有不透明几何体共用同一对 VS/FS，只写 ID：

```glsl
// vbuffer_id.frag.glsl
layout(location=0) out uvec2 VBufferOutput;
// x = SurfaceType (4bit) | PresetID (4bit) | MaterialInstanceIndex (16bit) | Flags (8bit)
// y = TriangleID (gl_PrimitiveID)

void main()
{
    VBufferOutput = uvec2(
        (uint(SURFACE_TYPE) << 28)
      | (uint(PRESET_ID)   << 24)
      | (MaterialInstanceID & 0x0000FFFF) << 8
      | uint(MATERIAL_FLAGS),
        uint(gl_PrimitiveID)
    );
}
```

### 8.2 VBuffer Resolve Pass（Compute Shader）

```glsl
// vbuffer_resolve.comp.glsl
layout(local_size_x=8, local_size_y=8) in;

layout(set=0, binding=0) uniform usampler2D VBuffer;
layout(set=0, binding=1) uniform sampler2D  DepthBuffer;
layout(set=0, binding=2, rgba16f) writeonly uniform image2D LitColorOutput;

// 全局资源
layout(set=1, binding=0) uniform UBO_Camera { CameraInfo camera; };
layout(set=1, binding=1) uniform UBO_Sky    { SkyInfo sky; };

// 各预设材质的 MI 数据（SSBO 数组）
layout(set=2, binding=0) readonly buffer MaterialData { ... };

// 纹理 Bindless 或 Texture Array
layout(set=3, binding=0) uniform sampler2D MaterialTextures[MAX_TEXTURES];

void main()
{
    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    uvec2 vbuf  = texelFetch(VBuffer, pixel, 0).xy;

    uint surfaceType = vbuf.x >> 28;
    uint presetID    = (vbuf.x >> 24) & 0xF;
    uint miIndex     = (vbuf.x >> 8)  & 0xFFFF;

    // 从深度重建世界坐标
    float depth = texelFetch(DepthBuffer, pixel, 0).r;
    vec3 worldPos = ReconstructWorldPosition(pixel, depth);

    // 按 surfaceType 分支执行对应光照
    // （同一 surfaceType 内部再按 QUALITY_TIER define 选择 BlinnPhong/PBR/PBR+）
    vec3 litColor;
    switch(surfaceType)
    {
        case SURFACE_UNLIT:
            litColor = EvalUnlit(miIndex);
            break;
        case SURFACE_STANDARD:
            litColor = EvalStandard(miIndex, worldPos, ...);
            break;
        case SURFACE_SKIN:
            litColor = EvalSkin(miIndex, worldPos, ...);
            break;
        // ... 未来扩展
    }

    imageStore(LitColorOutput, pixel, vec4(litColor, 1.0));
}
```

### 8.3 LitColor RT（"小 GBuffer"）

| 属性 | 值 |
|------|-----|
| 格式 | RGBA16F |
| 内容 | 光照计算后的最终颜色 (HDR) |
| 用途 | 后期处理输入（Bloom、ToneMapping、FXAA 等） |
| 无后期时 | 直接 ToneMap → SwapChain |

**与传统 GBuffer 的区别**：

| | 传统 GBuffer | VBuffer 小 GBuffer |
|--|---|---|
| RT 数量 | 3-5 张（Albedo, Normal, MetallicRoughness, Depth, Emissive） | 1 张（LitColor） |
| 存储内容 | 中间量（待光照） | 最终颜色（已光照） |
| 带宽 | 高（多 RT 读写） | 低（VBuffer 32bit + Depth + 1 RT） |
| 光照位置 | 单独 Lighting Pass 读 GBuffer | VBuffer Resolve 中直接完成 |

---

## 9. 编译流水线

### 9.1 构建期

```
for each preset in MaterialPresetRegistry:
    for each valid quality_tier in [preset.min_tier .. preset.max_tier]:
        for each shadow_mode in applicable_shadows(quality_tier):
            for each flag_combo in applicable_flags(preset):
                defines = BuildDefines(preset.surface_type, quality_tier, shadow_mode, flags)
                glsl_vs = InjectDefines(preset.vs_template, defines)
                glsl_fs = InjectDefines(preset.fs_template, defines)
                spv_vs  = glslangValidator(glsl_vs)
                spv_fs  = glslangValidator(glsl_fs)
                SPVCache.Store(preset.id, quality_tier, shadow_mode, flags, spv_vs, spv_fs)
```

> Unlit 材质只编译 1 个变体（无档位概念）。\
> Standard Surface 编译 4(tier) × 3(shadow) × flags 子集 ≈ 24 个变体。\
> Special Surface 预留编译同理。

### 9.2 运行时

```cpp
// ===== 引擎启动时：检测设备画质档位 =====
DeviceQualityProfile device_profile = DeviceQualityProfile::Detect(physical_device);
// device_profile.tier = QualityTier::Medium  (例如: Intel Iris Xe)

// ===== 创建材质实例（美术侧 — 只看到表面类型和参数）=====
auto mi = MaterialSystem::CreateInstance(SurfaceType::Standard);

// 美术填写完整的最高规格纹理集（与画质档位无关）
mi->SetTexture(TextureSlot::Albedo,            LoadTexture("rock_albedo.png"));
mi->SetTexture(TextureSlot::Normal,            LoadTexture("rock_normal.png"));
mi->SetTexture(TextureSlot::MetallicRoughness, LoadTexture("rock_mr.png"));
mi->SetTexture(TextureSlot::AO,               LoadTexture("rock_ao.png"));    // High+ 才用
mi->SetTexture(TextureSlot::Emissive,          nullptr);                       // 不需要就留空

// 美术设置参数
mi->SetColor  ("base_color",       Color(255,255,255,255));
mi->SetFloat  ("metallic_factor",  0.0f);
mi->SetFloat  ("roughness_factor", 0.8f);
mi->SetFloat  ("normal_strength",  1.0f);

// ===== 渲染时（引擎自动选择档位，美术无感知）=====
ShaderPermutationKey key = device_profile.ToPermutationKey(mi->surface_type);
auto [spv_vs, spv_fs] = SPVCache.Get(mi->preset_id, key);
VkPipeline pipeline = PipelineCache.GetOrCreate(mi->preset_id, key, render_pass);

// 引擎自动跳过低画质不需要的纹理绑定
DescriptorSet ds = BuildDescriptorSet(mi, device_profile.tier);
// ↑ 如果 tier=Low，AO/Emissive 纹理槽自动绑定 1x1 白色/黑色默认纹理

vkCmdBindPipeline(cmd, pipeline);
vkCmdBindDescriptorSets(cmd, ..., ds, ...);
vkCmdDrawIndexed(cmd, ...);
```

---

## 10. 删除清单（相对现有代码）

### 要删除的概念和代码

| 删除项 | 原文件 | 原因 |
|--------|--------|------|
| `ComposedMaterialDef` | ShaderComposition.h | 不再需要运行时组合 |
| `MaterialLogicDef` | ShaderLogic.h | 不再需要 Logic 注入 |
| `ShaderCompositionBridge` | ShaderCompositionBridge.cpp | 不再需要桥接层 |
| `BuiltinHelpers` 自动注入 | BuiltinHelpers.cpp/.h | 模板静态 #include 替代 |
| `ShaderOutputMode::DualRTDeferred` | ShaderComposition.h | 删除传统 GBuffer 路径 |
| `S_*_Logic.h` 逻辑文件 | src/ShaderGen/3d/S_*_Logic.h | 逻辑直接写在 .glsl 模板中 |
| `VertexShaderBusiness` / `FragmentShaderBusiness` | ShaderComposition.h | 无需运行时代码块 |
| `LightingModel` 枚举 | ShaderPermutationKey.h | 被 SurfaceType + QualityTier 取代 |
| `LightModel::Lambert` | ShaderPermutationKey.h | 合并到 Low tier（BlinnPhong specular=0） |
| `LightModel::CelShading` | ShaderPermutationKey.h | 暂不支持 |
| `LightModel::PBR_Lite` | ShaderPermutationKey.h | 合并到 Medium tier PBR |
| `SkyLightAmbientModel::SphericalHarmonics` | SkyLight.h | 合并到 IBL（High tier 内部实现） |
| `SkyLightAmbientModel::CubeMap` | SkyLight.h | 合并到 IBL |
| `SpecularChannel` 枚举 | ShaderPermutationKey.h | 永远 Combined |
| `GBufferLayout` 枚举和所有变体 | 相关头文件 | 删除传统 GBuffer |
| `SubpassInput` 相关代码 | ShaderDescriptorInfo.cpp | 无传统延迟渲染子 Pass |
| `DescriptorSetType` 动态分配 | MaterialDescriptorInfo.cpp | 改为全局静态布局 |
| `ResourceLayoutGenerator` 动态生成 | ResourceLayoutGenerator.cpp | 改为静态模板 |
| 所有 `M_TextureBlinnPhong` / `M_BasicLit` / `M_PBRColor3D` | src/ShaderGen/3d/ | 合并为 Standard Surface 模板 |

### 要保留并简化的

| 保留项 | 简化方式 |
|--------|----------|
| `FixedMaterialDef` | 改名 `MaterialPresetDef`，扩展为含 SurfaceType、min/max QualityTier、GLSL 模板路径 |
| `FixedVertexEntry` | 保留，定义不变 |
| `FixedDescriptorEntry` | 保留，但 set/binding 全局固定不再动态分配 |
| `ShaderPermutationKey` | 字段改为 surface_type + quality_tier + shadow + flags（不再暴露给美术） |
| `MaterialCreateConfig` | 大幅简化，仅保留 surface_type + 纹理/参数绑定 |
| `GLSLCompiler` | 保留，输入从动态拼接改为模板文件 |
| `StdMaterial` 基类 | 删除虚函数链，改为简单的编译函数 |
| `TextureSlotDef` | 新增：定义纹理槽名、最低 QualityTier 要求、降级 FallbackMode |
| `DeviceQualityProfile` | 新增：设备能力检测 → 自动选择 QualityTier |

---

## 11. 面向美术的接口设计

### 11.1 编辑器 UI（目标体验）

美术看到的编辑器不暴露任何 QualityTier / Shadow 选项，只有表面类型 + 纹理 + 参数：

```
┌─────────────────────────────────────────────────┐
│ Material Instance Editor                         │
├─────────────────────────────────────────────────┤
│ Surface Type: [Standard Surface     ▼]          │  ← 选表面类型（固定列表）
│                                                  │
│ ── Textures ──                                   │
│ Albedo:             [rock_albedo.png     ] [🔍]  │  ← 必填
│ Normal:             [rock_normal.png     ] [🔍]  │  ← 必填
│ MetallicRoughness:  [rock_mr.png         ] [🔍]  │  ← 选填 ⓘ Medium 及以上使用
│ AO:                 [rock_ao.png         ] [🔍]  │  ← 选填 ⓘ High 及以上使用
│ Emissive:           [                    ] [🔍]  │  ← 选填 ⓘ High 及以上使用
│                                                  │
│ ── Parameters ──                                 │
│ Base Color Tint:     [████████] #FFFFFF          │
│ Metallic Factor:     [=====●====] 0.80           │
│ Roughness Factor:    [==●=======] 0.30           │
│ Normal Strength:     [======●===] 0.70           │
│ AO Strength:         [========●=] 0.90           │
│ Emissive Intensity:  [●=========] 0.00           │
│                                                  │
│ ── Render Options ──                             │
│ [☐] Alpha Test    Threshold: [====●=====] 0.50  │
│ [☐] Double Sided                                 │
│                                                  │
│ ── Preview (只读) ──                             │
│ 当前设备档位: Medium (PBR)                        │
│ 活跃纹理槽: Albedo, Normal, MetallicRoughness    │
│ 未使用纹理: AO (需 High+), Emissive (需 High+)   │
│                                                  │
│ [Apply] [Reset to Default]                       │
└─────────────────────────────────────────────────┘
```

### 11.2 C++ API

```cpp
// ===== 创建材质实例（美术侧：只选表面类型）=====
auto mi = MaterialSystem::CreateInstance(SurfaceType::Standard);

// ===== 设置纹理（美术总是配完整的最高规格纹理集）=====
mi->SetTexture(TextureSlot::Albedo,            LoadTexture("rock_albedo.png"));
mi->SetTexture(TextureSlot::Normal,            LoadTexture("rock_normal.png"));
mi->SetTexture(TextureSlot::MetallicRoughness, LoadTexture("rock_mr.png"));
mi->SetTexture(TextureSlot::AO,               LoadTexture("rock_ao.png"));
// 如果某纹理不需要，不 Set 即可——引擎自动使用默认值

// ===== 设置参数 =====
mi->SetColor  ("base_color",       Color(255,255,255,255));
mi->SetFloat  ("metallic_factor",  0.0f);
mi->SetFloat  ("roughness_factor", 0.8f);
mi->SetFloat  ("normal_strength",  1.0f);

// ===== 渲染选项 =====
mi->SetFlag(MaterialFlag::AlphaTest, true);
mi->SetFlag(MaterialFlag::DoubleSided, false);

// ===== 绑定到渲染对象（画质档位由引擎自动处理）=====
renderable->SetMaterial(mi);

// ===== JSON 序列化（数据驱动）=====
// rock_wall.material.json:
// {
//     "surface": "Standard",           ← 表面类型
//     "textures": {
//         "albedo": "textures/rock_albedo.png",
//         "normal": "textures/rock_normal.png",
//         "metallic_roughness": "textures/rock_mr.png",
//         "ao": "textures/rock_ao.png"
//     },
//     "params": {
//         "base_color": "FFFFFFFF",
//         "metallic_factor": 0.0,
//         "roughness_factor": 0.8,
//         "normal_strength": 1.0,
//         "ao_strength": 1.0
//     },
//     "flags": {
//         "alpha_test": false,
//         "double_sided": false
//     }
// }
// 注意：JSON 中没有任何画质/光照/阴影设置——这些全是引擎根据设备自动决定
```

---

## 12. 顶点格式标准化

### 固定 5 种顶点布局（所有预设材质从中选一）

```cpp
// Layout A: Position Only (Sky, Billboard, PureColor3D)
struct VertexLayout_PosOnly {
    vec3 Position;
};

// Layout B: Position + Color (VertexColor3D, Gizmo3D)
struct VertexLayout_PosColor {
    vec3 Position;
    vec4 Color;      // 或 uint (PaletteIndex)
};

// Layout C: Position + TexCoord (Texture2D, Emissive3D)
struct VertexLayout_PosTex {
    vec3 Position;
    vec2 TexCoord;
};

// Layout D: Position + TexCoord + Normal (StandardColor, StandardVertexColor)
struct VertexLayout_PosTexNorm {
    vec3 Position;
    vec2 TexCoord;
    vec3 Normal;
};

// Layout E: Position + TexCoord + Normal + Tangent (StandardTexture, 所有 Lit Surface)
struct VertexLayout_PosTexNormTan {
    vec3 Position;
    vec2 TexCoord;
    vec3 Normal;
    vec4 Tangent;       // xyz=tangent dir, w=handedness
};
```

### 预设 → 顶点布局 映射

| 预设 | 顶点布局 | 附加 per-instance 数据 |
|------|---------|----------------------|
| PureColor2D | vec2 Position | — |
| Texture2D | vec2 Position + vec2 TexCoord | — |
| Text2D | vec2 Position + vec2 TexCoord | — |
| PureColor3D | A (PosOnly) | TransformID, MaterialInstanceID |
| VertexColor3D | B (PosColor) | TransformID |
| PaletteColor3D | B (PosColor, uint) | TransformID |
| Gizmo3D | D (PosTexNorm, TexCoord 不用) | — |
| Emissive3D | C (PosTex) | TransformID, MaterialInstanceID |
| Billboard | A (PosOnly) | TransformID, MaterialInstanceID |
| **StandardTexture** | **E (PosTexNormTan)** | TransformID, MaterialInstanceID |
| StandardColor | D (PosTexNorm) | TransformID, MaterialInstanceID |
| StandardVertexColor | D + Color | TransformID |
| Skin (预留) | E (PosTexNormTan) | TransformID, MaterialInstanceID |
| Hair (预留) | E (PosTexNormTan) | TransformID, MaterialInstanceID |
| Cloth (预留) | E (PosTexNormTan) | TransformID, MaterialInstanceID |
| ClearCoat (预留) | E (PosTexNormTan) | TransformID, MaterialInstanceID |
| Sky | A (PosOnly) | — |
| Terrain | 无（从 VertexID 生成） | — |

> **所有 Lit Surface 统一使用 Layout E**（Position + TexCoord + Normal + Tangent），
> 这确保 Low→High 档位切换不会改变顶点布局，Pipeline 兼容性最好。

---

## 13. 实施路线

### Phase 1：基础框架（核心重构）
1. 定义 `SurfaceType`、`QualityTier`、`ShaderPermutationKey` 枚举/结构体
2. 定义 `MaterialPresetDef` + `TextureSlotDef`（含降级规则）
3. 实现 `DeviceQualityProfile::Detect()`（GPU 检测 → 自动选档位）
4. 定义全局固定 Descriptor Set Layout（4 个 Set）
5. 实现 `PresetShaderCompiler`（读模板 + 注入 `#define` + 调用 glslang）
6. 实现 `SPVCache`（preset_id + tier + shadow + flags → SPV 查表）
7. 编写共享 .glsl include 文件（structs, lighting, ambient, shadow, transform, normal_mapping）

### Phase 2：Unlit 材质移植
8. 移植 PureColor2D, Texture2D, Text2D
9. 移植 PureColor3D, VertexColor3D, PaletteColor3D, Gizmo3D
10. 移植 Emissive3D, Billboard

### Phase 3：Standard Surface（核心）
11. 实现 Standard Surface 统一 GLSL 模板（`#if QUALITY_TIER` 三条路径）
12. 实现纹理降级机制（默认纹理绑定，引擎自动处理）
13. 从现有 TextureBlinnPhong + BasicLit + PBRColor3D 合并迁移
14. 实现 StandardColor 和 StandardVertexColor 变体

### Phase 4：场景材质
15. 移植 Sky, Terrain

### Phase 5：VBuffer 路径
16. 实现 VBuffer ID Pass（通用 VS/FS）
17. 实现 VBuffer Resolve Compute Shader（按 SurfaceType 分支）
18. 实现 LitColor RT 管理和后期处理衔接
19. 实现 Forward / VBuffer 路径自动切换

### Phase 6：Special Surface（后续逐步填充）
20. 实现 Skin Surface（SSS）
21. 实现 Hair Surface（各向异性）
22. 实现 Cloth, ClearCoat, Foliage 等
23. 验证 Special Surface 在所有 QualityTier 下的降级行为

### Phase 7：清理
24. 删除旧的 ShaderComposition / Logic / Bridge 代码
25. 删除传统 GBuffer 相关代码和枚举
26. 更新 Pipeline 创建逻辑使用固定 Layout
27. 更新编辑器 UI（Material Instance 编辑面板）

---

## 附录 A：预设材质与旧代码对应关系

| 新预设 | 旧代码 | 迁移备注 |
|--------|--------|----------|
| PureColor2D | M_PureColor2D.cpp | 直出 |
| Texture2D | M_PureTexture2D.cpp + M_RectTexture2D.cpp | 合并，删除 Rect 变体 |
| Text2D | M_Text.cpp | 直出 |
| PureColor3D | M_PureColor3D.cpp + S_PureColor3D*.h | 逻辑写入 .glsl |
| VertexColor3D | M_VertexColor3D.cpp + S_VertexColor3D*.h | 逻辑写入 .glsl |
| PaletteColor3D | M_VertexPattleColor3D.cpp + S_VertexPattleColor3D*.h | 逻辑写入 .glsl |
| Gizmo3D | M_Gizmo3D.cpp + S_Gizmo3D*.h | 逻辑写入 .glsl |
| **StandardTexture** | M_TextureBlinnPhong.cpp + M_BasicLit.cpp + M_PBRColor3D.cpp | **三合一**：Low=BlinnPhong, Med=PBR, High=PBR+ |
| **StandardColor** | M_PBRColor3D.cpp 简化版 | 纯参数材质，三档光照统一模板 |
| **StandardVertexColor** | VertexColor3D + BlinnPhong 光照 | 顶点色 + 三档光照 |
| Emissive3D | 新增 | Unlit + BaseColor 纹理 + HDR 输出 |
| Sky | M_SkyMinimal.cpp + S_SkyMinimal*.h | 逻辑写入 .glsl |
| Terrain | M_TerrainGrid.cpp | 逻辑写入 .glsl |
| Billboard | M_Billboard*.cpp + S_BillboardVertex.h | 合并 Dynamic/Fixed 两种模式为参数 |
| Skin (预留) | 无 | 新增 SSS 通道，参考 UE SubsurfaceProfile |
| Hair (预留) | 无 | 新增 Kajiya-Kay / Marschner 各向异性高光 |
| Cloth (预留) | 无 | 新增 Sheen + Charlie Model |
| ClearCoat (预留) | 无 | 新增双层 BRDF |

## 附录 B：Texture2DArray 处理

当前 `PBRColor3D` 使用 `Texture2DArray` 通过 MI 中的 `texture_id` 索引纹理层。在新系统中：

- **StandardTexture** 使用独立纹理槽（每个 MaterialInstance 绑定自己的纹理集）
- **VBuffer Resolve** 阶段如需 Bindless 纹理，使用 `VK_EXT_descriptor_indexing` 扩展
- 不再将 `Texture2DArray` 作为主要纹理接口（仅 Terrain SplatMap 保留）

## 附录 C：文件数量对比

| | 旧系统 | 新系统 |
|--|--------|--------|
| C++ 源文件 | ~40+ (.cpp/.h) | ~10 (.cpp/.h) |
| GLSL 模板 | 0（全 C++ 字符串） | ~55 (.glsl 文件，含 postprocess/ 和 debug/) |
| 编译期 Shader 变体 | 动态，不可预测 | ≤11 (SurfaceType) × ≤4 (QualityTier) × ≤4 (shadow+flags) = ~176 最大 |
| 概念数 | FixedMaterialDef, ComposedMaterialDef, MaterialLogicDef, ShaderCompositionBridge, BuiltinHelpers, ShaderCreateInfo (5种), MaterialCreateConfig (3种), LightingModel enum... | MaterialPresetDef, MaterialInstance, PresetShaderCompiler, SPVCache, DeviceQualityProfile, TextureSlotDef |
