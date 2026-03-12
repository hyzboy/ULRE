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

> **平台路径约束**（§2.9）：Android Mid/Low (VBO 平台) 强制 Forward Only，
> 不支持 VBuffer 路径。VBuffer 路径仅在 SSBO 平台 (PC/Apple/Android High) 上可用。

### 2.4 完整渲染管线 Pass 顺序（程序关注）

> 美术不需要关心以下内容。这一节面向引擎程序员，描述每帧的完整 Pass 流程。

#### Forward 路径帧序列

```
┌────────────────────────────────────────────────────────────────────┐
│ 0. Meshlet Select & Cull (Compute, High+)               ← ★ 核心  │
│    - 输入: InstanceBuffer + MeshletBuffer + MeshletDAG + 上帧 HZB   │
│    - Pass A: Instance Cull (视锥 + 粗粒度 HZB 遮挡)                 │
│    - Pass B: Meshlet LOD Select + Cull (DAG 遍历):                 │
│      · LOD Cut: screenSpaceError vs threshold (受 QualityTier +     │
│        importance 调节)                                             │
│      · Frustum Cull (per-meshlet BoundingSphere)                   │
│      · Backface Cone Cull (法线锥全背向 → 剔除)                     │
│      · HZB Occlusion Cull (per-meshlet, 上帧 HZB)                  │
│    - 输出: IndirectDrawBuffer + DrawCount (meshlet 粒度)            │
│    - Lowest/Low/Medium 档位: CPU Frustum Cull + 离散 LOD → 传统 DrawCall│
├────────────────────────────────────────────────────────────────────┤
│ 1. ShadowMap Pass (per light, 离屏)                  ← ★ 双层架构  │
│    - Layer 0 — Near Dynamic Cascade:                               │
│      · 每帧全量渲染（动态+静态 shadowcaster）                        │
│      · 覆盖相机近景 R_near (~50m, 可调)                             │
│      · 手机可选: 仅渲染动态物体 / 或完全替换为 Capsule Shadow         │
│    - Layer 1 — Far Cached Cascade (Toroidal Scrolling):            │
│      · 仅渲染静态 shadowcaster                                      │
│      · 覆盖远景 R_near~R_far (~200m, ≈2× 屏幕范围)                 │
│      · 环形滚动: 相机移动超过 tile 步长时仅更新新露出的 tile strip     │
│      · 相机不动 → 零渲染开销                                        │
│    - Depth-Only VS, 极简 FS（Alpha-Test 时采样 Albedo.a discard）    │
│    - 输出: NearSM Depth + CachedSM Depth (per cascade/per light)   │
├────────────────────────────────────────────────────────────────────┤
│ 2. Early-Z Pre-Pass (Depth Pre-Pass)                               │
│    - 仅写 Depth Buffer，不写 Color                                  │
│    - VS 只做 MVP 变换，无 FS 或空 FS                                │
│    - Alpha-Test 物体使用简易 FS（采样 Albedo.a 做 discard）          │
│    - High+: vkCmdDrawIndexedIndirectCount (meshlet 级 Indirect)    │
│    - 输出: Depth RT (D32_SFLOAT 或 D24_UNORM_S8_UINT)             │
│    - 目的: 让后续 Forward Pass 受益于 Early-Z Rejection，减少 overdraw │
├────────────────────────────────────────────────────────────────────┤
│ 2b. HZB Generation (Compute, High+)                    ← ★ 新增   │
│    - Depth RT → 逐级 Downsample (每级取 min depth, Reversed-Z)     │
│    - 输出: HZB Pyramid (R32F, ~log2 级 mip chain)                  │
│    - 用途: Phase 2 遮挡补测 / SSR Hi-Z Trace / SSAO mip 采样       │
├────────────────────────────────────────────────────────────────────┤
│ 2c. Meshlet Cull Phase 2 (Compute, High+, 可选)        ← ★ 新增   │
│    - 上次标记为"存疑"的 meshlet 用当前帧 HZB 重新遮挡测试            │
│    - 补充 Draw 新出现的可见 meshlet → 追加写入 Depth RT              │
├────────────────────────────────────────────────────────────────────┤
│ 3. Clustered Light Assignment (Compute, High+)          ← ★ 新增  │
│    - 将视锥体分为 3D cluster grid (tile × depth slice)              │
│    - 每光源 vs cluster AABB 求交 → 写入 ClusterLightList SSBO       │
│    - Lowest/Low/Medium: 跳过此 Pass, Forward FS 直接循环少量灯光      │
├────────────────────────────────────────────────────────────────────┤
│ 4. ShadowMask Compose Pass (全屏 Compute / FS)       ← ★ 多来源   │
│    - 输入: Depth RT + NearSM + CachedSM + Camera/Light Matrices     │
│    - R 通道: 主平行光 — Near Cascade PCF/PCSS + Far Cached 采样       │
│      · 距离 < 0.85*R_near: 采样 Near SM                             │
│      · 距离 0.85~1.0*R_near: smoothstep 混合 Near↔Far               │
│      · 距离 > R_near: 仅采样 Far Cached SM (toroidal UV)            │
│    - G 通道: Capsule/Blob Shadow (per-pixel 解析计算, 可选)          │
│    - B 通道: Contact Shadow (屏幕空间 ray-march, High+)             │
│    - A 通道: 预留 (点光源阴影等)                                     │
│    - 输出: ShadowMask RT (RGBA8, 各通道独立阴影来源)                 │
│    - 目的: 避免 Forward Pass 中重复采样, 统一所有阴影来源             │
├────────────────────────────────────────────────────────────────────┤
│ 5. SSAO / SSDO Pass (全屏 Compute / FS)                            │
│    - 输入: Depth RT + (可选) 法线                                   │
│    - SSAO: Screen-Space Ambient Occlusion, 基于 depth-only          │
│    - SSDO: Screen-Space Directional Occlusion (High 档位可选)       │
│    - 输出: SSAO RT (R8 或 RG8, 可选模糊)                            │
│    - Low 档位跳过此 Pass（AO 只用纹理中的 AO Map 或默认 1.0）         │
├────────────────────────────────────────────────────────────────────┤
│ 6. Forward Lit Pass (主渲染)                                        │
│    - Depth Test = Equal（利用 Pre-Pass Depth）                      │
│    - Depth Write = Off（已有 Pre-Pass 写入的深度）                    │
│    - 输入 ShadowMask RT, SSAO RT, ClusterLightList (屏幕空间查找)   │
│    - Fog 在 FS 内联计算（litColor → mix with fog_color）            │
│    - 只渲染通过 Early-Z 的片元 → 零 overdraw                        │
│    - 按材质排序 → 批次化 (SurfaceType, PresetID, Pipeline)           │
│    - 输出: LitColor RT (RGBA16F) + Motion Vector RT (RG16F)        │
├────────────────────────────────────────────────────────────────────┤
│ 7. Sky Pass                                                        │
│    - Depth Test = Equal, Depth = 1.0 (远平面)                       │
│    - 程序化天空或天空盒                                              │
│    - 输出: 写入 LitColor RT                                         │
├────────────────────────────────────────────────────────────────────┤
│ 8. Translucent Pass (Forward, 后排序)                               │
│    - 透明物体、粒子、Billboard                                       │
│    - 从后往前排序 (painter's algorithm)                               │
│    - Depth Write = Off, Depth Test = Less, Blend = SrcAlpha         │
│    - 简化光照（通常 Unlit 或简单 BlinnPhong，不接收 SSAO）           │
│    - 输出: 写入 LitColor RT                                         │
├────────────────────────────────────────────────────────────────────┤
│ 9. Decal Pass (Forward Decal, P2)                       ← ★ 新增  │
│    - 渲染 Decal OBB mesh, FS 采样 Depth 反算 worldPos → 投影贴花    │
│    - Alpha Blend 写入 LitColor RT                                  │
│    - 无 decal 时跳过                                                │
├────────────────────────────────────────────────────────────────────┤
│ 10. Debug / Gizmo Overlay Pass (可选)                               │
│    - Gizmo3D 材质: 骨骼可视化、碰撞体线框、坐标轴、选择高亮            │
│    - Outline / Selection Highlight (Stencil + Dilate)   ← ★ 新增   │
│    - Depth Test 可选: 穿透/不穿透                                    │
│    - 使用硬编码太阳方向调试光照（见 2.5 节）                           │
│    - 输出: 写入 LitColor RT                                         │
├────────────────────────────────────────────────────────────────────┤
│ 11. Post-Processing Pass Chain                                      │
│    - 输入: LitColor RT (RGBA16F)                                   │
│    a. SSR (High+): Hi-Z Ray March → SSR RT → 混合到 LitColor       │
│    b. Auto Exposure: 亮度直方图 → 平均亮度 → exposure_value          │
│    c. Temporal AA (TAA): Motion Vector RT + 历史帧混合               │
│    d. Bloom: 提取高亮 → 分级模糊 → 叠加                              │
│    e. DOF (High+, 可选): CoC 计算 → 散景模糊                        │
│    f. Motion Blur (Ultra, 可选): per-pixel 运动模糊                  │
│    g. ToneMapping: exposure × litColor → ACES / Filmic → LDR       │
│    h. Color Grading / LUT: 3D LUT 颜色校正                         │
│    i. FXAA / Sharpening (CAS) → SwapChain                          │
│    - 输出: Backbuffer                                               │
└────────────────────────────────────────────────────────────────────┘
```

#### VBuffer 路径帧序列

```
┌────────────────────────────────────────────────────────────────────┐
│ 0. Meshlet Select & Cull (Compute, High+)         (同 Forward)     │
├────────────────────────────────────────────────────────────────────┤
│ 1. ShadowMap Pass (Near Dynamic + Far Cached)        (同 Forward)    │
├────────────────────────────────────────────────────────────────────┤
│ 2. VBuffer ID Pass (替代 Early-Z + Forward Lit)                    │
│    - 写入 VBuffer RT (InstanceID + MeshletIdx + TriangleID) + Depth │
│    - 极轻量 FS，无光照计算                                          │
│    - High+: vkCmdDrawIndexedIndirectCount (meshlet 级 Indirect)    │
├────────────────────────────────────────────────────────────────────┤
│ 2b. HZB Generation (Compute, High+)                               │
│    - Depth RT → HZB Pyramid 降采样 (用于 Phase 2 / SSR / SSAO)     │
├────────────────────────────────────────────────────────────────────┤
│ 2c. Meshlet Cull Phase 2 (Compute, High+, 可选)                   │
│    - 当前帧 HZB → 补充剔除存疑 meshlet → 追加绘制                    │
├────────────────────────────────────────────────────────────────────┤
│ 3. Tile SurfaceType Classification (Compute)          ← ★ 新增     │
│    - 每 TILE_SIZE×TILE_SIZE tile 统计 SurfaceType bitmask           │
│    - 输出 TileSurfaceMask + TileList (empty/single/multi) 三类      │
│    - 填充 indirect dispatch args（GPU 驱动，CPU 不回读）             │
├────────────────────────────────────────────────────────────────────┤
│ 4. Clustered Light Assignment (Compute, High+)    ← ★ 新增         │
├────────────────────────────────────────────────────────────────────┤
│ 5. ShadowMask Compose Pass (含Capsule+Contact)    (同 Forward)     │
│    ⤷ 可选优化: 读取 TileSurfaceMask，跳过 empty / Unlit tile        │
├────────────────────────────────────────────────────────────────────┤
│ 6. SSAO / SSDO Pass                               (同 Forward)     │
│    ⤷ 可选优化: 同上                                                 │
├────────────────────────────────────────────────────────────────────┤
│ 7. VBuffer Resolve (Tile-Based Indirect Dispatch)     ← ★ 改进     │
│    - Dispatch 1: Single-SurfaceType tiles (快速路径, ~60-80%)       │
│      → workgroup 内零分支发散，SIMD 利用率最优                       │
│    - Dispatch 2: Multi-SurfaceType tiles  (通用路径, ~10-25%)       │
│      → per-pixel 解析 SurfaceType，存在分支发散                     │
│    - 空 tile: 不在任何 list 中，不 dispatch → 零开销                 │
│    - 读取 ShadowMask + SSAO + ClusterLightList → 光照 + Fog inline │
│    - 输出: LitColor RT + Motion Vector RT                          │
├────────────────────────────────────────────────────────────────────┤
│ 8~12. Sky / Translucent / Decal / Debug / Post-Processing (同 Fwd) │
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

| Pass | Lowest | Low | Medium | High | Ultra | Cinematic |
|------|--------|-----|--------|------|-------|-----------|
| **Meshlet Select & Cull** | ❌ (CPU) | ❌ (CPU frustum+离散LOD) | ❌ (CPU frustum+离散LOD) | ✅ DAG遍历+Cull | ✅ 同 High | ✅ 同 High |
| Near Dynamic SM | ❌ | ❌/512² PCF | 1024² PCF | 2048² PCSS | 2048² PCSS | 4096² PCSS |
| Far Cached SM | ❌ | ❌ | 1024~2048² | 2048~4096² | 4096² | 4096² |
| Early-Z Pre-Pass | ✅ Direct Draw | ✅ Direct Draw | ✅ Direct Draw | ✅ Meshlet Indirect | ✅ Meshlet Indirect | ✅ Meshlet Indirect |
| **HZB Generation** | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |
| **Meshlet Cull Phase 2** | ❌ | ❌ | ❌ | ✅ (可选) | ✅ | ✅ |
| **Clustered Light Assign** | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ |
| ShadowMask (含Capsule+Contact) | ❌ | R8 或无 | RG8 PCF | RGBA8 PCSS | RGBA8 PCSS | RGBA8 PCSS |
| SSAO | ❌ | ❌ | ✅ 半分辨率 | ✅ 全分辨率 SSAO | ✅ SSDO | ✅ SSDO 高采样 |
| Forward Lit / VBuffer Resolve | 基础 | + Fog inline | + Fog inline | + Fog + Clustered | + Fog + Clustered | + Fog + Clustered |
| **Decal Pass** | ❌ | ❌ | ✅ 少量 | ✅ | ✅ | ✅ |
| **SSR** | ❌ | ❌ | ❌ | ✅ Hi-Z | ✅ | ✅ 高精度 |
| **Auto Exposure** | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ |
| TAA | ❌ | ❌（fallback FXAA） | ✅ 基础 TAA | ✅ TAA + Sharp | ✅ 同 High | ✅ 同 High |
| Bloom | ❌ | ❌ | ✅ 简易 | ✅ 多级 | ✅ | ✅ 高精度多级 |
| **DOF** | ❌ | ❌ | ❌ | ✅ (可选) | ✅ | ✅ 高精度 |
| **Motion Blur** | ❌ | ❌ | ❌ | ❌ | ✅ (可选) | ✅ |
| ToneMapping | ✅ 简化 | ✅ | ✅ | ✅ | ✅ | ✅ |
| **Color Grading / LUT** | ❌ | ❌ | ✅ 预设 | ✅ 自定义 | ✅ | ✅ |
| FXAA / CAS | FXAA | FXAA | 后备 | CAS | CAS | CAS |
| Motion Vector RT | ❌ | ❌（不分配） | ✅ | ✅ | ✅ | ✅ |

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

### 2.6 GPU-Driven Meshlet 裁剪与 HZB

> 本渲染器以 **Meshlet** 为最小渲染粒度（而非传统的 per-object），
> GPU 裁剪、LOD 选择、遮挡剔除全部在 meshlet 级别进行。
> 关于 Meshlet 几何管线的完整设计详见 §2.8。

#### Hierarchical Z-Buffer (HZB)

Early-Z Pre-Pass（或 VBuffer ID Pass）完成后，Depth RT 可降采样生成 **HZB 金字塔**（每级分辨率减半，取 4 像素中最远深度值）。HZB 用于后续帧的 GPU 遮挡剔除和 SSR 加速。

```
Depth RT (full res) → HZB Mip0 (half) → Mip1 (quarter) → ... → Mip N (1x1)
每级 texel = max(4 个来源 texel 的 depth)  // conservative：保留最远深度
```

```glsl
// hzb_downsample.comp.glsl — 逐级降采样 Depth → HZB（单 pass 多 mip 或多 pass）
layout(local_size_x=8, local_size_y=8) in;

