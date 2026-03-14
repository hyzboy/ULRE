# ULRE 渲染管线设计文档

> **配套文档**：本文档与 `SimplifiedMaterialSystem_Design.md` 配套阅读，专注于渲染管线结构，
> 供引擎渲染管线重构时参考。材质系统、Shader Compositor、预设材质目录等内容请参阅配套文档。

---

## 目录

1. [渲染管线总览](#1-渲染管线总览)
2. [双渲染路径](#2-双渲染路径)
3. [Forward 路径帧序列](#3-forward-路径帧序列)
4. [VBuffer 路径帧序列](#4-vbuffer-路径帧序列)
5. [GPU-Driven Meshlet 几何管线](#5-gpu-driven-meshlet-几何管线)
6. [阴影系统](#6-阴影系统)
7. [光照管线](#7-光照管线)
8. [后处理管线](#8-后处理管线)
9. [GPU 资源总览](#9-gpu-资源总览)
10. [Descriptor Set 布局](#10-descriptor-set-布局)
11. [平台后端与特性分级](#11-平台后端与特性分级)
12. [Reversed-Z 与无限远平面](#12-reversed-z-与无限远平面)
13. [Camera-Relative Rendering（相机相对渲染）](#13-camera-relative-rendering相机相对渲染)
14. [Shader Compositor 与编译管线](#14-shader-compositor-与编译管线)
15. [VBuffer Tile-Based Resolve 详细设计](#15-vbuffer-tile-based-resolve-详细设计)
16. [重构路线图](#16-重构路线图)

---

## 1. 渲染管线总览

### 1.1 核心架构

| 维度 | 选型 |
|------|------|
| **图形 API** | Vulkan 1.1+ (Android) / 1.2+ (PC) / MoltenVK (Apple) |
| **着色语言** | GLSL 450 → SPIR-V（构建期编译，零运行时编译） |
| **几何管线** | Meshlet 为最小渲染粒度，GPU-Driven LOD + Cull |
| **渲染路径** | Forward + VBuffer 双路径（无传统 GBuffer 延迟渲染） |
| **深度策略** | Reversed-Z + Infinite Far Plane (D32_SFLOAT) |
| **阴影架构** | 双层 SM (Near Dynamic + Far Cached Toroidal) + Capsule + Contact → ShadowMask |
| **光照模型** | QualityTier 自动选择: BlinnPhong / Cook-Torrance / Full PBR+IBL |
| **多光源** | Clustered Shading (High+) / 直接循环 (Med−) |
| **坐标精度** | Camera-Relative Rendering（Model Matrix 减去相机位置，支持无限大世界） |
| **抗锯齿** | TAA (Medium+) / FXAA (fallback) |

### 1.2 设计原则

1. **GPU-Driven**：Meshlet 级 LOD Select + Frustum/Cone/HZB Cull 全在 GPU Compute 完成，CPU 零干预
2. **平台自适应**：PC/Apple 统一 SSBO 路径，Android 按能力分 SSBO/VBO 路径
3. **最小 RT 带宽**：Forward 路径仅 LitColor + Depth + MotionVector；VBuffer 路径 VBuffer(R32G32UI) + Depth → LitColor
4. **统一着色入口**：Shader Compositor 架构，Surface Function(业务) + Compositor Template(main()) 分离
5. **Camera-Relative**：所有 L2W 矩阵在 CPU 端减去相机世界坐标后再上传 GPU，Shader 全程在相机相对坐标系下计算，彻底消除大世界浮点抖动

---

## 2. 双渲染路径

### 2.1 Forward Pass

```
VS (MVP + varying) → FS (Surface + Lighting + Shadow + Fog) → LitColor RT
```

- 每像素完整光照计算
- 输出：LitColor RT (RGBA16F) + Depth RT + Motion Vector RT (RG16F)
- 适用所有平台

### 2.2 VBuffer Pass

```
Pass 1: VBuffer ID Pass — VS (MVP only) → FS (写 ID) → VBuffer RT + Depth
Pass 2: Tile Classification — Compute → 分类为 Empty/Single/Fused/Multi
Pass 3: VBuffer Resolve  — Compute (3-tier dispatch) → LitColor RT
```

- Pass 1 极轻量，无光照
- Pass 2 按 MaterialKey(SurfaceType×PresetID) 分类 tile → 驱动 3 层 Indirect Dispatch
- Pass 3 GPU 侧完成材质求值+光照
- **仅 SSBO 平台可用**（PC / Apple / Android High）

### 2.3 路径选择规则

| 对象类型 | 路径 | 原因 |
|----------|------|------|
| 不透明 3D 几何体 | VBuffer (SSBO 平台) / Forward | 高密度场景 VBuffer 优势大 |
| 透明物体 / 粒子 | Forward | VBuffer 无法处理半透明 |
| 2D UI / HUD | Forward | 无需深度/光照 |
| 天空 | Forward (独立 Pass) | 特殊材质 |

> **平台约束**：Android Mid/Low (VBO 路径) 强制 Forward Only。

---

## 3. Forward 路径帧序列

```
┌──────────────────────────────────────────────────────────────────────────┐
│ Pass 0.  Meshlet Select & Cull (Compute, High+)                         │
│          · Instance Cull → Meshlet LOD DAG 遍历 + Frustum/Cone/HZB Cull │
│          · 输出: IndirectDrawBuffer + DrawCount                          │
│          · Low/Med: CPU Frustum Cull + 离散 LOD → 传统 DrawCall          │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 1.  ShadowMap Pass (离屏)                                           │
│          · Layer 0: Near Dynamic Cascade — 每帧全量渲染，R_near ~50m     │
│          · Layer 1: Far Cached Cascade — Toroidal Scrolling，增量更新     │
│          · 输出: NearSM Depth + CachedSM Depth                          │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 2.  Early-Z Pre-Pass                                                │
│          · 仅写 Depth，无 Color                                          │
│          · High+: vkCmdDrawIndexedIndirectCount (meshlet 级)             │
│          · 输出: Depth RT (D32_SFLOAT, Reversed-Z)                      │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 2b. HZB Generation (Compute, High+)                                 │
│          · Depth → 逐级 min downsample → HZB Pyramid (R32F)             │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 2c. Meshlet Cull Phase 2 (Compute, High+, 可选)                    │
│          · 当前帧 HZB → 补充剔除存疑 meshlet → 追加 Draw               │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 3.  Clustered Light Assignment (Compute, High+)                    │
│          · 视锥 3D cluster grid → 灯光-cluster 求交 → ClusterLightList  │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 4.  ShadowMask Compose (全屏 Compute)                               │
│          · R: Near PCF/PCSS + Far Cached (smoothstep 混合)               │
│          · G: Capsule/Blob Shadow                                        │
│          · B: Contact Shadow (屏幕空间 ray-march, High+)                 │
│          · 输出: ShadowMask RT (RGBA8)                                   │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 5.  SSAO / SSDO (全屏 Compute, Medium+)                            │
│          · 输出: SSAO RT (R8)                                            │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 6.  Forward Lit Pass (主渲染)                                       │
│          · Depth Test = Equal, Depth Write = Off                         │
│          · 读取 ShadowMask + SSAO + ClusterLightList                    │
│          · Fog inline 计算                                               │
│          · 按 (SurfaceType, EffectiveTier, PresetID) 排序批次化          │
│          · 输出: LitColor RT (RGBA16F) + Motion Vector RT (RG16F)       │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 7.  Sky Pass                                                        │
│          · Depth Test = Equal, Depth = 0.0 (Reversed-Z 最远)            │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 8.  Translucent Pass (后排序, back-to-front)                       │
│          · 透明物体、粒子、Billboard                                     │
│          · Depth Write = Off, Blend = SrcAlpha                          │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 9.  Decal Pass (High+)                                              │
│          · 投影盒 OBB → 采样 Depth 反算 worldPos → 投射贴花纹理          │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 10. Debug / Gizmo Overlay (可选)                                    │
│          · 硬编码调试光照，独立于场景光照                                 │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 11. Post-Processing Chain                                           │
│          · SSR → Auto Exposure → TAA → Bloom → DOF → Motion Blur        │
│          · → ToneMapping → Color Grading → FXAA/CAS → SwapChain         │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 4. VBuffer 路径帧序列

```
┌──────────────────────────────────────────────────────────────────────────┐
│ Pass 0.  Meshlet Select & Cull                               (同 Forward)│
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 1.  ShadowMap Pass                                      (同 Forward)│
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 2.  VBuffer ID Pass (替代 Early-Z + Forward Lit)                    │
│          · 写入 VBuffer RT (R32G32UI):                                   │
│            x = SurfaceType(4) | PresetID(4) | InstanceID(16) | Flags(8) │
│            y = TriangleID(8) | MeshletIndex(24)                         │
│          · + Depth RT                                                    │
│          · 极轻量 FS，无光照计算                                          │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 2b. HZB Generation                                      (同 Forward)│
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 2c. Meshlet Cull Phase 2                                (同 Forward)│
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 3.  Tile Material Classification (Compute)                ← ★ 新增 │
│          · 按 TILE_SIZE×TILE_SIZE 统计每 tile 的 MaterialKey 集合        │
│          · 匹配 FusedComboLUT                                            │
│          · 输出四类 tile list: Empty / Single / Fused / Multi            │
│          · 填充 indirect dispatch args (GPU 驱动)                        │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 4.  Clustered Light Assignment                          (同 Forward)│
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 5.  ShadowMask Compose                                  (同 Forward)│
│          ⤷ 可选优化: TileSurfaceMask 跳过 empty/Unlit tile              │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 6.  SSAO / SSDO                                         (同 Forward)│
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 7.  VBuffer Resolve (Tile-Based 3-Tier Indirect Dispatch)   ← ★   │
│          · Dispatch 1: Single-Material tiles — 零分支发散 (~50-70%)      │
│          · Dispatch 2: Fused-Material tiles — 极低发散 (~10-25%)         │
│          · Dispatch 3: Multi-Material tiles — per-pixel switch (~5-15%)  │
│          · 光照 + Fog inline                                             │
│          · 输出: LitColor RT + Motion Vector RT                          │
├──────────────────────────────────────────────────────────────────────────┤
│ Pass 8-12. Sky / Translucent / Decal / Debug / Post-Processing (同 Fwd) │
└──────────────────────────────────────────────────────────────────────────┘
```

### Forward 与 VBuffer 路径对比

| 维度 | Forward | VBuffer |
|------|---------|---------|
| 不透明光照位置 | FS 逐像素 | Compute Resolve |
| Overdraw 控制 | Early-Z Pre-Pass | VBuffer ID 天然零 overdraw |
| RT 带宽 | LitColor + Depth + MV | VBuffer(64bit) + Depth → LitColor + MV |
| 多材质 tile 开销 | 不存在此问题 | Tile Classification + 3-tier dispatch |
| 透明物体 | 原生支持 | 必须 fallback Forward |
| 平台要求 | 全平台 | SSBO 平台 only |

---

## 5. GPU-Driven Meshlet 几何管线

### 5.1 Meshlet 定义

**Meshlet** = 64~128 顶点、最多 124~256 三角形的几何小块。原始 Mesh 在离线阶段拆解为 meshlet，
递归简化合并构建 **LOD DAG**（叶节点=LOD0 原始 meshlet，根=最粗 LOD 的单个 meshlet）。

### 5.2 GPU 数据结构

```cpp
struct MeshletGPU  // 64 bytes, 存入 SSBO
{
    uint32_t vertex_offset, index_offset;
    uint16_t vertex_count, triangle_count;
    vec3     bounding_center;     // 局部空间包围球
    float    bounding_radius;
    int8_t   cone_axis_x, cone_axis_y, cone_axis_z, cone_cutoff;  // 法线锥 (backface cull)
    uint32_t parent_meshlet, children_start;
    uint16_t children_count;
    float    max_error;           // LOD 几何误差
    uint16_t instance_id;
    uint8_t  material_preset_id;
    uint8_t  meshlet_flags;       // TERRAIN_CONTACT / TWO_SIDED / ALPHA_TEST / WIND_ANIM
};
```

### 5.3 GPU 裁剪流程

```
┌─────────────────────────────────────────────────────────────────┐
│ GPU Pass A: Instance Cull (Compute)                             │
│   · BoundingSphere vs 视锥 6 平面                                │
│   · 粗粒度 HZB 遮挡查询 → 整体不可见则跳过全部 meshlet           │
│   · 计算 screenSize + importance → InstanceVisData              │
├─────────────────────────────────────────────────────────────────┤
│ GPU Pass B: Meshlet LOD Select + Cull (核心 Compute Pass)       │
│   1. LOD Cut: screenSpaceError vs threshold                     │
│      · threshold 受 QualityTier + importance 调节               │
│      · error < threshold → 选中此 meshlet, 不细化子节点          │
│      · error >= threshold → 展开子 meshlet 继续评估              │
│   2. Frustum Cull (per-meshlet BoundingSphere)                  │
│   3. Backface Cone Cull (法线锥全背向 → 剔除)                    │
│   4. HZB Occlusion Cull (上帧 HZB, per-meshlet)                │
│   → 通过所有测试 → 写入 IndirectDrawBuffer                       │
├─────────────────────────────────────────────────────────────────┤
│ 输出: IndirectDrawBuffer (VkDrawIndexedIndirectCommand[])       │
│       DrawCount (uint32)                                        │
│ 调用: vkCmdDrawIndexedIndirectCount(...)                        │
└─────────────────────────────────────────────────────────────────┘
```

### 5.4 两阶段遮挡剔除

```
Phase 1: 上帧 HZB → Instance Cull + Meshlet LOD/Cull → Draw 已知可见 meshlets
         ↓ 产生当前帧 Depth → 生成当前帧 HZB
Phase 2: 当前帧 HZB → 补充 Cull Phase 1 存疑的 meshlets → 补充 Draw
```

> 当相机快速移动或新对象入场时，Phase 1 可能错误遮挡实际可见的 meshlet，Phase 2 用当前帧真实深度纠正。

### 5.5 LOD QualityTier 联动

```
QualityTierFactor: Lowest=16.0, Low=4.0, Med=2.0, High=1.0, Ultra=0.5, Cinematic=0.25
threshold = BASE_ERROR_THRESHOLD × QualityTierFactor[tier] / max(importance, 0.1)
```

- importance=1.0 (主角) → threshold 最低 → 几何最精细
- 低 tier → threshold 大 → 更早使用粗 LOD meshlet
- Meshlet LOD 与 Material LOD (EffectiveTier) 联动——几何已降到最粗时材质也应降级

### 5.6 低端回退策略

| 平台后端 | 档位 | 几何获取 | 裁剪方式 | LOD | DrawCall |
|---------|------|---------|---------|-----|---------|
| PC / Apple | High+ | SSBO | GPU Meshlet Cull | Meshlet DAG (GPU) | IndirectCount |
| PC / Apple | Low~Med | SSBO | CPU Frustum | 离散 LOD | DrawIndexed |
| Android High | High | SSBO | GPU Meshlet Cull | Meshlet DAG (GPU) | IndirectCount |
| Android Mid | Medium | VBO | CPU Frustum | 离散 LOD | DrawIndexed + Instancing |
| Android Low | Low | VBO | CPU Frustum | 离散 LOD (2级) | DrawIndexed |

> 离散 LOD mesh 从 meshlet DAG 各级别"拍平"导出，无需维护两套 LOD 数据。

### 5.7 Mesh Shader 可选加速 (VK_EXT_mesh_shader)

```
传统路径: Compute Cull → IndirectDrawBuffer → vkCmdDrawIndexedIndirectCount → VS/FS
Mesh Shader: Task Shader (替代 Compute Cull) → Mesh Shader (替代 VS) → FS
```

- 省去 IndirectDrawBuffer 读写
- Task → Mesh 在 GPU 内部流转
- 非必须，作为 PC 可选加速路径

### 5.8 离线预处理流程

```
原始 Mesh → meshopt_buildMeshlets (LOD0)
  → 递归 k-means 聚类 + meshopt_simplify → LOD1, LOD2, ...
  → 标记 MeshletFlags (TERRAIN_CONTACT / ALPHA_TEST / TWO_SIDED / WIND_ANIM)
  → 打包输出 .meshlet 二进制 (MeshletBuffer + VertexBuffer + IndexBuffer)
```

---

## 6. 阴影系统

### 6.1 架构总览

所有阴影来源最终合成到 **ShadowMask RT (RGBA8)**：

| 通道 | 内容 | 来源 |
|------|------|------|
| **R** | 主平行光 Shadow (Near Dynamic + Far Cached) | Shadow Map |
| **G** | 胶囊阴影 / Blob 阴影 | Capsule/Blob Shadow |
| **B** | Contact Shadow | 屏幕空间 ray-march |
| **A** | 预留 (点光源等) | — |

Forward Lit / VBuffer Resolve 中仅需一次 ShadowMask 采样：

```glsl
vec4 sm = texture(ShadowMaskRT, screenUV);
float finalShadow = min(sm.r, min(sm.g, sm.b));
litColor *= finalShadow;
```

### 6.2 双层 Shadow Map

```
┌──────── Layer 0: Near Dynamic Cascade (全动态) ─────────┐
│  覆盖: 相机周围 R_near (~50m, 可调)                      │
│  更新: 每帧全量渲染 (动态+静态 shadowcaster)              │
│  分辨率: PC 2048², Android High 1024², Android Mid 512²  │
│  滤波: PCSS (High+) / PCF (Medium)                       │
├──────── Layer 1: Far Cached Cascade (环形滚动缓存) ──────┤
│  覆盖: R_near ~ R_far (~200m)                            │
│  更新: 仅相机移动超过 tile 步长时渲染新露出 tile strip     │
│  对象: 仅静态 shadowcaster                                │
│  分辨率: PC 4096², Android High 2048²                     │
│  寻址: Toroidal — UV 取模环形复用, 不搬移纹理内容         │
│  相机不动 → 零渲染开销                                    │
└──────────────────────────────────────────────────────────┘
```

**ShadowMask Compose 中双层合并**：

```glsl
float nearShadow = SampleDynamicCascadeShadow(worldPos, depth);   // Layer 0
float farShadow  = SampleCachedShadow(worldPos, lightSpaceDepth); // Layer 1
float t = smoothstep(R_near * 0.85, R_near, distFromCamera);
shadowMask.r = mix(nearShadow, farShadow, t);
```

### 6.3 Capsule Shadow / Blob Shadow

| 类型 | 原理 | 开销 |
|------|------|------|
| **Capsule Shadow** | 胶囊体 (head+body+legs) 解析遮挡计算 | 极低，per-pixel 纯数学 |
| **Blob Shadow** | 地面贴圆形/椭圆形衰减纹理 | 极低，一个 quad draw |

- 适用：手机近景动态角色、远处小型 NPC
- 结果写入 ShadowMask.g

### 6.4 Contact Shadow (High+)

屏幕空间沿光源方向 ray-march（16 步），检测前方深度遮挡，为小尺度接触处提供高频阴影细节。
结果写入 ShadowMask.b。

### 6.5 各平台阴影策略

| 平台 / 档位 | Near SM | Far Cached SM | Capsule | Contact | ShadowMask |
|------------|---------|---------------|---------|---------|------------|
| PC Cinematic | 4096² PCSS | 4096² Cached | 可选 | ✅ | RGBA8 |
| PC High/Ultra | 2048² PCSS | 4096² Cached | 可选 | ✅ | RGBA8 |
| PC Medium | 1024² PCF | 2048² Cached | ❌ | ❌ | RG8 |
| PC Low | 512² PCF 或 ❌ | 1024² 可选 | Blob | ❌ | R8 |
| Apple High/Ultra | 1024² PCSS | 2048² Cached | 可选 | ✅ | RGBA8 |
| Android High | 1024² PCF | 2048² Cached | ✅ | ❌ | RG8 |
| Android Mid | ❌/512² | 1024² Cached | ✅ (主阴影) | ❌ | RG8 |
| Android Low | ❌ | ❌ | Blob 可选 | ❌ | R8 或 ❌ |

### 6.6 可调参数

```cpp
struct ShadowConfig
{
    // Near Dynamic Cascade
    bool     enableNearCascade;           // default: true
    float    nearCascadeRadius;           // default: 50.0m
    uint32_t nearCascadeResolution;       // 512/1024/2048
    ShadowFilterMode nearFilter;          // PCF / PCSS

    // Far Cached Cascade
    bool     enableFarCached;             // default: true
    float    farCascadeRadius;            // default: 200.0m
    uint32_t farCascadeResolution;        // 1024/2048/4096
    uint32_t tileCount;                   // default: 16

    // Capsule / Blob
    bool     enableCapsuleShadow;         // Android=true
    float    capsuleMaxDistance;           // 30m
    uint32_t maxCapsuleCount;             // 8

    // Contact
    bool     enableContactShadow;         // High+=true
    int      contactShadowSteps;          // 16
    float    contactShadowLength;         // 0.1

    // 混合
    float    cascadeBlendWidth;           // 0.15 (占 nearRadius 比例)
};
```

---

## 7. 光照管线

### 7.1 统一光照入口

所有 SurfaceType 共用 `EvalLighting()` 函数，按 `QUALITY_TIER` 编译期选择路径：

| QualityTier | 直接光照 | 环境光 | 多光源 |
|-------------|---------|--------|--------|
| **Lowest** | 顶点 NdotL | 固定常量色 | 1 光源 |
| **Low** | Half-Lambert + BlinnPhong | 指数天空色 | ≤4 灯循环 |
| **Medium** | Cook-Torrance BRDF | FakeAtmosphere | ≤16 灯循环 |
| **High+** | Cook-Torrance BRDF | IBL (CubeMap) + SSAO | Clustered Shading |

### 7.2 Clustered Shading (High+)

```
视锥 3D 分割:
  X × Y: tileCountX × tileCountY (64px tile)
  Z: 24 slices (指数分布)
  → ~12,240 clusters @1080p

Compute Pass: Light Assignment
  · 每灯源 bounding sphere/cone vs cluster AABB 求交
  · 写入 ClusterLightList SSBO

Forward Lit / VBuffer Resolve:
  clusterIdx = GetClusterIndex(screenUV, linearDepth)
  for each light in ClusterLightList[clusterIdx]:
      litColor += EvalPointLight(...)
```

### 7.3 环境光模型

| 模型 | 档位 | 实现 |
|------|------|------|
| Constant | Lowest | 固定颜色 |
| Simple | Low | 指数梯度天空色 |
| FakeAtmosphere | Medium | 地平线暖色 + 大气散射 |
| IBL | High+ | Irradiance CubeMap + Prefiltered CubeMap + BRDF LUT (纹理法/函数近似法双模式) |

### 7.4 BRDF LUT 双模式

| 模式 | #define | 实现 | 适用 |
|------|---------|------|------|
| 纹理法 | `BRDF_LUT_TEXTURE 1` | 512×512 RG16F 预计算 LUT | PC / Apple (默认) |
| 函数近似法 | `BRDF_LUT_TEXTURE 0` | Karis 2014 多项式拟合 (误差 <0.5%) | 移动端 / 极简管线 |

### 7.5 Material LOD (EffectiveTier)

```
EffectiveTier = min(deviceTier, objectLODTier, surfaceLODCap)
```

- `deviceTier`: 设备硬件能力上限
- `objectLODTier`: 屏幕面积 + 距离 + 重要性偏移 per-object 计算
- `surfaceLODCap`: SurfaceType 特有降级阈值

> 同一帧内不同物体可使用不同 EffectiveTier，远处 NPC 不跑完整 SSS / Eye Refraction。
> 渲染排序按 (SurfaceType, EffectiveTier, PassType) 分组以减少 Pipeline 切换。

---

## 8. 后处理管线

### 8.1 处理链顺序

```
LitColor RT (RGBA16F, HDR)
  ├→ SSR (High+): Hi-Z Ray March → 混合反射
  ├→ Auto Exposure: 亮度直方图 → 平均亮度 → exposure_value
  ├→ TAA (Medium+): Motion Vector + History 混合
  ├→ Bloom (Medium+): 高亮提取 → 分级模糊 → 叠加
  ├→ DOF (High+, 可选): CoC 计算 → 散景模糊
  ├→ Motion Blur (Ultra, 可选): per-pixel 运动模糊
  ├→ ToneMapping: exposure × litColor → ACES/Filmic → LDR
  ├→ Color Grading / LUT (Medium+): 3D LUT 颜色校正
  └→ FXAA / CAS → SwapChain
```

### 8.2 各效果与档位关系

| 效果 | Lowest | Low | Medium | High | Ultra | Cinematic |
|------|--------|-----|--------|------|-------|-----------|
| ToneMapping | ✅ 简化 | ✅ | ✅ | ✅ | ✅ | ✅ |
| FXAA | ✅ | ✅ | fallback | fallback | fallback | fallback |
| TAA | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ |
| Bloom | ❌ | ❌ | ✅ 简易 | ✅ 多级 | ✅ | ✅ 高精度 |
| Auto Exposure | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ |
| Fog | ❌ | ✅ 简易 | ✅ | ✅ | ✅ | ✅ |
| SSAO | ❌ | ❌ | ✅ 半分辨率 | ✅ 全分辨率 | ✅ SSDO | ✅ SSDO 高采样 |
| SSR | ❌ | ❌ | ❌ | ✅ Hi-Z | ✅ | ✅ 高精度 |
| CAS (Sharpening) | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |
| DOF | ❌ | ❌ | ❌ | ✅ 可选 | ✅ | ✅ 高精度 |
| Motion Blur | ❌ | ❌ | ❌ | ❌ | ✅ 可选 | ✅ |
| Color Grading | ❌ | ❌ | ✅ 预设 | ✅ 自定义 | ✅ | ✅ |

### 8.3 TAA 资源

| 资源 | 格式 | 用途 |
|------|------|------|
| Motion Vector RT | RG16F | 每像素运动向量 |
| History LitColor | RGBA16F | 上一帧 LitColor (双缓冲 ping-pong) |
| Jitter Offset | CPU uniform | 每帧 sub-pixel Halton 抖动 |

> VS 注入 jitter: `gl_Position.xy += jitter * gl_Position.w`
> TAA Resolve: motion vector 采样 history → 邻域 clamp → 指数混合

### 8.4 Fog

Fog 不是独立 post-process pass，而是在 Forward Lit FS / VBuffer Resolve 内联计算：

```glsl
float fogFactor = CalcFogFactor(worldPos, cameraPos, fogParams);  // Exp/Exp2/Height
vec3 finalColor = mix(litColor, fogParams.fog_color, fogFactor);
```

---

## 9. GPU 资源总览

### 9.1 Render Targets

| RT | 格式 | 分辨率 | 用途 | 生命周期 |
|-----|------|--------|------|---------|
| Depth RT | D32_SFLOAT (D24 fallback) | full | 主深度缓冲 (Reversed-Z) | 帧内常驻 |
| LitColor RT | RGBA16F | full | 光照后颜色 (HDR) | 帧内常驻 |
| Motion Vector RT | RG16F | full | TAA 运动向量 | Medium+ |
| ShadowMask RT | RGBA8 | full | 统一阴影合成 | 帧内常驻 |
| SSAO RT | R8 | full/half | 环境遮蔽 | Medium+ |
| SSR RT | RGBA16F | full/half | 屏幕空间反射 | High+ |
| VBuffer RT | R32G32UI | full | VBuffer ID | VBuffer 路径 only |
| HZB Pyramid | R32F mipchain | ~1.33× full | 遮挡剔除 + SSR | High+ |
| NearSM Depth | D32_SFLOAT | 512²~4096² | 近景动态 ShadowMap | per light |
| CachedSM Depth | D32_SFLOAT | 1024²~4096² | 远景缓存 ShadowMap | 常驻 |
| History LitColor | RGBA16F | full | TAA 历史帧 (ping-pong) | Medium+ |

### 9.2 GPU Buffers

| 资源 | 类型 | 大小 | 用途 |
|------|------|------|------|
| InstanceBuffer | SSBO | N_inst × 128B | 全场景实例 (transform, bounds, flags) |
| MeshletBuffer | SSBO | N_meshlet × 64B | 全场景 meshlet 元数据 |
| MeshletVertexBuffer | SSBO | 变长 | 所有 meshlet 顶点数据 (共享池) |
| MeshletIndexBuffer | SSBO | 变长 | 所有 meshlet 索引 (共享池) |
| IndirectDrawBuffer | SSBO | N_meshlet × 20B | Meshlet Indirect Draw Commands |
| DrawCount | SSBO | 4B | Indirect Draw 计数 |
| InstanceVisData | SSBO | N_inst × 16B | Instance Cull 输出 |
| LightBuffer | SSBO | maxLights × 48B | 场景灯光参数 |
| ClusterLightList | SSBO | clusters × index_list | 每 cluster 灯光索引 |
| ClusterAABB | SSBO | clusters × 32B | Cluster 空间 AABB (预计算) |
| LocalToWorld SSBO | SSBO | N × mat4 | 全场景 L2W 矩阵池（Camera-Relative：平移列已减去相机世界坐标） |
| MI Data SSBO | SSBO | M × MI_size | 全场景 MaterialInstance 数据池 |
| CapsuleShadowData | SSBO | maxCount × 48B | 胶囊阴影参数 |
| VertexDataBuffer | SSBO | 变长 | 全场景顶点数据池 (SSBO 平台) |
| IndexDataBuffer | SSBO | 变长 | 全场景索引数据池 (SSBO 平台) |

### 9.3 VBuffer Tile Classification 专用资源

| 资源 | 格式 | 大小 | 用途 |
|------|------|------|------|
| TileSurfaceMask | R32UI image | tileX × tileY | SurfaceType bitmask |
| TileMaterialKeys | SSBO (uint[4]) | maxTile × 16B | 每 tile 至多 4 个 MaterialKey |
| TileMaterialCount | R8UI image | tileX × tileY | 唯一 MaterialKey 计数 |
| TileList_Single | SSBO (uvec2[]) | maxTile × 8B | 单一材质 tile 列表 |
| TileList_Fused | SSBO (uvec4[]) | maxTile × 16B | 融合材质 tile + fusedShaderID |
| TileList_Multi | SSBO (uvec2[]) | maxTile × 8B | 多材质 tile 列表 |
| FusedComboLUT | SSBO | comboCount × 20B | 融合材质组合 → shaderID 映射 |
| TileDispatchArgs | SSBO | 72B | 3 组 indirect dispatch 参数 + 计数 |

> 1080p@8×8 tile ≈ 32,400 tiles, 总计内存 < 1 MB。

---

## 10. Descriptor Set 布局

全局固定 4 个 Descriptor Set，所有材质共用同一布局：

### Set 0 — Global (每帧一次绑定)

| binding | 内容 | 类型 |
|---------|------|------|
| 0 | ViewportInfo | UBO |
| 1 | CameraInfo | UBO | 含 `cameraPosWorld`（double 精度原始位置，供 CPU 侧使用）；GPU 侧 `cameraPos` 恒为 `(0,0,0)`（Camera-Relative 坐标系原点） |
| 2 | SkyInfo | UBO |
| 3 | LightBuffer | SSBO |

### Set 1 — PerObject (每 DrawCall)

| binding | 内容 | 类型 |
|---------|------|------|
| 0 | LocalToWorld SSBO (mat4 池, TransformID 索引；**已减去相机世界坐标**) | SSBO |

### Set 2 — PerMaterial (Standard/Special Surface)

| binding | 内容 | 类型 | 备注 |
|---------|------|------|------|
| 0 | MI SSBO (MaterialInstanceID 索引) | SSBO | |
| 1-6 | 纹理槽 (Albedo, Normal, MR, AO, Emissive, DetailNormal) | sampler2D / sampler2DArray | 按 QualityTier 裁剪 |
| 7-12 | Special Surface 扩展纹理 | sampler2D | Skin/Hair/ClearCoat 等 |

> **Terrain 专用布局**：SurfaceType == Terrain 时整个 Set 2 替换为 Terrain 专用 (TerrainGlobalUBO + LayerSSBO + AlbedoArray + NormalArray + MRArray + SplatMap + HeightMap + NormalMapGlobal)。

### Set 3 — Environment (全局光照 + 管线 RT)

| binding | 内容 | 类型 | 条件 |
|---------|------|------|------|
| 0 | ColorPalette | UBO | |
| 1 | ShadowMap_Near | sampler2DShadow | |
| 2 | ShadowMask RT | sampler2D | |
| 3 | SSAO RT | sampler2D | |
| 4 | IBL_Irradiance | samplerCube | |
| 5 | IBL_Prefiltered | samplerCube | |
| 6 | IBL_BRDF_LUT | sampler2D | 仅纹理法模式 |
| 7 | SSS_LUT | sampler2D | Skin |
| 8 | DebugLightingConfig | UBO | Debug only |
| 9 | HZB_Pyramid | sampler2D | High+ |
| 10 | ClusterLightList | SSBO | High+ |
| 11 | ClusterAABB | SSBO | High+ |
| 12 | FogParams | UBO | |
| 13 | SSR RT | sampler2D | High+ |
| 14 | ExposureData | UBO/SSBO | Med+ |
| 15 | MeshletBuffer | SSBO | SSBO 平台 |
| 16 | InstanceBuffer | SSBO | SSBO 平台 |
| 17 | TerrainHeightMap | sampler2D | Terrain contact |
| 18 | VertexDataBuffer | SSBO | SSBO 平台 |
| 19 | IndexDataBuffer | SSBO | SSBO 平台 |
| 20 | ShadowMap_Cached | sampler2DShadow | |
| 21 | CapsuleShadowData | SSBO | |

> Binding 15-19 仅 SSBO 平台 (PC/Apple/Android High)。VBO 平台使用独立 Pipeline Layout。

---

## 11. 平台后端与特性分级

### 11.1 平台后端

```cpp
enum class PlatformBackend : uint8_t { PC = 0, Apple = 1, Android = 2 };
enum class GeometryFetchMode : uint8_t { SSBO = 0, VBO = 1 };
```

### 11.2 平台 × 几何后端

| 平台 | 画质范围 | GeometryFetchMode | 顶点获取 |
|------|---------|-------------------|---------|
| **PC** | Lowest~Cinematic | SSBO | VS 内 SSBO fetch |
| **Apple** | Lowest~Ultra | SSBO | 同上 |
| **Android High** | Medium~High | SSBO | 同上 |
| **Android Mid** | Low~Medium | VBO | 传统 vertex attribute |
| **Android Low** | Lowest~Low | VBO | 传统 vertex attribute |

### 11.3 SSBO vs VBO 路径

| 维度 | SSBO 路径 | VBO 路径 |
|------|-----------|---------|
| Pipeline VertexInput | 空 (无 vertex binding) | 声明 VkVertexInputBindingDescription |
| 顶点获取 | `FetchVertex(gl_VertexIndex)` 从 SSBO 手动读取 | `layout(location=N) in` 硬件自动填充 |
| ID 传递 | ID SSBO + `gl_InstanceIndex` | Instance-rate R16UI VBO |
| Indirect Draw 兼容 | ★★★★★ 天然兼容 | ★★★ 需确保 VAB 连续性 |

### 11.4 Instance ID 分发架构

无论 SSBO/VBO 路径，L2W 矩阵和 MI 数据始终存储在 SSBO 中。只有 **ID 本身的传递方式** 按平台分流：

| 平台 | ID 传递 | 理由 |
|------|---------|------|
| PC / Apple / PowerVR | ID SSBO + `gl_InstanceIndex` | SSBO 在桌面/Apple Silicon 上极高效 |
| Android Mid/Low | Instance-rate R16UI VBO | 移动 GPU vertex fetch 硬件有专用缓存 |

> 每实例仅 4 字节 (TransformID + MaterialInstanceID) 即可分发完整 L2W 矩阵 + MI 数据。
>
> **Camera-Relative 约定**：L2W 矩阵在 CPU 端上传前，其平移列 `(m[3][0], m[3][1], m[3][2])` 已被减去
> `cameraWorldPosition`，因此 VS 输出的 `worldPos` 实际是相机相对坐标。GPU 侧 `cameraPos` 恒为 `vec3(0)`。

### 11.5 Android 特性裁剪矩阵

| 特性 | Android Low | Android Mid | Android High |
|------|------------|------------|-------------|
| 几何获取 | VBO | VBO | SSBO |
| 渲染路径 | Forward Only | Forward Only | Forward + VBuffer |
| LOD | 离散 (CPU) | 离散 (CPU) | Meshlet DAG (GPU) |
| 裁剪 | CPU Frustum | CPU Frustum | GPU Meshlet Cull |
| HZB | ❌ | ❌ | ✅ |
| Clustered | ❌ (≤4灯) | ❌ (≤8灯) | ✅ |
| Near SM | ❌ | ❌/512² | 1024² PCF |
| Cached SM | ❌ | 1024² static | 2048² static |
| Capsule | Blob 可选 | ✅ 主阴影 | ✅ |
| TAA | ❌ | ✅ | ✅ |
| Bloom | ❌ | ✅ 简易 | ✅ 多级 |
| Decals | ❌ | ❌ | ✅ |
| 最大纹理 | 1024 | 2048 | 4096 |
| 渲染分辨率 | 0.5×~0.75× | 0.75×~1.0× | 1.0× |

### 11.6 编译策略

Shader 变体按 **PlatformBackend × QualityTier × SurfaceType × Flags** 四维组合编译，通过裁剪规则控制变体数：

```
PC:           SSBO only, Lowest~Cinematic (6 tiers)
Apple:        SSBO only, Lowest~Ultra (5 tiers, 无 Cinematic)
Android High: SSBO, Medium/High
Android Mid:  VBO, Low/Medium
Android Low:  VBO, Lowest/Low; FEATURE_TERRAIN_CONTACT_DITHER=0 不编译
```

编译期 `#define` 注入：`PLATFORM_*`, `GEOMETRY_FETCH_*`, `QUALITY_*`, `FEATURE_*`

---

## 12. Reversed-Z 与无限远平面

### 12.1 核心配置

| 参数 | 值 |
|------|-----|
| 近平面 depth | 1.0 |
| 远平面 depth | 0.0 |
| 远平面距离 | +∞ (Infinite Far Plane) |
| 深度格式 | D32_SFLOAT (优先), D24_UNORM_S8_UINT (fallback) |
| DepthCompareOp | GREATER (非 LESS) |
| Clear Depth | 0.0 (非 1.0) |

### 12.2 全管线影响

| 模块 | Reversed-Z 行为 |
|------|-----------------|
| VkPipelineDepthStencilState | depthCompareOp = **GREATER** |
| Clear Depth | **0.0** |
| HZB Downsample | **min** = 最远 |
| HZB Occlusion Test | meshlet depth **>** hzb → visible |
| 线性化深度 | `linearZ = near / z` (无限远简化) |
| 天空 Pass | depth = **0.0** (最远) |
| SSR Hi-Z Trace | step compare **>** |
| ShadowMap | 独立投影，推荐 Reversed-Z for consistency |

### 12.3 投影矩阵

```cpp
mat4 MakeInfiniteReversedZProj(float fov, float aspect, float near)
{
    float f = 1.0f / tan(fov * 0.5f);
    return mat4(
        f / aspect, 0,  0,     0,
        0,         -f,  0,     0,    // -f: Vulkan Y flip
        0,          0,  0,    -1,    // perspective divide: w = -z
        0,          0,  near,  0     // z → near/z (near→1.0, ∞→0.0)
    );
}
```

### 12.4 深度工具函数

```glsl
float LinearizeDepth(float d)           { return ULRE_NEAR_PLANE / d; }

// 返回 camera-relative world position（非绝对世界坐标）
vec3  ReconstructWorldPos(vec2 ndc, float depth) {
    vec4 clip = vec4(ndc, depth, 1.0);
    vec4 view = ULRE_INV_PROJ * clip;
    view /= view.w;
    return (ULRE_INV_VIEW * view).xyz;   // camera-relative
}

// 需要绝对世界坐标时：worldPosAbsolute = ReconstructWorldPos(...) + cameraPosWorld
```

---

## 13. Camera-Relative Rendering（相机相对渲染）

Camera-Relative Rendering (CRR) 是支撑**无限大世界**的核心坐标空间策略：将相机世界坐标从所有 L2W 矩阵中减去后再上传 GPU，使 GPU 侧始终在以相机为原点的坐标系下计算，彻底消除远离世界原点时 float32 的精度抖动。

### 13.1 核心原理

| 概念 | 说明 |
|---|---|
| **传统方式** | GPU 中 `worldPos = L2W * localPos`，当物体距世界原点数万米时 `worldPos` 的 float32 尾数不足，产生顶点抖动 / Z-fighting |
| **CRR 方式** | CPU 端：`L2W_relative = L2W; L2W_relative[3].xyz -= cameraWorldPosition;`<br>GPU 端：`worldPos_relative = L2W_relative * localPos`，结果始终在相机附近，float32 精度充足 |
| **相机位置** | GPU 侧 `cameraPos ≡ vec3(0,0,0)`（Camera-Relative 原点），需要绝对世界坐标时用 CPU 端存储的 `cameraPosWorld`（double 精度） |

> **One-liner**：`L2W[3].xyz -= cameraPos` on CPU → 所有 GPU 坐标自动变为 camera-relative。

### 13.2 CPU 端实现

```
// 伪代码：PerFrame Upload
void UploadTransforms(const Camera& cam, span<mat4> l2wPool)
{
    dvec3 camWorldPos = cam.GetWorldPositionDouble();   // double 精度

    for (auto& l2w : l2wPool) {
        // 仅修改平移列（第 3 列）
        l2w[3][0] -= float(camWorldPos.x);
        l2w[3][1] -= float(camWorldPos.y);
        l2w[3][2] -= float(camWorldPos.z);
    }

    ssboL2W.Upload(l2wPool);
}
```

**要点：**
- 原始 L2W 在 CPU 端保持 double 精度（或 float + 分区偏移），减法用 double 执行后转 float 上传
- 每帧重新计算；Camera 移动后 L2W SSBO 整体刷新

### 13.3 CameraUBO 变化

| 字段 | CRR 之前 | CRR 之后 |
|---|---|---|
| `mat4 view` | `lookAt(cameraPos, target, up)` | `lookAt(vec3(0), target - cameraPos, up)`（相机平移归零） |
| `mat4 proj` | 不变 | 不变 |
| `mat4 viewProj` | `proj * view` | `proj * view_relative` |
| `vec3 cameraPos` | 世界坐标 | **恒为 `vec3(0,0,0)`** |
| `vec3 cameraPosWorld` **(新增)** | — | 供 CPU 回读 / shader 中需要绝对坐标时使用<br>（如 Fog 距离、Terrain LOD、大地图坐标） |

> 如果需要更高精度，可将 `cameraPosWorld` 拆为 `highPart + lowPart`（双 float 编码）。

### 13.4 Shader 影响总览

| 模块 / Shader | 影响 | 处理方式 |
|---|---|---|
| **Compositor VS** | `worldPos = L2W * localPos` 已是 camera-relative | 无需改动 |
| **Compositor FS — viewDir** | `viewDir = normalize(cameraPos - worldPos)` | 简化为 `viewDir = normalize(-worldPos)`（因 `cameraPos == 0`） |
| **depth_utils.glsl** | `ReconstructWorldPos()` | 返回 camera-relative position（见 §12.4） |
| **Shadow Mapping** | 光源 VP 矩阵 | CPU 端同样将光源 View 矩阵的平移列减去 `cameraWorldPosition`，保持一致坐标空间 |
| **Fog / Atmosphere** | 基于绝对世界高度/距离 | `absoluteWorldPos = worldPos + cameraPosWorld` |
| **Terrain / Decal** | 基于绝对世界 XZ 坐标 | 同上 |
| **VBuffer Resolve** | Tile Shader 中 `ReconstructWorldPos()` | 返回 camera-relative，后续光照计算不受影响 |

### 13.5 与 Reversed-Z 的协同

CRR 与 Reversed-Z (§12) 是正交的两个精度优化维度：

| 维度 | 关注点 | 解决方案 |
|---|---|---|
| **深度精度** | 远平面附近 Z-buffer 分辨率不足 | Reversed-Z + Infinite Far Plane |
| **坐标精度** | 远离世界原点的顶点 float32 抖动 | Camera-Relative Rendering |

两者同时启用，可在不改变 Shader 核心逻辑的前提下获得双重精度提升。

### 13.6 注意事项

1. **双精度保留**：CPU 端 Camera 位置必须使用 `double` 存储；SubWorld 分区坐标 + double 偏移可覆盖 $> 10^{15}$ m 范围
2. **PerFrame 更新**：Camera 移动后 L2W SSBO **整体重新上传**（或增量标脏+partial update）
3. **物理 / 逻辑层**：物理系统仍在绝对世界坐标中运算，仅**渲染上传阶段**执行减法
4. **Debug 渲染**：Debug line / gizmo 的顶点也需减去 cameraPosWorld

---

## 14. Shader Compositor 与编译管线

### 14.1 架构概述

Shader 分为两层：

| 层 | 职责 | 维护方 |
|---|---|---|
| **Surface Function** | 纯材质计算：纹理采样、法线映射、材质属性输出 | 每 SurfaceType 一份 |
| **Compositor Template** | 提供 `main()`：构造 SurfaceInput、调用 Surface Function、执行光照/阴影/雾 | 引擎统一，按 PassType 分 |

Surface Function **不做**的事：不写 `main()`、不计算光照、不采样阴影、不写 RT 输出、不执行 discard。

### 14.2 PassType → Compositor 模板映射

| PassType | 模板 | 行为 |
|----------|------|------|
| FORWARD_OPAQUE | main_forward_opaque.frag | EvalSurface → EvalLighting → Shadow → Fog → FragColor |
| FORWARD_MASKED | main_forward_masked.frag | + alpha test discard |
| FORWARD_TRANSPARENT | main_forward_transparent.frag | + alpha blend output |
| FORWARD_DITHER | main_forward_dither.frag | Bayer dither → discard → lighting |
| FORWARD_A2C | main_forward_a2c.frag | Alpha-to-Coverage 硬件混合 |
| SHADOW_OPAQUE | 仅 VS, 无 FS | 纯深度输出 |
| SHADOW_MASKED | main_shadow_masked.frag | EvalAlpha → discard (无光照) |
| VBUFFER_ID | main_vbuffer_id.frag | 写 ID + MeshletIndex (不调用 Surface Function) |
| VBUFFER_RESOLVE | main_vbuffer_resolve.comp | Compute: 读 VBuffer → EvalSurface → EvalLighting |

### 14.3 自动变体生成

BlendMode 决定需要生成的 PassType：

| BlendMode | 自动生成 |
|-----------|---------|
| Opaque | FORWARD_OPAQUE + SHADOW_OPAQUE + VBUFFER_ID |
| Masked | FORWARD_MASKED + SHADOW_MASKED + VBUFFER_ID |
| Transparent | FORWARD_TRANSPARENT |
| Dither | FORWARD_DITHER + SHADOW_MASKED + VBUFFER_ID |
| A2C | FORWARD_A2C + SHADOW_MASKED + VBUFFER_ID |

### 14.4 构建期编译流程

```
for each preset:
  for each pass_type in GetRequiredPassTypes(blend_mode):
    for each quality_tier in [min_tier .. max_tier]:
      for each shadow_mode:
        defines = BuildDefines(...)
        root_shader = #defines + #include "surface/xxx.glsl" + #include "compositor/main_xxx.glsl"
        spv = glslangValidator(root_shader)
        SPVCache.Store(preset_id, tier, shadow, pass_type, spv)
```

### 14.5 SPV 部署阶段与分发策略

**Shader/SPV 生成仅在渲染器开发阶段实时执行。** 游戏编辑器和游戏运行时均不包含 GLSL 源码或编译器，
只使用离线预编译的 SPV 二进制包。三个部署阶段如下：

| 阶段 | GLSL 生成 | SPV 编译 | SPV 来源 |
|------|-----------|----------|----------|
| 渲染器开发 | ✅ 实时 | ✅ 实时 | 内存 / 本地缓存 |
| 游戏编辑器 | ❌ | ❌ | 离线 SPV 包（资产管线自动构建） |
| 游戏运行时 | ❌ | ❌ | 分发或按需下载的 SPV 包 |

**SPV 分发打包**：构建服务器按 `PlatformBackend × QualityTier` 独立生成 SPV 包。
游戏客户端根据目标平台和设备检测结果，仅安装或下载匹配档位的 SPV 包。
运行时通过 `SPVCache.LoadFromFile()` 加载后纯查表使用。

### 14.6 运行时 Pipeline 查询

```cpp
QualityTier effectiveTier = min(deviceTier, CalcObjectLODTier(obj, cam));
auto [resolvedPreset, resolvedSurface] = ResolveSPVFallback(preset_id, surface_type, effectiveTier);
ShaderPermutationKey key = BuildPermutationKey(resolvedSurface, effectiveTier, backend);
auto [vs, fs] = SPVCache.Get(resolvedPreset, key, PassType::ForwardOpaque);
VkPipeline pipe = PipelineCache.GetOrCreate(resolvedPreset, key, passType, renderPass);
```

> SPV 回退：Skin@Medium/Low → 复用 Standard SPV, Eye@Low → 复用 Standard SPV, 减少 ~50% Special Surface 编译量。

### 14.6 Shader 目录结构

```
ShaderLibrary/
  common/     — 共享 include (surface_interface, lighting, shadow, fog, dither, depth_utils ...)
  compositor/ — Compositor main() 模板 (per PassType VS/FS)
  surface/    — Surface Function (standard, skin, hair, cloth, clearcoat ...)
  unlit/      — Unlit 材质 (独立 main(), 不走 Compositor)
  debug/      — Gizmo, UV checker, Overdraw heatmap, Depth vis, Tile complexity
  scene/      — Sky, Terrain, Decal
  vbuffer/    — Tile classify, Prepare args, Resolve (single/fused/multi)
  gpudrive/   — HZB downsample, Instance cull, Meshlet LOD select, Meshlet cull, Cluster light assign
  postprocess/— ShadowMask compose, SSAO, Auto exposure, SSR, TAA, Bloom, DOF, ToneMapping, FXAA ...
```

---

## 15. VBuffer Tile-Based Resolve 详细设计

### 15.1 VBuffer RT 编码

```
VBuffer RT: R32G32UI (2× uint32)
  x = SurfaceType(4bit) | PresetID(4bit) | InstanceID(16bit) | MeshletFlags(8bit)
  y = MeshletLocalTriID(8bit) | MeshletIndex(24bit)
```

### 15.2 Tile Classification

屏幕划分为 TILE_SIZE×TILE_SIZE（8×8）tile，统计每 tile 内出现的 MaterialKey（= SurfaceType×PresetID, 8-bit）集合。

四类输出：

| 类别 | 条件 | 典型占比 |
|------|------|---------|
| **Empty** | 全部像素 VBuffer == 0 | ~15-30% |
| **Single** | 仅 1 种 MaterialKey | ~50-75% (简单场景) |
| **Fused** | 2-4 种 MaterialKey + 匹配 FusedComboLUT | ~10-45% (高交错场景) |
| **Multi** | 2+ 种, 无匹配融合 | ~5-15% |

### 15.3 FusedComboLUT

CPU 侧场景加载时构建，上传为 SSBO。注册常见 MaterialKey 共现组合：

```cpp
struct FusedCombo {
    uint8_t materialKeys[4];  // 排序后, 未使用填 0xFF
    uint8_t keyCount;         // 2-4
    uint8_t fusedShaderID;
};
// 典型 4-16 条，由 Debug 统计 top-N 高频组合后手动或自动注册
```

### 15.4 3-Tier Dispatch

```
Dispatch 1 (Single): 零分支发散, SIMD 100%, ~50-75%
  → workgroup 内所有线程走同一 SurfaceType 分支

Dispatch 2 (Fused): 极低发散, SIMD ~90-100%, ~10-45% (森林/城镇)
  → 2-4 种已知材质, GPU 编译器 predication 优化
  → 同 SurfaceType 融合时甚至无需 if/else (MI 不同, 路径相同)

Dispatch 3 (Multi): per-pixel N-way switch, ~5-15%
  → 仅极端复杂交界区

Empty: 不在任何 list, 不 dispatch
```

### 15.5 CPU 端调度

```cpp
// Tile Classification
vkCmdBindPipeline(cmd, tileClassifyPipeline);
vkCmdDispatch(cmd, tileCountX, tileCountY, 1);
barrier();

// Prepare Indirect Args
vkCmdBindPipeline(cmd, tileArgsPipeline);
vkCmdDispatch(cmd, 1, 1, 1);
barrier();

// Dispatch 1: Single
vkCmdBindPipeline(cmd, resolveSinglePipeline);
vkCmdDispatchIndirect(cmd, args, offset_single);

// Dispatch 2: Fused
vkCmdBindPipeline(cmd, resolveFusedPipeline);
vkCmdDispatchIndirect(cmd, args, offset_fused);

// Dispatch 3: Multi
vkCmdBindPipeline(cmd, resolveMultiPipeline);
vkCmdDispatchIndirect(cmd, args, offset_multi);
```

### 15.6 调优工具 — Debug 可视化

```
Tile Classification Overlay:
  绿色 = Single, 蓝色 = Fused (标注 fusedID), 红色 = Multi (标注材质数), 灰色 = Empty
  热力图: KeyCount 1→绿 2→黄 3→橙 4+→红
```

---

## 16. 重构路线图

> 以下为建议的重构执行顺序，优先建立管线骨架，后续逐步填充特性。

### Phase 0: 基础设施

- [ ] Reversed-Z + Infinite Far Plane 投影矩阵
- [ ] D32_SFLOAT 深度缓冲 + DepthCompareOp = GREATER
- [ ] **Camera-Relative Rendering**：L2W 减去 cameraPos、CameraUBO 调整、depth_utils 适配
- [ ] Descriptor Set Layout 四套固定布局
- [ ] DeviceQualityProfile 自动检测 (PlatformBackend + GeometryFetchMode + QualityTier)

### Phase 1: Forward 路径骨架

- [ ] Early-Z Pre-Pass (Depth only)
- [ ] Forward Lit Pass (EvalSurface → EvalLighting → FragColor)
- [ ] Shader Compositor 架构 (Surface Function + Template 分离)
- [ ] Standard Surface Function (EvalSurface / EvalAlpha)
- [ ] 基础光照 (Low=BlinnPhong, Medium=Cook-Torrance, High=PBR+IBL)
- [ ] Sky Pass
- [ ] Translucent Pass (基础 alpha blend)

### Phase 2: 阴影系统

- [ ] Near Dynamic Cascade Shadow Map
- [ ] ShadowMask Compose (R 通道)
- [ ] Shadow Masked Pass (compositor/main_shadow_masked)
- [ ] Far Cached Cascade (Toroidal Scrolling)
- [ ] Capsule / Blob Shadow (G 通道)
- [ ] Contact Shadow (B 通道, High+)

### Phase 3: GPU-Driven Meshlet 管线

- [ ] 离线预处理 (meshopt_buildMeshlets + LOD DAG 构建)
- [ ] MeshletGPU 上传 → SSBO
- [ ] Instance Cull Compute Pass
- [ ] Meshlet LOD Select + Frustum/Cone Cull Compute Pass
- [ ] IndirectDrawBuffer → vkCmdDrawIndexedIndirectCount
- [ ] HZB Generation
- [ ] HZB Occlusion Cull (Phase 1 + Phase 2 两阶段)
- [ ] CPU 离散 LOD fallback (Low/Med 档位)

### Phase 4: 全局光照增强

- [ ] SSAO / SSDO
- [ ] Clustered Shading (light assignment + per-cluster iteration)
- [ ] IBL (Irradiance + Prefiltered + BRDF LUT 双模式)
- [ ] Fog inline

### Phase 5: VBuffer 路径

- [ ] VBuffer ID Pass (main_vbuffer_id)
- [ ] Tile Material Classification
- [ ] VBuffer Resolve Single
- [ ] VBuffer Resolve Multi
- [ ] FusedComboLUT + Fused Resolve Shader
- [ ] LitColor RT → 后处理衔接

### Phase 6: 后处理管线

- [ ] ToneMapping (ACES)
- [ ] Auto Exposure (Luminance Histogram)
- [ ] TAA (Motion Vector + History blend)
- [ ] Bloom (extract + multi-pass blur)
- [ ] FXAA / CAS
- [ ] SSR (Hi-Z Ray March)
- [ ] DOF, Motion Blur, Color Grading (High+/Ultra)

### Phase 7: 平台适配

- [ ] SSBO 顶点获取 (vertex_fetch_ssbo.glsl)
- [ ] VBO 顶点获取 (vertex_fetch_vbo.glsl)
- [ ] Instance ID 双路径 (SSBO vs VBO)
- [ ] Android 特性裁剪 (编译期 #define 门控)
- [ ] Shader 变体裁剪 (per-platform tier × feature 矩阵)
- [ ] D24 深度格式 fallback

### Phase 8: 特殊渲染

- [ ] Terrain-Contact Dither (可开关)
- [ ] Decal Pass (Screen-Space Decal)
- [ ] Outline / Selection Highlight
- [ ] Debug / Gizmo Overlay
- [ ] Mesh Shader 加速路径 (VK_EXT_mesh_shader, 可选)

---

## 附录 A: Pass 数据流依赖图

```
                        ┌──────────────┐
                        │ InstanceBuffer│
                        │ MeshletBuffer │
                        └──────┬───────┘
                               │
                    ┌──────────▼──────────┐
                    │ Pass 0: Meshlet Cull │
                    │       (Compute)      │
                    └──────────┬──────────┘
                               │ IndirectDrawBuffer
              ┌────────────────┼────────────────┐
              ▼                ▼                 ▼
     ┌────────────┐   ┌──────────────┐   ┌──────────────┐
     │ Pass 1:    │   │ Pass 2:      │   │ Pass 2:      │
     │ ShadowMap  │   │ Early-Z      │   │ VBuffer ID   │
     │ (离屏)     │   │ (Forward)    │   │ (VBuffer)    │
     └─────┬──────┘   └──────┬───────┘   └──────┬───────┘
           │                 │ Depth RT          │ VBuffer RT + Depth
           │          ┌──────▼───────┐    ┌──────▼───────┐
           │          │ Pass 2b: HZB │    │ Pass 2b: HZB │
           │          └──────┬───────┘    └──────┬───────┘
           │                 │                   │
           │          ┌──────▼───────┐    ┌──────▼───────┐
           │          │ Pass 2c:     │    │ Pass 3: Tile │
           │          │ Cull Phase 2 │    │ Classification│
           │          └──────┬───────┘    └──────┬───────┘
           │                 │                   │
     ┌─────▼─────────────────▼───────────────────▼──────────┐
     │ Pass 4: ShadowMask Compose (Near SM + Cached SM +     │
     │         Capsule + Contact)      → ShadowMask RT       │
     └─────────────────────┬────────────────────────────────┘
                           │
     ┌─────────────────────▼────────────────────────────────┐
     │ Pass 5: SSAO                    → SSAO RT             │
     └─────────────────────┬────────────────────────────────┘
                           │
              ┌────────────┼────────────────┐
              ▼ (Forward)                   ▼ (VBuffer)
     ┌────────────────┐           ┌─────────────────────┐
     │ Pass 6:        │           │ Pass 7: VBuffer     │
     │ Forward Lit    │           │ Resolve (3-Tier)    │
     │ → LitColor RT  │           │ → LitColor RT       │
     └───────┬────────┘           └─────────┬───────────┘
             │                              │
             └──────────┬───────────────────┘
                        ▼
              ┌─────────────────┐
              │ Pass 8-10:      │
              │ Sky + Translucent│
              │ + Decal + Debug  │
              └────────┬────────┘
                       ▼
              ┌─────────────────┐
              │ Pass 11:        │
              │ Post-Processing │
              │ Chain           │
              │ → SwapChain     │
              └─────────────────┘
```

---

## 附录 B: 关键枚举定义速查

```cpp
enum class PlatformBackend : uint8_t   { PC=0, Apple=1, Android=2 };
enum class GeometryFetchMode : uint8_t { SSBO=0, VBO=1 };
enum class QualityTier : uint8_t       { Lowest=0, Low=1, Medium=2, High=3, Ultra=4, Cinematic=5 };
enum class SurfaceType : uint8_t       { Unlit=0, Standard=1, Skin=2, Hair=3, Cloth=4,
                                          Eye=5, Foliage=6, ClearCoat=7, Water=8, Terrain=9, Sky=10 };
enum class BlendMode : uint8_t         { Opaque=0, Masked=1, Transparent=2, Dither=3, A2C=4 };
enum class PassType : uint8_t          { ForwardOpaque=0, ForwardMasked=1, ForwardTransparent=2,
                                          ForwardDither=3, ForwardA2C=4, ShadowOpaque=5,
                                          ShadowMasked=6, VBufferID=7, VBufferResolve=8,
                                          TerrainContactDither=9 };
enum class AmbientModel : uint8_t      { Constant=0, Simple=1, FakeAtmosphere=2, IBL=3 };
enum class ShadowFilterMode : uint8_t  { None=0, PCF=1, PCSS=2 };
enum class ShadowUpdateMode : uint8_t  { FullDynamic=0, CachedToroidal=1, Static=2 };
```

---

> **交叉引用**：
> - 材质预设目录、MaterialCategory 分类 → `SimplifiedMaterialSystem_Design.md` §5
> - ShaderPermutationKey 位域定义 → `SimplifiedMaterialSystem_Design.md` §4.2
> - Surface Complexity LOD 详细降级表 → `SimplifiedMaterialSystem_Design.md` §3.5
> - 顶点格式标准化 (Layout A~E) → `SimplifiedMaterialSystem_Design.md` §12
> - 实现路线图完整版 → `SimplifiedMaterialSystem_Design.md` §13, §14