layout(set=0, binding=0) uniform sampler2D DepthPrevMip;
layout(set=0, binding=1, r32f) writeonly uniform image2D HZBNextMip;

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 srcPos = pos * 2;
    float d0 = texelFetch(DepthPrevMip, srcPos + ivec2(0,0), 0).r;
    float d1 = texelFetch(DepthPrevMip, srcPos + ivec2(1,0), 0).r;
    float d2 = texelFetch(DepthPrevMip, srcPos + ivec2(0,1), 0).r;
    float d3 = texelFetch(DepthPrevMip, srcPos + ivec2(1,1), 0).r;
    // Reversed-Z: 近平面=1.0, 远平面=0.0 → min = 最远
    float hzb = min(min(d0, d1), min(d2, d3));
    imageStore(HZBNextMip, pos, vec4(hzb));
}
```

> **HZB 生成时机**：Early-Z / VBuffer ID Pass 之后，ShadowMask 之前（可并行）。
> **HZB 用途**：
> 1. GPU Meshlet 级遮挡剔除（下一帧或当前帧两阶段）
> 2. SSR 光线步进加速（Hi-Z Tracing）
> 3. 用于 SSAO 的 mip-chain 采样

#### GPU-Driven Meshlet 渲染流程

> 传统 GPU-Driven 以 Object 为粒度做剔除 → IndirectDraw。
> 本渲染器以 **Meshlet** 为粒度：LOD DAG 遍历 + 裁剪 + 遮挡 → 输出 Meshlet 级 Indirect Draw。

```
┌──────────────────────────────────────────────────────────────────────┐
│ CPU 每帧提交 (一次性上传, 场景变化时增量更新):                          │
│   InstanceBuffer SSBO: 所有实例 (transform, boundingSphere, flags)    │
│   MeshletBuffer  SSBO: 所有 meshlet (见 §2.8 MeshletGPU 结构体)      │
│   MeshletDAG     SSBO: LOD 层级关系 (parentIdx, childrenRange, error) │
│   MeshletVertexBuffer / MeshletIndexBuffer: meshlet 几何数据           │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│ GPU Pass A: Instance Cull (Compute)                                  │
│   - 对每个 Instance: BoundingSphere vs 视锥 6 平面                    │
│   - 大包围体 vs 上帧 HZB 遮挡查询 (整体不可见 → 跳过其所有 meshlet)    │
│   - 计算屏幕空间大小 + 重要性因子 → 写入 InstanceVisData              │
│                                                                      │
│ GPU Pass B: Meshlet LOD Select + Cull (Compute, 核心 Pass)           │
│   对每个可见 Instance 的 meshlet DAG:                                 │
│   1. LOD Cut Selection:                                              │
│      screenSpaceError = projectedError(meshlet.maxError, distance)    │
│      if screenSpaceError < threshold → 使用此 meshlet (不细化到子节点) │
│      else → 细化到子 meshlet                                         │
│      threshold 受 QualityTier + importance 调节                      │
│   2. Frustum Cull (per-meshlet BoundingSphere)                       │
│   3. HZB Occlusion Cull (per-meshlet, 上一帧 HZB)                   │
│   4. Normal Cone Cull (背面锥: 如果 meshlet 全部背向相机 → 剔除)      │
│   通过全部测试 → 写入 IndirectDrawBuffer (VkDrawIndexedIndirectCmd)    │
│                                                                      │
│ 输出: IndirectDrawBuffer + DrawCount (meshlet 粒度)                   │
├──────────────────────────────────────────────────────────────────────┤
│ vkCmdDrawIndexedIndirectCount(...)                                    │
│   → 每个 meshlet 一个 IndirectDraw                                   │
│   → 或使用 VK_EXT_mesh_shader: Task+Mesh Shader 路径 (可选加速)      │
└──────────────────────────────────────────────────────────────────────┘
```

#### 两阶段遮挡剔除（Two-Phase Occlusion Culling）

第一阶段用上一帧 HZB 对 meshlet 做遮挡剔除（保守，可能漏剔新出现的遮挡体）；
Early-Z / VBuffer ID Pass 完成后生成当前帧 HZB，用于第二阶段对"可疑 meshlet"（上帧不存在 / 上帧被遮挡但可能现在可见）做补充剔除。

```
Phase 1: 上一帧 HZB → Instance Cull + Meshlet LOD/Cull → Draw 已知可见 meshlets
Phase 2: 当前帧 HZB (from Phase1 depth) → 补充 Cull Phase1 存疑的 meshlets → 补充 Draw
```

> **两阶段的必要性**：当相机快速移动或新对象入场时，Phase 1 可能错误遮挡掉实际可见的 meshlet。
> Phase 2 用当前帧的真实深度做纠正，代价是多一趟 Compute + 少量额外 Draw。

#### Meshlet 裁剪 GPU 资源

| 资源 | 格式 | 大小 | 用途 |
|------|------|------|------|
| HZB Pyramid | R32F, mipmap chain | ~1.33× 全分辨率 | 遮挡查询 + SSR |
| InstanceBuffer | SSBO | N_inst × 128B | 所有场景实例 (transform, bounds, flags, importance) |
| MeshletBuffer | SSBO | N_meshlet × 64B | 所有 meshlet 元数据 (见 §2.8) |
| MeshletDAG | SSBO | N_meshlet × 16B | LOD 层级 (parentIdx, childRange, maxError) |
| InstanceVisData | SSBO | N_inst × 16B | Instance Cull 输出 (可见性 + screenSize) |
| IndirectDrawBuffer | SSBO | N_meshlet × 20B | VkDrawIndexedIndirectCommand per meshlet |
| DrawCount | SSBO | 4B | vkCmdDrawIndexedIndirectCount 的 count |
| MeshletVertexBuffer | SSBO | 变长 | 所有 meshlet 的顶点数据（共享池） |
| MeshletIndexBuffer | SSBO | 变长 | 所有 meshlet 的局部索引（共享池） |

### 2.7 渲染器特性总览

> 本节汇总渲染器的所有特性模块，标注与画质档位的关系和实现优先级。

#### 光照与阴影

| 特性 | Lowest | Low | Medium | High | Ultra | Cinematic | 优先级 | 备注 |
|------|--------|-----|--------|------|-------|-----------|-------|------|
| 主方向光 (Sun) | ✅ 顶点级 | ✅ | ✅ | ✅ | ✅ | ✅ | P0 | 始终存在 |
| **Near Dynamic Cascade** | ❌ | ❌/512² PCF | 1024² PCF | 2048² PCSS | 2048² PCSS | 4096² PCSS | P0 | 近景全动态 SM §3.6.3 |
| **Far Cached Shadow Map** | ❌ | ❌ | 1024~2048² | 2048~4096² | 4096² | 4096² | P0 | 环形滚动缓存 §3.6.3 |
| **ShadowMask Compose** | ❌ | R8 或无 | RG8 | RGBA8 | RGBA8 | RGBA8 | P0 | 统一合成 §3.6.2 |
| **Capsule / Blob Shadow** | ❌ | Blob 可选 | ✅ 手机优先 | ✅ 可选 | ✅ 可选 | ✅ 可选 | P1 | §3.6.4 |
| **Contact Shadow** | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | P2 | 屏幕空间 ray-march §3.6.5 |
| 多点光源/聚光 | 1 个 | 4 个 | 16 个 | 64 个 | 256+ | 256+ | P1 | 需要光源裁剪 |
| **Clustered Shading** | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | P2 | 见下文 |

#### 环境光与间接光照

| 特性 | Lowest | Low | Medium | High | Ultra | Cinematic | 优先级 | 备注 |
|------|--------|-----|--------|------|-------|-----------|-------|------|
| 固定常量环境色 | ✅ | — | — | — | — | — | P0 | Constant Ambient |
| 固定渐变环境色 | — | ✅ | — | — | — | — | P0 | Simple Ambient |
| FakeAtmosphere | — | — | ✅ | — | — | — | P0 | |
| IBL (CubeMap) | — | — | — | ✅ | ✅ | ✅ | P1 | Irradiance + Prefiltered |
| Reflection Probes | ❌ | ❌ | ❌ | ✅ 手动放置 | ✅ | ✅ | P2 | 局部 CubeMap 采集 |
| **SSR** (Screen-Space Reflection) | ❌ | ❌ | ❌ | ✅ Hi-Z Trace | ✅ | ✅ 高精度 | P2 | 利用 HZB 加速 |
| Light Probes / 间接漫反射 | ❌ | ❌ | ❌ | ❌ | ❌ | ✅ | P3 | Irradiance Volume / SH Probe |

#### 后处理管线

| 特性 | Lowest | Low | Medium | High | Ultra | Cinematic | 优先级 | shader 文件 |
|------|--------|-----|--------|------|-------|-----------|-------|------------|
| ToneMapping (ACES) | ✅ 简化 | ✅ | ✅ | ✅ | ✅ | ✅ | P0 | tone_mapping.comp.glsl |
| FXAA | ✅ | ✅ | 后备 | 后备 | 后备 | 后备 | P0 | fxaa.comp.glsl |
| TAA | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | P1 | taa_resolve.comp.glsl |
| Bloom | ❌ | ❌ | ✅ 简易 | ✅ 多级 | ✅ | ✅ 高精度 | P1 | bloom_extract/blur.comp.glsl |
| **Auto Exposure** | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | P1 | auto_exposure.comp.glsl |
| **Fog** (Distance + Height) | ❌ | ✅ 简易 | ✅ | ✅ | ✅ | ✅ | P1 | fog.glsl (inline) |
| **Screen-Space Reflection** | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ 高精度 | P2 | ssr.comp.glsl |
| Sharpening (CAS) | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | P2 | sharpen.comp.glsl |
| **Depth of Field** | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ 高精度 | P3 | dof.comp.glsl |
| **Motion Blur** | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | P3 | motion_blur.comp.glsl |
| **Color Grading / LUT** | ❌ | ❌ | ✅ 预设 | ✅ 自定义 | ✅ | ✅ | P2 | color_grading.comp.glsl |
| **Vignette** | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | P3 | inline (ToneMap pass) |
| **Film Grain** | ❌ | ❌ | ❌ | ❌ | ✅ | ✅ | P3 | inline (ToneMap pass) |

#### GPU 裁剪与性能优化

| 特性 | Lowest | Low | Medium | High | Ultra | Cinematic | 优先级 | 平台限制 |
|------|--------|-----|--------|------|-------|-----------|-------|---------|
| Frustum Culling (CPU) + 离散 LOD | ✅ | ✅ | ✅ | — | — | — | P0 | 全平台 |
| **Meshlet DAG 遍历 + LOD Select** | — | — | — | ✅ | ✅ | ✅ | P0 | SSBO 平台 (PC/Apple/Android High) |
| **Meshlet Frustum Cull (GPU)** | — | — | — | ✅ | ✅ | ✅ | P0 | SSBO 平台 |
| **Meshlet Backface Cone Cull** | — | — | — | ✅ | ✅ | ✅ | P0 | SSBO 平台 |
| **HZB 生成** | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | P1 | SSBO 平台 (Android High 可选) |
| **HZB Meshlet Occlusion Cull** | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | P1 | SSBO 平台 |
| **Importance 因子** | — | — | — | ✅ | ✅ | ✅ | P1 | SSBO 平台 |
| Meshlet Indirect Draw | ❌ | ❌ | ❌ | ✅ | ✅ | ✅ | P0 | SSBO 平台 |
| Mesh Shader 路径 (可选) | ❌ | ❌ | ❌ | ✅ 可选 | ✅ 可选 | ✅ 可选 | P2 | PC only (VK_EXT_mesh_shader) |
| Terrain-Contact Dither | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | P1 | Android Low 不支持 |
| Texture Streaming | ❌ | ❌ | ✅ | ✅ | ✅ | ✅ | P2 | Android Low 不支持 |

> Android Mid/Low 走 VBO 传统路径，不使用 GPU meshlet 裁剪和 Indirect Draw（详见 §2.9.4）。

#### 编辑器 / 特殊效果

| 特性 | 优先级 | 备注 |
|------|-------|------|
| **Decals** (投射贴花) | P2 | Screen-Space Decal 或 Mesh Decal |
| **Outline / Selection Highlight** | P1 | 模板 + 膨胀 或 Jump Flood |
| Gizmo / Debug Overlay | P0 | 已设计（2.5 节） |
| Wireframe Override | P0 | 已设计 |
| Grid (无限地面网格) | P1 | 编辑器标配 |

#### Clustered Shading（多光源管理 — High+）

当场景灯光数量超过 16 个时，Forward Pass 逐光源循环效率急剧下降。
Clustered Shading 将视锥体分割为 3D cluster grid（屏幕空间 tile × 深度切片），
每个 cluster 只计算影响它的灯光子集。

```
视锥体 3D 分割:
  X: tileCountX = ceil(screenW / 64)
  Y: tileCountY = ceil(screenH / 64)
  Z: sliceCount = 24 (指数分布, near=0.1 .. far=1000)
  → cluster 总数 ≈ 30×17×24 ≈ 12,240 (1080p)

Compute Pass: Light Assignment
  - 对每个点光源/聚光: 求 bounding sphere / cone 与 cluster 的交集
  - 写入 ClusterLightList[clusterIdx] = { lightIdx0, lightIdx1, ... }

Forward Lit / VBuffer Resolve 中:
  uint clusterIdx = GetClusterIndex(screenUV, linearDepth);
  uint lightCount = ClusterLightList[clusterIdx].count;
  for (uint i = 0; i < lightCount; i++)
  {
      Light light = LightBuffer[ClusterLightList[clusterIdx].lights[i]];
      litColor += EvalPointLight(light, worldPos, N, V, ...);
  }
```

| 资源 | 格式 | 大小 | 用途 |
|------|------|------|------|
| LightBuffer | SSBO | maxLights × 48B | 所有灯光参数 |
| ClusterLightList | SSBO | clusters × (count + indices) | 每 cluster 的灯光索引 |
| ClusterAABB | SSBO (预计算) | clusters × 32B | 每 cluster 的视锥空间 AABB |

> Lowest/Low/Medium 档位不使用 Clustered Shading，直接循环少量灯光（4~16 个）。

#### Fog（雾效 — 所有档位）

雾效不是后处理，而是在光照计算后、输出 LitColor 前应用（Forward Lit FS / VBuffer Resolve 内联）：

```glsl
// fog.glsl — inline, 不是独立 compute pass
struct FogParams {
    vec3  fog_color;        // 雾颜色（通常接近天空色）
    float fog_density;      // 指数雾密度
    float fog_height_start; // 高度雾起始 Y
    float fog_height_end;   // 高度雾结束 Y
    float fog_max;          // 最大雾浓度 [0,1]
    uint  fog_mode;         // 0=None, 1=Linear, 2=Exp, 3=Exp2, 4=Height
};

float CalcFogFactor(vec3 worldPos, vec3 cameraPos, FogParams fog)
{
    float dist = length(worldPos - cameraPos);

    float distFog;
    if (fog.fog_mode == 1)      distFog = clamp((dist - fog.fog_start) / (fog.fog_end - fog.fog_start), 0, 1);
    else if (fog.fog_mode == 2) distFog = 1.0 - exp(-fog.fog_density * dist);
    else if (fog.fog_mode == 3) distFog = 1.0 - exp(-pow(fog.fog_density * dist, 2.0));
    else                        distFog = 0.0;

    // 高度雾叠加
    float heightFog = 1.0 - smoothstep(fog.fog_height_start, fog.fog_height_end, worldPos.y);

    return min(max(distFog, heightFog), fog.fog_max);
}

// 在 Forward Lit / VBuffer Resolve 的最终输出前:
vec3 finalColor = mix(litColor, fog.fog_color, fogFactor);
```

#### Auto Exposure（自动曝光 — Medium+）

通过计算当前帧平均亮度，自动调整曝光值。使用 luminance histogram 方法：

```
Pass 1: Luminance Histogram (Compute)
  - 对 LitColor RT 降采样，统计 256-bin 亮度直方图

Pass 2: Average Luminance (Compute, 1 workgroup)
  - 从直方图剔除极端值 (top/bottom 5%)
  - 计算加权平均亮度
  - 指数平滑适应上一帧曝光值（避免闪烁）
  - 输出: exposure_value (float, UBO push)

ToneMapping 时: finalColor = litColor * exposure_value → ToneMap(...)
```

#### SSR (Screen-Space Reflections — High+)

利用 HZB 金字塔加速屏幕空间光线步进：

```
输入: LitColor RT, Depth RT, Normal (from VBuffer 或 reconstructed), HZB Pyramid
输出: SSR RT (RGBA16F) — 反射颜色 + 置信度

算法: Hi-Z Ray Marching
  1. 从像素出发，沿反射方向步进
  2. 每步查询 HZB 的粗 mip → 快速跳过空区域
  3. 命中时切换到细 mip → 精确求交
  4. 边缘淡出 + 时间稳定性（历史帧混合）

在 Forward Lit / VBuffer Resolve 输出后:
  litColor = mix(litColor, ssrColor, ssrConfidence * fresnel * roughnessAttenuation);
```

#### Screen-Space Decals（屏幕空间贴花 — P2）

> 不改变材质系统设计，Decal 作为独立 Pass 在 Forward Lit / VBuffer Resolve 之后、
> 后处理之前执行。用投影盒投射贴花纹理到 Depth Buffer 上的表面。

```
Decal Pass (在 Lit 之后):
  - 渲染 Decal 投影盒（OBB mesh）
  - FS 中: 采样 Depth → 反算 worldPos → 投影到 Decal 本地空间 → 采样 Decal 纹理
  - 混合写入 LitColor RT（additive / alpha blend）
  - 可选: 写入额外的 albedo/normal 修改（需 thin-GBuffer 或 deferred decal）
```

#### Outline / Selection Highlight（编辑器选择高亮 — P1）

```
方案 A: Stencil + 膨胀
  1. 选中对象渲染到 Stencil Buffer (标记 = 1)
  2. 全屏 Pass: 对 Stencil=1 的像素做 3×3 膨胀（dilate）
  3. 膨胀后 Stencil=1 但原始 Stencil=0 的像素 → 描边像素 → 输出描边色

方案 B: Jump Flood Algorithm (JFA)
  1. 选中对象渲染到 seed RT
  2. 多趟 JFA Pass → 生成 distance field
  3. distance < outline_width 的像素 → 描边
```

---

### 2.8 Meshlet 几何管线

> 本渲染器采用 **Meshlet** 作为几何管线的核心单元。传统渲染以整个 Mesh 为粒度提交 DrawCall，
> 而 Meshlet 管线将每个 Mesh 预处理拆分为小型 meshlet 块，GPU 侧以 meshlet 为粒度做
> LOD 选择 + 裁剪 + 遮挡剔除 → 输出 Indirect Draw，实现极细粒度的几何管理。

#### 2.8.1 Meshlet 定义与 LOD 层级

**Meshlet** 是一个包含 64~128 个顶点、最多 124~256 个三角形的几何小块。
原始模型（LOD0）在离线预处理阶段被拆解为一组 meshlet。

```
LOD 层级构建（离线预处理，Build-Time）:

LOD 0: 原始模型 → 拆分为 N 个 meshlet (每个 64~128 顶点)
         ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐
         │ M0 │ │ M1 │ │ M2 │ │ M3 │ │ M4 │ │ M5 │ │ M6 │ │ M7 │
         └──┬─┘ └──┬─┘ └──┬─┘ └──┬─┘ └──┬─┘ └──┬─┘ └──┬─┘ └──┬─┘
            └──┬───┘      └──┬───┘      └──┬───┘      └──┬───┘
LOD 1:     ┌──┴──┐      ┌──┴──┐      ┌──┴──┐      ┌──┴──┐
           │ M0' │      │ M1' │      │ M2' │      │ M3' │
           └──┬──┘      └──┬──┘      └──┬──┘      └──┬──┘
              └─────┬──────┘            └─────┬──────┘
LOD 2:          ┌──┴──┐                  ┌──┴──┐
                │ M0" │                  │ M1" │
                └──┬──┘                  └──┬──┘
                   └──────────┬──────────┘
LOD 3:                    ┌──┴──┐
                          │ Root│  (整个模型简化为 1 个 meshlet)
                          └─────┘
```

**LOD 层级构建规则**：
- 每级 LOD 将 2~4 个空间相邻的子 meshlet **合并+简化** 为 1 个新的父 meshlet
- 父 meshlet 的顶点数仍保持 64~128（通过 mesh simplification 降面）
- 每个 meshlet 记录 `maxError`：该级别引入的最大几何误差（与原始模型的 Hausdorff 距离）
- 形成 **DAG（有向无环图）**，叶节点 = LOD0 原始 meshlet，根节点 = 最粗 LOD

#### 2.8.2 Meshlet GPU 数据结构

```cpp
// 离线构建，运行时只读（上传到 SSBO）
struct MeshletGPU
{
    // 几何数据定位
    uint32_t vertex_offset;       // 在 MeshletVertexBuffer 中的起始偏移
    uint32_t index_offset;        // 在 MeshletIndexBuffer 中的起始偏移
    uint16_t vertex_count;        // 顶点数 (≤128)
    uint16_t triangle_count;      // 三角形数 (≤256)

    // 包围体 (用于 frustum cull + occlusion cull)
    vec3     bounding_center;     // 包围球心 (模型局部空间)
    float    bounding_radius;     // 包围球半径

    // 法线锥 (用于 backface cone cull)
    // 如果从相机位置看, 整个 meshlet 的面法线都朝反方向 → 可安全剔除
    int8_t   cone_axis_x;        // 法线锥轴 (归一化, snorm8 × 3)
    int8_t   cone_axis_y;
    int8_t   cone_axis_z;
    int8_t   cone_cutoff;        // cos(半角), snorm8, < 0 表示 > 90° 不可剔除

    // LOD 层级
    uint32_t parent_meshlet;      // 父 meshlet 在 MeshletBuffer 中的索引 (0xFFFFFFFF = root)
    uint32_t children_start;      // 子 meshlet 起始索引 (叶节点 = 0xFFFFFFFF)
    uint16_t children_count;      // 子 meshlet 数量 (0 = 叶节点)
    float    max_error;           // 此级别引入的最大几何误差

    // 材质 / 渲染属性
    uint16_t instance_id;         // 所属 Instance 索引
    uint8_t  material_preset_id;  // 材质预设 ID (与 MaterialPresetDef 对应)
    uint8_t  meshlet_flags;       // 见下方 MeshletFlags
};
// sizeof(MeshletGPU) = 64 bytes

// Meshlet 级标志位
enum MeshletFlags : uint8_t
{
    MESHLET_FLAG_NONE            = 0x00,
    MESHLET_FLAG_TERRAIN_CONTACT = 0x01, // 与 Terrain 接触 → dither 混合
    MESHLET_FLAG_TWO_SIDED       = 0x02, // 双面渲染 (跳过 backface cone cull)
    MESHLET_FLAG_ALPHA_TEST      = 0x04, // 需要 Alpha-Test (树叶等)
    MESHLET_FLAG_WIND_ANIM       = 0x08, // 风力动画 (顶点偏移)
};
```

#### 2.8.3 GPU Meshlet LOD 选择算法

Compute Shader 遍历每个可见 Instance 的 meshlet DAG，选择最优的 LOD "切面"（DAG Cut）。

```glsl
// meshlet_lod_select.comp.glsl — 核心 LOD 选择逻辑（简化伪代码）

// 每个工作项处理一个 meshlet 节点
void ProcessMeshlet(uint meshletIdx, InstanceVisData inst)
{
    MeshletGPU m = MeshletBuffer[meshletIdx];

    // 1. 计算屏幕空间误差
    vec3 worldCenter = (inst.localToWorld * vec4(m.bounding_center, 1.0)).xyz;
    float dist = length(worldCenter - cameraPos);
    float screenError = (m.max_error / dist) * screenHeight * 0.5 / tan(fovY * 0.5);

    // 2. 误差阈值受画质档位和重要性调节
    //    importance: 1.0=主角(总是最高精度), 0.5=NPC, 0.2=远景
    float threshold = BASE_ERROR_THRESHOLD
                    * QualityTierFactor[qualityTier]   // Lowest=16.0, Low=4.0, Med=2.0, High=1.0, Ultra=0.5, Cinematic=0.25
                    / max(inst.importance, 0.1);        // importance 越高 → threshold 越低 → 越精细

    if (screenError < threshold || m.children_count == 0)
    {
        // 使用此 meshlet（不再细化）
        // 3. Frustum Cull
        float worldRadius = m.bounding_radius * inst.uniformScale;
        if (!FrustumTestSphere(worldCenter, worldRadius))
            return; // 视锥外

        // 4. Backface Cone Cull (仅非双面)
        if ((m.meshlet_flags & MESHLET_FLAG_TWO_SIDED) == 0)
        {
            vec3 coneAxis = mat3(inst.localToWorld) * UnpackSnorm3(m.cone_axis_xyz);
            float coneCutoff = float(m.cone_cutoff) / 127.0;
            vec3 viewDir = normalize(worldCenter - cameraPos);
            if (dot(viewDir, coneAxis) > coneCutoff)
                return; // 整个 meshlet 背向相机
        }

        // 5. HZB Occlusion Cull (使用上帧 HZB)
        vec4 screenAABB = ProjectSphereToScreen(worldCenter, worldRadius, viewProjMatrix);
        float hzbMip = ceil(log2(max(screenAABB.z - screenAABB.x, screenAABB.w - screenAABB.y)));
        float hzbDepth = textureLod(HZB_Pyramid, (screenAABB.xy + screenAABB.zw) * 0.5, hzbMip).r;
        float meshletNearDepth = ProjectDepth(worldCenter - viewDir * worldRadius);
        if (meshletNearDepth < hzbDepth)  // Reversed-Z: near=1.0, far=0.0
            return; // 被遮挡

        // 6. 通过所有测试 → 写入 Indirect Draw
        uint drawIdx = atomicAdd(DrawCount, 1);
        IndirectDrawBuffer[drawIdx] = VkDrawIndexedIndirectCommand(
            m.triangle_count * 3,   // indexCount
            1,                       // instanceCount
            m.index_offset,          // firstIndex
            m.vertex_offset,         // vertexOffset
            meshletIdx               // firstInstance (传递 meshletIdx 给 VS)
        );
    }
    else
    {
        // screenError >= threshold → 需要细化到子 meshlet
        // 将子 meshlet 范围加入下一轮处理队列
        for (uint i = 0; i < m.children_count; i++)
            AppendToProcessQueue(m.children_start + i, inst);
    }
}
```

> **DAG Cut 一致性**：必须确保父节点和子节点不同时被渲染（否则会出现重叠几何体）。
> 当一个 meshlet 被选中渲染时，其所有祖先和后代都不应出现在 IndirectDrawBuffer 中。
> 这通过 LOD 选择的自上而下遍历天然保证：一旦选中某节点，就不再展开其子树。

#### 2.8.4 Meshlet 与材质系统的关系

GPU Meshlet 管线不改变材质系统的核心设计（SurfaceType × QualityTier × Preset），
只改变 **DrawCall 提交方式**：

- `MaterialInstance` 仍然按 Preset 绑定到 Instance 上
- Meshlet 选择/裁剪只影响"渲染哪些 meshlet"，不影响"用什么材质渲染"
- 每个 meshlet 继承其所属 Instance 的 `MaterialPresetDef` + `MaterialInstance`
- **例外**：per-meshlet `meshlet_flags` 可以覆盖某些渲染行为（如 terrain contact dither）

```
Instance "Tree_Pine_01"
├── MaterialPreset = StandardTexture (PresetID = 9)
├── MaterialInstance = { albedo_tint=(0.3,0.5,0.2), roughness=0.8, ... }
└── Meshlets:
    ├── M0 (树冠上部)      flags = ALPHA_TEST | WIND_ANIM    → 正常渲染
    ├── M1 (树冠中部)      flags = ALPHA_TEST | WIND_ANIM    → 正常渲染
    ├── M2 (树干)          flags = NONE                       → 正常渲染
    ├── M3 (树根/地面接触) flags = TERRAIN_CONTACT            → dither 混合
    └── M4 (树根/地面接触) flags = TERRAIN_CONTACT            → dither 混合
```

#### 2.8.5 Terrain-Contact Meshlet Dither 混合

当一棵树（或岩石、草丛等）的底部 meshlet 与 Terrain 交叉时，硬边裁剪会产生明显的
"浮空"或"穿插"瑕疵。Terrain-Contact Dither 通过渐变抖动实现柔和过渡。

##### Forward 路径

```
Terrain-Contact meshlet 在 Forward Lit Pass 中:
1. VS 正常变换
2. FS 额外计算:
   - 采样 Terrain HeightMap → 获取当前像素下方的 terrain 高度
   - blendFactor = smoothstep(terrain_height - blend_range,
                              terrain_height + blend_range,
                              worldPos.y)
   - 使用 ordered dither (Bayer 矩阵 4×4 或 8×8):
     if (blendFactor < BayerMatrix[pixelPos.xy % 8])
         discard;   // 此像素属于 terrain → 让 terrain pass 填充
   - 未 discard 的像素正常执行树的光照
   - 视觉效果: 树根处逐渐稀疏化 → terrain 逐渐显露 → 自然过渡
```

```glsl
// forward_terrain_contact.glsl — inline 在 Forward Lit FS 中
// 仅 MESHLET_FLAG_TERRAIN_CONTACT 时启用

uniform sampler2D TerrainHeightMap;
uniform vec4 terrain_world_bounds;  // (minX, minZ, 1/sizeX, 1/sizeZ)
uniform float terrain_blend_range;  // 混合带宽度 (世界空间, 如 0.5m)

// Bayer 8×8 有序抖动矩阵 (归一化到 [0,1])
const float BayerMatrix8[64] = float[64]( /* ... 标准 8×8 Bayer */ );

float GetTerrainHeight(vec3 worldPos)
{
    vec2 uv = (worldPos.xz - terrain_world_bounds.xy) * terrain_world_bounds.zw;
    return texture(TerrainHeightMap, uv).r;
}

void ApplyTerrainContactDither(vec3 worldPos, ivec2 pixelCoord)
{
    float terrainH = GetTerrainHeight(worldPos);
    float blend = smoothstep(terrainH - terrain_blend_range,
                             terrainH + terrain_blend_range,
                             worldPos.y);
    float dither = BayerMatrix8[(pixelCoord.x % 8) + (pixelCoord.y % 8) * 8];
    if (blend < dither)
        discard;
}
```

##### VBuffer 路径

```
VBuffer ID Pass:
  - Terrain-Contact meshlet 与普通 meshlet 一样写入 VBuffer RT
  - meshlet_flags.TERRAIN_CONTACT 编码进 VBuffer RT 的 flags 字段

VBuffer Resolve:
  - 检测到 TERRAIN_CONTACT flag 的像素:
    1. 计算 blendFactor (同上: worldPos.y vs terrain height)
    2. 使用 Bayer dither 决定此像素使用 树材质 还是 terrain 材质
    3. dither 选中 terrain → 从 TerrainMaterialInstance 获取参数做 terrain 光照
    4. dither 选中 tree   → 正常执行 tree 材质光照
  - 效果等同 Forward 路径的 discard，但在 Resolve 阶段以纯 Compute 实现
  - 优点: VBuffer ID Pass 无分支，dither 决策延迟到 Resolve
```

> **dither 的对比 alpha blend**：
> - Alpha blend 要求排序，对不透明物体管线（Early-Z / VBuffer）不友好
> - Dither 是 per-pixel 的二选一（要么树要么地形），不需排序
> - TAA 可以进一步平滑 dither 噪声（时间超采样消除棋盘格感）
> - 与 VBuffer 完美兼容：每像素仍然只有一个材质 ID

##### Terrain-Contact 的其他应用

同样的 dither 混合机制可推广到：
- **草丛/灌木与地面**：底部 meshlet 标记 TERRAIN_CONTACT
- **岩石嵌入地面**：底部 meshlet 与 terrain dither 过渡
- **建筑地基**：避免硬切地面线
- **雪地覆盖**：顶部 meshlet 标记 SNOW_CONTACT，dither 切换到雪材质

#### 2.8.6 Mesh Shader 加速路径（可选 — VK_EXT_mesh_shader）

如果 GPU 支持 `VK_EXT_mesh_shader`，可以用 Task Shader + Mesh Shader 替代
Compute Cull + Indirect Draw 管线，进一步减少 CPU-GPU 同步和中间 buffer：

```
传统路径 (所有 Vulkan GPU):
  Compute Cull → IndirectDrawBuffer → vkCmdDrawIndexedIndirectCount → VS/FS

Mesh Shader 路径 (VK_EXT_mesh_shader, 可选加速):
  Task Shader (替代 Compute Cull):
    - 每个工作组处理一批 meshlet
    - LOD Select + Frustum Cull + Backface Cone Cull + HZB Occlusion
    - 通过的 meshlet → EmitMeshTasks() 启动对应 Mesh Shader
  Mesh Shader (替代 VS):
    - 从 MeshletVertexBuffer/IndexBuffer 读取几何数据
    - 直接输出三角形 → FS
    - 无需传统的 VBO/IBO 绑定

优势:
  - 省去 IndirectDrawBuffer 的读写 (减少带宽)
  - Task → Mesh 是 GPU 内部流转，无需 CPU dispatch
  - 天然适合 meshlet 粒度的几何管线

劣势:
  - 需要 VK_EXT_mesh_shader 支持 (非所有 GPU)
  - 调试工具支持不如传统管线成熟
  - 作为可选加速路径，非必须
```

#### 2.8.7 Meshlet 离线预处理流水线

```
原始 Mesh (.obj / .glb / .fbx)
    │
    ▼
Step 1: Meshlet 化 (meshoptimizer::meshopt_buildMeshlets)
    - 输入: 顶点 + 索引
    - 输出: N 个 meshlet (每个 ≤128 顶点, ≤256 三角形)
    - 同时计算每个 meshlet 的 bounding sphere + normal cone
    │
    ▼
Step 2: LOD 层级构建 (递归)
    - 对当前层的所有 meshlet 做空间聚类 (k-means / METIS 图分割)
    - 每 2~4 个相邻 meshlet 合并 → mesh simplification (meshoptimizer::meshopt_simplify)
    - 简化后重新拆分为新 meshlet (保持 64~128 顶点约束)
    - 记录 parentIdx, childrenRange, maxError
    - 重复直到只剩 1 个 meshlet (根节点)
    │
    ▼
Step 3: 标记 Meshlet Flags (美术/工具辅助)
    - TERRAIN_CONTACT: 自动检测 (meshlet 的 bounding box 底部 < terrain 阈值)
                        或美术在编辑器中手动标记
    - ALPHA_TEST: 继承自材质 (如果材质有 alpha cutoff)
    - TWO_SIDED: 继承自材质
    - WIND_ANIM: 美术标记（树叶、草等需要风力动画的部分）
    │
    ▼
Step 4: 打包输出
    - MeshletBuffer:      N_total × 64B (所有 LOD 级别的 MeshletGPU)
    - MeshletVertexBuffer: 所有 meshlet 的顶点数据 (共享池)
    - MeshletIndexBuffer:  所有 meshlet 的局部三角形索引 (共享池)
    - 存储为引擎自定义二进制格式 (.meshlet / .ulm)
```

> **meshoptimizer**：推荐使用 [meshoptimizer](https://github.com/zeux/meshoptimizer) 库，
> 提供 meshlet 构建、mesh simplification、顶点缓存优化等功能，已广泛应用于工业级引擎。

#### 2.8.8 低端回退 (Lowest/Low/Medium 档位 + 平台维度)

Meshlet 管线主要面向 **SSBO 平台的 High/Ultra/Cinematic 档位**。回退策略按 **平台后端** × **画质档位** 双重维度决定（详见 §2.9）：

| 平台后端 | 档位 | 几何获取 | 裁剪方式 | LOD 策略 | DrawCall |
|---------|------|---------|---------|---------|--------|
| PC / Apple | High/Ultra/Cinematic | SSBO | GPU Meshlet Cull (DAG+Frustum+Cone+HZB) | Meshlet DAG (GPU) | vkCmdDrawIndexedIndirectCount |
| PC / Apple | Lowest/Low/Medium | SSBO | CPU Frustum Cull | 离散 LOD | vkCmdDrawIndexed（仍从 SSBO 读顶点） |
| Android High | High | SSBO | GPU Meshlet Cull | Meshlet DAG (GPU) | vkCmdDrawIndexedIndirectCount |
| Android Mid | Medium | VBO | CPU Frustum Cull | 离散 LOD | vkCmdDrawIndexed + Instancing |
| Android Low | Lowest/Low | VBO | CPU Frustum Cull | 离散 LOD（2级） | vkCmdDrawIndexed |

```
回退策略总览:

  SSBO 平台 (PC / Apple / Android High):
    High/Ultra/Cinematic:
      GPU: Meshlet DAG → LOD Select + Cull → IndirectDrawBuffer
      GPU: vkCmdDrawIndexedIndirectCount (per-meshlet, 从全局 SSBO 读取顶点)
      可选: Task Shader + Mesh Shader 路径
    Lowest/Low/Medium (PC/Apple 低画质配置):
      CPU: Frustum Cull per-object + 离散 LOD select
      GPU: vkCmdDrawIndexed (仍从全局 SSBO 读取顶点，VS 用 FetchVertex())

  VBO 平台 (Android Mid/Low):
    CPU: Frustum Cull per-object
         Select LOD level (离散: LOD0/1/2/3 整个 mesh 切换)
    GPU: vkCmdDrawIndexed (传统 VBO/IBO, VkVertexInputAttributeDescription)
         Mid 支持 Instancing
```

> **关键区分**：PC/Apple 即使在 Low 档位也走 SSBO 路径（只是不开 GPU meshlet 裁剪），
> 因为 Desktop GPU 的 SSBO 读取性能足够好且简化了绑定管理。
> Android Mid/Low 必须走 VBO 是因为低端移动 GPU 的 SSBO 随机访问延迟高、带宽有限。

> **两套 LOD 的统一**：离线预处理时，Lowest/Low/Medium 的离散 LOD mesh 就是从 meshlet DAG
> 的各级别"拍平"导出的（将该级所有 meshlet 合并回一个普通 mesh）。
> 这样不需要维护两套独立的 LOD 数据。

### 2.9 平台后端与特性分级 ★

引擎面向三大平台族群，几何数据获取方式和特性集在编译期/初始化期分流：

#### 2.9.1 平台后端定义

```cpp
enum class PlatformBackend : uint8_t
{
    PC      = 0,    // Windows / Linux — Vulkan 1.2+，Desktop 独显/核显
    Apple   = 1,    // macOS / iOS — MoltenVK 或 Metal，Apple Silicon
    Android = 2,    // Android — Vulkan 1.1+，Mali / Adreno / PowerVR
};

enum class GeometryFetchMode : uint8_t
{
    SSBO    = 0,    // 顶点/索引数据存储在 SSBO，VS 内手动 fetch
    VBO     = 1,    // 传统 VBO/IBO，VkVertexInputAttributeDescription 自动装填
};
```

#### 2.9.2 平台 × 几何后端映射

| 平台 | 画质范围 | GeometryFetchMode | 顶点获取方式 | 备注 |
|------|---------|-------------------|-------------|------|
| **PC** | Lowest ~ Cinematic | **SSBO** | VS 内 `VertexBuffer[gl_VertexIndex]` | 所有档位统一 SSBO |
| **Apple** | Lowest ~ Ultra | **SSBO** | 同上 | Apple Silicon 统一架构，SSBO 高效 |
| **Android High** | Medium ~ High | **SSBO** | 同上 | 高端 Adreno 7xx / Mali-G7xx |
| **Android Mid** | Low ~ Medium | **VBO** | `layout(location=N) in` 传统属性 | Adreno 6xx / Mali-G5x~G7x 中端 |
| **Android Low** | Lowest ~ Low | **VBO** | 同上 | Adreno 5xx / Mali-G5x 及更低 |

> **判定逻辑**：`DeviceQualityProfile::Detect()` 在初始化时执行：
> - PC / Apple → 始终 SSBO 路径
> - Android → 检查 `maxStorageBufferRange >= 128MB` 且 GPU 系列 ≥ 阈值 → SSBO，否则 VBO

#### 2.9.3 SSBO 顶点获取 vs. 传统 VBO

**SSBO 路径 (PC / Apple / Android High)**：

```glsl
// common/vertex_fetch_ssbo.glsl
#define VERTEX_FETCH_SSBO 1

layout(set = 3, binding = 18) readonly buffer VertexDataBuffer {
    float data[];   // 紧凑 float 数组，按 stride 访问
} _VertexBuffer;

layout(set = 3, binding = 19) readonly buffer IndexDataBuffer {
    uint data[];
} _IndexBuffer;

// 从 SSBO 手动获取顶点属性 (以 Layout E 为例)
struct VertexAttrib {
    vec3 position;
    vec2 texcoord;
    vec3 normal;
    vec4 tangent;
};

VertexAttrib FetchVertex(uint vertexIndex)
{
    // stride = 12 floats (3+2+3+4)
    uint base = vertexIndex * 12;
    VertexAttrib v;
    v.position = vec3(_VertexBuffer.data[base+0],
                      _VertexBuffer.data[base+1],
                      _VertexBuffer.data[base+2]);
    v.texcoord = vec2(_VertexBuffer.data[base+3],
                      _VertexBuffer.data[base+4]);
    v.normal   = vec3(_VertexBuffer.data[base+5],
                      _VertexBuffer.data[base+6],
                      _VertexBuffer.data[base+7]);
    v.tangent  = vec4(_VertexBuffer.data[base+8],
                      _VertexBuffer.data[base+9],
                      _VertexBuffer.data[base+10],
                      _VertexBuffer.data[base+11]);
    return v;
}
```

**传统 VBO 路径 (Android Mid/Low)**：

```glsl
// common/vertex_fetch_vbo.glsl
#define VERTEX_FETCH_VBO 1

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;

struct VertexAttrib {
    vec3 position;
    vec2 texcoord;
    vec3 normal;
    vec4 tangent;
};

VertexAttrib FetchVertex(uint vertexIndex)  // vertexIndex 参数忽略
{
    VertexAttrib v;
    v.position = inPosition;
    v.texcoord = inTexCoord;
    v.normal   = inNormal;
    v.tangent  = inTangent;
    return v;
}
```

**统一调用接口**：所有 VS 模板中通过 `#include` 选择路径，业务代码调用 `FetchVertex()` 即可：

```glsl
// 所有 surface VS 模板的统一写法
#if VERTEX_FETCH_SSBO
  #include "common/vertex_fetch_ssbo.glsl"
#else
  #include "common/vertex_fetch_vbo.glsl"
#endif

void main()
{
    VertexAttrib v = FetchVertex(gl_VertexIndex);
    gl_Position = ubo_camera.viewProj * ubo_object.model * vec4(v.position, 1.0);
    // ... 后续与获取方式无关
}
```

> **Pipeline 创建差异**：
> - SSBO 路径：`VkPipelineVertexInputStateCreateInfo` 为空（无 vertex binding / attribute），
>   Pipeline 只声明 VS 输入为空，顶点数据完全从 SSBO 读取
> - VBO 路径：正常声明 `VkVertexInputBindingDescription` + `VkVertexInputAttributeDescription`

#### 2.9.4 Android Mid/Low 特性裁剪

Android 中低端设备砍掉的特性（编译期 `#define` + 运行时能力检测双重门控）：

| 特性 | Android Low | Android Mid | Android High | 备注 |
|------|------------|------------|-------------|------|
| **几何获取** | VBO | VBO | SSBO | 核心差异 |
| **渲染路径** | Forward Only | Forward Only | Forward + VBuffer 可选 | VBuffer 需 Compute 能力 |
| **LOD** | 离散 LOD (CPU) | 离散 LOD (CPU) | Meshlet DAG (GPU) | |
| **裁剪** | CPU Frustum | CPU Frustum | GPU Meshlet Cull | |
| **DrawCall 方式** | vkCmdDrawIndexed | vkCmdDrawIndexed + Instancing | vkCmdDrawIndexedIndirectCount | |
| **HZB** | ❌ | ❌ | ✅ | |
| **Clustered Shading** | ❌ (≤4 灯) | ❌ (≤8 灯) | ✅ | 低端直接循环灯光 |
| **阴影 Near SM** | None | 512² PCF (可选) | 1024² PCF | Near SM 仅 Android High 开启 |
| **阴影 Cached SM** | ❌ | 1024² 静态 Cached | 2048² 静态 Cached | 环形滚动 §3.6.3 |
| **Capsule Shadow** | Blob 可选 | ✅ 角色主阴影 | ✅ 动态角色 | 替代/补充 Near SM |
| **SSR** | ❌ | ❌ | ❌ (暂不开) | 即使 High 也不默认开启 |
| **DOF / Motion Blur** | ❌ | ❌ | ❌ | 带宽敏感 |
| **Bloom** | ❌ | ✅ 简易 | ✅ 多级 | |
| **TAA** | ❌ | ✅ | ✅ | |
| **Auto Exposure** | ❌ | ✅ | ✅ | |
| **Decals** | ❌ | ❌ | ✅ | |
| **Terrain-Contact Dither** | ❌ | ✅ | ✅ | |
| **VBuffer Tile Classification** | ❌ | ❌ | ✅ | |
| **Texture Streaming** | ❌ | ✅ | ✅ | |
| **最大纹理尺寸** | 1024 | 2048 | 4096 | 降 mip level |
| **渲染分辨率** | 0.5× ~ 0.75× | 0.75× ~ 1.0× | 1.0× | 动态分辨率 |

#### 2.9.5 平台后端编译策略

Shader 变体在构建期按 **PlatformBackend × QualityTier × SurfaceType × Flags** 四维组合编译。
但实际变体数通过裁剪保持可控：

```
变体裁剪规则:
  - PC:            SSBO only → 不编译 VBO 变体，编译 Lowest ~ Cinematic 全 6 档
  - Apple:         SSBO only → 不编译 VBO 变体，编译 Lowest ~ Ultra（无 Cinematic）
  - Android High:  SSBO + 有限特性 → 编译 SSBO 路径 + Medium/High 变体
  - Android Mid:   VBO + Medium 以下特性 → 编译 VBO 路径 + Low/Medium 变体
  - Android Low:   VBO + Low 特性 → 编译 VBO 路径 + Lowest/Low 变体

编译期 #define 注入:
  #define PLATFORM_PC          / PLATFORM_APPLE     / PLATFORM_ANDROID
  #define GEOMETRY_FETCH_SSBO  / GEOMETRY_FETCH_VBO
  #define QUALITY_LOWEST        / QUALITY_LOW       / QUALITY_MEDIUM   / QUALITY_HIGH / QUALITY_ULTRA / QUALITY_CINEMATIC
  #define FEATURE_HZB          0 或 1
  #define FEATURE_CLUSTERED    0 或 1
  #define FEATURE_VBUFFER      0 或 1
  #define FEATURE_MESHLET_CULL 0 或 1
```

> **ShaderPermutationKey 扩展**（见 §4.2）：增加 `platform : 2` 位（PC/Apple/Android），
> 使 SPVCache 按平台区分缓存。GeometryFetchMode 由 platform 隐含决定。

#### 2.9.6 运行时抽象层

```cpp
// 平台几何后端接口
class GeometryBackend
{
public:
    static GeometryBackend* Create(PlatformBackend platform);

    // --- 上传几何数据 ---
    virtual void UploadMesh(MeshAsset* mesh) = 0;
        // SSBO: 写入全局 VertexDataBuffer / IndexDataBuffer SSBO
        // VBO:  创建 per-mesh VkBuffer (VBO + IBO)

    // --- 绑定几何数据 ---
    virtual void BindGeometry(VkCommandBuffer cmd, MeshAsset* mesh) = 0;
        // SSBO: 绑定全局 SSBO (已在 Set 3 descriptor 中)，无需额外操作
        // VBO:  vkCmdBindVertexBuffers + vkCmdBindIndexBuffer

    // --- DrawCall ---
    virtual void Draw(VkCommandBuffer cmd, const DrawBatch& batch) = 0;
        // SSBO + High+: vkCmdDrawIndexedIndirectCount (meshlet 粒度)
        // VBO  + Mid:        vkCmdDrawIndexed / vkCmdDrawIndexedIndirect
        // VBO  + Low:        vkCmdDrawIndexed (per-object)
};
```

> **全局 SSBO 内存管理**：PC/Apple 路径下，所有 mesh 的 vertex/index 数据上传到一个
> 大的 GPU SSBO 中（类似 UE5 的 GPUScene 概念），通过 meshlet 的 vertexOffset/indexOffset
> 定位。这消除了频繁 bind VBO 的开销，且配合 Indirect Draw 实现零 CPU 绑定切换。

---

## 3. 画质档位与光照模型

### 核心思想：表面类型 × 画质档位

美术看到的是**表面类型**（Standard、Skin、Hair ...），不会看到 BlinnPhong / PBR 这种底层概念。
引擎根据设备能力自动选画质档位（QualityTier），每个档位对应不同的光照算法和纹理需求。

### 3.1 画质档位定义

```cpp
enum class QualityTier : uint8_t
{
    Low     = 0,    // BlinnPhong FakePBR — Android Low / PC 极低配
    Medium  = 1,    // 标准 PBR (Cook-Torrance) — Android Mid / 核显/中端独显
    High    = 2,    // 完整 PBR + 高级特性 — Android High / PC 独显 / Apple
    Ultra   = 3,    // 保留：PC 高端独显，未来可能加入 Ray-Tracing 等
};
```

### 3.2 每档位的光照算法

| 档位 | 直接光照 | 环境光 | 阴影 | 纹理需求 |
|------|---------|--------|------|---------|
| **Low** | Half-Lambert + Blinn-Phong Specular | 指数天空色 (Simple) | Cached SM + Blob | BaseColor, Normal |
| **Medium** | Cook-Torrance BRDF | FakeAtmosphere | Near PCF + Cached SM | BaseColor, Normal, MetallicRoughness |
| **High** | Cook-Torrance BRDF | IBL (CubeMap) | Near PCSS + Cached SM + Contact | BaseColor, Normal, MetallicRoughness, AO, Emissive |
| **Ultra** | 同 High（预留扩展） | 同 High | 同 High | 同 High + DetailNormal 等 |

### 3.3 纹理降级策略

美术总是配置最高规格纹理集。低档位自动忽略多余纹理，引擎用缺省值替代：

```
Cinematic/Ultra: Albedo + Normal + MR + AO + Emissive + DetailNormal + ...
                                                         ↓ 低档位忽略
High:            Albedo + Normal + MR + AO + Emissive
                                             ↓ 低档位忽略
Medium:          Albedo + Normal + MetallicRoughness
                                         ↓ 低档位忽略
Low:             Albedo + Normal
                            ↓ 低档位忽略
Lowest:          Albedo only (NdotL 用顶点法线)
```

降级规则（编译期 `#define` 控制，不是运行时分支）：

| 纹理 | Lowest | Low | Medium | High | Ultra/Cinematic |
|------|--------|-----|--------|------|------------------|
| Albedo / BaseColor | ✅ 采样 | ✅ 采样 | ✅ 采样 | ✅ 采样 | ✅ 采样 |
| Normal Map | ❌ 顶点法线 | ✅ 采样 | ✅ 采样 | ✅ 采样 | ✅ 采样 |
| MetallicRoughness | ❌ MI 常量 | ❌ MI 常量 | ✅ 采样 | ✅ 采样 | ✅ 采样 |
| AO | ❌ 1.0 | ❌ 1.0 | ❌ 1.0 | ✅ 采样 | ✅ 采样 |
| Emissive | ❌ 0.0 | ❌ 0.0 | ❌ MI 常量色 | ✅ 采样 | ✅ 采样 |
| Detail Normal | ❌ | ❌ | ❌ | ❌ | ✅ 可选采样 |

### 3.4 环境光模型（内部实现，美术不可见）

```cpp
enum class AmbientModel : uint8_t
{
    Constant            = 0,    // 固定常量环境色（Lowest 档位使用）
    Simple              = 1,    // 指数梯度天空色（Low 档位使用）
    FakeAtmosphere      = 2,    // 地平线暖色调 + 大气散射（Medium 档位使用）
    IBL                 = 3,    // Image-Based Lighting（High+ 档位使用）
};
```

### 3.5 Surface Complexity LOD（材质复杂度降级）★★★

#### 3.5.1 问题

`QualityTier` 是**设备级**全局画质档位——同一台设备上所有物体使用相同的 tier。
但实际游戏场景中，**同一帧内不同物体的材质需求差异巨大**：

| 场景 | 期望复杂度 | 理由 |
|------|-----------|------|
| 贴脸对话时的主角脸部 | Cinematic: 全 SSS + 毛孔法线 + 眼球折射 + Light Probes | 占屏幕面积大、玩家注视 |
| 主角近景（5m 内） | Ultra: SSS + 全纹理 + Contact Shadow | 近距离较大面积 |
| 对话 NPC（10m 内） | High: 完整 PBR + IBL + SSAO | 中近距离，重要角色 |
| 20m 外的普通 NPC | Medium: 标准 PBR，无 SSS | 像素较少 |
| 50m 外的背景群演 | Low: BlinnPhong + 基础纹理 | 小尺寸，精度浪费 |
| 极远处装饰物/人影 | Lowest: 顶点光照 + Albedo only | 亚像素尺寸 |

如果不做 Material LOD，远处 NPC 的皮肤仍然跑完整 SSS、眼球仍然做折射采样——**GPU 白白浪费在看不见的细节上**。

#### 3.5.2 设计方案：EffectiveTier（运行时有效档位）

`EffectiveTier` = 最终用于查询 SPV 的画质档位，由三个因素取最小值：

```
EffectiveTier = min(deviceTier, objectLODTier, surfaceLODCap)
```

| 因素 | 来源 | 含义 |
|------|------|------|
| `deviceTier` | `DeviceQualityProfile::Detect()` | 设备硬件能力上限（全局） |
| `objectLODTier` | 屏幕空间大小 + 距离 + 重要性 | 每个渲染对象每帧计算（per-object） |
| `surfaceLODCap` | SurfaceType 特有规则 | 某些 SurfaceType 的特殊降级阈值 |

> `EffectiveTier` ≤ `deviceTier` 恒成立——设备不支持的功能不会被强行开启。

#### 3.5.3 objectLODTier 计算

```cpp
// 每帧、每个渲染对象执行一次
QualityTier CalcObjectLODTier(const RenderObject& obj, const Camera& cam)
{
    // 1. 屏幕空间覆盖面积（用 AABB 投影近似，单位：像素）
    float screenArea = EstimateScreenArea(obj.worldAABB, cam);

    // 2. 基于面积的基础档位
    QualityTier baseTier;
    if      (screenArea >= THRESHOLD_CINEMATIC) baseTier = Cinematic;  // 如 > 80000 px²
    else if (screenArea >= THRESHOLD_ULTRA)     baseTier = Ultra;      // 如 > 40000 px²
    else if (screenArea >= THRESHOLD_HIGH)      baseTier = High;       // 如 > 8000 px²
    else if (screenArea >= THRESHOLD_MEDIUM)    baseTier = Medium;     // 如 > 1000 px²
    else if (screenArea >= THRESHOLD_LOW)       baseTier = Low;        // 如 > 100 px²
    else                                        baseTier = Lowest;     // 极小/极远

    // 3. 重要性偏移（Hero / NPC / Prop）
    baseTier = clamp(baseTier + obj.importanceBias, Lowest, Cinematic);
    //  Hero: +2（强力偏向更高档位）
    //  MainNPC: +1
    //  Normal: 0
    //  BackgroundNPC: -1
    //  Distant: -2

    return baseTier;
}
```

阈值参数放在 `DeviceQualityProfile` 中，可按设备调优：

| 阈值 | PC | Apple | Android High | Android Mid/Low |
|------|-----|-------|-------------|------------------|
| `THRESHOLD_CINEMATIC` | 80000 px² | 60000 px² | — (不支持) | — |
| `THRESHOLD_ULTRA` | 40000 px² | 30000 px² | — (不支持) | — |
| `THRESHOLD_HIGH` | 8000 px² | 6000 px² | 4000 px² | — (不支持 High) |
| `THRESHOLD_MEDIUM` | 1000 px² | 800 px² | 500 px² | 500 px² |
| `THRESHOLD_LOW` | 100 px² | 100 px² | 100 px² | 100 px² |

> **Meshlet LOD 联动**：Meshlet LOD DAG 选择的几何精度与 Material LOD 联动——
> 当几何 LOD 已降到最粗级别时，材质也应降至 Low（反推：objectLODTier ≤ meshletLODLevel 对应的上限）。

#### 3.5.4 Surface Complexity LOD — 特殊表面分级降级

对于 Special Surface（Skin、Eye、Hair 等），6 档画质提供了更平滑的降级梯度。
每个 SurfaceType 定义自己的 **特性降级表**——高档位开启专有特性，低档位回退到 Standard 光照：

##### Skin（皮肤）

| EffectiveTier | 光照模型 | SSS | 毛孔 Detail Normal | 曲率 AO | 等价行为 |
|--------------|---------|-----|-------------------|--------|----------|
| **Cinematic** | Cook-Torrance + SSS | ✅ 全精度 SSS + 高采样 Thickness | ✅ Detail Normal + Micro-detail | ✅ 曲率贴图 + 高精度 AO | 过场特写、照片模式 |
| **Ultra** | Cook-Torrance + SSS | ✅ Pre-integrated SSS（全精度） | ✅ Detail Normal Map | ✅ 曲率贴图驱动 AO | 贴脸特写品质 |
| **High** | Cook-Torrance + SSS | ✅ 简化 SSS（无 Thickness Map，用常量） | ❌ | ❌ | 近景人物 |
| **Medium** | Cook-Torrance | ❌（退化为 Standard PBR） | ❌ | ❌ | 中景群演 — fallback Standard |
| **Low** | BlinnPhong | ❌ | ❌ | ❌ | 远景/低端 — 与 Standard Low 相同 |
| **Lowest** | 顶点 NdotL | ❌ | ❌ | ❌ | 极远处 — 仅 Albedo + 顶点光照 |

> **关键优化**：Medium 和 Low 档位的 Skin 实际执行的 shader 代码与 Standard Surface 的同档位完全一致。
> 编译器可将 Skin@Medium/Low 映射到 Standard 的 SPV 变体，避免额外编译开销。

##### Eye（眼球）

| EffectiveTier | 效果 | 虹膜折射 | 环境反射 | 焦散/SSS | 等价行为 |
|--------------|------|---------|---------|---------|----------|
| **Cinematic** | 极致眼球 | ✅ 多层 Raymarch + 色散 | ✅ Specular Probe + Fresnel | ✅ 角膜 SSS + 焦散 + 反射折射 | 过场特写 |
| **Ultra** | 全效果眼球 | ✅ Parallax Refraction（虹膜深度 raymarch） | ✅ Specular Probe 反射 | ✅ 角膜 SSS + 焦散近似 | 剧情特写 |
| **High** | 高质量 | ✅ 简化 Parallax（单层偏移） | ✅ 环境 CubeMap 采样 | ❌ | 近距离对话 |
| **Medium** | 简化 | ❌（平面 iris 纹理） | ❌（仅高光点） | ❌ | 中距离 NPC |
| **Low** | 最简 | ❌ | ❌（一个 Phong 高光） | ❌ | 远处/低端 — iris 只是 Albedo |
| **Lowest** | 最简 | ❌ | ❌ | ❌ | 极远处 — 仅颜色点 + 顶点光照 |

##### Hair（头发）

| EffectiveTier | 高光模型 | 双高光 | 各向异性 | 半透明 | 等价行为 |
|--------------|---------|-------|---------|-------|----------|
| **Cinematic** | Marschner 双高光 | ✅ R + TT + 色散 | ✅ Shift Map + 高精度 | ✅ Alpha2Coverage | 近距离特写，头发丝维感 |
| **Ultra** | Marschner 双高光 | ✅ R + TT 通道 | ✅ Shift Map 控制 | ✅ Alpha2Coverage | 近距离特写 |
| **High** | Kajiya-Kay 双高光 | ✅ 简化双高光 | ✅ 固定 shift | ✅ Alpha2Coverage | 近距离 |
| **Medium** | 单高光 PBR | ❌（单高光） | ❌ | ✅ AlphaTest | 中距离 |
| **Low** | BlinnPhong | ❌ | ❌ | ✅ AlphaTest | 远距离/低端 |
| **Lowest** | 无高光 | ❌ | ❌ | ✅ AlphaTest | 极远处 — 纯颜色填充 |

##### ClearCoat（车漆/清漆）

| EffectiveTier | 模型 | 双层 BRDF | 清漆法线 | 等价行为 |
|--------------|------|----------|---------|----------|
| **Cinematic** | 双层 Cook-Torrance + Fresnel 过渡 | ✅ Base + ClearCoat + 高精度 | ✅ 独立法线 + Micro-flake | 极致车漆效果 |
| **Ultra/High** | 双层 Cook-Torrance | ✅ Base + ClearCoat | ✅ 独立法线 | 完整效果 |
| **Medium** | 单层 PBR + 额外高光 | ❌（单层近似） | ❌ | 近似效果 |
| **Low** | BlinnPhong + 高 specular | ❌ | ❌ | 有光泽感但无双层 |
| **Lowest** | 顶点光照 + 高 specular | ❌ | ❌ | 基础光泽 |

##### Foliage（植被）

| EffectiveTier | 透光 | 风动画 | BlendMode | 等价行为 |
|--------------|------|-------|-----------|----------|
| **Cinematic/Ultra** | ✅ Thin Translucency + 背光散射 | ✅ 多层顶点 Wind + 细节摸 | A2C | 完整效果 |
| **High** | ✅ Thin Translucency | ✅ 顶点 Wind | A2C | 近距离植被 |
| **Medium** | ❌（背面用 wrap lighting 替代） | ✅ 简化 Wind | AlphaTest | 中距离 |
| **Low** | ❌ | ❌ | AlphaTest | 远距离静态 |
| **Lowest** | ❌ | ❌ | AlphaTest | 极远处 — 静态平板 |

#### 3.5.5 Compositor 集成

`EffectiveTier` 在 Compositor 层面透明工作——

**编译期**：每个 Special Surface Function 内部使用 `#if QUALITY_TIER >= N` 裁剪特性。
编译器为每个 tier 生成对应变体（与 Standard 相同机制）。

**运行时**：渲染循环中用 `EffectiveTier`（而非全局 `deviceTier`）查询 SPV：

```cpp
// 原来：
// ShaderPermutationKey key = device_profile.ToPermutationKey(mi->surface_type);

// 现在：
QualityTier effectiveTier = CalcObjectLODTier(obj, cam);
effectiveTier = min(effectiveTier, device_profile.tier);  // 不超过设备上限
ShaderPermutationKey key = BuildPermutationKey(mi->surface_type, effectiveTier, ...);
auto [spv_vs, spv_fs] = SPVCache.Get(mi->preset_id, key, pass_type);
VkPipeline pipeline = PipelineCache.GetOrCreate(mi->preset_id, key, pass_type, render_pass);
```

> **Pipeline 切换代价**：同一 SurfaceType 不同 `EffectiveTier` 需要不同 Pipeline。
> 为避免过多 Pipeline 切换，渲染排序时先按 (SurfaceType, EffectiveTier, PassType) 分组，
> 再按材质/纹理排序——同档位的物体批量绘制。
> 注意 6 档的分组比 4 档多，但实际上同一帧内大部分物体集中在 2~3 个档位，
> Pipeline 切换增量有限。

#### 3.5.6 回退等价性 — SPV 复用

当 Special Surface 降级到某个 tier 后行为与 Standard 相同时，可以**直接复用 Standard 的 SPV 变体**：

```
Skin  @ Lowest/Low/Medium  → 复用 Standard @ Lowest/Low/Medium SPV
Eye   @ Lowest/Low         → 复用 Standard @ Lowest/Low SPV
Hair  @ Lowest/Low         → 复用 Standard @ Lowest/Low SPV（+ AlphaTest flag）
```

这由 `MaterialPresetDef` 中的 `fallback_surface_type` 和 `fallback_min_tier` 配置：

```cpp
struct MaterialPresetDef {
    SurfaceType  surface_type;
    BlendMode    blend_mode;
    QualityTier  min_tier;              // 该材质支持的最低档位
    QualityTier  max_tier;              // 最高档位
    // ★ Material LOD 回退
    SurfaceType  fallback_surface_type; // 低于 unique_feature_min_tier 时回退到此类型
    QualityTier  unique_feature_min_tier; // 该 SurfaceType 独有特性的最低档位
    // ... 其他字段
};

// 例：Skin
// fallback_surface_type = Standard, unique_feature_min_tier = High (3)
// → Skin @ Cinematic/Ultra/High: 用 Skin SPV（含 SSS）
// → Skin @ Medium/Low/Lowest: 用 Standard SPV（与 Standard 完全相同）
```

> **编译节省**：Skin 不需要编译 Lowest/Low/Medium 的独立变体（直接指向 Standard SPV），
> 减少 ~50% 的 Special Surface 编译量。Cinematic 变体只在 PC 平台编译。

#### 3.5.7 重要性偏置（Importance Bias）

由游戏逻辑设置，影响 objectLODTier 计算：

```cpp
enum class ObjectImportance : int8_t
{
    Hero          = +2,   // 玩家控制的主角 — 最高优先
    MainNPC       = +1,   // 剧情 NPC、对话对象 — 偏向高档
    Normal        =  0,   // 普通物体
    BackgroundNPC = -1,   // 背景群演 — 偏向低档
    Distant       = -2,   // 强制最低（如极远处装饰物）
};
```

> 重要性偏移与 6 档配合，`clamp(baseTier + bias, Lowest, Cinematic)` 提供了充分的调节空间。

典型使用场景：

| 游戏事件 | 引擎行为 |
|---------|----------|
| 进入对话镜头 | 对话 NPC 的 `importanceBias` 设为 `MainNPC (+1)` → 面部/眼球升至 High/Ultra |
| 对话结束 | 恢复 `Normal (0)` → 随距离自动降级 |
| 过场动画主角特写 | `Hero (+2)` → 强制 Ultra/Cinematic（不受距离影响，直到 deviceTier 上限） |
| 大量背景士兵 | 全部 `BackgroundNPC (-1)` → 即使近了也只到 Medium/High |
| 照片模式 | 全场景 `importanceBias += 2` → 尽可能升至 Cinematic |

### 3.6 阴影系统（内部实现，美术不可见）★

引擎的阴影系统由多个独立来源组成，最终全部合成到 **ShadowMask RT** 中统一使用。

#### 3.6.1 阴影来源枚举

```cpp
// 阴影采样质量
enum class ShadowFilterMode : uint8_t
{
    None    = 0,    // 无阴影
    PCF     = 1,    // Percentage Closer Filtering
    PCSS    = 2,    // Percentage Closer Soft Shadows
};

// Shadow Map 更新策略
enum class ShadowUpdateMode : uint8_t
{
    FullDynamic     = 0,    // 每帧全量渲染（近景 cascade）
    CachedToroidal  = 1,    // 环形滚动缓存，增量更新（远景 cascade）
    Static          = 2,    // 完全静态，仅 dirty 时更新
};
```

#### 3.6.2 阴影来源与 ShadowMask 整合

所有阴影来源最终写入同一张 **ShadowMask RT (RGBA8)**，不同通道存储不同来源：

| 通道 | 内容 | 来源 |
|------|------|------|
| R | 主平行光 Shadow（近景 Dynamic Cascade + 远景 Cached） | Shadow Map |
| G | 假阴影 / 胶囊阴影（动态物体简易阴影） | Capsule Shadow / Blob Shadow |
| B | Contact Shadow（接触阴影） | 屏幕空间 ray-march |
| A | 预留（点光源/聚光阴影 或 额外通道） | — |

Forward Lit / VBuffer Resolve 中只需采样 ShadowMask 一次：

```glsl
vec4 shadowMask = texture(ShadowMaskRT, screenUV);
float sunShadow     = shadowMask.r;    // 主平行光
float capsuleShadow = shadowMask.g;    // 假阴影/胶囊阴影
float contactShadow = shadowMask.b;    // 接触阴影
float finalShadow   = min(sunShadow, min(capsuleShadow, contactShadow));
litColor *= finalShadow;
```

#### 3.6.3 双层 Shadow Map 架构 — Dynamic Near + Cached Far ★

主平行光使用两层 Shadow Map，共享一张物理 Atlas（或分开为两张纹理），开发者可调参数控制：

```
┌─────────────────────────────────────────────────────────────────┐
│ Layer 0: Dynamic Near Cascade (全动态)                          │
│   - 覆盖范围: 相机周围 R_near (可调, 默认 ~50m)                 │
│   - 更新频率: 每帧全量重新渲染                                   │
│   - 渲染对象: 动态 + 静态 所有 shadowcaster                     │
│   - 分辨率: PC 2048², Android High 1024², Android Mid 512²     │
│   - 滤波: PCSS (High+) / PCF (Medium)                          │
│   - PC: 所有近处物体走此层                                      │
│   - 手机: 仅近处动态物体 (甚至可替换为 Capsule Shadow)           │
├─────────────────────────────────────────────────────────────────┤
│ Layer 1: Cached Far Cascade (环形滚动缓存) ★                    │
│   - 覆盖范围: R_near ~ R_far (可调, 默认 ~200m, ≈ 2× 屏幕范围) │
│   - 更新频率: 仅相机移动超过 tile 步长时更新新露出的 tile strip   │
│   - 渲染对象: 仅静态 shadowcaster (标记为 StaticShadow)         │
│   - 分辨率: PC 4096², Android High 2048²                      │
│   - Toroidal Addressing: UV 取模环形复用                       │
│   - 相机不移动 → 零渲染开销                                    │
└─────────────────────────────────────────────────────────────────┘
```

**Toroidal Scrolling 原理**：

```
相机向右移动 ΔX > tileWorldSize:

  Shadow Map 纹素 (不移动纹理内容!):
  ┌──────┬──────────────┐
  │ stale│  cached      │
  │ 旧列 │  (仍然有效)   │
  ├──────┤              │
  │ 重渲 │              │    只渲染 stale 列 → 写入新的深度
  │ 新列 │              │    采样坐标 = (worldXZ - origin) / extent
  └──────┴──────────────┘    自动 fract() 环形寻址
```

**Tile 粒度更新**：远景 Cached Shadow Map 划分为 tile grid（如 16×16 tile = 每 tile 256² texel @4096²）。
相机移动超过 `tileWorldSize = shadowExtent / tileCount` 时，整列/整行 tile 标记为 stale，下帧重新渲染：

```cpp
struct CachedShadowMap
{
    vec2    origin;         // 当前 SM 左下角的世界 XZ
    float   extent;         // SM 覆盖的世界范围
    ivec2   scrollOffset;   // 环形偏移 (tile 坐标)
    uint    tileCount;      // 每轴 tile 数 (16)
    uint32_t tileDirtyMask[16]; // 每行一个 bitmask, bit=1 表示该 tile 需重渲

    void OnCameraMove(vec2 newCenterXZ);
    void OnStaticObjectChanged(AABB worldBounds); // 场景编辑/流式加载
    bool IsTileDirty(uint tx, uint ty) const;
};
```

**采样 GLSL**：

```glsl
// shadow_cached.glsl
uniform vec2  u_cachedSM_origin;
uniform float u_cachedSM_extent;

vec2 WorldToCachedShadowUV(vec3 worldPos)
{
    vec2 localXZ = (worldPos.xz - u_cachedSM_origin) / u_cachedSM_extent;
    return fract(localXZ);  // toroidal wrap → [0,1)
}

float SampleCachedShadow(vec3 worldPos, float receiverDepth)
{
    vec2 uv = WorldToCachedShadowUV(worldPos);
    float shadowDepth = texture(CachedShadowMapTex, uv).r;
    return (receiverDepth <= shadowDepth + bias) ? 1.0 : 0.0;  // 可加 PCF
}
```

**ShadowMask Compose 中的双层合并**：

```glsl
// shadow_mask_compose.comp.glsl
float nearShadow = SampleDynamicCascadeShadow(worldPos, depth);  // Layer 0
float farShadow  = SampleCachedShadow(worldPos, lightSpaceDepth); // Layer 1

// 根据距离混合
float t = smoothstep(R_near * 0.85, R_near, distFromCamera);
float sunShadow = mix(nearShadow, farShadow, t);

shadowMask.r = sunShadow;
```

#### 3.6.4 Capsule Shadow / Blob Shadow（假阴影）

对于不适合走 Shadow Map 的场景（手机近景动态角色、远处小型 NPC），使用解析几何阴影：

| 类型 | 原理 | 适用场景 | 开销 |
|------|------|---------|------|
| **Capsule Shadow** | 胶囊体 (head+body+legs) 解析遮挡计算 | 人形角色 | 极低，per-pixel 纯数学 |
| **Blob Shadow** | 地面贴圆形/椭圆形衰减纹理 | 远处 NPC、小型动态物体 | 极低，一个 quad draw |
| **Sphere Shadow** | 球体解析遮挡 | 简单动态物体 | 极低 |

```glsl
// capsule_shadow.glsl
struct CapsuleShadowData {
    vec3 capsuleA;      // 胶囊体端点 A (世界空间)
    vec3 capsuleB;      // 胶囊体端点 B
    float radius;       // 胶囊体半径
    float shadowLength; // 投影长度 (沿光源方向)
};

// Compute Shader 中逐像素计算:
// 反算世界坐标 → 计算像素到胶囊体投影射线的距离 → smoothstep 衰减
float EvalCapsuleShadow(vec3 worldPos, vec3 lightDir, CapsuleShadowData cap)
{
    // 沿 lightDir 投影胶囊体 → 计算 worldPos 到投影胶囊体的最近距离
    vec3 projA = cap.capsuleA + lightDir * cap.shadowLength;
    vec3 projB = cap.capsuleB + lightDir * cap.shadowLength;
    float dist = DistPointToSegment(worldPos, projA, projB);
    float projRadius = cap.radius * (1.0 + cap.shadowLength * 0.02); // 远处略扩
    return smoothstep(projRadius * 0.5, projRadius, dist);
}
```

Capsule Shadow 结果写入 ShadowMask.g 通道。

#### 3.6.5 Contact Shadow（接触阴影, High+）

屏幕空间 ray-march，为小尺度接触处提供高频阴影细节（Shadow Map 分辨率不够的地方）：

```glsl
// shadow_contact.glsl
// 从 pixel 出发沿 light direction 在屏幕空间步进
// 检测是否被前方深度遮挡
float ComputeContactShadow(vec2 screenUV, float linearDepth, vec3 lightDirVS)
{
    const int MAX_STEPS = 16;
    const float STEP_SIZE = 0.005;  // 屏幕空间步长 (可调)

    vec2 rayUV = screenUV;
    float rayDepth = linearDepth;

    for (int i = 0; i < MAX_STEPS; i++)
    {
        rayUV    += lightDirVS.xy * STEP_SIZE;
        rayDepth += lightDirVS.z  * STEP_SIZE * linearDepth;

        float sceneDepth = LinearizeDepth(texture(DepthRT, rayUV).r);
        if (rayDepth > sceneDepth && rayDepth - sceneDepth < THICKNESS)
            return 0.0;  // 被遮挡
    }
    return 1.0;  // 无遮挡
}
```

Contact Shadow 结果写入 ShadowMask.b 通道。

#### 3.6.6 各平台阴影策略总览

| 平台 / 档位 | Near Cascade | Far Cached SM | Capsule/Blob | Contact Shadow | ShadowMask |
|------------|--------------|---------------|-------------|----------------|------------|
| **PC Cinematic** | 4096² 全动态 PCSS | 4096² Cached | 可选 | ✅ | RGBA8 全通道 |
| **PC Ultra** | 2048² 全动态 PCSS | 4096² Cached | 可选 (远处NPC) | ✅ | RGBA8 全通道 |
| **PC High** | 2048² 全动态 PCSS | 4096² Cached | 可选 | ✅ | RGBA8 |
| **PC Medium** | 1024² 全动态 PCF | 2048² Cached | ❌ | ❌ | RG8 (R=sun) |
| **PC Low** | 512² PCF 或关闭 | 1024² Cached (可选) | Blob | ❌ | R8 |
| **PC Lowest / iGPU** | ❌ | ❌ | Blob (可选) | ❌ | R8 或无 |
| **Apple Ultra** | 1024² PCSS | 2048² Cached | 可选 | ✅ | RGBA8 |
| **Apple High** | 1024² PCSS | 2048² Cached | 可选 | ✅ | RGBA8 |
| **Android High** | 1024² PCF | 2048² Cached (静态) | ✅ 近处动态角色 | ❌ | RG8 |
| **Android Mid** | ❌ 或 512² PCF ※ | 1024² Cached (静态) | ✅ 角色主阴影 | ❌ | RG8 |
| **Android Low** | ❌ | ❌ | Blob (可选) | ❌ | R8 或无 |
| **Android Lowest** | ❌ | ❌ | ❌ | ❌ | ❌ |

> ※ Android Mid 近处动态物体阴影：优先用 Capsule Shadow 替代实时 Shadow Map，省去 ShadowMap Pass 开销。
> 开发者可通过配置强制开启 Near SM（如 Intel iGPU 场景）。

#### 3.6.7 开发者可调参数

```cpp
struct ShadowConfig
{
    // --- Near Dynamic Cascade ---
    bool     enableNearCascade      = true;      // 是否开启近景动态 SM
    float    nearCascadeRadius      = 50.0f;     // 近景覆盖半径 (m)
    uint32_t nearCascadeResolution  = 2048;       // SM 分辨率 (512/1024/2048)
    ShadowFilterMode nearFilter     = PCSS;       // PCF / PCSS

    // --- Far Cached Cascade ---
    bool     enableFarCached        = true;       // 是否开启远景缓存 SM
    float    farCascadeRadius       = 200.0f;     // 远景覆盖半径 (m)
    uint32_t farCascadeResolution   = 4096;       // SM 分辨率 (1024/2048/4096)
    uint32_t tileCount              = 16;         // 每轴 tile 数
    bool     farCascadeStaticOnly   = true;       // 仅渲染静态物体

    // --- Capsule / Blob Shadow ---
    bool     enableCapsuleShadow    = false;      // 平台默认: Android=true
    float    capsuleMaxDistance     = 30.0f;      // 胶囊阴影最大距离
    uint32_t maxCapsuleCount        = 8;          // 同时最多几个胶囊阴影

    // --- Contact Shadow ---
    bool     enableContactShadow    = false;      // 平台默认: High+=true
    int      contactShadowSteps     = 16;         // ray-march 步数
    float    contactShadowLength    = 0.1f;       // 最大 trace 距离

    // --- Near/Far 混合 ---
    float    cascadeBlendWidth      = 0.15f;      // 近/远过渡带占 nearRadius 比例
};
```

> **美术不可见**——这些参数由引擎根据 `DeviceQualityProfile` 自动配置默认值，
> 程序/TA 可在项目设置或 ini 文件中覆盖。即使是 Intel iGPU 也能通过调整参数启用完整阴影。

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
    QualityTier     quality     : 3;    // 0-5，画质档位（Lowest~Cinematic，引擎自动选）
    ShadowMode      shadow      : 2;    // 0-2，阴影模式（由 quality 决定）
    // ---- byte boundary ----
    uint8_t         flags       : 3;    // bit0: alpha_test
                                        // bit1: double_sided
                                        // bit2: vertex_color_blend
    PlatformBackend platform    : 2;    // 0=PC, 1=Apple, 2=Android (§2.9) ★
    uint8_t         _reserved   : 2;

    uint16_t ToU16() const;

    // GeometryFetchMode 由 platform 隐含决定，不占 key 位
    // PC/Apple → SSBO, Android → 运行时检测 (High=SSBO, Mid/Low=VBO)
};
```

### 4.3 BlendMode 与 PassType（Compositor 用）

```cpp
// ===== 混合模式（材质预设属性，决定可生成哪些 Pass 变体）=====
enum class BlendMode : uint8_t
{
    Opaque      = 0,    // 不透明
    Masked      = 1,    // Alpha Test 遮罩（discard）
    Transparent = 2,    // Alpha Blend 半透明
    Dither      = 3,    // Bayer Dither 替代半透明（LOD fade-out、terrain contact 等）
    A2C         = 4,    // Alpha-to-Coverage（MSAA 硬件子样本遮罩：植被、铁丝网等）
};

// ===== Pass 类型（决定 Compositor 选哪个 main() 模板）=====
enum class PassType : uint8_t
{
    ForwardOpaque       = 0,
    ForwardMasked       = 1,
    ForwardTransparent  = 2,
    ForwardDither       = 3,
    ForwardA2C          = 4,
    ShadowOpaque        = 5,    // 仅 VS depth-write，无 FS
    ShadowMasked        = 6,    // EvalAlpha() → discard
    VBufferID           = 7,    // 写 ID，不调用 Surface Function
    VBufferResolve      = 8,    // Compute: EvalSurface() → 光照
    TerrainContactDither= 9,    // Terrain 接地 dither 变体
};
```

> **SPV Cache Key** = (`preset_id`, `quality_tier`, `shadow_mode`, `flags`, `platform`, **`pass_type`**) → SPV\
> PassType 不在 `ShaderPermutationKey` 中——同一个材质同时需要多种 Pass 变体（如 ForwardOpaque + ShadowOpaque + VBufferID）。\
> Compositor 根据 `BlendMode` 自动决定生成哪些 `PassType`（详见 §7.6）。

### 4.4 有效排列矩阵

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
| 20 | Skin | 角色皮肤 | ✅ 已设计 | SubsurfaceColor, Thickness, CurvatureMap, DetailNormal — Material LOD §3.5.4 |
| 21 | Hair | 角色头发 | ✅ 已设计 | HairDirection, ShiftMap, AnisotropyRotation — Material LOD §3.5.4 |
| 22 | Cloth | 布料 | ✅ 已设计 | SheenColor, SheenRoughness — Material LOD §3.5.4 |
| 23 | Eye | 眼球 | ✅ 已设计 | IrisTexture, IrisDepth, RefractionIndex, CorneaSSS — Material LOD §3.5.4 |
| 24 | Foliage | 植被/树叶 | ✅ 已设计 | TranslucencyColor, Thickness, WindParams — Material LOD §3.5.4 |
| 25 | ClearCoat | 车漆/瓷釉 | ✅ 已设计 | ClearCoatRoughness, ClearCoatNormal — Material LOD §3.5.4 |
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

#### Standard Surface Function（纯业务逻辑 — 无 main()）

> **核心变化**：Surface Function 只定义材质表面属性的计算，不包含 `main()`、不做光照、不写输出。\
> `main()` 由 Compositor 模板自动提供（详见 §7）。\
> 光照计算统一在 Compositor 的 `EvalLighting()` 中完成（详见 §7.5）。

```glsl
// surface/standard_surface.glsl
// ★ Surface Function — 仅包含材质计算逻辑，不含 main()
//
// 编译期 #define:
//   QUALITY_TIER  0=Lowest, 1=Low, 2=Medium, 3=High, 4=Ultra, 5=Cinematic
//   HAS_VERTEX_COLOR  0/1

#include "common/surface_interface.glsl"     // SurfaceInput / SurfaceOutput 结构体
#include "common/material_instance.glsl"
#include "common/normal_mapping.glsl"

// ===== 纹理采样（按档位编译期裁剪）=====
vec4  SampleAlbedo(vec2 uv)            { return texture(TextureAlbedo, uv); }

#if QUALITY_TIER >= 1  // Low+: Normal Map
vec3  SampleNormal(vec2 uv)            { return texture(TextureNormal, uv).xyz * 2.0 - 1.0; }
#endif

#if QUALITY_TIER >= 2  // Medium+
vec2  SampleMetallicRoughness(vec2 uv) { return texture(TextureMetallicRoughness, uv).bg; }
#endif

#if QUALITY_TIER >= 3  // High+
float SampleAO(vec2 uv)               { return texture(TextureAO, uv).r; }
vec3  SampleEmissive(vec2 uv)          { return texture(TextureEmissive, uv).rgb; }
#endif

// ===== 完整表面求值 — 供 Forward / VBuffer Resolve 等色彩 Pass 使用 =====
SurfaceOutput EvalSurface(SurfaceInput si)
{
    MI_Standard mi = GetMI();
    SurfaceOutput o;

    // Albedo
    vec4 albedo = SampleAlbedo(si.uv) * unpackUnorm4x8(mi.base_color);
#if HAS_VERTEX_COLOR
    albedo.rgb *= si.vertexColor.rgb;
#endif
    o.baseColor = albedo.rgb;
    o.alpha     = albedo.a;

    // Normal（法线贴图后的世界空间法线）
#if QUALITY_TIER >= 1  // Low+: 有 Normal Map
    o.normal = ApplyNormalMap(SampleNormal(si.uv), si.worldNormal, si.tangent,
                              mi.normal_strength);
#else
    // Lowest: 仅用顶点法线
    o.normal = si.worldNormal;
#endif

    // Material Properties（按 QualityTier 编译期裁剪）
#if QUALITY_TIER <= 1
    // Lowest/Low: 无 MR 纹理，直接使用参数
    o.metallic  = mi.metallic_factor;
    o.roughness = mi.roughness_factor;
    o.ao        = 1.0;
    o.emissive  = vec3(0.0);
#elif QUALITY_TIER == 2
    // Medium: 有 MR 纹理
    vec2 mr     = SampleMetallicRoughness(si.uv);
    o.metallic  = mr.x * mi.metallic_factor;
    o.roughness = mr.y * mi.roughness_factor;
    o.ao        = 1.0;
    o.emissive  = vec3(0.0);
#else
    // High/Ultra/Cinematic: 完整纹理集
    vec2 mr     = SampleMetallicRoughness(si.uv);
    o.metallic  = mr.x * mi.metallic_factor;
    o.roughness = mr.y * mi.roughness_factor;
    o.ao        = SampleAO(si.uv) * mi.ao_strength;
    o.emissive  = SampleEmissive(si.uv)
                  * unpackUnorm4x8(mi.emissive_color).rgb
                  * mi.emissive_intensity;
#endif

    return o;
}

// ===== 轻量 Alpha 求值 — 供 ShadowMap Masked Pass 使用（无需完整表面计算）=====
float EvalAlpha(vec2 uv)
{
    MI_Standard mi = GetMI();
    vec4 albedo = SampleAlbedo(uv) * unpackUnorm4x8(mi.base_color);
    return albedo.a;
}
```

> **为什么分两个函数？**\
> ShadowMap 对 Masked 物体只需判断 alpha 做 `discard`，不需要法线/金属度/粗糙度等属性。\
> `EvalAlpha()` 省去多次纹理采样和法线映射计算，ShadowMap Pass 性能更佳。\
> 不透明物体的 ShadowMap Pass 甚至无需 FS（纯深度输出），Surface Function 完全不参与。

### 5.3 Special Surface Function 降级示例（配合 §3.5）

以下示例展示 Skin 和 Eye 的 Surface Function 如何根据 `QUALITY_TIER` define 在编译期裁剪特性。

#### skin_surface.glsl

```glsl
// skin_surface.glsl — Skin Surface Function
// 由 Compositor 根据 PassType 注入 main()

#include "surface_output.glsl"

// ---- SurfaceOutputExt: SSS 额外字段 ----
#if QUALITY_TIER >= 3   // High+
struct SurfaceOutputExt {
    float  sssStrength;       // SSS 强度
    float  sssThickness;      // 透射厚度 (Ultra/Cinematic 从贴图读, High 用常量)
    float  curvatureAO;       // 曲率 AO (Ultra/Cinematic only)
};
#endif

SurfaceOutput EvalSurface(in vec2 uv, in mat3 TBN)
{
    SurfaceOutput o;

    // 基础 PBR 属性（所有档位都有）
    o.albedo    = texture(tex_albedo, uv).rgb;
    o.metallic  = 0.0;     // 皮肤固定非金属
    o.roughness = texture(tex_roughness, uv).r;

#if QUALITY_TIER >= 1   // Low+: Normal Map
    o.normal    = TBN * (texture(tex_normal, uv).xyz * 2.0 - 1.0);
#else
    // Lowest: 仅顶点法线
    o.normal    = TBN[2]; // z-axis = vertex normal
#endif

#if QUALITY_TIER >= 4   // Ultra/Cinematic: 毛孔 Detail Normal
    vec3 detailN = texture(tex_detail_normal, uv * detail_uv_scale).xyz * 2.0 - 1.0;
    o.normal = BlendNormals(o.normal, TBN * detailN);
#endif

    o.ao = texture(tex_ao, uv).r;

#if QUALITY_TIER >= 4   // Ultra/Cinematic: 曲率驱动 AO
    o.ext.curvatureAO = texture(tex_curvature, uv).r;
    o.ao *= o.ext.curvatureAO;
#endif

#if QUALITY_TIER >= 3   // High+: SSS 参数
    o.ext.sssStrength = u_sss_strength;
  #if QUALITY_TIER >= 4 // Ultra/Cinematic: 从 Thickness Map 读
    o.ext.sssThickness = texture(tex_thickness, uv).r;
  #else                 // High: 常量近似
    o.ext.sssThickness = u_sss_thickness_const;
  #endif
#endif

    // Medium/Low/Lowest: 不输出 SSS 字段 → 退化为 Standard PBR / BlinnPhong / 顶点光照
    return o;
}

float EvalAlpha(in vec2 uv)
{
    return 1.0;  // 皮肤始终不透明
}
```

> **Skin @ Medium/Low/Lowest** 的 `EvalSurface()` 只输出基础字段，
> 与 Standard Surface 完全一致——编译器可以 fallback 到 Standard SPV（§3.5.6）。

#### eye_surface.glsl

```glsl
// eye_surface.glsl — Eye Surface Function

#include "surface_output.glsl"

SurfaceOutput EvalSurface(in vec2 uv, in mat3 TBN)
{
    SurfaceOutput o;

#if QUALITY_TIER >= 5   // Cinematic: 多层 Raymarch + 色散 + 全效果
    vec2 irisUV = MultiLayerRefraction(uv, TBN, u_iris_depth, u_ior, u_chromatic_aberration);
    o.albedo = texture(tex_iris, irisUV).rgb;
    o.ext.sssStrength = u_cornea_sss;
    o.ext.caustic = ComputeIrisCaustic(irisUV, u_light_dir);
    o.emission = textureLod(tex_env_cubemap, reflect(-viewDir, o.normal), 0.5).rgb
               * u_env_reflection_strength * FresnelSchlick(NdotV, 0.04);

#elif QUALITY_TIER >= 4 // Ultra: 虹膜 Parallax Refraction (raymarch)
    vec2 irisUV = ParallaxRefraction(uv, TBN, u_iris_depth, u_ior);
    o.albedo = texture(tex_iris, irisUV).rgb;
    // 角膜 SSS + 焦散
    o.ext.sssStrength = u_cornea_sss;
    o.ext.caustic = ComputeIrisCaustic(irisUV, u_light_dir);

#elif QUALITY_TIER >= 3 // High: 简化 Parallax (单层偏移)
    vec2 irisUV = SimpleParallaxOffset(uv, TBN, u_iris_depth);
    o.albedo = texture(tex_iris, irisUV).rgb;
    // 环境 CubeMap 反射
    o.emission = textureLod(tex_env_cubemap, reflect(-viewDir, o.normal), 2.0).rgb
               * u_env_reflection_strength;

#elif QUALITY_TIER >= 2 // Medium: 平面 iris 纹理 + 标准 PBR
    o.albedo = texture(tex_iris, uv).rgb;
    o.roughness = 0.3;
    o.metallic  = 0.0;

#elif QUALITY_TIER >= 1 // Low: 最简 — albedo + 一个 Phong 高光点
    o.albedo = texture(tex_iris, uv).rgb;
    o.roughness = 0.15;  // 低粗糙度 → 窄高光
    o.metallic  = 0.0;

#else                   // Lowest: 仅颜色
    o.albedo = texture(tex_iris, uv).rgb;
    o.roughness = 0.5;
    o.metallic  = 0.0;
#endif

    o.normal = TBN * (texture(tex_normal, uv).xyz * 2.0 - 1.0);
    o.ao = 1.0;

    return o;
}

float EvalAlpha(in vec2 uv)
{
    return 1.0;  // 眼球不透明
}
```

> **Eye @ Lowest/Low** 就是一个 albedo 贴图 + 一个窄高光——几乎和 Standard 一样；
> **Eye @ Cinematic** 多层折射 + 色散 + Fresnel 反射，品质堆满，专为过场特写。

### 5.4 每个预设的 MaterialInstance 结构

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
│  PlatformBackend  platform                                │ ← PC / Apple / Android (§2.9)
│  GeometryFetchMode geometry_fetch                         │ ← SSBO 或 VBO (由 platform+能力决定)
│  QualityTier      tier                                    │ ← 根据 GPU 能力自动判定
│  AmbientModel     ambient                                 │
│  ShadowMode       shadow                                  │
│  bool             supports_bindless                       │
│  bool             supports_compute                        │
│  bool             supports_meshlet_pipeline               │ ← SSBO + High+
│  bool             supports_vbuffer                        │ ← SSBO + compute 能力
│  uint32_t         max_texture_units                       │
│  uint32_t         max_ssbo_range                          │ ← maxStorageBufferRange
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
  binding 1: ShadowMap_Near      (sampler2DShadow — 近景 Dynamic Cascade, §3.6.3)
  binding 2: ShadowMask          (sampler2D, RGBA8 — ShadowMask Compose 输出, §3.6.2)
  binding 3: SSAO_RT             (sampler2D, R8 — SSAO/SSDO Pass 输出)
  binding 4: IBL_Irradiance      (samplerCube)
  binding 5: IBL_Prefiltered     (samplerCube)
  binding 6: IBL_BRDF_LUT        (sampler2D)
  binding 7: SSS_LUT             (sampler2D, Skin 预留)
  binding 8: DebugLightingConfig  (UBO — 仅 Gizmo3D/Debug 材质绑定，见 2.5 节)
  binding 9: HZB_Pyramid          (sampler2D, R32F mipmap — SSR Hi-Z Trace 读取) ★
  binding 10: ClusterLightList    (SSBO — Clustered Shading 灯光索引)            ★
  binding 11: ClusterAABB         (SSBO — 预计算的 Cluster AABB, compute 写入)   ★
  binding 12: FogParams           (UBO — 雾效参数, 见 2.7 节)                    ★
  binding 13: SSR_RT              (sampler2D, RGBA16F — SSR 输出)                ★
  binding 14: ExposureData        (UBO/SSBO — auto exposure value, 1 float)      ★
  binding 15: MeshletBuffer       (SSBO — 全场景 meshlet 元数据, §2.8)            ★
  binding 16: InstanceBuffer      (SSBO — 全场景实例 transform+bounds+flags)       ★
  binding 17: TerrainHeightMap    (sampler2D — terrain contact dither 用)          ★
  binding 18: VertexDataBuffer    (SSBO — 全场景顶点数据池, §2.9.3)               ★ SSBO平台
  binding 19: IndexDataBuffer     (SSBO — 全场景索引数据池, §2.9.3)               ★ SSBO平台
  binding 20: ShadowMap_Cached    (sampler2DShadow — 远景 Cached Toroidal SM, §3.6.3)  ★
  binding 21: CapsuleShadowData   (SSBO — 胶囊阴影参数, §3.6.4, maxCount × 48B)       ★
```

> **纹理槽设计策略**：binding 1-6 是 Standard Surface 的纹理（按需求频率排列），
> binding 7-12 是 Special Surface 扩展。同一个 binding 不同表面类型复用（靠 SurfaceType 区分语义）。
> 编译器根据 SurfaceType 和 QualityTier 仅声明实际使用的 binding。

> **平台条件 binding**：Set 3 binding 15-19 仅在 SSBO 平台 (PC/Apple/Android High) 上使用。
> Android Mid/Low (VBO 路径) 的 Set 3 不包含 binding 15-19（对应 meshlet 和全局几何 buffer），
> 这些 binding 在 VBO 变体的 GLSL 中不声明，不影响 Descriptor Set Layout 兼容性
> （VBO 平台使用独立的 Pipeline Layout）。

#### VBuffer Tile Classification 专用 GPU 资源

> 以下资源由 VBuffer 管线内部 Compute Shader 自行绑定，不经过上述 4 个全局 Set，
> 在帧开始时分配（或常驻），帧结束后可选释放。

| 资源 | 格式 | 大小 | 用途 |
|------|------|------|------|
| TileSurfaceMask | R32UI image | tileCountX × tileCountY | 每 tile 的 SurfaceType bitmask |
| TileList_Single | SSBO (uvec2[]) | maxTileCount × 8B | 单一 SurfaceType tile 坐标列表 |
| TileList_Multi | SSBO (uvec2[]) | maxTileCount × 8B | 多 SurfaceType tile 坐标列表 |
| TileDispatchArgs | SSBO (48B) | 48 bytes | indirect dispatch 参数 + 计数器 |

> `maxTileCount = ceil(W/TILE_SIZE) × ceil(H/TILE_SIZE)`，1080p@8×8 tile ≈ 32,400 tiles，内存总计 < 1 MB。

---

## 7. Shader Compositor 合成器架构

### 7.1 设计理念

传统做法：每个材质 Shader 包含完整 `main()`，内部处理纹理采样、光照、阴影、雾、输出。
不同 Pass（Forward、ShadowMap、VBuffer）需要维护多份冗余模板，改一处逻辑要改所有变体。

**Compositor 架构**：将 Shader 拆分为两层——

| 层 | 职责 | 谁写 |
|---|---|---|
| **Surface Function（表面函数）** | 纯材质计算：纹理采样、法线映射、材质属性输出 | 每个 SurfaceType 各一份 |
| **Compositor Template（合成模板）** | 提供 `main()`：构造 `SurfaceInput`、调用表面函数、执行光照/阴影/雾、写入 RT 输出 | 引擎统一提供，按 PassType 分 |

材质作者只关心 **"这个表面长什么样"**（业务函数），不关心 "它最终怎么输出到屏幕"（合成器处理）。
所有 Pass 变体（Forward、ShadowMap、VBuffer、Dither、A2C…）由 Compositor 自动合成。

```
┌──────────────────────────────────────────────────────────────┐
│                     编译器生成 root shader                    │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ #version 450                                            │ │
│  │ #define QUALITY_TIER 1                                  │ │
│  │ #define SHADOW_MODE 2                                   │ │
│  │ // ... 其他 #define                                     │ │
│  │                                                         │ │
│  │ #include "surface/standard_surface.glsl"   ← 表面函数   │ │
│  │ #include "compositor/main_forward_opaque.frag.glsl"     │ │
│  │               ↑ 合成模板（提供 main()）                  │ │
│  └─────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

### 7.2 Surface Function 接口规范

#### 公共结构体（`common/surface_interface.glsl`）

```glsl
// common/surface_interface.glsl — 所有 Surface Function 和 Compositor 共用

struct SurfaceInput
{
    vec2  uv;              // 纹理坐标
    vec3  worldPos;        // 世界空间位置
    vec3  worldNormal;     // 世界空间法线（几何法线，未经法线贴图）
    vec4  tangent;         // 切线 (xyz=tangent, w=handedness)
#if HAS_VERTEX_COLOR
    vec4  vertexColor;     // 顶点色（有 VERTEX_COLOR 时可用）
#endif
};

struct SurfaceOutput
{
    vec3  baseColor;       // 线性空间基础色
    float alpha;           // 透明度 [0,1]
    vec3  normal;          // 世界空间法线（法线贴图后）
    float metallic;        // 金属度 [0,1]
    float roughness;       // 粗糙度 [0,1]
    float ao;              // 环境遮蔽 [0,1]，默认 1.0
    vec3  emissive;        // 自发光 (线性 HDR)，默认 vec3(0)
};
```

#### 表面函数契约

每个 Surface Function 文件（如 `surface/standard_surface.glsl`）必须实现：

| 函数签名 | 用途 | 必须实现 |
|---|---|---|
| `SurfaceOutput EvalSurface(SurfaceInput si)` | 完整表面求值：纹理采样 + 法线映射 + 材质属性 | ✅ 必须 |
| `float EvalAlpha(vec2 uv)` | 轻量 Alpha 求值（仅采样 alpha 通道，用于 ShadowMap Masked） | ★ BlendMode 含 alpha 操作时必须 |

> **Surface Function 不做的事**：
> - ❌ 不写 `main()`
> - ❌ 不计算光照（BlinnPhong / PBR 等）
> - ❌ 不采样阴影（ShadowMap / ShadowMask / Contact Shadow）
> - ❌ 不计算雾
> - ❌ 不写 `FragColor` 或任何 RT 输出
> - ❌ 不执行 `discard`（alpha test / dither 由 Compositor 处理）
>
> 只输出材质属性 → Compositor 决定怎么使用。

### 7.3 Compositor 模板 — Pass 类型

Compositor 根据 **PassType**（§4.3）选择对应的 `main()` 模板：

| PassType | Compositor 模板文件 | 行为 |
|---|---|---|
| `FORWARD_OPAQUE` | `compositor/main_forward_opaque.frag.glsl` | `EvalSurface()` → `EvalLighting()` → Shadow → Fog → `FragColor = vec4(litColor, 1.0)` |
| `FORWARD_MASKED` | `compositor/main_forward_masked.frag.glsl` | `EvalSurface()` → `discard if alpha < threshold` → 光照 → `FragColor` |
| `FORWARD_TRANSPARENT` | `compositor/main_forward_transparent.frag.glsl` | `EvalSurface()` → 光照 → `FragColor = vec4(litColor, alpha)` |
| `FORWARD_DITHER` | `compositor/main_forward_dither.frag.glsl` | `EvalSurface()` → Bayer dither → `discard` → 光照（替代 AlphaBlend） |
| `FORWARD_A2C` | `compositor/main_forward_a2c.frag.glsl` | `EvalSurface()` → 光照 → `FragColor = vec4(litColor, alpha)`（MSAA A2C 硬件处理） |
| `SHADOW_OPAQUE` | 仅 VS depth-write，**无 FS** | 纯深度输出，Surface Function 完全不参与 |
| `SHADOW_MASKED` | `compositor/main_shadow_masked.frag.glsl` | `EvalAlpha(uv)` → `discard if < threshold`（无光照、无颜色输出） |
| `VBUFFER_ID` | `compositor/main_vbuffer_id.frag.glsl` | 写 ID + MeshletIndex（不调用 Surface Function） |
| `VBUFFER_RESOLVE` | `compositor/main_vbuffer_resolve.comp.glsl` | Compute: 读 VBuffer → 重建 UV → `EvalSurface()` → `EvalLighting()` |
| `TERRAIN_CONTACT_DITHER` | `compositor/main_terrain_dither.frag.glsl` | `EvalSurface()` → terrain contact dither → 光照 |

### 7.4 Compositor 模板示例

#### 7.4.1 Forward Opaque — 不透明标准渲染

```glsl
// compositor/main_forward_opaque.frag.glsl
// ★ Compositor 自动提供的 main() — Forward 不透明渲染

#include "common/surface_interface.glsl"
#include "common/lighting.glsl"     // 统一光照入口 (内部按 QUALITY_TIER 分支)
#include "common/shadow.glsl"
#include "common/shadow_mask.glsl"
#include "common/fog.glsl"

// ★ Surface Function 已通过 root shader 的 #include 引入（EvalSurface 可用）

void main()
{
    // 1. 构造 SurfaceInput（从 VS 输出的 varying 读取）
    SurfaceInput si;
    si.uv          = Input.TexCoord;
    si.worldPos    = Input.WorldPosition;
    si.worldNormal = Input.Normal;
    si.tangent     = Input.Tangent;
#if HAS_VERTEX_COLOR
    si.vertexColor = Input.Color;
#endif

    // 2. 调用 Surface Function（纯业务逻辑 — 无光照）
    SurfaceOutput surf = EvalSurface(si);

    // 3. 统一光照（Compositor 处理 — 按 QUALITY_TIER 自动选择 BlinnPhong/PBR/PBR+IBL）
    vec3 V = normalize(camera.pos - si.worldPos);
    vec3 litColor = EvalLighting(surf, si.worldPos, V);

    // 4. 阴影（ShadowMask RT 采样 — 已包含所有阴影源）
    litColor *= SampleShadowMask(si.worldPos);

    // 5. 雾
    litColor = ApplyFog(litColor, si.worldPos);

    FragColor = vec4(litColor, 1.0);
}
```

#### 7.4.2 Forward Masked — Alpha Test 遮罩

```glsl
// compositor/main_forward_masked.frag.glsl

#include "common/surface_interface.glsl"
#include "common/lighting.glsl"
#include "common/shadow.glsl"
#include "common/shadow_mask.glsl"
#include "common/fog.glsl"

void main()
{
    SurfaceInput si;
    si.uv          = Input.TexCoord;
    si.worldPos    = Input.WorldPosition;
    si.worldNormal = Input.Normal;
    si.tangent     = Input.Tangent;

    SurfaceOutput surf = EvalSurface(si);

    // ★ Alpha Test — 由 Compositor 统一处理，Surface Function 不做 discard
    if (surf.alpha < ALPHA_CLIP_THRESHOLD) discard;

    vec3 V = normalize(camera.pos - si.worldPos);
    vec3 litColor = EvalLighting(surf, si.worldPos, V);
    litColor *= SampleShadowMask(si.worldPos);
    litColor = ApplyFog(litColor, si.worldPos);

    FragColor = vec4(litColor, 1.0);
}
```

#### 7.4.3 Forward Dither — 自动替代 AlphaBlend

```glsl
// compositor/main_forward_dither.frag.glsl
// ★ Bayer Dither 替代 Alpha Blend — LOD fade-out、terrain contact 等

#include "common/surface_interface.glsl"
#include "common/lighting.glsl"
#include "common/shadow.glsl"
#include "common/shadow_mask.glsl"
#include "common/fog.glsl"
#include "common/dither.glsl"

void main()
{
    SurfaceInput si;
    si.uv          = Input.TexCoord;
    si.worldPos    = Input.WorldPosition;
    si.worldNormal = Input.Normal;
    si.tangent     = Input.Tangent;

    SurfaceOutput surf = EvalSurface(si);

    // ★ Bayer 4×4 dither — 用屏幕空间 dither 模式替代 alpha blending
    float dither = BayerDither4x4(gl_FragCoord.xy);
    if (surf.alpha < dither) discard;

    vec3 V = normalize(camera.pos - si.worldPos);
    vec3 litColor = EvalLighting(surf, si.worldPos, V);
    litColor *= SampleShadowMask(si.worldPos);
    litColor = ApplyFog(litColor, si.worldPos);

    FragColor = vec4(litColor, 1.0);  // 不透明输出（dither 已处理透明度）
}
```

#### 7.4.4 Forward Alpha2Coverage — MSAA 硬件混合

```glsl
// compositor/main_forward_a2c.frag.glsl
// ★ Alpha-to-Coverage — 植被、铁丝网、头发等半透明遮罩

#include "common/surface_interface.glsl"
#include "common/lighting.glsl"
#include "common/shadow.glsl"
#include "common/shadow_mask.glsl"
#include "common/fog.glsl"

void main()
{
    SurfaceInput si;
    si.uv          = Input.TexCoord;
    si.worldPos    = Input.WorldPosition;
    si.worldNormal = Input.Normal;
    si.tangent     = Input.Tangent;

    SurfaceOutput surf = EvalSurface(si);

    vec3 V = normalize(camera.pos - si.worldPos);
    vec3 litColor = EvalLighting(surf, si.worldPos, V);
    litColor *= SampleShadowMask(si.worldPos);
    litColor = ApplyFog(litColor, si.worldPos);

    // ★ 输出 alpha — 驱动 MSAA Alpha-to-Coverage 硬件子样本遮罩
    // VkPipelineMultisampleStateCreateInfo.alphaToCoverageEnable = VK_TRUE
    FragColor = vec4(litColor, surf.alpha);
}
```

#### 7.4.5 Shadow Masked — ShadowMap Alpha Test

```glsl
// compositor/main_shadow_masked.frag.glsl
// ★ ShadowMap Pass — 仅求值 alpha 做 discard（无光照、无颜色输出）

#include "common/surface_interface.glsl"

// ★ Surface Function 已通过 root shader 引入（EvalAlpha 可用）

void main()
{
    float alpha = EvalAlpha(Input.TexCoord);
    if (alpha < ALPHA_CLIP_THRESHOLD) discard;
    // 无颜色输出 — 仅写深度
}
```

> **对比旧方案**：旧方案每个材质的 ShadowMap 变体需要复制整个 `main()` 再手动裁剪光照代码。\
> 新方案中同一个 `main_shadow_masked.frag.glsl` **被所有带 alpha test 的 SurfaceType 共用**。

#### 7.4.6 Forward Transparent — 半透明混合

```glsl
// compositor/main_forward_transparent.frag.glsl
// ★ 半透明物体 — Order-dependent alpha blend

#include "common/surface_interface.glsl"
#include "common/lighting.glsl"
#include "common/shadow.glsl"
#include "common/shadow_mask.glsl"
#include "common/fog.glsl"

void main()
{
    SurfaceInput si;
    si.uv          = Input.TexCoord;
    si.worldPos    = Input.WorldPosition;
    si.worldNormal = Input.Normal;
    si.tangent     = Input.Tangent;

    SurfaceOutput surf = EvalSurface(si);

    vec3 V = normalize(camera.pos - si.worldPos);
    vec3 litColor = EvalLighting(surf, si.worldPos, V);
    litColor *= SampleShadowMask(si.worldPos);
    litColor = ApplyFog(litColor, si.worldPos);

    // 半透明输出 — 需 back-to-front 排序或 OIT
    FragColor = vec4(litColor, surf.alpha);
}
```

### 7.5 统一光照入口 `EvalLighting()`

Compositor 模板中调用的 `EvalLighting()` 是统一光照函数。
所有 SurfaceType 共用同一份光照代码，按 `QUALITY_TIER` 编译期选择路径：

```glsl
// common/lighting.glsl — 统一光照入口

#include "common/lighting_blinnphong.glsl"   // Low 档位底层实现
#include "common/lighting_pbr.glsl"          // Medium+ Cook-Torrance
#include "common/ambient.glsl"               // 环境光（Simple / FakeAtm / IBL）
#if QUALITY_TIER >= 2
#include "common/lighting_clustered.glsl"    // High+ Clustered Shading 多光源
#endif

vec3 EvalLighting(SurfaceOutput surf, vec3 worldPos, vec3 V)
{
    vec3 N = surf.normal;
    vec3 L = normalize(ULRE_SUN_DIR);

#if QUALITY_TIER == 0
    // ---- Low: BlinnPhong FakePBR ----
    float NdotL = dot(N, L) * 0.5 + 0.5;  // Half-Lambert
    vec3 diffuse = surf.baseColor * NdotL * ULRE_SUN_COLOR;

    vec3 H = normalize(V + L);
    float spec_power = mix(128.0, 8.0, surf.roughness);
    float spec = pow(max(dot(N, H), 0.0), spec_power) * (1.0 - surf.roughness);
    vec3 specular = vec3(spec) * ULRE_SUN_COLOR * surf.metallic;

    return diffuse + specular + GetAmbientSimple(N, surf.baseColor);

#elif QUALITY_TIER == 1
    // ---- Medium: Cook-Torrance PBR (主光源) ----
    vec3 F0 = mix(vec3(0.04), surf.baseColor, surf.metallic);
    vec3 direct = EvalCookTorranceBRDF(N, V, L, surf.baseColor, F0,
                                        surf.metallic, surf.roughness);
    direct *= ULRE_SUN_COLOR;
    return direct + GetAmbientFakeAtm(N, surf.baseColor);

#else
    // ---- High/Ultra/Cinematic: Full PBR + IBL + AO + Emissive + Clustered Multi-Light ----
    vec3 F0 = mix(vec3(0.04), surf.baseColor, surf.metallic);

    // 主光源
    vec3 direct = EvalCookTorranceBRDF(N, V, L, surf.baseColor, F0,
                                        surf.metallic, surf.roughness);
    direct *= ULRE_SUN_COLOR;

    // Clustered Shading 附加灯光
    direct += EvalClusteredLights(N, V, worldPos, surf.baseColor, F0,
                                   surf.metallic, surf.roughness);

    // IBL 环境光 + AO
    vec3 ambient = GetAmbientIBL(N, V, surf.baseColor, F0,
                                  surf.metallic, surf.roughness) * surf.ao;

    return direct + ambient + surf.emissive;
#endif
}
```

> **关键优势**：光照代码中心化——修改 PBR 算法或新增光源只改 `lighting.glsl`，所有 SurfaceType 自动受益。\
> 旧方案光照分散在每个材质模板中，改一处要改 N 份。

### 7.6 自动变体生成规则

编译器根据材质预设的 `BlendMode`（§4.3）自动决定需要生成哪些 Pass 变体：

```
┌────────────────┬──────────────────────────────────────────────────────┐
│ BlendMode      │ 自动生成的 PassType 变体                             │
├────────────────┼──────────────────────────────────────────────────────┤
│ Opaque         │ FORWARD_OPAQUE + SHADOW_OPAQUE + VBUFFER_ID         │
│ Masked         │ FORWARD_MASKED + SHADOW_MASKED + VBUFFER_ID         │
│ Transparent    │ FORWARD_TRANSPARENT                                  │
│                │ (无 Shadow、无 VBuffer — 透明物体不写深度)             │
│ Dither         │ FORWARD_DITHER + SHADOW_MASKED + VBUFFER_ID         │
│                │ (Bayer dither 替代 AlphaBlend: LOD fade, Contact 等) │
│ A2C            │ FORWARD_A2C + SHADOW_MASKED + VBUFFER_ID            │
│                │ (MSAA Alpha-to-Coverage: 植被、铁丝网、头发等)        │
└────────────────┴──────────────────────────────────────────────────────┘
```

**附加变体规则：**
- `TERRAIN_CONTACT` flag 为 true 时额外生成 `TERRAIN_CONTACT_DITHER` 变体
- `SKINNED` flag 为 true 时 VS 切换为含骨骼蒙皮的版本（`compositor/main_forward_skinned.vert.glsl`）
- `VBUFFER_RESOLVE` 由 Compute Shader 通用处理，不需要每个材质生成独立变体（在 Resolve 着色器中 `#include` 对应 Surface Function 并用 switch 分发）
- Unlit 材质（SurfaceType = Unlit 类）较简单，保留独立完整 VS/FS（不经过 Compositor）

**变体总数估算（Standard Surface, Opaque BlendMode, PC）：**
```
PassType(3: Forward+Shadow+VBuffer) × QualityTier(6) × ShadowMode(3) × flags(~4) ≈ 216 变体
```
相比旧方案无增长（旧方案同样 tier × shadow × flags，只是 Pass 维度以前是手写冗余模板）。

### 7.7 共享 Include 文件与目录结构

> 不再使用 C++ 字符串拼接 GLSL，使用真正的 GLSL `#include`（glslang 支持）。\
> 新增 `compositor/` 文件夹存放所有 `main()` 合成模板。\
> `surface/` 文件夹中的文件改为纯 Surface Function（无 `main()`、无 `.frag` 后缀）。

```
ShaderLibrary/
  common/
    structs.glsl                   // ViewportInfo, CameraInfo, SkyInfo, LocalToWorld 结构体
    surface_interface.glsl         // ★ SurfaceInput / SurfaceOutput 结构体定义
    lighting.glsl                  // ★ EvalLighting() 统一光照入口（按 QUALITY_TIER 分支）
    dither.glsl                    // ★ BayerDither4x4() + ordered dither 工具函数
    meshlet.glsl                   // MeshletGPU 结构体 + 解包工具函数
    material_instance.glsl         // GetMI() 函数
    transform.glsl                 // GetLocalToWorld(), GetWorldPosition() 等
    vertex_fetch_ssbo.glsl         // SSBO 顶点获取 (PC/Apple/Android High) — §2.9.3
    vertex_fetch_vbo.glsl          // 传统 VBO 顶点获取 (Android Mid/Low) — §2.9.3
    position_2d.glsl               // GetPosition2D() 各坐标系
    normal_mapping.glsl            // ApplyNormalMap(), TBN 构造
    lighting_blinnphong.glsl       // 底层 BlinnPhong 实现（被 lighting.glsl 引用）
    lighting_pbr.glsl              // 底层 Cook-Torrance BRDF 实现
    lighting_clustered.glsl        // Clustered Shading 灯光循环 (High+)
    ambient.glsl                   // 环境光计算 (Simple / FakeAtm / IBL)
    shadow.glsl                    // 阴影采样 + PCF/PCSS
    shadow_cached.glsl             // Toroidal Cached SM 采样 §3.6.3
    shadow_contact.glsl            // Contact Shadow 屏幕空间 ray-march §3.6.5
    capsule_shadow.glsl            // Capsule/Blob 解析阴影 §3.6.4
    shadow_mask.glsl               // ShadowMask RT 采样 (RGBA8 多通道)
    ssao.glsl                      // SSAO RT 采样
    fog.glsl                       // FogParams + CalcFogFactor() inline
    terrain_contact_dither.glsl    // Terrain height 采样 + dither 逻辑
    motion_vector.glsl             // Motion Vector 计算 (TAA 用)
    debug_lighting.glsl            // DebugLightingConfig 调试光照
    tone_mapping.glsl              // 色调映射工具函数
  compositor/                        // ★★★ 合成器 main() 模板 ★★★
    main_forward_opaque.frag.glsl    // Forward 不透明: EvalSurface → Lighting → Shadow → Fog
    main_forward_masked.frag.glsl    // Forward 遮罩: EvalSurface → AlphaTest → Lighting
    main_forward_transparent.frag.glsl // Forward 半透明: EvalSurface → Lighting → AlphaBlend
    main_forward_dither.frag.glsl    // Forward Dither: EvalSurface → BayerDither → Lighting
    main_forward_a2c.frag.glsl      // Forward A2C: EvalSurface → Lighting → Alpha2Coverage
    main_forward.vert.glsl          // 共用 Forward VS（构造 SurfaceInput varying 输出）
    main_forward_skinned.vert.glsl  // 骨骼蒙皮 VS
    main_shadow_opaque.vert.glsl    // Shadow depth-only VS（极简 — 仅输出 gl_Position）
    main_shadow_masked.vert.glsl    // Shadow VS（传递 UV — 供 FS alpha test 使用）
    main_shadow_masked.frag.glsl    // Shadow alpha-test FS: EvalAlpha → discard
    main_terrain_dither.frag.glsl   // Terrain Contact Dither 变体
    main_vbuffer_id.vert.glsl       // VBuffer ID Pass VS
    main_vbuffer_id.frag.glsl       // VBuffer ID Pass FS（写 ID，不调用 Surface Function）
  surface/                            // ★ Surface Function 文件（纯业务逻辑，无 main()）
    standard_surface.glsl             // Standard Surface: EvalSurface() + EvalAlpha()
    standard_color.glsl               // StandardColor（无纹理变体）
    standard_vtxcolor.glsl            // StandardVertexColor
    skin_surface.glsl                 // Skin Surface（预留 — SSS 参数）
    hair_surface.glsl                 // Hair Surface（预留 — 各向异性）
    cloth_surface.glsl                // Cloth Surface（预留 — Sheen）
    clearcoat_surface.glsl            // ClearCoat Surface（预留 — 双层 BRDF）
  unlit/                              // Unlit 材质（较简单，保留独立 main()，不经过 Compositor）
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
    gizmo_3d.vert.glsl
    gizmo_3d.frag.glsl
    uv_checker.frag.glsl
    overdraw_heatmap.frag.glsl
    depth_visualize.frag.glsl
    normal_visualize.frag.glsl
    tile_complexity.frag.glsl
    outline.frag.glsl
    infinite_grid.vert.glsl
    infinite_grid.frag.glsl
  scene/
    sky.vert.glsl
    sky.frag.glsl
    terrain.vert.glsl
    terrain.frag.glsl
    decal.vert.glsl
    decal.frag.glsl
  vbuffer/
    vbuffer_tile_classify.comp.glsl
    vbuffer_tile_prepare_args.comp.glsl
    vbuffer_resolve_single.comp.glsl   // 内部 #include 对应 Surface Function
    vbuffer_resolve_multi.comp.glsl
  gpudrive/
    hzb_downsample.comp.glsl
    instance_cull.comp.glsl
    meshlet_lod_select.comp.glsl
    meshlet_cull.comp.glsl
    meshlet_cull_phase2.comp.glsl
    cluster_light_assign.comp.glsl
  postprocess/
    shadowmask_compose.comp.glsl
    ssao.comp.glsl
    ssdo.comp.glsl
    auto_exposure.comp.glsl
    ssr.comp.glsl
    taa_resolve.comp.glsl
    bloom_extract.comp.glsl
    bloom_blur.comp.glsl
    dof.comp.glsl
    motion_blur.comp.glsl
    tone_mapping.comp.glsl
    color_grading.comp.glsl
    fxaa.comp.glsl
    sharpen.comp.glsl
```

---

## 8. VBuffer 渲染路径详细设计

### 8.1 VBuffer ID Pass

所有不透明几何体共用同一对 VS/FS，只写 ID。**不调用 Surface Function** — 由 Compositor 模板
`compositor/main_vbuffer_id.frag.glsl` 提供 `main()`（参见 §7.3 PassType `VBUFFER_ID`）：

```glsl
// compositor/main_vbuffer_id.frag.glsl
layout(location=0) out uvec2 VBufferOutput;
// x = SurfaceType (4bit) | PresetID (4bit) | InstanceID (16bit) | MeshletFlags (8bit)
// y = MeshletLocalTriID (8bit) | MeshletIndex (24bit)
//
// MeshletFlags 包含 TERRAIN_CONTACT 等 per-meshlet 标志
// MeshletIndex = gl_InstanceIndex (由 Indirect Draw 的 firstInstance 传入)
// MeshletLocalTriID = gl_PrimitiveID (meshlet 内局部三角形 ID, 0~255)

void main()
{
    MeshletGPU m = MeshletBuffer[gl_InstanceIndex];
    VBufferOutput = uvec2(
        (uint(SURFACE_TYPE)   << 28)
      | (uint(PRESET_ID)      << 24)
      | (uint(m.instance_id)  << 8)
      | uint(m.meshlet_flags),
        (uint(gl_PrimitiveID) << 24)
      | (gl_InstanceIndex     & 0x00FFFFFF)
    );
}
```

> VBuffer Resolve（Compute Shader）内部按 SurfaceType 分发，`#include` 对应的 Surface Function
> 并调用 `EvalSurface()` → `EvalLighting()`，等效于 Compositor 的 `VBUFFER_RESOLVE` PassType。

### 8.2 Tile SurfaceType Classification（Tile 表面类型统计）

在 VBuffer ID Pass 写入完成后、Resolve 之前，插入一个 **Tile Classification** 阶段。
将屏幕划分为固定大小的 tile（如 8×8 或 16×16 像素），统计每个 tile 内出现了哪些 SurfaceType，
用 bitmask 记录，以便后续 Resolve 按 tile 复杂度分层调度。

#### 设计动机

| 场景特征 | tile 内 SurfaceType 数量 | 占比（典型场景估算） |
|----------|------------------------|--------------------|
| 大面积墙壁/地面/岩石 | 1 (Standard) | ~60-75% |
| 天空区域 | 1 (Sky) 或 0 (empty) | ~10-20% |
| 角色边缘、场景交界 | 2-3 (Standard + Skin/Hair...) | ~10-20% |
| 极端复杂（植被+角色+场景重叠） | 4+ | <5% |

> **绝大多数 tile 只含单一 SurfaceType**，可直接 dispatch 对应的专用 Resolve kernel，
> 避免 switch/分支开销，且利好 GPU wavefront 的分支一致性（SIMD occupancy）。

#### Tile 大小选择

```cpp
constexpr uint32_t TILE_SIZE = 8;  // 8×8 像素，与 Compute local_size 对齐
// 也可以 16×16，视 GPU 架构和场景复杂度调优
// tile 数量 = ceil(screenWidth/TILE_SIZE) × ceil(screenHeight/TILE_SIZE)
```

#### Phase 1: Tile Classification Compute Shader

```glsl
// vbuffer_tile_classify.comp.glsl
layout(local_size_x=TILE_SIZE, local_size_y=TILE_SIZE) in;

layout(set=0, binding=0) uniform usampler2D VBuffer;

// 输出: 每个 tile 一个 uint32，每 bit 对应一种 SurfaceType
//   bit 0 = Unlit, bit 1 = Standard, bit 2 = Skin, ..., bit 10 = Terrain
//   bit 31 = empty tile (所有像素 depth=far 或 VBuffer=0)
layout(set=0, binding=1, r32ui) writeonly uniform uimage2D TileSurfaceMask;

// Tile 分类计数输出（可选，用于 indirect dispatch）
layout(set=0, binding=2) buffer TileDispatchArgs {
    uint single_type_count;     // 单一 SurfaceType 的 tile 数量
    uint multi_type_count;      // 多 SurfaceType 的 tile 数量
    uint empty_count;           // 空 tile 数量
    uint _pad;
    // Indirect dispatch args for each category
    uvec4 single_dispatch;      // (num_groups_x, 1, 1, 0)
    uvec4 multi_dispatch;       // (num_groups_x, 1, 1, 0)
};

// Tile 列表（按分类收集 tile 坐标）
layout(set=0, binding=3) buffer TileList_Single { uvec2 single_tiles[]; };
layout(set=0, binding=4) buffer TileList_Multi  { uvec2 multi_tiles[];  };

shared uint s_surfaceMask;  // workgroup 内共享的 bitmask

void main()
{
    // 初始化共享 bitmask
    if (gl_LocalInvocationIndex == 0)
        s_surfaceMask = 0u;
    barrier();

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    uvec2 vbuf  = texelFetch(VBuffer, pixel, 0).xy;

    if (vbuf.x != 0u)  // 非空像素
    {
        uint surfaceType = vbuf.x >> 28;  // 高 4 bit = SurfaceType
        atomicOr(s_surfaceMask, 1u << surfaceType);
    }
    barrier();

    // 第一个线程写出结果
    if (gl_LocalInvocationIndex == 0)
    {
        ivec2 tileCoord = ivec2(gl_WorkGroupID.xy);
        uint mask = s_surfaceMask;

        if (mask == 0u)
        {
            // 空 tile（天空或无几何体）
            imageStore(TileSurfaceMask, tileCoord, uvec4(0x80000000u));
            atomicAdd(empty_count, 1);
        }
        else if (bitCount(mask) == 1)
        {
            // 单一 SurfaceType → 进入快速路径
            imageStore(TileSurfaceMask, tileCoord, uvec4(mask));
            uint idx = atomicAdd(single_type_count, 1);
            single_tiles[idx] = uvec2(tileCoord);
        }
        else
        {
            // 多 SurfaceType → 进入通用路径
            imageStore(TileSurfaceMask, tileCoord, uvec4(mask));
            uint idx = atomicAdd(multi_type_count, 1);
            multi_tiles[idx] = uvec2(tileCoord);
        }
    }
}
```

#### Phase 2: 更新 Indirect Dispatch Args

```glsl
// vbuffer_tile_prepare_args.comp.glsl
layout(local_size_x=1) in;

layout(set=0, binding=0) buffer TileDispatchArgs {
    uint single_type_count;
    uint multi_type_count;
    uint empty_count;
    uint _pad;
    uvec4 single_dispatch;
    uvec4 multi_dispatch;
};

void main()
{
    // 每个 workgroup 处理一个 tile
    single_dispatch = uvec4(single_type_count, 1, 1, 0);
    multi_dispatch  = uvec4(multi_type_count,  1, 1, 0);
}
```

#### Tile 分类结果 — 数据结构

```
TileSurfaceMask (R32UI image, tileCountX × tileCountY):
  每个 texel = uint32 bitmask
  bit 0  = Unlit        (SurfaceType 0)
  bit 1  = Standard     (SurfaceType 1)
  bit 2  = Skin         (SurfaceType 2)
  bit 3  = Hair         (SurfaceType 3)
  ...                   ...
  bit 10 = Terrain      (SurfaceType 10)
  bit 11-30 = 预留（未来扩展更多 SurfaceType）
  bit 31 = empty tile

TileList_Single[]: 所有单一 SurfaceType 的 tile 坐标 (uvec2)
TileList_Multi[]:  所有多 SurfaceType 的 tile 坐标 (uvec2)
TileDispatchArgs:  indirect dispatch 参数（由 GPU 自行填充，CPU 不回读）
```

> **扩展性**：bitmask 设计天然支持新增 SurfaceType——只需分配新的 bit 位，
> Classification shader 无需修改逻辑（`atomicOr(1u << surfaceType)` 自动覆盖）。
> 如果 SurfaceType 未来超过 32 种，可扩展为 uvec2（64 bit）。

### 8.3 VBuffer Resolve Pass（Tile-Based Dispatch）

Resolve 阶段分为两批 dispatch，由 Classification 阶段的 indirect args 驱动：

#### Dispatch 1: 单一 SurfaceType Tile（快速路径）

```glsl
// vbuffer_resolve_single.comp.glsl
// 每个 workgroup 处理一个 tile（TILE_SIZE × TILE_SIZE 线程）
layout(local_size_x=TILE_SIZE, local_size_y=TILE_SIZE) in;

layout(set=0, binding=0) uniform usampler2D VBuffer;
layout(set=0, binding=1) uniform sampler2D  DepthBuffer;
layout(set=0, binding=2, rgba16f) writeonly uniform image2D LitColorOutput;
layout(set=0, binding=3) readonly buffer TileListSingle { uvec2 tile_coords[]; };
layout(set=0, binding=4) uniform usampler2D TileSurfaceMask;

// 全局资源
layout(set=1, binding=0) uniform UBO_Camera { CameraInfo camera; };
layout(set=1, binding=1) uniform UBO_Sky    { SkyInfo sky; };
layout(set=2, binding=0) readonly buffer MaterialData { ... };
layout(set=3, binding=0) uniform sampler2D MaterialTextures[MAX_TEXTURES];

void main()
{
    uvec2 tileCoord = tile_coords[gl_WorkGroupID.x];
    ivec2 pixel = ivec2(tileCoord * TILE_SIZE + gl_LocalInvocationID.xy);

    // 读取 tile 的 SurfaceType mask → 因为是单一类型，直接取 findLSB
    uint mask = texelFetch(TileSurfaceMask, ivec2(tileCoord), 0).r;
    uint surfaceType = findLSB(mask);  // 只有 1 个 bit 被设置

    uvec2 vbuf = texelFetch(VBuffer, pixel, 0).xy;
    if (vbuf.x == 0u) { return; }  // 空像素（tile 内可能有部分空像素）

    uint presetID = (vbuf.x >> 24) & 0xF;
    uint miIndex  = (vbuf.x >> 8)  & 0xFFFF;
    float depth   = texelFetch(DepthBuffer, pixel, 0).r;
    vec3 worldPos = ReconstructWorldPosition(pixel, depth);

    // ★ 整个 workgroup 走同一条 SurfaceType 分支 → 分支一致性最优
    vec3 litColor;
    switch(surfaceType)
    {
        case SURFACE_UNLIT:    litColor = EvalUnlit(miIndex); break;
        case SURFACE_STANDARD: litColor = EvalStandard(miIndex, worldPos, ...); break;
        case SURFACE_SKIN:     litColor = EvalSkin(miIndex, worldPos, ...); break;
        case SURFACE_HAIR:     litColor = EvalHair(miIndex, worldPos, ...); break;
        // ... 其他 SurfaceType
    }

    imageStore(LitColorOutput, pixel, vec4(litColor, 1.0));
}
```

> **关键优势**：整个 workgroup（= 一个 tile）内所有线程走同一个 switch 分支，
> GPU wavefront 没有分支发散（divergence），SIMD 利用率 100%。

#### Dispatch 2: 多 SurfaceType Tile（通用路径）

```glsl
// vbuffer_resolve_multi.comp.glsl
// 与 single 版本结构相同，但逻辑不同
layout(local_size_x=TILE_SIZE, local_size_y=TILE_SIZE) in;

// ... 相同的 set/binding 声明 ...
layout(set=0, binding=3) readonly buffer TileListMulti { uvec2 tile_coords[]; };

void main()
{
    uvec2 tileCoord = tile_coords[gl_WorkGroupID.x];
    ivec2 pixel = ivec2(tileCoord * TILE_SIZE + gl_LocalInvocationID.xy);

    uvec2 vbuf = texelFetch(VBuffer, pixel, 0).xy;
    if (vbuf.x == 0u) { return; }

    // 每个像素独立解析自己的 SurfaceType
    uint surfaceType = vbuf.x >> 28;
    uint presetID    = (vbuf.x >> 24) & 0xF;
    uint miIndex     = (vbuf.x >> 8)  & 0xFFFF;
    float depth      = texelFetch(DepthBuffer, pixel, 0).r;
    vec3 worldPos    = ReconstructWorldPosition(pixel, depth);

    // ★ 此 tile 内存在分支发散，但只影响 <25% 的 tile
    vec3 litColor;
    switch(surfaceType)
    {
        case SURFACE_UNLIT:    litColor = EvalUnlit(miIndex); break;
        case SURFACE_STANDARD: litColor = EvalStandard(miIndex, worldPos, ...); break;
        case SURFACE_SKIN:     litColor = EvalSkin(miIndex, worldPos, ...); break;
        case SURFACE_HAIR:     litColor = EvalHair(miIndex, worldPos, ...); break;
        // ... 其他 SurfaceType
    }

    imageStore(LitColorOutput, pixel, vec4(litColor, 1.0));
}
```

#### CPU 端调度伪代码

```cpp
// ===== Pass: Tile Classification =====
vkCmdBindPipeline(cmd, tileClassifyPipeline);
vkCmdDispatch(cmd, tileCountX, tileCountY, 1);

vkCmdPipelineBarrier(cmd, ...);  // Storage buffer → indirect read barrier

// ===== Pass: Prepare Indirect Args =====
vkCmdBindPipeline(cmd, tileArgsPipeline);
vkCmdDispatch(cmd, 1, 1, 1);  // 单个 workgroup

vkCmdPipelineBarrier(cmd, ...);  // indirect args barrier

// ===== Dispatch 1: Single-SurfaceType tiles (快速路径) =====
vkCmdBindPipeline(cmd, resolveSinglePipeline);
vkCmdDispatchIndirect(cmd, tileDispatchArgs, offsetof(single_dispatch));

// ===== Dispatch 2: Multi-SurfaceType tiles (通用路径) =====
vkCmdBindPipeline(cmd, resolveMultiPipeline);
vkCmdDispatchIndirect(cmd, tileDispatchArgs, offsetof(multi_dispatch));

// 空 tile 不 dispatch，自然跳过 → 零开销
```

#### 性能分析

| 场景特征 | 快速路径 tile 占比 | 通用路径 tile 占比 | 空 tile 占比 |
|----------|-------------------|--------------------|-------------|
| 室内简单场景 | ~80% | ~5% | ~15% |
| 开放世界远景 | ~55% | ~15% | ~30% (天空) |
| 角色近景特写 | ~40% | ~35% | ~25% |
| 密集植被城市 | ~35% | ~40% | ~25% |

> **空 tile 零开销**：不在任何 tile list 中，不参与 dispatch。
> **快速路径**：整个 workgroup 走同一 switch 分支，wavefront 无发散。
> **通用路径**：存在分支发散，但占比小，且每个 wavefront 内通常只有 2-3 种 SurfaceType。

#### 进阶扩展

| 扩展方向 | 方案 | 备注 |
|----------|------|------|
| **Per-SurfaceType 专用 kernel** | 为 Standard / Skin / Hair 各编译一个 resolve shader，Classification 阶段额外输出 per-type tile list → 每种类型独立 indirect dispatch | 适合 Special Surface 数量增多后进一步优化 |
| **Tile 粒度自适应** | 如果 16×16 粒度下 multi-tile 占比过高，可动态切换到 8×8 以降低发散 | 可通过 Classification 统计结果在 CPU 端决策 |
| **Debug 可视化** | TileSurfaceMask 直接渲染为颜色热力图（bit count → 冷暖色），用于性能调优 | 加入 Debug Overlay Pass |
| **Tile-based SSAO/Shadow 屏蔽** | 空 tile 和纯 Unlit tile 跳过 SSAO 采样和 ShadowMask 合成（将 mask 传入对应 pass） | 减少无效计算 |
| **异步 Compute** | Classification → Single Resolve 可与 ShadowMask/SSAO 的一部分重叠执行 | 需要 async compute queue |

### 8.4 LitColor RT（"小 GBuffer"）

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

### 9.1 构建期 — Compositor 组装 + 编译

构建期新增 **PassType** 维度，编译器自动为每个材质生成多 Pass 变体。
核心流程：**生成 root shader → 组装 Surface Function + Compositor 模板 → glslang 编译 → SPV 缓存**

```
for each preset in MaterialPresetRegistry:
    if preset.is_unlit:
        // Unlit 材质保留独立 VS/FS（不经过 Compositor）
        CompileUnlit(preset)
        continue

    // ★ 根据 BlendMode 查表得到需要生成的 PassType 集合（§7.6）
    pass_types = GetRequiredPassTypes(preset.blend_mode, preset.flags)

    for each pass_type in pass_types:
        for each valid quality_tier in [preset.min_tier .. preset.max_tier]:
            for each shadow_mode in applicable_shadows(quality_tier):
                for each flag_combo in applicable_flags(preset):
                    defines = BuildDefines(preset, quality_tier, shadow_mode,
                                           flags, pass_type)

                    // ★ Compositor 组装 — 生成 root shader
                    vs_template = Compositor.GetVS(pass_type, flags)
                    fs_template = Compositor.GetFS(pass_type)  // null for SHADOW_OPAQUE
                    surface_func = preset.surface_function_file

                    // root shader 内容：
                    //   #version 450
                    //   <defines>
                    //   #include "<surface_func>"    ← Surface Function
                    //   #include "<fs_template>"     ← Compositor main()
                    root_vs = AssembleRoot(defines, vs_template)
                    root_fs = AssembleRoot(defines, surface_func, fs_template)

                    spv_vs = glslangValidator(root_vs)
                    spv_fs = glslangValidator(root_fs)  // null for depth-only
                    SPVCache.Store(preset.id, quality_tier, shadow_mode,
                                   flags, pass_type, spv_vs, spv_fs)
```

> **示例 root shader（Forward Opaque, Standard Surface, Medium）：**
> ```glsl
> #version 450
> #extension GL_ARB_separate_shader_objects : enable
> #define SURFACE_TYPE 1
> #define QUALITY_TIER 1
> #define SHADOW_MODE 2
> #define PASS_TYPE 0  // FORWARD_OPAQUE
> #define HAS_VERTEX_COLOR 0
> #define GEOMETRY_FETCH_SSBO 1
>
> #include "surface/standard_surface.glsl"              // Surface Function
> #include "compositor/main_forward_opaque.frag.glsl"   // Compositor main()
> ```

> Unlit 材质只编译 1 个变体（无档位/Pass 概念）。\
> Standard Surface (Opaque) 编译 3(PassType) × 4(tier) × 3(shadow) × flags ≈ **72** 个变体。\
> Standard Surface (Masked) 编译 3(PassType) × 4(tier) × 3(shadow) × flags ≈ **72** 个变体。\
> 旧方案变体数相同（手写冗余模板），Compositor 方案代码维护量大幅减少。

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

// ===== 渲染时（引擎自动选择档位 + Pass 类型，美术无感知）=====

// ★ Material LOD: 计算 per-object 有效档位 (§3.5)
QualityTier objectLODTier = CalcObjectLODTier(render_obj, camera);
QualityTier effectiveTier = min(device_profile.tier, objectLODTier);

// ★ SPV 回退: 如果该 SurfaceType 在 effectiveTier 下与 Standard 等价，直接复用 (§3.5.6)
auto [resolvedPreset, resolvedSurface] = ResolveSPVFallback(mi->preset_id,
                                                             mi->surface_type,
                                                             effectiveTier);

ShaderPermutationKey key = BuildPermutationKey(resolvedSurface, effectiveTier,
                                               device_profile.backend);

// ★ 每个渲染 Pass 使用不同的 PassType 查询 SPV
// Forward Pass:
auto [spv_vs, spv_fs] = SPVCache.Get(resolvedPreset, key, PassType::ForwardOpaque);
VkPipeline fwd_pipeline = PipelineCache.GetOrCreate(resolvedPreset, key,
                                                     PassType::ForwardOpaque, render_pass);

// ShadowMap Pass (同一个材质，不同 PassType):
auto [shd_vs, shd_fs] = SPVCache.Get(resolvedPreset, key, PassType::ShadowOpaque);
VkPipeline shd_pipeline = PipelineCache.GetOrCreate(resolvedPreset, key,
                                                     PassType::ShadowOpaque, shadow_pass);

// 引擎自动跳过低画质不需要的纹理绑定（用 effectiveTier）
DescriptorSet ds = BuildDescriptorSet(mi, effectiveTier);
// ↑ 如果 effectiveTier=Low，AO/Emissive/DetailNormal 纹理槽自动绑定默认纹理
// ↑ 如果 Skin@Medium, SSS 相关纹理（thickness/curvature）也绑定默认值

// Forward 绘制
vkCmdBindPipeline(cmd, fwd_pipeline);
vkCmdBindDescriptorSets(cmd, ..., ds, ...);
vkCmdDrawIndexed(cmd, ...);

// ShadowMap 绘制（同一材质实例，不同 Pipeline — Compositor 自动裁剪光照代码）
vkCmdBindPipeline(cmd, shd_pipeline);
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
| Skin | E (PosTexNormTan) | TransformID, MaterialInstanceID |
| Hair | E (PosTexNormTan) | TransformID, MaterialInstanceID |
| Cloth | E (PosTexNormTan) | TransformID, MaterialInstanceID |
| ClearCoat | E (PosTexNormTan) | TransformID, MaterialInstanceID |
| Sky | A (PosOnly) | — |
| Terrain | 无（从 VertexID 生成） | — |

> **所有 Lit Surface 统一使用 Layout E**（Position + TexCoord + Normal + Tangent），
> 这确保 Low→High 档位切换不会改变顶点布局，Pipeline 兼容性最好。

### 12.2 SSBO vs VBO 平台差异 ★

| 维度 | SSBO 路径 (PC/Apple/Android High) | VBO 路径 (Android Mid/Low) |
|------|----------------------------------|---------------------------|
| **数据存储** | 全局 VertexDataBuffer SSBO (Set 3 binding 18) | per-mesh VkBuffer (VBO + IBO) |
| **VS 输入声明** | 无 `layout(location=N) in`，从 SSBO 手动 fetch | 标准 `layout(location=N) in` 属性声明 |
| **Pipeline 创建** | `VkPipelineVertexInputStateCreateInfo` 为空 | 正常 VertexBinding + VertexAttribute |
| **顶点布局约束** | stride 按 float 计算 (Layout E = 12 floats) | 复用 §12.1 的 5 种标准布局 |
| **绑定开销** | 零 — 全局 SSBO 在帧开始时绑定一次 | 每 mesh / 每 DrawCall bind VBO/IBO |
| **兼容性** | 需要 `maxStorageBufferRange ≥ 128MB` | 所有 Vulkan 设备支持 |

> SSBO 路径下，§12.1 中的 5 种 Layout 仍然有效——它定义了**数据在 SSBO 中的内存布局** (stride/offset)，
> 只是不再通过 `VkVertexInputAttributeDescription` 告知 GPU，而是 VS 内按 stride 手动读取。

---

## 13. 实施路线

### Phase 1：基础框架（核心重构）
1. 定义 `SurfaceType`、`QualityTier`、`PlatformBackend`、`GeometryFetchMode`、`BlendMode`、`PassType`、`ShaderPermutationKey` 枚举/结构体
2. 定义 `MaterialPresetDef` + `TextureSlotDef`（含降级规则 + **BlendMode 属性**）
3. 实现 `DeviceQualityProfile::Detect()`（GPU 检测 → 自动选档位 + 平台后端 + 几何获取模式）
4. 定义全局固定 Descriptor Set Layout（4 个 Set，SSBO 平台含 binding 18-19）
5. 定义 `SurfaceInput` / `SurfaceOutput` 公共结构体（`surface_interface.glsl`）
6. 编写共享 .glsl include 文件（structs, lighting, ambient, shadow, transform, normal_mapping, **surface_interface, dither**, vertex_fetch_ssbo, vertex_fetch_vbo）

### Phase 1.5：平台几何后端 ★
7. 实现 `GeometryBackend` 抽象层（§2.9.6）
8. 实现 SSBO 路径：全局 VertexDataBuffer / IndexDataBuffer SSBO 分配 + mesh 上传
9. 实现 VBO 路径：per-mesh VkBuffer 创建 + VBO/IBO 绑定
10. 实现 vertex_fetch_ssbo.glsl / vertex_fetch_vbo.glsl include 文件
11. 实现 Pipeline 创建分支（SSBO: 空 VertexInput / VBO: 标准 VertexInput）

### Phase 2：Compositor 合成器框架 ★★★（新增）
12. 实现统一光照入口 `EvalLighting()`（`lighting.glsl`，按 QUALITY_TIER 编译期分支）
13. 实现 Compositor 模板集：`main_forward_opaque/masked/transparent/dither/a2c.frag.glsl`
14. 实现 Shadow Compositor 模板：`main_shadow_opaque.vert.glsl` + `main_shadow_masked.frag.glsl`
15. 实现 `CompositorAssembler`：根据 (BlendMode, PassType) 查表选择 VS/FS 模板，生成 root shader
16. 实现 `PresetShaderCompiler`（Compositor 组装 + 注入 `#define` + 调用 glslang）
17. 实现 `SPVCache`（preset_id + tier + shadow + flags + platform + **pass_type** → SPV 查表）
18. 实现 BlendMode → PassType[] 自动变体查表（§7.6 规则）

### Phase 3：Unlit 材质移植
19. 移植 PureColor2D, Texture2D, Text2D（保留独立 main()，不经过 Compositor）
20. 移植 PureColor3D, VertexColor3D, PaletteColor3D, Gizmo3D
21. 移植 Emissive3D, Billboard

### Phase 4：Standard Surface（核心）
22. 实现 Standard Surface Function（`surface/standard_surface.glsl`：`EvalSurface()` + `EvalAlpha()`）
23. 验证 Compositor 自动生成所有 Pass 变体（Forward + Shadow + VBuffer）
24. 实现纹理降级机制（默认纹理绑定，引擎自动处理）
25. 从现有 TextureBlinnPhong + BasicLit + PBRColor3D 合并迁移
26. 实现 StandardColor 和 StandardVertexColor 的 Surface Function 变体

### Phase 5：场景材质
27. 移植 Sky, Terrain（可保留独立 main() 或改为 Surface Function）

### Phase 6：VBuffer 路径（SSBO 平台专属）
28. 实现 VBuffer ID Pass（`compositor/main_vbuffer_id.frag.glsl`，SSBO 路径）
29. 实现 Tile SurfaceType Classification Compute Shader
    - TileSurfaceMask (R32UI image)、TileList (SSBO)、TileDispatchArgs (SSBO)
    - Indirect dispatch args 填充 shader
30. 实现 VBuffer Resolve — 单一 SurfaceType tile 快速路径（内部 `#include` Surface Function）
31. 实现 VBuffer Resolve — 多 SurfaceType tile 通用路径
32. 实现 LitColor RT 管理和后期处理衔接
33. 实现 Forward / VBuffer 路径自动切换（VBO 平台强制 Forward Only）
34. (可选) Tile-based SSAO/ShadowMask 屏蔽优化
35. (可选) Tile Complexity Debug 可视化热力图

### Phase 7：Meshlet 几何管线（SSBO 平台 High+）
36. 集成 meshoptimizer 库（meshlet 构建 + mesh simplification）
37. 实现离线 Meshlet 预处理工具（Mesh → MeshletBuffer + LOD DAG + Flags）
38. 定义 MeshletGPU 结构体 + 引擎二进制格式 (.ulm)
39. 实现 Instance Cull Compute Shader（视锥 + 粗 HZB）
40. 实现 Meshlet LOD Select + Cull Compute Shader（DAG 遍历 + Frustum + Cone + HZB）
41. 集成 vkCmdDrawIndexedIndirectCount (meshlet 粒度 Indirect Draw，从全局 SSBO 读取)
42. 实现 Two-Phase Meshlet Occlusion Culling
43. 实现 Terrain-Contact Dither 混合（Compositor `TERRAIN_CONTACT_DITHER` PassType）
44. 实现 MESHLET_FLAG_ALPHA_TEST / WIND_ANIM 变体支持
45. (可选) 实现 Mesh Shader 加速路径（VK_EXT_mesh_shader: Task + Mesh Shader, PC only）
46. 实现 Lowest/Low/Medium SSBO 回退路径（离散 LOD mesh 从 DAG 导出，仍走 SSBO 顶点获取）
47. 实现 VBO 平台回退路径（离散 LOD mesh + CPU Frustum Cull + vkCmdDrawIndexed）

### Phase 8：渲染管线扩展 — 阴影 & 后处理 & 光照
48. 实现双层 ShadowMap 架构: Near Dynamic Cascade (每帧全量) + Far Cached Cascade (环形滚动, §3.6.3)
49. 实现 Toroidal Scrolling 逻辑: tile dirty mask + 增量渲染 + fract() UV 采样
50. 实现 ShadowMask Compose Pass: Near+Far 距离混合 + Capsule Shadow (G) + Contact Shadow (B) → RGBA8
51. 实现 Capsule Shadow: CapsuleShadowData SSBO + per-pixel 解析遮挡计算 (§3.6.4)
52. 实现 Blob Shadow 回退: 贴地 quad + 衰减纹理 (低端动态物体用)
53. 实现 Contact Shadow: 屏幕空间 ray-march, High+ (§3.6.5)
54. 实现 ShadowConfig 可调参数体系 (§3.6.7) + DeviceQualityProfile 默认值映射
55. 实现 HZB 降采样 Compute Shader（Depth RT → HZB Pyramid，SSBO 平台 High+）
56. 实现 Clustered Shading（Cluster 预计算 + Light Assignment Compute + **Compositor `EvalLighting()` 集成**，SSBO High+）
57. 实现 Auto Exposure（Luminance Histogram Compute + Average + Temporal Smooth）
58. 实现 SSR（Hi-Z Ray March Compute, PC/Apple High+ only）
59. 实现 Fog 内联计算（FogParams UBO + `ApplyFog()` 集成到 **Compositor 模板** / VBuffer Resolve）
60. 实现 Color Grading / 3D LUT（Compute Pass, 在 ToneMap 之后）
61. 实现 CAS / Sharpening（Compute Pass）
62. (可选) 实现 DOF Compute Shader（CoC 计算 + 散景模糊，PC/Apple High+）
63. (可选) 实现 Per-Pixel Motion Blur Compute Shader（PC Ultra only）
64. 实现 Decal Pass（Screen-Space Decal: OBB mesh + Depth 反算 + 投影采样，SSBO High+）
65. 实现 Outline / Selection Highlight（Stencil + Dilate 或 JFA）

### Phase 9：Material LOD + Special Surface ★★★
66. 实现 `CalcObjectLODTier()` 函数：屏幕空间面积估算 + 阈值表 + `importanceBias` 偏移（§3.5.3）
67. 实现 `ResolveSPVFallback()` 函数：根据 `MaterialPresetDef.fallback_surface_type` + `unique_feature_min_tier` 路由 SPV（§3.5.6）
68. 渲染排序支持 `EffectiveTier` 分组：`(SurfaceType, EffectiveTier, PassType)` 排序键，减少 Pipeline 切换
69. 扩展 `SurfaceOutput` 支持 `SurfaceOutputExt`（SSS / Anisotropy / Caustic 等 Special Surface 专属字段）
70. 实现 Skin Surface Function（`surface/skin_surface.glsl`，Ultra: 全 SSS + Detail Normal + 曲率 AO，High: 简化 SSS，Medium/Low: fallback Standard）
71. 实现 Eye Surface Function（`surface/eye_surface.glsl`，Ultra: Parallax Refraction + 焦散 + 角膜 SSS，High: 单层 Parallax + CubeMap，Medium: 平面纹理 PBR，Low: Albedo + Phong）
72. 实现 Hair Surface Function（Ultra: Marschner 双高光，High: Kajiya-Kay，Medium: 单高光 PBR，Low: BlinnPhong）
73. 实现 Cloth Surface Function（Sheen + Charlie Model，Medium: 简化 wrap lighting，Low: Standard）
74. 实现 ClearCoat Surface Function（High+: 双层 BRDF，Med: 单层近似，Low: 高 specular BlinnPhong）
75. 实现 Foliage Surface Function（High+: Thin Translucency + Wind，Med: Wrap + 简化 Wind，Low: 静态 AlphaTest）
76. 实现 `EvalLighting_Skin()` / `EvalLighting_Eye()` 等 Compositor lighting 模块（配合 SurfaceOutputExt 处理 SSS / Anisotropy）
77. 验证 SPV fallback 等价性：Skin@Medium == Standard@Medium SPV 输出完全一致
78. 实现 `ObjectImportance` 游戏接口：对话镜头 → MainNPC(+1), 过场特写 → Hero(+2), 群演 → BackgroundNPC(-1)

### Phase 10：Android 适配与测试 ★
79. Android VBO 路径端到端集成测试（Lowest/Low/Medium 材质 × 传统 DrawCall）
80. Android High SSBO 路径验证（Adreno 7xx / Mali-G7xx 真机测试）
81. Android 动态分辨率实现（0.5× ~ 1.0× 根据 GPU 负载调节）
82. Android GPU 能力检测阈值调优（SSBO vs VBO 分界线校准）
83. Android 特性裁剪验证（确认 §2.9.4 中被砍特性的 shader 变体不被加载）
84. Android Cached SM + Capsule Shadow 联调测试
85. Android Material LOD 阈值调优（§3.5.3 各平台阈值表验证）

### Phase 11：清理
86. 删除旧的 ShaderComposition / Logic / Bridge 代码
87. 删除传统 GBuffer 相关代码和枚举
88. 更新 Pipeline 创建逻辑使用固定 Layout（SSBO: 空 VertexInput / VBO: 标准 VertexInput）
89. 更新编辑器 UI（Material Instance 编辑面板 — 含 BlendMode 选择 + ObjectImportance 预览）

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
| Skin | 无 | SSS + Detail Normal + 曲率 AO，Material LOD 降级至 Standard（§3.5.4） |
| Hair | 无 | Marschner/Kajiya-Kay 各向异性双高光，Material LOD 降级至 BlinnPhong |
| Cloth | 无 | Sheen + Charlie Model，Material LOD 降级至 Standard |
| ClearCoat | 无 | 双层 BRDF，Material LOD 降级至单层 PBR |

## 附录 B：Texture2DArray 处理

当前 `PBRColor3D` 使用 `Texture2DArray` 通过 MI 中的 `texture_id` 索引纹理层。在新系统中：

- **StandardTexture** 使用独立纹理槽（每个 MaterialInstance 绑定自己的纹理集）
- **VBuffer Resolve** 阶段如需 Bindless 纹理，使用 `VK_EXT_descriptor_indexing` 扩展
- 不再将 `Texture2DArray` 作为主要纹理接口（仅 Terrain SplatMap 保留）

## 附录 C：文件数量对比

| | 旧系统 | 新系统 |
|--|--------|--------|
| C++ 源文件 | ~40+ (.cpp/.h) | ~12 (.cpp/.h，含 CompositorAssembler) |
| GLSL 文件 | 0（全 C++ 字符串） | ~105 (.glsl 文件，含 compositor/ + surface/ + common/ + vbuffer/ + gpudrive/ + postprocess/ + debug/ + scene/) |
| 其中 Compositor 模板 | — | ~13 (compositor/*.glsl — 所有 SurfaceType 共用) |
| 其中 Surface Function | — | ~11 (surface/*.glsl — 每个 SurfaceType 各一份；Special Surface 低档位可 fallback 到 Standard SPV §3.5.6) |
| 编译期 Shader 变体 | 动态，不可预测 | ≤11 (SurfaceType) × ≤6 (QualityTier) × ≤4 (shadow+flags) × ≤3 (PassType per BlendMode) = ~792 最大（SPV fallback 复用后实际 ~550，Cinematic 仅 PC 编译） |
| 概念数 | FixedMaterialDef, ComposedMaterialDef, MaterialLogicDef, ShaderCompositionBridge, BuiltinHelpers, ShaderCreateInfo (5种), MaterialCreateConfig (3种), LightingModel enum... | MaterialPresetDef, MaterialInstance, **CompositorAssembler**, PresetShaderCompiler, SPVCache, DeviceQualityProfile, TextureSlotDef, BlendMode, PassType |
