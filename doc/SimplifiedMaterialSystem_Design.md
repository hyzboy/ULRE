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

### Shader/SPV 部署生命周期

**终极目标：完全离线生成 Shader 与 SPV。** 实时生成 GLSL / 编译 SPV 仅存在于渲染器开发阶段，
不会暴露给游戏开发者，更不会出现在最终游戏运行时。整个 Shader 系统的部署分为三个阶段：

| 阶段 | GLSL 生成 | SPV 编译 | SPV 来源 | 说明 |
|------|-----------|----------|----------|------|
| **① 渲染器开发** | ✅ 实时（CompositorAssembler） | ✅ 实时（GLSLCompiler DLL） | 内存 / 本地缓存 | 开发迭代用，支持热重载、Shader 调试、变体验证 |
| **② 游戏编辑器** | ❌ 不可用 | ❌ 不可用 | 本地预编译 SPV 包 | 编辑器启动时加载离线 SPV 包，美术只调参数和纹理 |
| **③ 游戏运行时** | ❌ 不可用 | ❌ 不可用 | 分发/下载的 SPV 包 | 按设备画质档位分发对应等级的 SPV 数据 |

**SPV 分发模型：**

```
构建服务器 (CI/CD)
  └─ PresetShaderCompiler.CompileAll()
       ├─ SPV_PC_High.pack       ← PC 高画质全部变体
       ├─ SPV_PC_Medium.pack     ← PC 中画质全部变体
       ├─ SPV_PC_Low.pack        ← PC 低画质全部变体
       ├─ SPV_Android_High.pack  ← Android 高端 (SSBO vertex fetch)
       ├─ SPV_Android_Medium.pack← Android 中端 (SSBO vertex fetch)
       ├─ SPV_Android_Low.pack   ← Android 低端 (VBO vertex fetch)
       └─ SPV_Apple_*.pack       ← Apple 平台

游戏分发
  ├─ 客户端安装包：仅包含目标平台 SPV 全档位
  │   └─ 或按需下载：设备检测后只下载匹配档位的 SPV 包
  └─ 运行时：SPVCache.LoadFromFile() → 查表 → Pipeline 创建
```

> **关键约束**：
> - 游戏运行时 **不链接** GLSLCompiler DLL，不包含 GLSL 源码
> - SPV 包按 `PlatformBackend × QualityTier` 独立打包，客户端只需携带匹配设备的 SPV
> - 支持增量更新：新增/修改材质时只重新编译受影响的 SPV 变体
> - 编辑器阶段的 SPV 包由资产构建管线（Asset Pipeline）自动生成，美术无感知

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
│    - 输出: Depth RT (D32_SFLOAT 优先, 详见 §2.10 Reversed-Z)      │
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
│ 3. Tile Material Classification (Compute)                ← ★ 新增     │
│    - 每 TILE_SIZE×TILE_SIZE tile 统计 MaterialKey bitmask + 唯一材质数  │
│    - 匹配 FusedComboLUT，输出 TileList (empty/single/fused/multi) 四类 │
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
│    - Dispatch 1: Single-Material tiles (独占路径, ~50-70%)           │
│      → workgroup 内零分支发散，SIMD 利用率最优                       │
│    - Dispatch 2: Fused-Material tiles (融合路径, ~10-25%)    ★ 新增 │
│      → 2-4 种已知材质组合，预编译融合 shader，极低分支发散            │
│    - Dispatch 3: Multi-Material tiles  (通用路径, ~5-15%)           │
│      → per-pixel 解析 MaterialKey，存在分支发散                     │
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
| IBL (CubeMap) | — | — | — | ✅ | ✅ | ✅ | P1 | Irradiance + Prefiltered + BRDF LUT 双模式（纹理法/函数近似法 §7.5.1） |
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
| Terrain-Contact Dither | ❌ | ❌ | ✅ 可关 | ✅ | ✅ | ✅ | P1 | 可开关，Lowest/Low 默认 OFF；Android Low 不编译 (§2.8.5) |
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

##### 可开关设计

Terrain-Contact Dither 是 **可开关特性** —— 编译期 `#define` + 运行时 bool 双重门控：

| 维度 | 机制 | 说明 |
|------|------|------|
| **编译期** | `#define FEATURE_TERRAIN_CONTACT_DITHER 0或1` | 为 0 时 dither 相关代码完全被预处理器移除，不生成 `TERRAIN_CONTACT_DITHER` 变体，节省二进制体积 |
| **运行时** | `DeviceQualityProfile::enable_terrain_contact_dither` | 编译期为 1 但运行时关闭时，忽略 `MESHLET_FLAG_TERRAIN_CONTACT` 标志，绘制时跳过 dither 路径，等同普通 meshlet |

平台默认值：

| 平台 / 档位 | 编译期 FEATURE | 运行时默认 | 备注 |
|--------|---------|---------|------|
| PC / Apple Lowest~Low | 1 (编译) | **OFF** | 低端档位默认关，程序可手动开启 |
| PC / Apple Medium+ | 1 | **ON** | 正常开启 |
| Android High | 1 | **ON** | SSBO 平台，性能充足 |
| Android Mid | 1 | **OFF** | VBO 前向路径，默认关闭以保性能；可由 TA 手动开启 |
| Android Low | **0** (不编译) | — | 极低端不编译该变体，节省二进制体积 |

> **前向低端手机回退策略**：当 Terrain-Contact Dither 被关闭（运行时 OFF 或编译期未编译）时，
> 带 `MESHLET_FLAG_TERRAIN_CONTACT` 的 meshlet 作为普通 meshlet 绘制，不执行 dither / discard。
> 视觉上树根与地形会有硬边交插，但避免了 HeightMap 采样 + Bayer 计算 + discard 的开销。

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
// 仅 FEATURE_TERRAIN_CONTACT_DITHER == 1 且 MESHLET_FLAG_TERRAIN_CONTACT 时启用

#if FEATURE_TERRAIN_CONTACT_DITHER

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

#endif // FEATURE_TERRAIN_CONTACT_DITHER
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
| **Terrain-Contact Dither** | ❌ (不编译) | ✅ 默认 OFF | ✅ | 可开关，见 §2.8.5 |
| **VBuffer Tile Classification** | ❌ | ❌ | ✅ | MaterialKey 级分类 + Fused Combo 匹配（§8.2/§8.5） |
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
                   FEATURE_TERRAIN_CONTACT_DITHER = 0 → 不编译 Terrain-Contact Dither 变体

编译期 #define 注入:
  #define PLATFORM_PC          / PLATFORM_APPLE     / PLATFORM_ANDROID
  #define GEOMETRY_FETCH_SSBO  / GEOMETRY_FETCH_VBO
  #define QUALITY_LOWEST        / QUALITY_LOW       / QUALITY_MEDIUM   / QUALITY_HIGH / QUALITY_ULTRA / QUALITY_CINEMATIC
  #define FEATURE_HZB                    0 或 1
  #define FEATURE_CLUSTERED              0 或 1
  #define FEATURE_VBUFFER                0 或 1
  #define FEATURE_MESHLET_CULL           0 或 1
  #define FEATURE_TERRAIN_CONTACT_DITHER 0 或 1   // ← 可开关，Android Low 固定为 0
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

### 2.10 Reversed-Z 与无限远平面 ★

#### 2.10.1 设计动机

标准 OpenGL/Vulkan 深度映射（近=0, 远=1, 线性 NDC 映射）在远距离场景中
深度精度急剧下降——1000m 处两个相距 10cm 的物体可能映射到**同一个深度值**，
导致 Z-fighting 闪烁。这对大世界/开放世界场景不可接受。

**Reversed-Z**（近=1.0, 远=0.0）+ **浮点深度缓冲**（D32_SFLOAT）利用 IEEE 754 浮点数
在接近 0.0 时指数间距变密的特性，将高精度重新分配到远处：

| 深度映射 | 近平面精度 | 100m 精度 | 1000m 精度 | 10km 精度 |
|---|---|---|---|---|
| 标准 Z (D24) | 极高 | 中等 | 极差 (Z-fighting) | 不可用 |
| Reversed-Z (D32F) | 极高 | 极高 | 高 | 中等 (可用) |
| Reversed-Z + Infinite Far (D32F) | 极高 | 极高 | 高 | 中等 (可用, 无远平面裁切) |

#### 2.10.2 投影矩阵

引擎统一使用 **Reversed-Z Infinite Far Plane** 投影矩阵（Vulkan NDC: depth ∈ [0, 1]）：

```cpp
// Reversed-Z Infinite Far Plane Projection
// near = 近平面距离 (如 0.1m)
// fov = 垂直视场角, aspect = 宽/高
mat4 MakeInfiniteReversedZProj(float fov, float aspect, float near)
{
    float f = 1.0f / tan(fov * 0.5f);
    // Vulkan clip: depth [0, 1], Y flip
    return mat4(
        f / aspect, 0,  0,     0,
        0,         -f,  0,     0,       // -f: Vulkan Y flip
        0,          0,  0,    -1,       // perspective divide: w = -z
        0,          0,  near,  0        // z → near/z (Reversed: near→1.0, ∞→0.0)
    );
}
```

> **无限远平面**：远平面 = +∞，深度值在 ∞ 处趋近 0.0（但永远 > 0）。
> 天空 / 远景无需担心被裁切——只要 near 合理设置（0.05~0.2m），
> 10km 外物体仍有足够的深度精度。

#### 2.10.3 深度缓冲格式

| 平台 | 深度格式 | 说明 |
|------|---------|------|
| PC | **D32_SFLOAT** | 纯 32-bit 浮点，最高精度，PC 无性能损失 |
| Apple | **D32_SFLOAT** | Apple Silicon 原生支持，推荐 |
| Android High | **D32_SFLOAT** | 高端 Adreno/Mali 支持 |
| Android Mid/Low | **D24_UNORM_S8_UINT** | 低端不支持 D32F 时回退；仍使用 Reversed-Z |

> **Android 回退**：即使是 D24，Reversed-Z 的精度分布仍优于标准 Z。
> 运行时通过 `vkGetPhysicalDeviceFormatProperties()` 检测 D32_SFLOAT 支持，
> 不支持时自动降级到 D24_UNORM_S8_UINT。

#### 2.10.4 全管线影响

所有使用深度的模块必须统一 Reversed-Z 约定：

| 模块 | 标准 Z 行为 | Reversed-Z 行为 |
|------|-----------|----------------|
| **VkPipelineDepthStencilState** | depthCompareOp = LESS | depthCompareOp = **GREATER** |
| **Clear Depth** | clear = 1.0 | clear = **0.0** |
| **Early-Z / Z-Prepass** | less → reject | **greater** → reject |
| **HZB Downsample** | max = 最远 | **min** = 最远 (已实现, §2.6) |
| **HZB Occlusion Test** | meshlet depth < hzb → visible | meshlet depth **>** hzb → visible |
| **ShadowMap** | 独立投影，可用标准 Z 或 Reversed-Z (独立选择) | 推荐 Reversed-Z for consistency |
| **线性化深度** | `linearZ = near*far / (far - z*(far-near))` | `linearZ = near / z` (无限远简化) |
| **SSR Hi-Z Trace** | step compare < | step compare **>** |
| **SSAO** | depth → viewZ 转换公式需匹配 | 统一使用 `near / z` |
| **天空 Pass** | depth = 1.0 (最远) | depth = **0.0** (最远) |

```glsl
// common/depth_utils.glsl
// Reversed-Z Infinite Far: 线性化深度极其简洁
float LinearizeDepth(float d) {
    return ULRE_NEAR_PLANE / d;   // d ∈ (0, 1], near=1.0, ∞=0.0
}

// 重建世界空间位置 (从 NDC + depth)
vec3 ReconstructWorldPos(vec2 ndc, float depth) {
    vec4 clipPos = vec4(ndc, depth, 1.0);
    vec4 viewPos = ULRE_INV_PROJ * clipPos;
    viewPos /= viewPos.w;
    return (ULRE_INV_VIEW * viewPos).xyz;
}
```

#### 2.10.5 超远距离绘制支持

Reversed-Z + Infinite Far Plane 组合使得引擎可以渲染**任意远**的物体：

| 距离 | 典型内容 | 精度表现 |
|------|---------|---------|
| 0 ~ 100m | 角色、室内、近景物体 | 极高精度 |
| 100m ~ 1km | 建筑、中景地形 | 高精度 |
| 1km ~ 10km | 远景山体、城市天际线 | 中等精度（足够） |
| 10km ~ 100km | 地平线、远山轮廓 | 低精度（可见但不 Z-fight） |
| 100km+ | 云层、日月、太空 | 趋近 0.0（不裁切，正常渲染） |

> **与 LOD 配合**：超远物体本身三角形数极少（meshlet DAG 最粗级），
> Z-fighting 风险低；Reversed-Z 提供的精度绰绰有余。

> **与阴影配合**：ShadowMap 使用独立的正交投影矩阵，不受主相机 Reversed-Z 影响。
> Far Cached SM (§3.6.3) 覆盖 ~200m，超出阴影范围的远景自然无阴影（符合视觉预期）。

---

## 3. 画质档位与光照模型

### 核心思想：表面类型 × 画质档位

美术看到的是**表面类型**（Standard、Skin、Hair ...），不会看到 BlinnPhong / PBR 这种底层概念。
引擎根据设备能力自动选画质档位（QualityTier），每个档位对应不同的光照算法和纹理需求。

### 3.1 画质档位定义

```cpp
enum class QualityTier : uint8
{
    Lowest    = 0,    // 顶点光照 + Albedo only — 极低端设备 / 极远距离 LOD
    Low       = 1,    // BlinnPhong FakePBR — Android Low / PC 极低配
    Medium    = 2,    // 标准 PBR (Cook-Torrance) — Android Mid / 核显/中端独显
    High      = 3,    // 完整 PBR + 高级特性 — Android High / PC 独显 / Apple
    Ultra     = 4,    // PC 高端独显，完整特性 + DetailNormal 等
    Cinematic = 5,    // 电影级：全 SSS + 毛孔法线 + 眼球折射 + Light Probes 等

    ENUM_CLASS_RANGE(Lowest, Cinematic)
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
    SurfaceType      surface_type;
    MaterialCategory material_category;     // ★ 视觉材质类型（§5.1.1）
    BlendMode        blend_mode;
    QualityTier      min_tier;              // 该材质支持的最低档位
    QualityTier      max_tier;              // 最高档位
    // ★ Material LOD 回退
    SurfaceType      fallback_surface_type; // 低于 unique_feature_min_tier 时回退到此类型
    QualityTier      unique_feature_min_tier; // 该 SurfaceType 独有特性的最低档位
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
    EarlyZSolid         = 7,    // Z-Prepass 不透明：仅 VS depth-write
    EarlyZMasked        = 8,    // Z-Prepass 遮罩：EvalAlpha() → discard
    VBufferID           = 9,    // 写 ID，不调用 Surface Function

    ENUM_CLASS_RANGE(ForwardOpaque, VBufferID)
};
```

> **SPV Cache Key** = (`preset_id`, `quality_tier`, `shadow_mode`, `flags`, `platform`, **`pass_type`**) → SPV\
> PassType 不在 `ShaderPermutationKey` 中——同一个材质同时需要多种 Pass 变体（如 ForwardOpaque + ShadowOpaque + VBufferID）。\
> Compositor 根据 `BlendMode` 自动决定生成哪些 `PassType`（详见 §7.6）。

### 4.4 有效排列矩阵

对于 **Standard 表面**：

| Quality | 直接光照 | 环境光 | 阴影 | 纹理数 |
|---------|---------|--------|------|--------|
| Lowest | 顶点 NdotL | Constant | 无 | 1 (Albedo) |
| Low | BlinnPhong | Simple | None/PCF | 2 (Albedo+Normal) |
| Medium | Cook-Torrance | FakeAtm | PCF | 3 (Albedo+Normal+MR) |
| High | Cook-Torrance | IBL | PCSS | 5+ (Albedo+Normal+MR+AO+Emissive) |
| Ultra | Cook-Torrance | IBL | PCSS | 6+ (加 DetailNormal 等) |
| Cinematic | 同 Ultra + SSS | IBL + Light Probes | PCSS + Soft | 全纹理集 |

总变体数 = SurfaceType(实现数) × Quality(6) × Shadow(3) × Flags 子集 ≈ **可控范围**

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
| 31 | Terrain | 地形 | TerrainLayerSSBO + Texture2DArray (Albedo/Normal/MR) + SplatMap, 理论 256 层 — 详见 §5.5 |

### 5.1.1 双分类法：SurfaceType vs MaterialCategory

#### 设计动机

引擎内部存在两个**正交但互补**的材质分类维度：

| 维度 | 定义角度 | 决定什么 | 粒度 | 典型数量 |
|------|----------|---------|------|---------|
| **SurfaceType** | 光照模型 / BRDF 族 | Shader 管线路径、EvalLighting 分发 | 粗粒度 | 11 种（§4.1） |
| **MaterialCategory** | 视觉材质种类 | 纹理风格、MI 参数范围、美术语义 | 细粒度 | 引擎内置 4 + 项目扩展 20-60（开放枚举） |

> **类比**：SurfaceType 回答 *"用什么算法照亮"*，MaterialCategory 回答 *"看起来像什么东西"*。

两者是**多对一**关系——多种 MaterialCategory 映射到同一 SurfaceType：

```
MaterialCategory (视觉种类)              SurfaceType (光照模型)
─────────────────────────               ──────────────────────

  ┌ 引擎内置（0-3，恒定不变）─────────────────────────────────────┐
  │ GenericPBR (通用PBR)     ──────────▶  Standard (1)          │
  │ Sky        (天空/大气)   ──────────▶  Sky (9)               │
  │ Water      (水体)       ──────────▶  Water (8)             │
  │ Terrain    (地表)       ──────────▶  Terrain (10)          │
  └──────────────────────────────────────────────────────────────┘

  ┌ 项目扩展（64+，随项目需求增减）──────────────────────────────┐
  │ Metal (金属)              ─┐                                │
  │ Stone (石头)               │                                │
  │ Wood  (木材)               ├──────▶  Standard (1)           │
  │ Leather (皮革)             │          Cook-Torrance PBR     │
  │ Paint (油漆)               │                                │
  │ Plastic (塑料)             │                                │
  │ Ceramic (陶瓷)             │                                │
  │ Concrete (混凝土)         ─┘                                │
  │                                                             │
  │ HumanSkin (人类皮肤)      ─┬──────▶  Skin (2)  SSS         │
  │ CreatureSkin (生物皮肤)    ─┘                                │
  │                                                             │
  │ HumanHair (人类头发)       ─┬──────▶  Hair (3)  各向异性    │
  │ AnimalFur (动物毛发)       ─┘                                │
  │                                                             │
  │ Fabric (织物/布料)         ──────────▶  Cloth (4)  Sheen    │
  │ Eyeball (眼球)             ──────────▶  Eye (5)   折射+SSS  │
  │ TreeLeaf (树叶)            ─┬──────▶  Foliage (6) 薄层透光  │
  │ Grass (草)                 ─┘                               │
  │ CarPaint (车漆)            ─┬──────▶  ClearCoat (7) 双层    │
  │ Lacquer (瓷釉/清漆)        ─┘                               │
  │                                                             │
  │ （项目可自由扩展更多 Category...）                             │
  └─────────────────────────────────────────────────────────────┘
```

#### MaterialCategory 枚举

```cpp
enum class MaterialCategory : uint8_t
{
    // ================================================================
    //  引擎内置 (0-15) — 与 SurfaceType 1:1 的基础类型，跨项目恒定不变
    // ================================================================
    GenericPBR      = 0,    // 通用 Standard PBR — 所有 Standard 族 Category 的降级终点
    Sky             = 1,    // 天空 / 大气         → Sky SurfaceType
    Water           = 2,    // 水体                → Water SurfaceType
    Terrain         = 3,    // 地表                → Terrain SurfaceType
    // 4-15 保留给引擎未来内置扩展

    // ================================================================
    //  项目扩展 (64-255) — 按项目需求自由定义
    //  高画质时使用专有 SurfaceType + 独有特效；
    //  远距离 / 低端设备 → 材质 LOD 退化为 GenericPBR (Standard PBR / BlinnPhong)
    // ================================================================

    // --- Standard SurfaceType 族 (64-95): 通用 PBR，视觉差异仅靠纹理+参数 ---
    Metal           = 64,   // 金属（铁、铜、金、铝...）
    Stone           = 65,   // 石头（岩石、砖块、大理石...）
    Wood            = 66,   // 木材（木板、树皮、竹子...）
    Leather         = 67,   // 皮革
    Paint           = 68,   // 油漆 / 涂层
    Plastic         = 69,   // 塑料 / 橡胶
    Ceramic         = 70,   // 陶瓷 / 瓷器
    Concrete        = 71,   // 混凝土 / 水泥
    Sand            = 72,   // 沙子 / 泥土
    Ice             = 73,   // 冰 / 雪
    Glass           = 74,   // 玻璃（不透明/磨砂 — 透明玻璃走 Transparent 通道）
    Food            = 75,   // 食物 / 有机物
    // 76-95 Standard 族预留

    // --- Skin SurfaceType 族 (96-103): 需要 SSS 光照 ---
    HumanSkin       = 96,   // 人类皮肤            → Skin SurfaceType
    CreatureSkin    = 97,   // 生物/怪物皮肤        → Skin SurfaceType
    // 98-103 Skin 族预留

    // --- Hair SurfaceType 族 (104-111): 需要各向异性高光 ---
    HumanHair       = 104,  // 人类头发             → Hair SurfaceType
    AnimalFur       = 105,  // 动物毛发             → Hair SurfaceType
    // 106-111 Hair 族预留

    // --- Cloth SurfaceType 族 (112-115) ---
    Fabric          = 112,  // 织物/布料            → Cloth SurfaceType

    // --- Eye SurfaceType 族 (116-119) ---
    Eyeball         = 116,  // 眼球                → Eye SurfaceType

    // --- Foliage SurfaceType 族 (120-127) ---
    TreeLeaf        = 120,  // 树叶                → Foliage SurfaceType
    Grass           = 121,  // 草                  → Foliage SurfaceType

    // --- ClearCoat SurfaceType 族 (128-135) ---
    CarPaint        = 128,  // 车漆                → ClearCoat SurfaceType
    Lacquer         = 129,  // 瓷釉/清漆           → ClearCoat SurfaceType

    // 136-255 自由扩展

    MAX_CATEGORY
};
```

> **编号规则**：
> - **0-15**：引擎内置，跨项目不变。GenericPBR / Sky / Water / Terrain 是引擎层面的"基底"类型。
> - **64-255**：项目扩展段，按 SurfaceType 族分组（每族预留 4-8 个空位），方便项目追加。
> - **16-63**：保留未分配，未来引擎层面若需要新的内置 Category 可使用此段。
>
> Metal-Food 等"视觉材质种类"全部在 64+ 段——这些是项目相关的美术语义分类，
> 不同项目需要的种类完全不同（见下方示例），所以不放在引擎内置范围。

#### 项目特化示例 — 动物主题游戏

不同游戏项目可在 64-255 段**自由增补**符合题材的 MaterialCategory：

```cpp
// ========= 动物主题游戏：项目扩展 MaterialCategory =========
// 在基础枚举的空位中追加新类别

// Standard 族扩展 (76-95 预留段)
constexpr auto BirdFeather       = MaterialCategory(76);  // 鸟类羽毛 → Standard
constexpr auto AmphibianSkin     = MaterialCategory(77);  // 两栖动物皮肤 → Standard（无需 SSS）
constexpr auto ReptileScale      = MaterialCategory(78);  // 爬虫鳞片 → Standard
constexpr auto FishScale         = MaterialCategory(79);  // 鱼类鳞片 → Standard
constexpr auto Chitin            = MaterialCategory(80);  // 昆虫甲壳 → Standard

// Skin 族扩展 (98-103 预留段)
constexpr auto AmphibianSkinWet  = MaterialCategory(98);  // 湿润两栖皮肤 → Skin (SSS + 湿润高光)
constexpr auto WhaleSkin         = MaterialCategory(99);  // 鲸类皮肤 → Skin (SSS)

// Hair 族扩展 (106-111 预留段)
constexpr auto BirdPlumage       = MaterialCategory(106); // 鸟类绒羽 → Hair (各向异性)
constexpr auto AnimalWhisker     = MaterialCategory(107); // 动物触须 → Hair
```

> **关键**：这些项目特化的 MaterialCategory 在**远距离或低端设备上全部退化为 GenericPBR**——
> 鸟类羽毛的独特光泽、两栖皮肤的湿润高光等效果只在近距离 + 高画质时呈现。
> 手机上 `deviceTier ≤ Medium`，所有特殊类别都走 Standard PBR 或 BlinnPhong，
> MaterialCategory 的差异**仅靠纹理和 MI 参数**维持视觉区分。

#### MaterialCategory → SurfaceType 映射表

| MaterialCategory | SurfaceType | Metallic 典型范围 | Roughness 典型范围 | 备注 |
|-----------------|-------------|------------------|--------------------|------|
| GenericPBR | Standard | 0.0 – 1.0 | 0.0 – 1.0 | 通用，降级终点 |
| Sky | Sky | — | — | 程序化散射 |
| Water | Water | — | — | FFT 波形 |
| Terrain | Terrain | — | — | 多层 Splat |
| Metal (64) | Standard | 0.8 – 1.0 | 0.1 – 0.6 | 铁/铜/金/铝 |
| Stone (65) | Standard | 0.0 – 0.1 | 0.5 – 0.9 | 岩石/砖块/大理石 |
| Wood (66) | Standard | 0.0 | 0.4 – 0.8 | 木板/树皮 |
| Leather (67) | Standard | 0.0 | 0.4 – 0.7 | 皮革/皮具 |
| Paint (68) | Standard | 0.0 – 0.5 | 0.2 – 0.6 | 油漆表面 |
| Plastic (69) | Standard | 0.0 | 0.3 – 0.7 | 塑料/橡胶 |
| Ceramic (70) | Standard | 0.0 | 0.1 – 0.4 | 瓷器/釉面 |
| Concrete (71) | Standard | 0.0 | 0.7 – 1.0 | 混凝土/水泥 |
| Sand (72) | Standard | 0.0 | 0.8 – 1.0 | 沙/土 |
| Ice (73) | Standard | 0.0 – 0.2 | 0.0 – 0.3 | 冰面/雪 |
| Glass (74) | Standard | 0.0 | 0.0 – 0.2 | 磨砂/不透明玻璃 |
| HumanSkin (96) | Skin | — | 0.3 – 0.6 | SSS, Thickness |
| CreatureSkin (97) | Skin | — | 0.4 – 0.8 | SSS |
| HumanHair (104) | Hair | — | 0.3 – 0.6 | 各向异性 Shift |
| AnimalFur (105) | Hair | — | 0.5 – 0.9 | Kajiya-Kay |
| Fabric (112) | Cloth | — | 0.6 – 1.0 | Sheen + Charlie |
| Eyeball (116) | Eye | — | 0.0 – 0.1 | 折射 + SSS |
| TreeLeaf (120) | Foliage | 0.0 | 0.4 – 0.7 | Thin Translucency |
| Grass (121) | Foliage | 0.0 | 0.5 – 0.8 | 同上 |
| CarPaint (128) | ClearCoat | 0.0 – 0.3 | 0.1 – 0.3 | 双层 BRDF |
| Lacquer (129) | ClearCoat | 0.0 | 0.05 – 0.2 | 高光泽 |

> 此表为**美术参考值域**——MaterialCategory 不强制参数范围，美术可超出，但编辑器可提供参考线。
> 项目扩展的 Category（如 BirdFeather, ReptileScale 等）由项目自行补充映射表条目。

#### 双维度画质降级

两个分类维度在低画质时**同时收敛**——所有特殊 MaterialCategory 最终退化为 GenericPBR：

```
高画质 (Cinematic/Ultra/High)              低画质 (Medium/Low/Lowest)
─────────────────────────                  ───────────────────────────
MaterialCategory: HumanSkin (96)           MaterialCategory: HumanSkin (96)
     SurfaceType: Skin                          SurfaceType: Standard (回退)
        Lighting: Cook-Torrance + SSS              Lighting: Cook-Torrance → BlinnPhong
      Unique Ftr: SSS + 毛孔法线 + 曲率 AO         Unique Ftr: 无（退化为 GenericPBR）

效果链:
  Skin@Cinematic ──────▶ 全 SSS + 毛孔 + 曲率 AO
  Skin@Ultra     ──────▶ Pre-integrated SSS + DetailNormal
  Skin@High      ──────▶ 简化 SSS
  Skin@Medium    ──▶ Standard@Medium（Cook-Torrance PBR，无 SSS）  ← SurfaceType 回退点
  Skin@Low       ──▶ Standard@Low（BlinnPhong FakePBR）
  Skin@Lowest    ──▶ Standard@Lowest（顶点光照 + Albedo）

项目特化示例（动物主题）:
  BirdFeather@High    ──▶ Standard@High + 独有各向异性高光贴图     ← 项目定义的特效
  BirdFeather@Medium  ──▶ Standard@Medium（Cook-Torrance PBR）     ← 退化为通用 PBR
  BirdFeather@Low     ──▶ Standard@Low（BlinnPhong）               ← 看起来就是普通羽毛纹理
```

> **回退点由 `MaterialPresetDef::unique_feature_min_tier` 决定**（§3.5.6 回退等价性）。
> 低于此阈值时，特殊 SurfaceType 回退到 `fallback_surface_type`（通常是 Standard），
> 此时不同 MaterialCategory 之间的视觉差异**仅靠纹理和 MI 参数**维持——
> HumanSkin@Low 和 Metal@Low 都走 BlinnPhong，但纹理不同所以看起来仍然不同。
>
> **手机端**：`deviceTier ≤ Medium` 时，所有 64+ 段的 MaterialCategory 最高也只走 Standard PBR，
> 不会触发任何特殊 SurfaceType 的独有效果。MaterialCategory 在此场景下纯粹是**美术标签**，
> 用于资源管理和编辑器分类，不影响运行时 Shader 路径。

#### 与融合 Shader 的关系（§8.5）

MaterialCategory 为**融合 Combo 注册**提供了语义基础：

```
传统注册（仅按 MaterialKey）：
  Register({KEY_PRESET_10, KEY_PRESET_10}, FUSED_ID);  // 含义不直观

语义化注册（按 MaterialCategory 共现模式）：
  Register(TreeLeaf + Wood,    FUSED_FOREST_CANOPY);    // 森林冠层：树叶 + 树皮/木材
  Register(Stone + Concrete,   FUSED_BUILDING_WALL);    // 建筑墙面：砖石 + 混凝土
  Register(Metal + Paint,      FUSED_VEHICLE_BODY);     // 载具车身：金属 + 油漆
  Register(Grass + Sand,       FUSED_TERRAIN_EDGE);     // 地形边缘：草地 + 沙土
  Register(HumanSkin + Fabric, FUSED_CHARACTER_EDGE);   // 角色边缘：皮肤 + 衣物
```

> 场景加载时，引擎根据场景中实际出现的 MaterialCategory 组合自动筛选需要激活的融合 Shader，
> 避免编译无关组合。`FusedComboRegistry::AutoRecommend()` 也按 MaterialCategory 统计共现频率。

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

    // Texture Array 索引（仅 TEXTURE_ARRAY 模式使用 — §6.4.4）
    // 传统纹理模式下这些字段不使用（纹理直接绑定在 binding 1-6 的 sampler2D 上）
    uint  tex_albedo;           // 4B, Albedo 纹理在 AlbedoArray 中的层索引
    uint  tex_normal;           // 4B, Normal 纹理在 NormalArray 中的层索引
    uint  tex_metallic_roughness; // 4B, MetallicRoughness 纹理层索引 (Medium+)
    uint  tex_ao;               // 4B, AO 纹理层索引 (High+)
    uint  tex_emissive;         // 4B, Emissive 纹理层索引 (High+)
    uint  _pad0;                // 4B, 对齐填充

                                // 总计 56 bytes (14 × 4B)，SSBO 中按 std430 自然对齐
};
```

> **纹理双模式**：MI_Standard 结构体布局在两种纹理模式下**完全一致**（56B），\
> 传统纹理模式下 `tex_*` 字段闲置但不影响内存对齐。\
> 编译期 `#define TEXTURE_ARRAY 0/1` 决定 Surface Function 是从 `sampler2D` 还是 `sampler2DArray` 采样。\
> 详见 §6.4.4。

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

// ===== 纹理采样（支持传统纹理和纹理阵列双模式 — §6.4.4）=====
// 编译期 #define TEXTURE_ARRAY 0/1 切换采样方式

#if TEXTURE_ARRAY
// 纹理阵列模式：从 sampler2DArray 按 MI 中的 texture_id 索引
vec4  SampleAlbedo(vec2 uv, MI_Standard mi)            { return texture(AlbedoArray, vec3(uv, float(mi.tex_albedo))); }
#else
// 传统纹理模式：直接从 sampler2D 采样
vec4  SampleAlbedo(vec2 uv, MI_Standard mi)            { return texture(TextureAlbedo, uv); }
#endif

#if QUALITY_TIER >= 1  // Low+: Normal Map
  #if TEXTURE_ARRAY
vec3  SampleNormal(vec2 uv, MI_Standard mi)            { return texture(NormalArray, vec3(uv, float(mi.tex_normal))).xyz * 2.0 - 1.0; }
  #else
vec3  SampleNormal(vec2 uv, MI_Standard mi)            { return texture(TextureNormal, uv).xyz * 2.0 - 1.0; }
  #endif
#endif

#if QUALITY_TIER >= 2  // Medium+
  #if TEXTURE_ARRAY
vec2  SampleMetallicRoughness(vec2 uv, MI_Standard mi) { return texture(MetallicRoughnessArray, vec3(uv, float(mi.tex_metallic_roughness))).bg; }
  #else
vec2  SampleMetallicRoughness(vec2 uv, MI_Standard mi) { return texture(TextureMetallicRoughness, uv).bg; }
  #endif
#endif

#if QUALITY_TIER >= 3  // High+
  #if TEXTURE_ARRAY
float SampleAO(vec2 uv, MI_Standard mi)               { return texture(AOArray, vec3(uv, float(mi.tex_ao))).r; }
vec3  SampleEmissive(vec2 uv, MI_Standard mi)          { return texture(EmissiveArray, vec3(uv, float(mi.tex_emissive))).rgb; }
  #else
float SampleAO(vec2 uv, MI_Standard mi)               { return texture(TextureAO, uv).r; }
vec3  SampleEmissive(vec2 uv, MI_Standard mi)          { return texture(TextureEmissive, uv).rgb; }
  #endif
#endif

// ===== 完整表面求值 — 供 Forward / VBuffer Resolve 等色彩 Pass 使用 =====
SurfaceOutput EvalSurface(SurfaceInput si)
{
    MI_Standard mi = GetMI();
    SurfaceOutput o;

    // Albedo
    vec4 albedo = SampleAlbedo(si.uv, mi) * unpackUnorm4x8(mi.base_color);
#if HAS_VERTEX_COLOR
    albedo.rgb *= si.vertexColor.rgb;
#endif
    o.baseColor = albedo.rgb;
    o.alpha     = albedo.a;

    // Normal（法线贴图后的世界空间法线）
#if QUALITY_TIER >= 1  // Low+: 有 Normal Map
    o.normal = ApplyNormalMap(SampleNormal(si.uv, mi), si.worldNormal, si.tangent,
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
    vec2 mr     = SampleMetallicRoughness(si.uv, mi);
    o.metallic  = mr.x * mi.metallic_factor;
    o.roughness = mr.y * mi.roughness_factor;
    o.ao        = 1.0;
    o.emissive  = vec3(0.0);
#else
    // High/Ultra/Cinematic: 完整纹理集
    vec2 mr     = SampleMetallicRoughness(si.uv, mi);
    o.metallic  = mr.x * mi.metallic_factor;
    o.roughness = mr.y * mi.roughness_factor;
    o.ao        = SampleAO(si.uv, mi) * mi.ao_strength;
    o.emissive  = SampleEmissive(si.uv, mi)
                  * unpackUnorm4x8(mi.emissive_color).rgb
                  * mi.emissive_intensity;
#endif

    return o;
}

// ===== 轻量 Alpha 求值 — 供 ShadowMap Masked Pass 使用（无需完整表面计算）=====
float EvalAlpha(vec2 uv)
{
    MI_Standard mi = GetMI();
    vec4 albedo = SampleAlbedo(uv, mi) * unpackUnorm4x8(mi.base_color);
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

// [31] Terrain — 多层 Splat + Texture2DArray 架构（详见 §5.5）
//
// ★ 地形不使用 Standard 的 PerMaterial binding，而是专用布局：
//   - 所有 TerrainLayer 的材质属性打包在一个 SSBO/UBO 中
//   - 所有地形纹理堆叠为 texture2DArray（Albedo / Normal / MR 各一个 Array）
//   - SplatMap（权重图）决定像素级混合
//
// TerrainLayer — 单层地形材质参数
struct TerrainLayerParams {
    uint  albedo_array_index;   // 4B, 在 TerrainAlbedoArray 中的 layer 索引
    uint  normal_array_index;   // 4B, 在 TerrainNormalArray 中的 layer 索引
    uint  mr_array_index;       // 4B, 在 TerrainMRArray 中的 layer 索引 (Medium+, 否则 0xFFFF)
    float uv_scale;             // 4B, 本层 UV 平铺密度
    uint  tint_color;           // 4B, RGBA packed 色调叠加
    float roughness_bias;       // 4B, 粗糙度偏移 [-1,1]
    float metallic_bias;        // 4B, 金属度偏移 [-1,1]
    float height_blend_sharpness; // 4B, height-based blending 锐度
};                              // 32 bytes per layer

// MI_Terrain — 地形全局参数
struct MI_Terrain {
    float tile_scale;           // 4B, 全局 UV 缩放基数
    float height_scale;         // 4B, 高度缩放
    uint  layer_count;          // 4B, 实际使用的层数 (1~256)
    uint  splat_channels;       // 4B, SplatMap 通道数 (layer_count / 4, 向上取整)
    float blend_range;          // 4B, 层间混合过渡带宽度
    float normal_strength;      // 4B, 全局法线强度
    uint  flags;                // 4B, bit0: triplanar, bit1: height_blend, bit2: tessellation
    uint  _padding;             // 4B
};                              // 32 bytes
//
// GPU 资源布局:
//   Set 2, binding 0:  TerrainGlobalUBO     (MI_Terrain, 32 bytes)
//   Set 2, binding 1:  TerrainLayerSSBO     (TerrainLayerParams[layer_count], ≤ 256×32 = 8KB)
//   Set 2, binding 2:  TerrainAlbedoArray   (texture2DArray, 最多 256 层)
//   Set 2, binding 3:  TerrainNormalArray   (texture2DArray, 最多 256 层)
//   Set 2, binding 4:  TerrainMRArray       (texture2DArray, Medium+, 最多 256 层)
//   Set 2, binding 5:  TerrainSplatMap      (texture2DArray, ceil(layer_count/4) 层, RGBA8 权重)
//   Set 2, binding 6:  TerrainHeightMap     (sampler2D, R16 全局高度图)
//   Set 2, binding 7:  TerrainNormalMapGlobal (sampler2D, 地形整体法线图)
```

### 5.5 Terrain Surface 详细设计 ★

地形（`SurfaceType::Terrain`, Preset ID=31）是专用渲染路径——不走 Standard 的 PerMaterial 纹理槽，
而是以 **Texture2DArray + SSBO** 实现**理论 256 层地形混合**。

#### 5.5.1 设计动机

| 传统方案（4 层 Splat） | 新方案（Texture2DArray N 层） |
|---|---|
| 每层独占一个纹理槽 → 最多 4~8 层 | Albedo / Normal / MR 各一个 Texture2DArray → 层数仅受 GPU 限制 |
| 增加层数需增加 DrawCall 或 Shader 变体 | 同一 DrawCall / Compute 内循环所有权重 >0 的层 |
| 每层材质参数硬编码在 Shader 或分散 Uniform | 统一 `TerrainLayerSSBO`，运行时可增删层，无需重编译 |
| 扩展困难（加雪/加泥需新 Shader） | 加层 = 添加一组纹理到 Array + 在 SSBO 追加一个 `TerrainLayerParams` |

> **Vulkan 保证**：`maxImageArrayLayers ≥ 256`（所有 Vulkan 1.0 设备）。
> 实际使用的层数 `layer_count` 由地形编辑器管理，运行时写入 `MI_Terrain.layer_count`。

#### 5.5.2 SplatMap 编码

SplatMap 使用 **texture2DArray, RGBA8** 格式——每层 4 通道存储 4 个地形层的权重：

```
SplatMap layer 0 (RGBA8):  层 0/1/2/3   的权重
SplatMap layer 1 (RGBA8):  层 4/5/6/7   的权重
SplatMap layer 2 (RGBA8):  层 8/9/10/11  的权重
...
SplatMap layer N (RGBA8):  层 4N/4N+1/4N+2/4N+3 的权重

总层数: ceil(layer_count / 4)
```

> **归一化约束**：所有层权重之和 = 1.0，由编辑器在笔刷绘制时保证。
> Shader 中不做归一化（节省 ALU），相信编辑器输出。

对于典型场景（8~16 层），SplatMap 仅需 2~4 个 Array Layer，内存开销极小。

#### 5.5.3 渲染路径 — VBuffer

在 VBuffer 路径中，地形渲染分为两个阶段：

**阶段一：VBuffer ID Pass**

```
地形 mesh 绘制时:
  - SurfaceType = Terrain (编码进 VBuffer.x 高 4 位)
  - PresetID = 31
  - InstanceID = terrain patch ID
  - 与普通几何体共用同一个 VBuffer ID Pass（只写 ID，不采样地形纹理）
```

**阶段二：VBuffer Resolve（Compute）— 地形专用 Dispatch**

```
Tile Classification 已识别出 terrain tile → 走 Terrain 专用 resolve kernel:

for each pixel in tile:
    if (pixel.SurfaceType != Terrain) → 走通用 resolve
    
    // 1. 重建 UV、worldPos
    vec2 terrainUV = ReconstructTerrainUV(worldPos);
    
    // 2. 采样 SplatMap → 获取各层权重
    //    只遍历权重 > threshold 的层（典型 2~4 层有效，skip 0 权重层）
    SurfaceOutput blended = {};
    for (uint i = 0; i < layer_count_ceil4; i++) {
        vec4 weights = texture(TerrainSplatMap, vec3(terrainUV, float(i)));
        for (int c = 0; c < 4; c++) {
            uint layerIdx = i * 4 + c;
            if (layerIdx >= layer_count) break;
            float w = weights[c];
            if (w < 0.004) continue;  // skip 权重 < 1/255
            
            TerrainLayerParams layer = TerrainLayerSSBO[layerIdx];
            vec2 layerUV = terrainUV * layer.uv_scale;
            
            // 3. 采样该层的 Albedo/Normal/MR (从 Texture2DArray)
            vec3 albedo = texture(TerrainAlbedoArray,
                                  vec3(layerUV, float(layer.albedo_array_index))).rgb;
            // ... Normal, MR 同理
            
            // 4. 加权累加
            blended.baseColor += albedo * w;
            blended.normal    += layerNormal * w;
            blended.roughness += layerR * w;
            blended.metallic  += layerM * w;
        }
    }
    
    // 5. Height-Based Blending（可选，增强地形层过渡自然度）
    //    使用各层高度图值调制权重 → 高处岩石自然覆盖低处泥土
    
    // 6. 走标准 EvalLighting() 光照
    vec3 litColor = EvalLighting(blended, ...);
```

> **关键优化**：绝大多数像素仅有 2~4 层权重 > 0，Texture2DArray 的 `texture()` 调用
> 在 GPU 缓存命中率高（相邻像素访问相同 Array Index + 相近 UV）。
> `TerrainLayerSSBO` 极小（≤8KB），完全在 L1 缓存内。

**Dither 混合（VBuffer Terrain-Contact Dither 变体）**：

> 当地形与其他物体（树根、岩石）接触时，VBuffer Resolve 可用 Bayer dither
> 在像素级切换 Terrain MaterialInstance 和物体 MaterialInstance（详见 §2.8.5）。
> 具体决策：检测到 `MESHLET_FLAG_TERRAIN_CONTACT` 的像素进入 dither 路径，
> blendFactor < dither → 使用 Terrain 材质，否则使用物体材质。

#### 5.5.4 渲染路径 — Forward

Forward 路径使用**同样的 Texture2DArray + SSBO 方案**，只是在 FS 中实时执行多层混合：

```glsl
// surface/terrain_surface.glsl — Terrain Forward Surface Function

#include "common/surface_interface.glsl"
#include "common/terrain_common.glsl"

// Terrain 专用 bindings
layout(set = 2, binding = 0) uniform TerrainGlobalUBO { MI_Terrain terrain; };
layout(set = 2, binding = 1) readonly buffer TerrainLayerBuffer {
    TerrainLayerParams layers[];
} TerrainLayerSSBO;
layout(set = 2, binding = 2) uniform sampler2DArray TerrainAlbedoArray;
layout(set = 2, binding = 3) uniform sampler2DArray TerrainNormalArray;
#if QUALITY_TIER >= 2  // Medium+
layout(set = 2, binding = 4) uniform sampler2DArray TerrainMRArray;
#endif
layout(set = 2, binding = 5) uniform sampler2DArray TerrainSplatMap;

SurfaceOutput EvalSurface(SurfaceInput si)
{
    SurfaceOutput o = SurfaceOutput(vec3(0), vec3(0,1,0), 0.0, 0.5, 1.0, vec3(0));
    vec2 terrainUV = si.uv;    // 地形 UV (世界空间 XZ → 归一化)

    uint layerGroupCount = (terrain.layer_count + 3u) >> 2u;

    for (uint g = 0; g < layerGroupCount; g++) {
        vec4 weights = texture(TerrainSplatMap, vec3(terrainUV, float(g)));
        for (int c = 0; c < 4; c++) {
            uint idx = g * 4u + uint(c);
            if (idx >= terrain.layer_count) break;

            float w = weights[c];
            if (w < 0.004) continue;

            TerrainLayerParams lp = TerrainLayerSSBO.layers[idx];
            vec2 luv = terrainUV * lp.uv_scale * terrain.tile_scale;

            vec3 albedo = texture(TerrainAlbedoArray,
                                  vec3(luv, float(lp.albedo_array_index))).rgb;
            albedo *= unpackUnorm4x8(lp.tint_color).rgb;

            vec3 nrm = texture(TerrainNormalArray,
                               vec3(luv, float(lp.normal_array_index))).xyz * 2.0 - 1.0;

            o.baseColor += albedo * w;
            o.normal    += nrm * w;

#if QUALITY_TIER >= 2  // Medium+: MR 纹理
            vec2 mr = texture(TerrainMRArray,
                              vec3(luv, float(lp.mr_array_index))).bg;
            o.roughness += (mr.y + lp.roughness_bias) * w;
            o.metallic  += (mr.x + lp.metallic_bias) * w;
#else
            o.roughness += (0.5 + lp.roughness_bias) * w;
            o.metallic  += lp.metallic_bias * w;
#endif
        }
    }

    o.normal = normalize(o.normal) * terrain.normal_strength;
    o.ao = 1.0;
    return o;
}

float EvalAlpha(vec2 uv) { return 1.0; }  // 地形完全不透明
```

#### 5.5.5 画质档位降级

| 档位 | 纹理采样 | 最大有效层数 | 额外特性 |
|------|---------|------------|---------|
| Lowest | Albedo only (顶点法线) | 4 | — |
| Low | Albedo + Normal | 8 | — |
| Medium | Albedo + Normal + MR | 16 | Height-Based Blending |
| High | 全纹理 | 64 | Height-Based + Triplanar (可选) |
| Ultra | 全纹理 | 128 | Tessellation 可选 |
| Cinematic | 全纹理 | 256 | Tessellation + Micro-Detail |

> **低端限制**：Android Low / Lowest 档位将 SplatMap 采样限制在前 1~2 个 Array Layer
> （4~8 层），且跳过 MR 纹理采样，以控制 texel fetch 开销。

#### 5.5.6 与 Tile Classification 的交互

在 VBuffer 路径中，Terrain 像素的 `SurfaceType == Terrain`，Tile Classification
会将纯 Terrain tile 归类为单一材质 tile—— dispatch Terrain 专用 resolve kernel。
混合 tile（Terrain + 角色/植被边缘）若匹配已注册的融合组合则走融合路径（§8.5），否则走通用 multi-material 路径。

> **性能特征**：开阔地形场景 70%+ tile 是纯 Terrain，走无分支的 Terrain resolve，
> Texture2DArray 的连续访问模式对 GPU 纹理缓存非常友好。

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
│  MaterialCategory      material_category                  │ ← 视觉材质种类（§5.1.1）
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
│  bool             enable_terrain_contact_dither           │ ← 可开关 (§2.8.5)
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
  binding 0: LocalToWorld SSBO   (mat4[], 全场景 L2W 矩阵池 —— 由 TransformAssignmentBuffer 管理)
                                  ★ 通过 Instance-Rate VBO (R16UI) 传入 TransformID，
                                    VS 中 GetLocalToWorld() 以 TransformID 索引此 SSBO

Set 2 — PerMaterial（每材质切换时绑定）
  ── 通用布局（Standard / Special Surface）──
  binding 0: MaterialInstance SSBO (MI_Standard[], 全场景 MI 数据池 —— 由 MaterialInstanceAssignmentBuffer 管理)
                                  ★ 通过 Instance-Rate VBO (R16UI) 传入 MaterialInstanceID，
                                    FS 中 GetMI() 以 MaterialInstanceID 索引此 SSBO
  binding 1-6: 纹理槽            (支持传统纹理 sampler2D 和纹理阵列 sampler2DArray 双模式 — §6.4.4)
    ── Standard Surface 纹理槽（编译期 #define TEXTURE_MODE 选择模式）──
      binding 1: Albedo               (sampler2D 或 sampler2DArray, Low+)
      binding 2: Normal               (sampler2D 或 sampler2DArray, Low+)
      binding 3: MetallicRoughness    (sampler2D 或 sampler2DArray, Medium+)
      binding 4: AO                   (sampler2D 或 sampler2DArray, High+)
      binding 5: Emissive             (sampler2D 或 sampler2DArray, High+)
      binding 6: DetailNormal         (sampler2D 或 sampler2DArray, Ultra, 预留)
    ── Special Surface 扩展纹理槽 ──
      binding 7: TextureExtra0   (Skin: SubsurfaceColor / Hair: Direction)
      binding 8: TextureExtra1   (Skin: Thickness / Hair: Shift)
      binding 9: TextureExtra2   (ClearCoat: CoatNormal)
      binding 10: TextureExtra3  (预留)
      binding 11: TextureExtra4  (预留)
      binding 12: TextureExtra5  (预留)
  ── Terrain 专用布局（SurfaceType == Terrain 时替换整个 Set 2）── ★
  binding 0: TerrainGlobalUBO         (UBO, MI_Terrain, 32B)
  binding 1: TerrainLayerSSBO         (SSBO, TerrainLayerParams[N], ≤256×32=8KB)
  binding 2: TerrainAlbedoArray       (sampler2DArray, ≤256 层)
  binding 3: TerrainNormalArray       (sampler2DArray, ≤256 层)
  binding 4: TerrainMRArray           (sampler2DArray, Medium+, ≤256 层)
  binding 5: TerrainSplatMap          (sampler2DArray, ceil(N/4) 层, RGBA8 权重)
  binding 6: TerrainHeightMap         (sampler2D, R16 全局高度图)
  binding 7: TerrainNormalMapGlobal   (sampler2D, 地形整体法线图)

Set 3 — Environment（环境/全局光照资源 + 管线 RT）
  binding 0: ColorPalette        (UBO, PaletteColor3D 专用)
  binding 1: ShadowMap_Near      (sampler2DShadow — 近景 Dynamic Cascade, §3.6.3)
  binding 2: ShadowMask          (sampler2D, RGBA8 — ShadowMask Compose 输出, §3.6.2)
  binding 3: SSAO_RT             (sampler2D, R8 — SSAO/SSDO Pass 输出)
  binding 4: IBL_Irradiance      (samplerCube)
  binding 5: IBL_Prefiltered     (samplerCube)
  binding 6: IBL_BRDF_LUT        (sampler2D — 仅 BRDF_LUT_TEXTURE 模式绑定；函数近似模式下此 binding 闲置)
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
| TileMaterialKeys | SSBO (uint[4]) | maxTileCount × 16B | 每 tile 至多 4 个 MaterialKey（SurfaceType×PresetID） |
| TileMaterialCount | R8UI image | tileCountX × tileCountY | 每 tile 的唯一 MaterialKey 计数 |
| TileList_Single | SSBO (uvec2[]) | maxTileCount × 8B | 单一材质 tile 坐标列表 |
| TileList_Fused | SSBO (uvec4[]) | maxTileCount × 16B | 融合材质 tile 坐标 + fusedShaderID |
| TileList_Multi | SSBO (uvec2[]) | maxTileCount × 8B | 多材质 tile 坐标列表 |
| FusedComboLUT | SSBO | fusedComboCount × 20B | 已注册的融合材质组合 → fusedShaderID 映射表 |
| TileDispatchArgs | SSBO (72B) | 72 bytes | 3 组 indirect dispatch 参数 + 计数器 |

> `maxTileCount = ceil(W/TILE_SIZE) × ceil(H/TILE_SIZE)`，1080p@8×8 tile ≈ 32,400 tiles，内存总计 < 1 MB。

---

### 6.4 Instance ID 分发机制（已有实现，直接复用） ★

本引擎**已完整实现**了一套 Instance ID 分发系统，将 TransformID 和 MaterialInstanceID
传递给 VS/FS，配合全场景 SSBO 数据池 + Texture2DArray 纹理池实现高效 GPU-Driven 渲染。
**新材质体系必须保留并延续此架构。**

ID 的传递方式按 **PlatformBackend**（§2.9）分流，与顶点数据获取方式保持一致：

| 平台 | ID 传递方式 | 理由 |
|------|-----------|------|
| **PC (SSBO)** | ID 存入 SSBO，VS 内按 `gl_InstanceIndex` 读取 | SSBO 在桌面 GPU 上访问效率极高 |
| **Apple / PowerVR (SSBO)** | 同上 | Apple Silicon 统一内存架构，SSBO 无额外开销 |
| **Android Mid/Low (VBO)** | ID 存入 Instance-Rate VBO (R16UI)，传统 vertex attribute 输入 | 非 Apple/PowerVR 的移动 GPU 上，传统 vertex attribute 硬件路径比 SSBO 读取更高效 |

#### 6.4.1 核心数据流

```
┌──────────────────────────────────────────────────────────────────────┐
│                    Instance ID 分发架构（双路径）                      │
│                                                                      │
│  CPU 侧                                                             │
│  ┌──────────────────────────┐  ┌──────────────────────────────┐     │
│  │ TransformAssignmentBuffer│  │ MaterialInstanceAssignment   │     │
│  │                          │  │ Buffer                       │     │
│  │  ① L2W 矩阵池 (SSBO)    │  │  ③ MI 数据池 (SSBO)          │     │
│  │     mat4 mats[N]         │  │     MI_Xxx mi[M]             │     │
│  │                          │  │                              │     │
│  │  ② TransformID           │  │  ④ MaterialInstanceID        │     │
│  │   ┌─ SSBO路径: ID SSBO   │  │   ┌─ SSBO路径: ID SSBO       │     │
│  │   └─ VBO路径:  R16UI VAB │  │   └─ VBO路径:  R16UI VAB     │     │
│  └─────────┬────────────────┘  └──────────┬───────────────────┘     │
│            │                               │                         │
│  GPU 侧   ▼                               ▼                         │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ Vertex Shader                                                │   │
│  │  #if PLATFORM_SSBO          │  #else (VBO 路径)              │   │
│  │   // ID 从 SSBO 读取        │   // ID 从 instance-rate VAB   │   │
│  │   uint tID = id_ssbo        │   layout(location=N) in uint   │   │
│  │     .transformIDs            │     TransformID;               │   │
│  │     [gl_InstanceIndex];      │   uint tID = TransformID;      │   │
│  │  #endif                     │                                │   │
│  │   mat4 l2w = l2w_ssbo.mats[tID];                ← SSBO ①    │   │
│  │   gl_Position = viewProj * l2w * vec4(pos, 1.0);             │   │
│  │   vs_out.materialInstanceID = miID;                          │   │
│  └──────────────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │ Fragment Shader                                              │   │
│  │   MI_Standard mi = mi_ssbo.mi[materialInstanceID]; ← SSBO ③ │   │
│  │   vec4 albedo = texture(TextureArray, vec3(uv, mi.tex_albedo));│  │
│  │                                          ↑ Texture2DArray ⑤  │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                                                                      │
│  ⑤ Texture2DArray: 所有 MI 用到的纹理打包成纹理数组                   │
│     MI 数据中存 texture_id → 在 FS 中以 texture_id 索引纹理层         │
└──────────────────────────────────────────────────────────────────────┘
```

> **注意**：无论哪条路径，L2W 矩阵和 MI 数据始终存储在 SSBO 中（①③ 不变）。
> 只有 **ID 本身的传递方式**（② ④）随平台分流。

#### 6.4.2 现有代码实现

| 组件 | 文件 | 职责 |
|------|------|------|
| `TransformAssignmentBuffer` | `inc/hgl/ecs/support/TransformAssignmentBuffer.h` | 管理 L2W 矩阵 SSBO + R16UI instance-rate VAB；支持 Static/Movable 两种模式；Movable 使用 `RingBufferWrapper` 多帧复用 |
| `MaterialInstanceAssignmentBuffer` | `inc/hgl/ecs/support/MaterialInstanceAssignmentBuffer.h` | 管理 MI 数据 SSBO + R16UI instance-rate VAB；`MaterialInstanceSet` 对 MI 去重 (UnorderedMap)；Ring buffer 多帧复用 |
| `GetLocalToWorld()` | `ShaderLibrary/common/transform.glsl` / `MFCommon.h` | GLSL helper：`l2w.mats[TransformID]` |
| `GetMI()` | `ShaderLibrary/common/material_instance.glsl` / `MFCommon.h` | GLSL helper：`mtl.mi[MaterialInstanceID]` |
| `HandoverMI()` | `MFCommon.h` | VS→FS 传递 MaterialInstanceID (flat varying) |

#### 6.4.3 ID 传递双路径

##### SSBO 路径（PC / Apple / PowerVR）

ID 与 L2W / MI 数据一样存储在 SSBO 中，VS 内按 `gl_InstanceIndex` 索引读取：

```glsl
// common/instance_id_fetch_ssbo.glsl
#define INSTANCE_ID_FETCH_SSBO 1

layout(set = 1, binding = 1) readonly buffer TransformIDBuffer {
    uint transformIDs[];     // 紧凑 uint 数组
} _TransformIDBuffer;

layout(set = 2, binding = 13) readonly buffer MaterialInstanceIDBuffer {
    uint miIDs[];            // 紧凑 uint 数组
} _MaterialInstanceIDBuffer;

uint GetTransformID()          { return _TransformIDBuffer.transformIDs[gl_InstanceIndex]; }
uint GetMaterialInstanceID()   { return _MaterialInstanceIDBuffer.miIDs[gl_InstanceIndex]; }
```

此路径下 **Pipeline 的 `VkPipelineVertexInputStateCreateInfo` 中无 instance-rate binding**
（与 SSBO 顶点获取路径一致 — 若同时使用 SSBO 顶点获取，则 VertexInput 完全为空）。

**优势**：
- 零 `vkCmdBindVertexBuffers` 绑定开销（ID SSBO 帧开始时一次性绑定）
- 与 Indirect Draw / Meshlet 管线天然兼容（`gl_InstanceIndex` 自动递增）
- 桌面 GPU 和 Apple Silicon 上 SSBO 随机访问效率极高

##### VBO 路径（Android Mid/Low — 非 Apple/PowerVR 移动 GPU）

ID 通过传统 instance-rate Vertex Attribute 输入：

```
Binding 0: 几何顶点数据 (vertex-rate)
           — Layout A~E 之一（见 §12）
           — inputRate = VK_VERTEX_INPUT_RATE_VERTEX

Binding 1: TransformID (instance-rate)        ← TransformAssignmentBuffer 提供
           — format = VK_FORMAT_R16_UINT
           — inputRate = VK_VERTEX_INPUT_RATE_INSTANCE

Binding 2: MaterialInstanceID (instance-rate)  ← MaterialInstanceAssignmentBuffer 提供
           — format = VK_FORMAT_R16_UINT
           — inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
```

```glsl
// common/instance_id_fetch_vbo.glsl
#define INSTANCE_ID_FETCH_VBO 1

layout(location = 5) in uint inTransformID;         // Binding 1, instance-rate
layout(location = 6) in uint inMaterialInstanceID;  // Binding 2, instance-rate

uint GetTransformID()          { return inTransformID; }
uint GetMaterialInstanceID()   { return inMaterialInstanceID; }
```

仅 2 字节 × 2 = **4 字节/实例**即完成 L2W 矩阵 + MI 完整数据的分发。
对比传统方案（per-instance 传递完整 mat4 = 64 字节 + MI 数据 32+ 字节），带宽减少 **96%**。

**优势**：
- 在 Mali / Adreno 等移动 GPU 上，硬件 Vertex Fetch 单元对 vertex attribute 有专用缓存和预取优化，
  效率高于通用 SSBO 随机访问
- 无需 `maxStorageBufferRange` 大容量要求

##### 双路径对比

| 维度 | SSBO 路径 (PC/Apple/PowerVR) | VBO 路径 (Android Mid/Low) |
|------|------------------------------|----------------------------|
| **ID 存储** | ID SSBO (Set 1 binding 1 / Set 2 binding 13) | R16UI instance-rate VBO |
| **VS 读取** | `_IDBuffer.ids[gl_InstanceIndex]` | `layout(location=N) in uint` |
| **Pipeline VertexInput** | 无 instance-rate binding | Binding 1-2 为 instance-rate |
| **绑定开销** | 帧开始一次性绑定 SSBO | 每 DrawCall `vkCmdBindVertexBuffers` |
| **GPU 效率** | 桌面/Apple Silicon SSBO 高效 | 移动 GPU vertex fetch 硬件优化 |
| **Indirect Draw 兼容** | ★★★★★ 天然兼容 | ★★★☆☆ 需确保 VAB 连续性 |
| **条件** | `maxStorageBufferRange ≥ 128MB` | 所有 Vulkan 设备支持 |

#### 6.4.4 纹理双模式：传统纹理 vs Texture2DArray

纹理绑定支持**两种模式**，通过编译期 `#define TEXTURE_ARRAY 0/1` 切换。
两种模式共用相同的 binding 编号（Set 2 binding 1-6），只是 sampler 声明类型不同。

##### 模式 A：传统纹理（sampler2D）

每个 MaterialInstance 绑定自己的独立纹理，切换材质时通过 `vkCmdBindDescriptorSets` 更换 Set 2。

```glsl
// #define TEXTURE_ARRAY 0
layout(set = 2, binding = 1) uniform sampler2D TextureAlbedo;
layout(set = 2, binding = 2) uniform sampler2D TextureNormal;
layout(set = 2, binding = 3) uniform sampler2D TextureMetallicRoughness;
layout(set = 2, binding = 4) uniform sampler2D TextureAO;
layout(set = 2, binding = 5) uniform sampler2D TextureEmissive;

// 采样：直接使用 uv
vec4 albedo = texture(TextureAlbedo, uv);
```

**适用场景**：
- 简单材质 / Debug / 编辑器预览
- 单个物体使用独特纹理、无需批量合并的情况
- 不支持 Texture2DArray 的极低端设备（理论上）
- 快速原型开发、单独测试材质效果

##### 模式 B：纹理阵列（sampler2DArray）

所有 MI 的纹理打包进 `sampler2DArray`，MI 数据中存储 `texture_id` 层索引。

```glsl
// #define TEXTURE_ARRAY 1
layout(set = 2, binding = 1) uniform sampler2DArray AlbedoArray;
layout(set = 2, binding = 2) uniform sampler2DArray NormalArray;
layout(set = 2, binding = 3) uniform sampler2DArray MetallicRoughnessArray;
layout(set = 2, binding = 4) uniform sampler2DArray AOArray;
layout(set = 2, binding = 5) uniform sampler2DArray EmissiveArray;

// 采样：uv + MI 中的层索引
MI_Standard mi = GetMI();
vec4 albedo = texture(AlbedoArray, vec3(uv, float(mi.tex_albedo)));
```

**适用场景**：
- GPU-Driven 批量渲染（单个 DrawCall 渲染多种纹理的实例）
- 配合 Indirect Draw / Meshlet 管线
- 大量相似物体（草地、石块、建筑组件等）

##### 双模式对比

| 维度 | 传统纹理 (sampler2D) | 纹理阵列 (sampler2DArray) |
|------|---------------------|--------------------------|
| **Descriptor 绑定** | 每个 MI 各自的 DescriptorSet | 全场景共享一个 DescriptorSet |
| **纹理分辨率** | 每张纹理可独立分辨率 | 同一 Array 内所有层分辨率必须一致 |
| **DrawCall 合并** | 不同纹理 = 不同 DrawCall | 不同纹理可合并为同一 DrawCall |
| **MI 结构** | `tex_*` 字段闲置 | `tex_*` 字段存储层索引 |
| **内存管理** | 各纹理独立分配 | 需预先打包进 Array + 分辨率分组 |
| **适合规模** | 少量独特材质 | 大量批量渲染的材质 |
| **编译期控制** | `#define TEXTURE_ARRAY 0` | `#define TEXTURE_ARRAY 1` |

> **运行时选择策略**：引擎可根据场景特征自动选择模式。\
> 大量同类物体（草/树/石块/建筑）→ 纹理阵列模式，减少 DrawCall。\
> 少量独特物体（主角/NPC/特殊道具）→ 传统纹理模式，保持灵活性。\
> 两种模式可在同一帧内混用（不同 DrawCall 使用不同模式的 Pipeline 变体）。

#### 6.4.5 在新材质体系中的角色

| 新设计组件 | Instance-Rate ID 分发的关系 |
|-----------|---------------------------|
| §6.3 Set 1 binding 0 (LocalToWorld SSBO) | TransformAssignmentBuffer 填充此 SSBO |
| §6.3 Set 2 binding 0 (MI SSBO) | MaterialInstanceAssignmentBuffer 填充此 SSBO |
| §6.3 Set 2 binding 1-6 (纹理槽) | 传统纹理 (sampler2D) 或纹理阵列 (sampler2DArray)，编译期 `TEXTURE_ARRAY` 切换 |
| §12 per-instance 列中的 TransformID | SSBO 路径: ID SSBO + `gl_InstanceIndex`; VBO 路径: R16UI VAB |
| §12 per-instance 列中的 MaterialInstanceID | SSBO 路径: ID SSBO + `gl_InstanceIndex`; VBO 路径: R16UI VAB |
| Surface Function `GetMI()` | 通过 MaterialInstanceID 索引 MI SSBO |
| Surface Function `GetLocalToWorld()` | 通过 TransformID 索引 L2W SSBO |
| VBuffer ID Pass | Instance ID 直接编码到 VBuffer，Resolve 阶段同样用 ID 索引 SSBO |
| Meshlet 管线 | Meshlet GPU Cull 输出的 Indirect Draw 携带 instance offset，自动对应 ID VAB |

> **结论**：Instance-Rate ID 分发是本引擎 GPU-Driven 渲染的**基石**，
> 新材质体系的所有渲染路径（Forward / VBuffer / Shadow / Meshlet）均依赖此机制。

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

#### 7.5.1 BRDF LUT 双模式：纹理法 vs 函数近似法

IBL 环境光计算需要 **Split-Sum Approximation** 的第二项——BRDF 预积分项 `EnvBRDF(F0, roughness, NdotV)`。
引擎同时支持两种获取方式，通过编译期 `#define BRDF_LUT_TEXTURE 0/1` 切换：

| | 纹理法 (`BRDF_LUT_TEXTURE 1`) | 函数近似法 (`BRDF_LUT_TEXTURE 0`) |
|--|------|------|
| 原理 | 离线预计算 BRDF 积分写入 2D LUT 纹理 (RG16F, 512×512)，运行时查表 | 用解析多项式直接在 FS 中计算，无需纹理 |
| binding | Set 3 binding 6 `IBL_BRDF_LUT` (sampler2D) | 不需要纹理绑定，binding 6 闲置 |
| 精度 | 高——精确数值积分 | 极好——Karis 2014 拟合误差 < 0.5% |
| 性能 | 1 次纹理采样（缓存友好，纹理很小） | 几条 ALU 指令，无纹理依赖 |
| 适用场景 | 默认方案；需要自定义 BRDF 模型时可替换 LUT | 移动端减少纹理 binding / 极简管线 / 不方便预计算 LUT 时 |
| 资源开销 | 需预计算并上传 512×512 RG16F 纹理 (~512KB) | 零额外资源 |

`ambient.glsl` 中的实现：

```glsl
// common/ambient.glsl — BRDF 预积分项获取（双模式）

#if BRDF_LUT_TEXTURE
// ---- 纹理法：从预计算 LUT 中查表 ----
vec2 EnvBRDF(float NdotV, float roughness)
{
    return texture(IBL_BRDF_LUT, vec2(NdotV, roughness)).rg;
}
#else
// ---- 函数近似法（Karis 2014 / Lazarov 2013 拟合）----
// 参考: Brian Karis, "Real Shading in Unreal Engine 4", SIGGRAPH 2013 Course Notes
//       Dimitar Lazarov, "Getting More Physical in Call of Duty: Black Ops II"
vec2 EnvBRDF(float NdotV, float roughness)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4( 1.0,  0.0425,  1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}
#endif

// ---- IBL 环境光（使用 EnvBRDF 双模式 — §7.5.1）----
vec3 GetAmbientIBL(vec3 N, vec3 V, vec3 baseColor, vec3 F0,
                   float metallic, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);

    // Diffuse: Irradiance CubeMap
    vec3 irradiance = texture(IBL_Irradiance, N).rgb;
    vec3 kD = (1.0 - F0) * (1.0 - metallic);
    vec3 diffuse = kD * baseColor * irradiance;

    // Specular: Prefiltered CubeMap + EnvBRDF
    vec3 R = reflect(-V, N);
    float mipLevel = roughness * float(textureQueryLevels(IBL_Prefiltered) - 1);
    vec3 prefilteredColor = textureLod(IBL_Prefiltered, R, mipLevel).rgb;
    vec2 envBRDF = EnvBRDF(NdotV, roughness);
    vec3 specular = prefilteredColor * (F0 * envBRDF.x + envBRDF.y);

    return diffuse + specular;
}
```

> **选择指南**：
> - **PC / 主机 / Apple 高端**：默认使用纹理法（`BRDF_LUT_TEXTURE 1`），LUT 纹理在引擎启动时一次性预计算
> - **移动端 / 极简管线**：可选函数近似法（`BRDF_LUT_TEXTURE 0`），省去一个纹理 binding 和预计算步骤
> - 两种模式输出结果视觉差异极小，可随 `QualityTier` 或 `PlatformBackend` 自动选择\
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
- `TERRAIN_CONTACT` flag 为 true 且 `FEATURE_TERRAIN_CONTACT_DITHER == 1` 时额外生成 `TERRAIN_CONTACT_DITHER` 变体
- `SKINNED` flag 为 true 时 VS 切换为含骨骼蒙皮的版本（`compositor/main_forward_skinned.vert.glsl`）
- `VBUFFER_RESOLVE` 由 Compute Shader 通用处理，不需要每个材质生成独立变体（在 Resolve 着色器中 `#include` 对应 Surface Function 并用 switch 分发）
- Unlit 材质（SurfaceType = Unlit 类）较简单，保留独立完整 VS/FS（不经过 Compositor）

**变体总数估算（Standard Surface, Opaque BlendMode, PC）：**
```
PassType(4: Forward+Shadow+EarlyZ+VBuffer) × QualityTier(6) × ShadowMode(3) × flags(~4) ≈ 288 变体
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
    ambient.glsl                   // 环境光计算 (Simple / FakeAtm / IBL) + BRDF LUT 双模式 (纹理法/函数近似法 §7.5.1)
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
    vbuffer_resolve_fused.comp.glsl    // 融合 2-4 材质 resolve（§8.5）
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

### 8.2 Tile Classification（Tile 材质统计 + 融合匹配）

在 VBuffer ID Pass 写入完成后、Resolve 之前，插入一个 **Tile Classification** 阶段。
将屏幕划分为固定大小的 tile（如 8×8 或 16×16 像素），统计每个 tile 内出现了哪些
**MaterialKey**（= SurfaceType × PresetID，8-bit 组合键），以便后续 Resolve 按 tile 复杂度分三层调度。

#### 设计动机

仅按 SurfaceType（11 种）分类虽然能区分光照模型差异，但**无法捕捉同一 SurfaceType 内不同
MaterialPreset 的材质交错**。典型问题场景：

| 场景特征 | tile 内材质组成 | 问题 |
|----------|----------------|------|
| 大面积墙壁/地面 | 1 种 Standard | 无问题，Single path |
| **森林冠层**（树叶 + 树干/树皮） | 2-3 种 Standard（不同 preset） | 同一 SurfaceType，但纹理/参数完全不同 |
| **城镇街道**（地面 + 建筑外墙 + 招牌） | 2-4 种 Standard | 高交错密度，大量 tile 含多材质 |
| 角色 + 场景边缘 | Standard + Skin + Hair | 跨 SurfaceType 交错 |

> 若仅按 SurfaceType 分类，森林场景中树叶和树皮同属 Standard → 被归为 "single type"，
> 但实际上这些 tile 内有 2-3 种不同材质预设（不同纹理集、不同参数）。
> 虽然 Compute 路径的 switch 不会发散（同一 SurfaceType），但
> **当需要对每种材质分别 dispatch（opaque/mask 模式）时**，高交错率会导致大量 tile
> 被推入 mask 路径，严重时接近全屏 mask → 性能灾难。
>
> **融合材质着色器（Fused Material Shader）** 通过把 2-4 种常见共现材质编译进一个着色器，
> 消除逐材质 dispatch 的开销，核心解决此类高交错场景的性能问题（§8.5）。

#### 分类层级

```
MaterialKey = (SurfaceType << 4) | PresetID     // 8-bit 组合键

Tile Classification 输出四类:
  ┌──────────────────────────────────────────────────────────────┐
  │ 1. Empty Tile        — 所有像素 VBuffer == 0（天空/无几何体）  │
  │ 2. Single-Material   — 仅含 1 种 MaterialKey                │
  │ 3. Fused-Material    — 含 2-4 种 MaterialKey，且匹配          │
  │                        已注册的融合材质组合（FusedComboLUT）    │
  │ 4. Multi-Material    — 含 2+ 种 MaterialKey，无匹配融合组合   │
  └──────────────────────────────────────────────────────────────┘
```

#### Tile 大小选择

```cpp
constexpr uint32_t TILE_SIZE = 8;  // 8×8 像素，与 Compute local_size 对齐
// 也可以 16×16，视 GPU 架构和场景复杂度调优
// tile 数量 = ceil(screenWidth/TILE_SIZE) × ceil(screenHeight/TILE_SIZE)
```

#### FusedComboLUT — 融合材质组合注册表

CPU 侧在场景加载时构建，上传为 SSBO：

```cpp
struct FusedCombo
{
    uint8_t  materialKeys[4];   // 排序后的 MaterialKey（未使用的填 0xFF）
    uint8_t  keyCount;          // 实际包含的材质数（2-4）
    uint8_t  fusedShaderID;     // 对应的融合 resolve shader 索引
    uint16_t _pad;
};

// 典型注册示例（按 MaterialCategory 共现模式 — §5.1.1）：
FusedCombo combos[] = {
    // 森林冠层: TreeLeaf + Wood（都是 Standard，同 SurfaceType 融合）
    { {KEY_TreeLeaf, KEY_TreeBark, 0xFF, 0xFF},  2, FUSED_FOREST_CANOPY,  0 },
    // 地形边缘: Grass + Sand + Stone（Standard 族内多材质）
    { {KEY_Ground, KEY_Grass, KEY_Rock, 0xFF},    3, FUSED_TERRAIN_EDGE,   0 },
    // 建筑墙面: Stone + Concrete（Standard 族）
    { {KEY_WallBrick, KEY_WallPlaster, 0xFF, 0xFF}, 2, FUSED_BUILDING_WALL, 0 },
};
// fusedComboCount 通常 4-16 条，不超过 64 条
```

> **注册策略**：由引擎/美术人员根据场景中出现的 MaterialCategory 共现模式注册常见组合（§5.1.1）。
> 典型方法：开发期通过 Debug 可视化（§8.6）统计 Multi-Material tile 占比最高的 MaterialCategory 组合，
> 将高频组合注册为 Fused Combo → 编译对应的融合 shader。

#### Phase 1: Tile Classification Compute Shader

```glsl
// vbuffer_tile_classify.comp.glsl
layout(local_size_x=TILE_SIZE, local_size_y=TILE_SIZE) in;

layout(set=0, binding=0) uniform usampler2D VBuffer;

// 输出: SurfaceType bitmask（用于 shadow/SSAO 剔空优化）
layout(set=0, binding=1, r32ui) writeonly uniform uimage2D TileSurfaceMask;
// 输出: 每 tile 唯一材质数
layout(set=0, binding=2, r8ui) writeonly uniform uimage2D TileMaterialCount;
// 输出: 每 tile 排序后的 MaterialKey 列表（至多 4 个）
layout(set=0, binding=3) buffer TileMaterialKeys { uint tileKeys[]; };
// maxTileCount × 4 uint (每 tile 4 个 key slot)

layout(set=0, binding=4) buffer TileDispatchArgs {
    uint single_count;
    uint fused_count;
    uint multi_count;
    uint empty_count;
    uvec4 single_dispatch;      // (num_groups_x, 1, 1, 0)
    uvec4 fused_dispatch;       // (num_groups_x, 1, 1, 0)
    uvec4 multi_dispatch;       // (num_groups_x, 1, 1, 0)
};

layout(set=0, binding=5) buffer TileList_Single { uvec2 single_tiles[]; };
layout(set=0, binding=6) buffer TileList_Fused  { uvec4 fused_tiles[];  };
// fused_tiles[i] = (tileCoord.x, tileCoord.y, fusedShaderID, keyCount)
layout(set=0, binding=7) buffer TileList_Multi  { uvec2 multi_tiles[];  };

// 融合材质组合查找表
layout(set=0, binding=8) readonly buffer FusedComboLUT {
    uint fusedComboCount;
    uint _pad[3];
    FusedCombo combos[];    // 按 materialKeys 排序，便于二分查找
};

// Workgroup 共享内存
shared uint  s_surfaceMask;
shared uint  s_keyCount;
shared uint  s_keys[4];        // 至多跟踪 4 个唯一 MaterialKey

void main()
{
    if (gl_LocalInvocationIndex == 0)
    {
        s_surfaceMask = 0u;
        s_keyCount    = 0u;
        s_keys[0] = s_keys[1] = s_keys[2] = s_keys[3] = 0xFFu;
    }
    barrier();

    ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
    uvec2 vbuf  = texelFetch(VBuffer, pixel, 0).xy;

    if (vbuf.x != 0u)
    {
        uint surfaceType = vbuf.x >> 28;
        uint presetID    = (vbuf.x >> 24) & 0xFu;
        uint matKey      = (surfaceType << 4) | presetID;

        atomicOr(s_surfaceMask, 1u << surfaceType);

        // 尝试将 matKey 加入唯一列表（至多 4 个）
        // 简单线性查找 + 原子插入
        bool found = false;
        for (uint i = 0; i < 4; i++)
        {
            uint existing = atomicCompSwap(s_keys[i], 0xFFu, matKey);
            if (existing == 0xFFu || existing == matKey)
            {
                found = true;
                if (existing == 0xFFu)  // 新插入
                    atomicAdd(s_keyCount, 1u);
                break;
            }
        }
        if (!found)  // 超过 4 种材质
            atomicMax(s_keyCount, 5u);
    }
    barrier();

    if (gl_LocalInvocationIndex == 0)
    {
        ivec2 tileCoord = ivec2(gl_WorkGroupID.xy);
        uint tileIndex  = gl_WorkGroupID.y * (gl_NumWorkGroups.x) + gl_WorkGroupID.x;
        uint mask     = s_surfaceMask;
        uint keyCount = s_keyCount;

        // 写出 SurfaceType bitmask（供 shadow/SSAO 剔空优化）
        imageStore(TileSurfaceMask, tileCoord,
                   uvec4(mask == 0u ? 0x80000000u : mask));

        // 写出 MaterialKey 列表
        for (uint i = 0; i < 4; i++)
            tileKeys[tileIndex * 4 + i] = s_keys[i];
        imageStore(TileMaterialCount, tileCoord, uvec4(keyCount));

        if (mask == 0u)
        {
            // ---- Empty ----
            atomicAdd(empty_count, 1u);
        }
        else if (keyCount == 1u)
        {
            // ---- Single-Material ----
            uint idx = atomicAdd(single_count, 1u);
            single_tiles[idx] = uvec2(tileCoord);
        }
        else if (keyCount <= 4u)
        {
            // 尝试匹配 FusedComboLUT
            // 对 s_keys 排序后与 LUT 逐条比对（combo 数量少，线性扫描即可）
            uint sortedKeys[4];
            for (uint i = 0; i < 4; i++) sortedKeys[i] = s_keys[i];
            // 简单冒泡排序（至多 4 元素）
            for (uint i = 0; i < 3; i++)
                for (uint j = i + 1; j < 4; j++)
                    if (sortedKeys[j] < sortedKeys[i])
                    { uint t = sortedKeys[i]; sortedKeys[i] = sortedKeys[j]; sortedKeys[j] = t; }

            int fusedID = -1;
            for (uint c = 0; c < fusedComboCount; c++)
            {
                if (combos[c].keyCount != keyCount) continue;
                bool match = true;
                for (uint k = 0; k < keyCount; k++)
                    if (combos[c].materialKeys[k] != sortedKeys[k])
                    { match = false; break; }
                if (match) { fusedID = int(combos[c].fusedShaderID); break; }
            }

            if (fusedID >= 0)
            {
                // ---- Fused-Material ----
                uint idx = atomicAdd(fused_count, 1u);
                fused_tiles[idx] = uvec4(tileCoord, uint(fusedID), keyCount);
            }
            else
            {
                // ---- Multi-Material（无匹配融合组合）----
                uint idx = atomicAdd(multi_count, 1u);
                multi_tiles[idx] = uvec2(tileCoord);
            }
        }
        else
        {
            // ---- Multi-Material（超过 4 种材质）----
            uint idx = atomicAdd(multi_count, 1u);
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
    uint single_count;
    uint fused_count;
    uint multi_count;
    uint empty_count;
    uvec4 single_dispatch;
    uvec4 fused_dispatch;
    uvec4 multi_dispatch;
};

void main()
{
    single_dispatch = uvec4(single_count, 1, 1, 0);
    fused_dispatch  = uvec4(fused_count,  1, 1, 0);
    multi_dispatch  = uvec4(multi_count,  1, 1, 0);
}
```

#### Tile 分类结果 — 数据结构

```
TileSurfaceMask (R32UI image, tileCountX × tileCountY):
  每个 texel = uint32 bitmask（SurfaceType 级别，用于 shadow/SSAO 剔空优化）
  bit 0-10 = SurfaceType 0-10
  bit 31   = empty tile

TileMaterialCount (R8UI image, tileCountX × tileCountY):
  每个 texel = uint8，tile 内唯一 MaterialKey 数量（0=empty, 1=single, 2-4=fused/multi, 5+=multi）

TileMaterialKeys (SSBO, maxTileCount × 4 × uint):
  每 tile 存储至多 4 个排序后的 MaterialKey（未使用的为 0xFF）

TileList_Single[]: 单一材质 tile 坐标 (uvec2)
TileList_Fused[]:  融合材质 tile (uvec4: tileCoord + fusedShaderID + keyCount)
TileList_Multi[]:  多材质 tile 坐标 (uvec2)
TileDispatchArgs:  3 组 indirect dispatch 参数 + 4 计数器（由 GPU 自行填充，CPU 不回读）
FusedComboLUT:     已注册融合组合查找表（CPU 上传，帧间稳定）
```

> **扩展性**：SurfaceType bitmask 仍保留供 shadow/SSAO 按 SurfaceType 剔空优化。
> MaterialKey 分类是对 SurfaceType 分类的**细化**——在同一 SurfaceType 内进一步区分不同 Preset。

### 8.3 VBuffer Resolve Pass（Tile-Based Dispatch）

Resolve 阶段分为**三批** dispatch，由 Classification 阶段的 indirect args 驱动：

#### Dispatch 1: 单一材质 Tile（独占路径 — 最快）

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

#### Dispatch 2: 融合材质 Tile（融合路径 — 中速）— §8.5 详述

```glsl
// vbuffer_resolve_fused.comp.glsl — 详见 §8.5
// 每个 workgroup 处理一个融合材质 tile
layout(local_size_x=TILE_SIZE, local_size_y=TILE_SIZE) in;

// ... 相同的 VBuffer / Depth / LitColor binding ...
layout(set=0, binding=3) readonly buffer TileListFused { uvec4 tile_data[]; };
// tile_data[i] = (tileCoord.x, tileCoord.y, fusedShaderID, keyCount)

void main()
{
    uvec4 td = tile_data[gl_WorkGroupID.x];
    uvec2 tileCoord = td.xy;
    uint fusedID = td.z;
    // ...

    // ★ 按 fusedShaderID 选择预编译的融合着色路径
    //   每个 fusedID 对应一个包含 2-4 个 Surface Function 的内联分支
    //   详见 §8.5 融合材质着色器设计
}
```

#### Dispatch 3: 多材质 Tile（通用路径 — 最慢）

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

    // 每个像素独立解析自己的 MaterialKey
    uint surfaceType = vbuf.x >> 28;
    uint presetID    = (vbuf.x >> 24) & 0xF;
    uint miIndex     = (vbuf.x >> 8)  & 0xFFFF;
    float depth      = texelFetch(DepthBuffer, pixel, 0).r;
    vec3 worldPos    = ReconstructWorldPosition(pixel, depth);

    // ★ 此 tile 内存在分支发散，但经 Fused 路径分流后仅影响极少量 tile
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

// ===== Dispatch 1: Single-Material tiles (独占路径) =====
vkCmdBindPipeline(cmd, resolveSinglePipeline);
vkCmdDispatchIndirect(cmd, tileDispatchArgs, offsetof(single_dispatch));

// ===== Dispatch 2: Fused-Material tiles (融合路径) — §8.5 =====
// 可能有多个融合 pipeline（每个 fusedShaderID 一个），按 fusedID 分批 dispatch
// 方案 A：单一 fused pipeline + 内部 switch(fusedID)
vkCmdBindPipeline(cmd, resolveFusedPipeline);
vkCmdDispatchIndirect(cmd, tileDispatchArgs, offsetof(fused_dispatch));
// 方案 B：每个 fusedID 独立 pipeline → 需进一步按 fusedID 子分类（高级优化）

// ===== Dispatch 3: Multi-Material tiles (通用路径) =====
vkCmdBindPipeline(cmd, resolveMultiPipeline);
vkCmdDispatchIndirect(cmd, tileDispatchArgs, offsetof(multi_dispatch));

// 空 tile 不 dispatch，自然跳过 → 零开销
```

#### 性能分析

| 场景特征 | 独占路径 | 融合路径 | 通用路径 | 空 tile |
|----------|---------|---------|---------|---------|
| 室内简单场景 | ~75% | ~5% | ~5% | ~15% |
| 开放世界远景 | ~50% | ~5% | ~15% | ~30% |
| 角色近景特写 | ~35% | ~10% | ~30% | ~25% |
| **密集森林冠层** | ~15% | **~45%** | ~15% | ~25% |
| 密集植被城市 | ~20% | **~35%** | ~20% | ~25% |

> **融合路径的核心价值**：在高交错场景（森林、城镇街道）中，原本全部走通用路径的 tile
> 现在大部分被融合路径接管（~35-45%），通用路径占比从 ~40% 降至 ~15-20%，
> 整体 GPU 利用率显著提升。

#### 进阶扩展

| 扩展方向 | 方案 | 备注 |
|----------|------|------|
| **Per-SurfaceType 专用 kernel** | 为 Standard / Skin / Hair 各编译一个 resolve shader，Classification 阶段额外输出 per-type tile list → 每种类型独立 indirect dispatch | 适合 Special Surface 数量增多后进一步优化 |
| **Per-FusedID 专用 dispatch** | 每个 fusedShaderID 对应独立 pipeline，fused tile 再按 fusedID 子分类 → 零 switch 开销 | 融合组合数多时性能更优 |
| **Tile 粒度自适应** | 如果 16×16 粒度下 multi-tile 占比过高，可动态切换到 8×8 以降低发散 | 可通过 Classification 统计结果在 CPU 端决策 |
| **Debug 可视化** | TileSurfaceMask / TileMaterialCount 渲染为颜色热力图，fused tile 标绿、multi 标红 → 性能调优 | 加入 Debug Overlay Pass（§8.6）|
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

### 8.5 融合材质着色器（Fused Material Shader）

#### 问题陈述

在**高材质交错密度**场景中（典型：森林冠层、城镇街道），相邻像素频繁交替使用不同的
MaterialPreset（如树叶 vs 树皮、地面 vs 墙砖）。即使它们属于同一 SurfaceType（Standard），
在通用路径（Dispatch 3）中也会产生以下开销：

```
典型"森林冠层"tile（8×8 = 64 像素）：
  ████████
  ██▓▓▓▓██    ██ = 树叶 (Standard, PresetA)
  █▓▓▓▓▓▓█    ▓▓ = 树皮 (Standard, PresetB)
  ▓▓▓▓▓▓▓▓
  ▓▓████▓▓    两种材质高度交错
  █████▓▓█    → 若逐材质 mask pass：需 2 次全 tile 遍历
  ██████▓▓    → 若通用 switch：workgroup 内 2 分支，浪费 ~50% SIMD
  ████████

融合 shader 方案：1 次遍历，2 条路径内联，per-pixel 选择
  → SIMD 利用率 ~100%，只需 1 次 dispatch
```

> **核心思路**：把 2-4 种已知常见共现材质的 Surface Function **内联编译进同一个 Compute Shader**，
> 每个像素按自己的 MaterialKey 选择路径。因为路径数极少（2-4），GPU 编译器能高效优化
> （predication / SIMD lanes 复用），远优于通用 N-way switch。

#### 融合 Shader 编译

构建期（§9）Compositor 额外为每个注册的 FusedCombo 编译一个 resolve shader：

```cpp
// 构建期伪代码 — 融合 shader 生成
for (const auto& combo : FusedComboRegistry)
{
    std::string code = GenerateFusedResolveShader(combo);
    // 生成的 shader 包含:
    //   #include "surface/standard_surface.glsl"      // 内联 PresetA 路径
    //   #include "surface/standard_surface.glsl"      // 内联 PresetB 路径（不同 MI 参数）
    //   main() 中按 pixel MaterialKey → 选择路径 A/B
    SPVResult spv = CompileGLSL(code, combo.qualityTier);
    SPVCache.Store(combo.fusedShaderID, spv);
}
```

> **注意**：同一 SurfaceType 内不同 Preset 通常共享相同的 Surface Function（如都是 `standard_surface.glsl`），
> 区别仅在 MI 中的纹理/参数不同。这种情况下融合 shader 本质上是**同一 Surface Function + 不同 MI 的多路分发**，
> 编译后的 shader 体积几乎不增加。
>
> 当跨 SurfaceType 融合时（如 Standard + Skin），需要内联两个不同的 Surface Function，
> shader 体积和寄存器占用会增大——建议仅注册确实高频的跨类型组合，且控制在 2-3 种材质。

#### 融合 Resolve 着色器实现

```glsl
// vbuffer_resolve_fused.comp.glsl
// 编译期：根据 FusedCombo 注册表生成多个 specialization，或使用 switch(fusedID)
layout(local_size_x=TILE_SIZE, local_size_y=TILE_SIZE) in;

layout(set=0, binding=0) uniform usampler2D VBuffer;
layout(set=0, binding=1) uniform sampler2D  DepthBuffer;
layout(set=0, binding=2, rgba16f) writeonly uniform image2D LitColorOutput;
layout(set=0, binding=3) readonly buffer TileListFused { uvec4 tile_data[]; };

// --- 内联的 Surface Function（构建期 #include 注入）---
// 假设此 fusedShaderID = FUSED_FOREST_CANOPY (树叶 + 树皮)
// 两者都是 Standard SurfaceType，共享 EvalStandard()

// 定义融合组合内的 MaterialKey
#define FUSED_KEY_A  ((SURFACE_STANDARD << 4) | PRESET_TREE_LEAF)
#define FUSED_KEY_B  ((SURFACE_STANDARD << 4) | PRESET_TREE_BARK)

void main()
{
    uvec4 td = tile_data[gl_WorkGroupID.x];
    uvec2 tileCoord = td.xy;
    // uint fusedID = td.z;  // 若多个融合 combo 共用一个 shader 可用

    ivec2 pixel = ivec2(tileCoord * TILE_SIZE + gl_LocalInvocationID.xy);
    uvec2 vbuf = texelFetch(VBuffer, pixel, 0).xy;
    if (vbuf.x == 0u) { return; }

    uint surfaceType = vbuf.x >> 28;
    uint presetID    = (vbuf.x >> 24) & 0xFu;
    uint matKey      = (surfaceType << 4) | presetID;
    uint miIndex     = (vbuf.x >> 8) & 0xFFFF;
    float depth      = texelFetch(DepthBuffer, pixel, 0).r;
    vec3 worldPos    = ReconstructWorldPosition(pixel, depth);

    // ★ 仅 2 条路径，极低分支发散
    //   GPU 编译器可将两条路径优化为 predicated execution
    vec3 litColor;
    if (matKey == FUSED_KEY_A)
    {
        // 树叶：读取 MI_A → EvalSurface → EvalLighting
        MI_Standard mi = GetMI(miIndex);
        SurfaceInput si = ReconstructSurfaceInput(pixel, worldPos, vbuf);
        SurfaceOutput surf = EvalSurface(si, mi);
        litColor = EvalLighting(surf, worldPos, normalize(camera.pos - worldPos));
    }
    else // matKey == FUSED_KEY_B
    {
        // 树皮：读取 MI_B → EvalSurface → EvalLighting
        MI_Standard mi = GetMI(miIndex);
        SurfaceInput si = ReconstructSurfaceInput(pixel, worldPos, vbuf);
        SurfaceOutput surf = EvalSurface(si, mi);
        litColor = EvalLighting(surf, worldPos, normalize(camera.pos - worldPos));
    }

    imageStore(LitColorOutput, pixel, vec4(litColor, 1.0));
}
```

> **同一 SurfaceType 融合的特殊优化**：上例中树叶和树皮都是 Standard，
> `EvalSurface()` 和 `EvalLighting()` 函数签名完全相同——唯一区别是 MI 中的纹理索引/参数。
> 因此融合 shader 实际上**不需要 if/else 分支**——直接 `GetMI(miIndex)` 后调用同一条路径即可！
> 编译器会识别到两条路径完全相同并合并。
>
> 真正需要 if/else 分支的是**跨 SurfaceType 融合**（如 Standard + Skin），
> 此时两条路径调用不同的 `EvalSurface_Standard()` vs `EvalSurface_Skin()`。

#### 跨 SurfaceType 融合示例

```glsl
// 融合 shader: Standard + Skin（角色近景 tile 常见组合）
#define FUSED_KEY_A  ((SURFACE_STANDARD << 4) | PRESET_DEFAULT)
#define FUSED_KEY_B  ((SURFACE_SKIN     << 4) | PRESET_HUMAN_SKIN)

// 内联两个 Surface Function
#include "surface/standard_surface.glsl"    // EvalSurface_Standard()
#include "surface/skin_surface.glsl"        // EvalSurface_Skin()

void main()
{
    // ... 同上解包像素 ...

    vec3 litColor;
    if (matKey == FUSED_KEY_A)
    {
        SurfaceOutput surf = EvalSurface_Standard(si, mi);
        litColor = EvalLighting(surf, worldPos, V);
    }
    else
    {
        SurfaceOutput surf = EvalSurface_Skin(si, mi);
        litColor = EvalLightingSkin(surf, worldPos, V);  // Skin 专用光照（含 SSS）
    }
    // ...
}
```

#### 融合 Combo 注册与管理

```cpp
// ===== 融合材质组合注册 =====
struct FusedMaterialCombo
{
    uint8_t  materialKeys[4];   // 排序后的 MaterialKey（未使用的填 0xFF）
    uint8_t  keyCount;          // 实际包含的材质数（2-4）
    uint8_t  fusedShaderID;     // 编译后的 SPV 索引
    uint16_t _pad;
};

class FusedComboRegistry
{
    std::vector<FusedMaterialCombo> combos;   // 通常 4-16 条

public:
    // 手动注册（按 MaterialCategory 语义 — §5.1.1）
    void RegisterByCategory(MaterialCategory catA, MaterialCategory catB, uint8_t shaderID);
    void Register(std::initializer_list<uint8_t> keys, uint8_t shaderID);

    // 自动推荐：从上帧的 Tile Classification 统计中提取 top-N 高频 MaterialCategory 共现组合
    void AutoRecommend(const TileClassificationStats& stats, uint32_t topN = 8);

    // 上传 GPU 端 FusedComboLUT SSBO
    void UploadToGPU(VkBuffer fusedComboBuffer);
};
```

> **推荐流程**：
> 1. 开发期：打开 Debug Overlay（§8.6），观察 Multi-Material tile 分布热力图
> 2. 收集高频 MaterialCategory 共现组合（引擎可自动按 MaterialCategory 统计 top-N，§5.1.1）
> 3. 手动 `RegisterByCategory()` 或使用 `AutoRecommend()` 自动注册 → 触发融合 shader 编译
> 4. 下次加载场景时融合 shader 生效 → Multi tile 占比显著下降

#### 融合方案对比总结

| | 独占路径 (Single) | 融合路径 (Fused) | 通用路径 (Multi) |
|--|---|---|---|
| 材质数/tile | 1 | 2-4（已知组合） | 2+（未知/超限） |
| 分支发散 | 零 | 极低（2-4 路径，编译器优化） | 中-高（N-way switch） |
| Dispatch 次数 | 1 | 1 | 1 |
| SIMD 利用率 | 100% | ~90-100% | ~50-75% |
| Shader 编译 | 标准 resolve | 额外编译融合 SPV | 标准 resolve |
| 典型占比 | 50-75% tile | 10-45% tile（高交错场景） | 5-15% tile |
| 适用场景 | 大面积单一材质表面 | 森林冠层、城镇街道、角色边缘 | 极端复杂交界区 |

### 8.6 Tile Classification Debug 可视化

为辅助融合 Combo 注册和性能调优，提供 Debug Overlay 模式：

```
Debug Mode: Tile Classification Overlay
  绿色: 独占路径 tile (Single)
  蓝色: 融合路径 tile (Fused) — 附标注 fusedShaderID
  红色: 通用路径 tile (Multi) — 附标注材质数量
  灰色: 空 tile
  热力图模式: 按 tile 内 MaterialKey 数量 1→绿 2→黄 3→橙 4+→红
```

| 统计项 | 来源 | 用途 |
|--------|------|------|
| 各路径 tile 数量/占比 | TileDispatchArgs.single/fused/multi_count | 总体性能评估 |
| Multi tile 中 top-N 材质组合频率 | Classification 额外统计 SSBO | 自动推荐 Fused Combo |
| 每个 FusedCombo 命中的 tile 数 | TileList_Fused 中按 fusedID 计数 | 评估已注册 Combo 的效果 |

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
> Standard Surface (Opaque) 编译 4(PassType) × 6(tier) × 3(shadow) × flags ≈ **144** 个变体。\
> Standard Surface (Masked) 编译 4(PassType) × 6(tier) × 3(shadow) × flags ≈ **144** 个变体。\
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

### 12.1 Instance ID 分发与顶点布局的关系

表中 "附加 per-instance 数据" 列的 `TransformID` 和 `MaterialInstanceID` 的传递方式
按 **PlatformBackend**（§2.9）分流，与顶点数据获取路径保持一致（详见 §6.4.3）：

**SSBO 路径（PC / Apple / PowerVR）**：
- ID 存储在专用 ID SSBO 中，VS 内通过 `gl_InstanceIndex` 索引读取
- Pipeline 无需 instance-rate VBO binding（VertexInput 可完全为空）
- 与 SSBO 顶点获取 + Indirect Draw / Meshlet 管线天然兼容

**VBO 路径（Android Mid/Low — 非 Apple/PowerVR 移动 GPU）**：
- **Binding 0** — 几何顶点（vertex-rate），对应上表的 Layout A~E
- **Binding 1** — `TransformID`（instance-rate, R16UI），由 `TransformAssignmentBuffer` 管理
- **Binding 2** — `MaterialInstanceID`（instance-rate, R16UI），由 `MaterialInstanceAssignmentBuffer` 管理
- 在 Mali / Adreno 等移动 GPU 上，硬件 vertex fetch 单元对 vertex attribute 有专用优化，效率高于 SSBO

无论哪条路径，VS 中统一通过 `GetTransformID()` / `GetMaterialInstanceID()` 获取 ID，
再通过 `GetLocalToWorld()` / `GetMI()` 从 SSBO 中读取完整的变换矩阵和材质参数。

整个机制仅消耗 **4 字节/实例**（VBO 路径）或 **0 字节额外顶点带宽**（SSBO 路径），
即可分发完整的变换矩阵 (64B) + 材质参数 (56B+)。

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
27. 移植 Sky（可保留独立 main() 或改为 Surface Function）
28. 实现 Terrain Surface Function（`surface/terrain_surface.glsl`：多层 TerrainLayerSSBO 混合 + SplatMap 权重采样，§5.5）
29. 实现 Terrain GPU 资源管理：TerrainGlobalUBO + TerrainLayerSSBO + Texture2DArray(Albedo/Normal/MR) 上传与绑定
30. 实现 SplatMap 编码工具：地形编辑器 → RGBA8 Texture2DArray，ceil(layer_count/4) 层
31. 实现 Terrain VBuffer Resolve 专用 Compute Kernel（per-pixel 多层 Dither 混合 → MaterialInstance ID，§5.5.3）
32. 实现 Terrain Forward Path（`EvalSurface()` 循环采样 TerrainLayerSSBO + weight threshold skip，§5.5.4）

### Phase 6：VBuffer 路径（SSBO 平台专属）
33. 实现 VBuffer ID Pass（`compositor/main_vbuffer_id.frag.glsl`，SSBO 路径）
34. 实现 Tile Material Classification Compute Shader
    - TileSurfaceMask (R32UI) + TileMaterialKeys (SSBO) + TileMaterialCount (R8UI)
    - 四类分流: empty / single / fused / multi
    - FusedComboLUT 匹配 + Indirect dispatch args 填充
35. 实现 VBuffer Resolve — 单一材质 tile 独占路径（内部 `#include` Surface Function）
36. 实现 VBuffer Resolve — 融合材质 tile 融合路径（§8.5，刍编译 2-4 材质内联 shader）
37. 实现 VBuffer Resolve — 多材质 tile 通用路径
38. 实现 LitColor RT 管理和后期处理衔接
39. 实现 Forward / VBuffer 路径自动切换（VBO 平台强制 Forward Only）
40. 实现 FusedComboRegistry（手动注册 + AutoRecommend + GPU 上传）
41. (可选) Tile-based SSAO/ShadowMask 屏蔽优化
42. 实现 Tile Classification Debug 可视化热力图（§8.6）

### Phase 7：Meshlet 几何管线（SSBO 平台 High+）
43. 集成 meshoptimizer 库（meshlet 构建 + mesh simplification）
44. 实现离线 Meshlet 预处理工具（Mesh → MeshletBuffer + LOD DAG + Flags）
45. 定义 MeshletGPU 结构体 + 引擎二进制格式 (.ulm)
46. 实现 Instance Cull Compute Shader（视锥 + 粗 HZB）
47. 实现 Meshlet LOD Select + Cull Compute Shader（DAG 遍历 + Frustum + Cone + HZB）
48. 集成 vkCmdDrawIndexedIndirectCount (meshlet 粒度 Indirect Draw，从全局 SSBO 读取)
49. 实现 Two-Phase Meshlet Occlusion Culling
50. 实现 Terrain-Contact Dither 混合（Compositor `TERRAIN_CONTACT_DITHER` PassType）
51. 实现 MESHLET_FLAG_ALPHA_TEST / WIND_ANIM 变体支持
52. (可选) 实现 Mesh Shader 加速路径（VK_EXT_mesh_shader: Task + Mesh Shader, PC only）
53. 实现 Lowest/Low/Medium SSBO 回退路径（离散 LOD mesh 从 DAG 导出，仍走 SSBO 顶点获取）
54. 实现 VBO 平台回退路径（离散 LOD mesh + CPU Frustum Cull + vkCmdDrawIndexed）

### Phase 7.5：Reversed-Z 管线集成 ★
55. 实现 `MakeInfiniteReversedZProj()` 投影矩阵（§2.10.2）
56. 切换全管线 DepthStencilState: depthCompareOp = GREATER, clearDepth = 0.0
57. 适配 HZB Downsample（min = 最远）、SSR Hi-Z Trace（compare > ）、SSAO depth linearization
58. 实现 `depth_utils.glsl`（`LinearizeDepth()` = near/d, `ReconstructWorldPos()`, §2.10.4）
59. 运行时 D32_SFLOAT 格式检测 + D24 回退（§2.10.3）
60. 全 Pass 深度约定验证：Z-Prepass / Shadow / VBuffer / Forward / Sky = 0.0 最远

### Phase 8：渲染管线扩展 — 阴影 & 后处理 & 光照
61. 实现双层 ShadowMap 架构: Near Dynamic Cascade (每帧全量) + Far Cached Cascade (环形滚动, §3.6.3)
62. 实现 Toroidal Scrolling 逻辑: tile dirty mask + 增量渲染 + fract() UV 采样
63. 实现 ShadowMask Compose Pass: Near+Far 距离混合 + Capsule Shadow (G) + Contact Shadow (B) → RGBA8
64. 实现 Capsule Shadow: CapsuleShadowData SSBO + per-pixel 解析遮挡计算 (§3.6.4)
65. 实现 Blob Shadow 回退: 贴地 quad + 衰减纹理 (低端动态物体用)
66. 实现 Contact Shadow: 屏幕空间 ray-march, High+ (§3.6.5)
67. 实现 ShadowConfig 可调参数体系 (§3.6.7) + DeviceQualityProfile 默认值映射
68. 实现 HZB 降采样 Compute Shader（Depth RT → HZB Pyramid，SSBO 平台 High+）
69. 实现 Clustered Shading（Cluster 预计算 + Light Assignment Compute + **Compositor `EvalLighting()` 集成**，SSBO High+）
70. 实现 Auto Exposure（Luminance Histogram Compute + Average + Temporal Smooth）
71. 实现 SSR（Hi-Z Ray March Compute, PC/Apple High+ only）
72. 实现 Fog 内联计算（FogParams UBO + `ApplyFog()` 集成到 **Compositor 模板** / VBuffer Resolve）
73. 实现 Color Grading / 3D LUT（Compute Pass, 在 ToneMap 之后）
74. 实现 CAS / Sharpening（Compute Pass）
75. (可选) 实现 DOF Compute Shader（CoC 计算 + 散景模糊，PC/Apple High+）
76. (可选) 实现 Per-Pixel Motion Blur Compute Shader（PC Ultra only）
77. 实现 Decal Pass（Screen-Space Decal: OBB mesh + Depth 反算 + 投影采样，SSBO High+）
78. 实现 Outline / Selection Highlight（Stencil + Dilate 或 JFA）

### Phase 9：Material LOD + Special Surface ★★★
79. 实现 `CalcObjectLODTier()` 函数：屏幕空间面积估算 + 阈值表 + `importanceBias` 偏移（§3.5.3）
80. 实现 `ResolveSPVFallback()` 函数：根据 `MaterialPresetDef.fallback_surface_type` + `unique_feature_min_tier` 路由 SPV（§3.5.6）
81. 渲染排序支持 `EffectiveTier` 分组：`(SurfaceType, EffectiveTier, PassType)` 排序键，减少 Pipeline 切换
82. 扩展 `SurfaceOutput` 支持 `SurfaceOutputExt`（SSS / Anisotropy / Caustic 等 Special Surface 专属字段）
83. 实现 Skin Surface Function（`surface/skin_surface.glsl`，Ultra: 全 SSS + Detail Normal + 曲率 AO，High: 简化 SSS，Medium/Low: fallback Standard）
84. 实现 Eye Surface Function（`surface/eye_surface.glsl`，Ultra: Parallax Refraction + 焦散 + 角膜 SSS，High: 单层 Parallax + CubeMap，Medium: 平面纹理 PBR，Low: Albedo + Phong）
85. 实现 Hair Surface Function（Ultra: Marschner 双高光，High: Kajiya-Kay，Medium: 单高光 PBR，Low: BlinnPhong）
86. 实现 Cloth Surface Function（Sheen + Charlie Model，Medium: 简化 wrap lighting，Low: Standard）
87. 实现 ClearCoat Surface Function（High+: 双层 BRDF，Med: 单层近似，Low: 高 specular BlinnPhong）
88. 实现 Foliage Surface Function（High+: Thin Translucency + Wind，Med: Wrap + 简化 Wind，Low: 静态 AlphaTest）
89. 实现 `EvalLighting_Skin()` / `EvalLighting_Eye()` 等 Compositor lighting 模块（配合 SurfaceOutputExt 处理 SSS / Anisotropy）
90. 验证 SPV fallback 等价性：Skin@Medium == Standard@Medium SPV 输出完全一致
91. 实现 `ObjectImportance` 游戏接口：对话镜头 → MainNPC(+1), 过场特写 → Hero(+2), 群演 → BackgroundNPC(-1)

### Phase 10：Android 适配与测试 ★
92. Android VBO 路径端到端集成测试（Lowest/Low/Medium 材质 × 传统 DrawCall）
93. Android High SSBO 路径验证（Adreno 7xx / Mali-G7xx 真机测试）
94. Android 动态分辨率实现（0.5× ~ 1.0× 根据 GPU 负载调节）
95. Android GPU 能力检测阈值调优（SSBO vs VBO 分界线校准）
96. Android 特性裁剪验证（确认 §2.9.4 中被砍特性的 shader 变体不被加载）
97. Android Cached SM + Capsule Shadow 联调测试
98. Android Material LOD 阈值调优（§3.5.3 各平台阈值表验证）

### Phase 11：清理
99. 删除旧的 ShaderComposition / Logic / Bridge 代码
100. 删除传统 GBuffer 相关代码和枚举
101. 更新 Pipeline 创建逻辑使用固定 Layout（SSBO: 空 VertexInput / VBO: 标准 VertexInput）
102. 更新编辑器 UI（Material Instance 编辑面板 — 含 BlendMode 选择 + ObjectImportance 预览）

---

## 14. 现有引擎对比分析与详细推进计划

本节基于 ULRE 引擎 `inc/`、`src/`、`example/`、`ShaderLibrary/` 的**完整代码审查**，
逐项对比设计文档的目标架构与现有实现的差距，给出每一步的**具体重构/新增方案**。

---

### 14.1 现有引擎能力清单

#### 14.1.1 已具备的基础设施（可直接复用）

| 能力 | 现有实现 | 对应代码 | 可复用程度 |
|------|---------|---------|-----------|
| Vulkan 设备/队列/内存管理 | 完整 | `src/Vulkan/VKInstance,Device,Memory` | ★★★★★ 直接使用 |
| 渲染命令录制 | 完整 | `VKCommandBuffer/Render` | ★★★★★ |
| Swapchain/RenderTarget | 完整（含离屏+多帧） | `VKRenderTarget*,VKSwapchain` | ★★★★★ |
| Pipeline State 管理 | 完整（含序列化/Hash/Cache） | `VKPipelineData,PipelineHash,PipelineCache` | ★★★★★ |
| Compute Pipeline | 基础类存在 | `VKComputePipeline` | ★★★★☆ 需扩展 |
| Descriptor Set 管理 | 完整 | `VKDescriptorSet,BindingManage` | ★★★★☆ 需调整 Set 布局 |
| Shader Module 编译 | glslang → SPV 完整 | `GLSLCompiler,ShaderModule` | ★★★★★ |
| Buffer 体系 | 完整(Staged/Ring/Indirect/ReBAR) | `VKBuffer*,IndirectCommandBuffer` | ★★★★★ |
| Vertex Input 系统 | VBO 完整, SSBO 定义完成 | `VKVertexInput*,VertexDataManager` | ★★★★☆ SSBO 路径需激活 |
| ECS 架构 | 完整(Entity/Component/System/World) | `inc/hgl/ecs/` | ★★★★★ |
| MaterialInstance 数组模式 | 完整(UBO/SSBO 数组 + MI_ID) | `VKMaterialInstance,ActiveMemoryBlockManager` | ★★★★★ 核心可复用 |
| Instance-Rate ID 分发 (L2W+MI) | **完整** — R16UI instance-rate VAB + SSBO 数据池 | `TransformAssignmentBuffer.h`, `MaterialInstanceAssignmentBuffer.h` | ★★★★★ **直接复用** §6.4 |
| Texture2DArray + MI texture_id | **完整** — 纹理打包为 Array, MI 中存层索引 | `PBRColor3D` 已实现 `sampler2DArray` + `mi.texture_id` | ★★★★★ **直接复用** §6.4.4 |
| Ring Buffer 多帧复用 | **完整** — Static/Movable 双模式 | `RingBufferWrapper`, `TransformAssignmentBuffer` | ★★★★★ 直接复用 |
| MI 去重 (MaterialInstanceSet) | **完整** — UnorderedMap 去重 | `MaterialInstanceAssignmentBuffer.h::MaterialInstanceSet` | ★★★★★ 直接复用 |
| Indirect Draw | Buffer 类完整 | `IndirectDraw(Indexed)Buffer` | ★★★★☆ GPU 填充未实现 |
| Texture 加载 | 完整 | `VKTexture,TextureLoader` | ★★★★★ |
| 骨骼动画 Joint 矩阵 | GLSL 已有 | `ShaderLibrary/GetJointMatrix.glsl` | ★★★☆☆ 需整合 |

#### 14.1.2 已有材质（需重构为 Surface Function）

| 旧材质 | 新设计对应 | 重构方案 |
|--------|-----------|---------|
| `PureColor2D` (M_PureColor2D) | Unlit/PureColor2D (preset 0) | 保留独立 main()，调整 Set Layout |
| `PureTexture2D` (M_PureTexture2D) | Unlit/Texture2D (preset 1) | 同上 |
| `RectTexture2D` | 合并入 Texture2D | 删除，作为 Texture2D 参数变体 |
| `RectTexture2DArray` | 合并入 Texture2D | 删除 |
| `Text2D` (M_Text2D) | Unlit/Text2D (preset 2) | 保留独立 main()，调整 Set Layout |
| `PureColor3D` (M_PureColor3D) | Unlit/PureColor3D (preset 3) | 保留，调整 Set Layout |
| `VertexColor3D` (M_VertexColor3D) | Unlit/VertexColor3D (preset 4) | 保留 |
| `VertexPattleColor3D` | Unlit/PaletteColor3D (preset 5) | 保留 |
| `Gizmo3D` (M_Gizmo3D) | Unlit/Gizmo3D (preset 6) | 保留 |
| `TextureBlinnPhong` | **Standard Surface** (preset 20) | ★ **核心重构** — 删除独立 main()，改为 `EvalSurface()` |
| `BasicLit` | **Standard Surface** | ★ 合并入 StandardTexture |
| `PBRColor3D` | **Standard Surface** 变体 | ★ 合并 |
| `TerrainGrid` | **Terrain Surface** (preset 31) | ★★ **重写** — 新架构 §5.5 |
| `SkyMinimal` | Sky (preset 32) | 保留独立 main()，调整 |
| `Billboard2D` | Billboard (preset 33) | 保留 |

#### 14.1.3 已有但需大幅重构的系统

| 系统 | 现状 | 差距 |
|------|------|------|
| **ShaderPermutationKey** | 4 轴: ambient(5)×light(6)×specular(2)×shadow(3) | 需改为设计文档的 16-bit packed key: surface(4)+quality(3)+shadow(2)+flags(3)+platform(2)+reserved(2) |
| **DescriptorSet 布局** | 7 个 Set (RenderTarget/Camera/World/Global/PerFrame/PerMaterial/Unknown) | 需简化为 4 个 Set (PerScene/PerView/PerDraw/PerMaterial) §6.3 |
| **Shader 组合系统** | ComposedMaterialDef + ShaderLogic + Bridge 三层 | **全部删除** — 改为 Surface Function + Compositor Template §9 |
| **GLSL 模组系统** | ShaderLibrary/ 的 modules + templates + recipes (JSON+inja) | 保留 GLSL 模组内容，删除 inja 模板引擎——改为 Compositor 直接 `#include` |
| **RenderFlowDef** | 25 个 RenderStage 枚举 + 10 个 Flow Preset | 已过度设计——简化为 Forward + VBuffer 两条路径 |
| **GBuffer 系统** | 完整枚举 + Format 规格 + Quality Preset | **全部删除** — 设计文档明确不做 GBuffer 延迟 |
| **Binding Contract** | Contract/Validator/MirrorDiff 等验证层 | 简化——固定 4-Set Layout 无需运行时验证 |

#### 14.1.4 完全缺失的系统（需从零实现）

| 系统 | 设计文档章节 | 复杂度 |
|------|------------|--------|
| **Compositor 模板引擎** (自动生成 main()) | §9 | ★★★★★ |
| **Surface Function 架构** | §9.2 | ★★★★☆ |
| **ShadowMap 渲染** (Near Dynamic + Far Cached) | §3.6 | ★★★★★ |
| **HZB 生成 + 遮挡剔除** | §2.6, §2.7 | ★★★★☆ |
| **Meshlet 管线** (GPU Cull + LOD Select + Indirect) | §2.8 | ★★★★★ |
| **VBuffer ID Pass + Tile Resolve** | §8 | ★★★★★ |
| **Material LOD 系统** | §3.5 | ★★★☆☆ |
| **Reversed-Z + Infinite Far** | §2.10 | ★★☆☆☆ |
| **SPV 离线缓存** (构建期全编译) | §12 | ★★★☆☆ |
| **后处理链** (Bloom/TAA/ToneMap/FXAA...) | 未详细设计 | ★★★★☆ |
| **Clustered Shading** | 未详细设计 | ★★★★☆ |
| **Terrain 256 层系统** | §5.5 | ★★★★☆ |
| **Special Surface** (Skin/Eye/Hair/Cloth...) | §5.3 | ★★★★☆ |

---

### 14.2 详细推进计划

以下按**依赖关系**编排，每个 Sprint 内的任务可并行。预估基于单人全职开发。

---

#### Sprint 0：准备工作 — 代码清理与基础对齐

**目标：** 清除旧系统中与新设计冲突的代码，建立新目录结构。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 0.1 | 清理 GBuffer 系统 | 删除 `GBufferChannel`, `GBufferFormatLevel`, `GBufferQualityPreset`, `GBufferConfiguration` 及所有引用。保留 `PipelineRenderPath::Forward` 和 `VBufferDeferred`，删除 `GBufferDeferred`, `MobileSubpassGBufferDeferred`. | `RenderFlowDef.h` | 精简后的 RenderFlowDef |
| 0.2 | 简化 RenderStage | 保留: `EarlyZ_Solid/Masked`, `ShadowMap_*`, `VisibilityBuffer_Fill`, `Forward_*`, `HZB_*`, `PostProcess_*`, `Debug_Visualization`。删除: `GBuffer_*`, `Deferred_Lighting*`. | `RenderFlowDef.h` | |
| 0.3 | 精简 RenderFlow Preset | 保留: `Forward_Basic`, `Forward_WithEarlyZ`, `ForwardPlus_SingleHZB`, `VisibilityBuffer_Deferred`, `Mobile_Forward`。删除其余。 | `RenderFlowDef.h` | |
| 0.4 | 建立新目录结构 | 创建 `ShaderLibrary/surface/`, `ShaderLibrary/compositor/`, `ShaderLibrary/common/` (如不存在), `ShaderLibrary/pass/` | 文件系统 | Surface Function + Compositor 存放位置 |
| 0.5 | 冻结旧材质测试基线 | 确保所有现有 example 编译运行正常，截图保存作为回归基线。编写自动化 smoke test。 | `example/*/` | 回归测试基线 |

---

#### Sprint 1：核心类型重定义 — SurfaceType / QualityTier / 新 PermutationKey

**目标：** 建立新的材质类型体系，替换旧的 `MaterialPreset` + `LightModel` 枚举。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 1.1 | 定义 `SurfaceType` 枚举 | 11 个表面类型: `PureColor2D=0..Terrain=10`，替代旧 `MaterialPreset` | 新建 `inc/hgl/mtl/SurfaceType.h` | SurfaceType 枚举 |
| 1.2 | 定义 `QualityTier` 枚举 | 6 级: `Lowest=0..Cinematic=5` | 新建 `inc/hgl/mtl/QualityTier.h` | QualityTier 枚举 |
| 1.3 | 定义 `BlendMode` 枚举 | `Opaque, Masked, AlphaTest_Dither, Transparent, Additive`，替代旧 `PipelineCoverageMode` + `ShaderOutputMode` | 新建 `inc/hgl/mtl/BlendMode.h` | BlendMode 枚举 |
| 1.4 | 定义 `PassType` 枚举 | `Forward_Opaque, Forward_Masked, ..., VBuffer_ID, Shadow_Opaque, Shadow_Masked` 共 10 个 | 新建 `inc/hgl/mtl/PassType.h` | PassType 枚举 |
| 1.5 | 定义 `PlatformBackend` 枚举 | `PC_SSBO, Apple_SSBO, Android_VBO` | 新建 `inc/hgl/mtl/PlatformBackend.h` | PlatformBackend 枚举 |
| 1.6 | 重写 `ShaderPermutationKey` | 16-bit packed: surface(4)+quality(3)+shadow(2)+flags(3)+platform(2)+reserved(2)。实现 `AppendGLSLDefines()` 生成 `#define SURFACE_TYPE N`, `#define QUALITY_TIER N` 等 | 重写 `inc/hgl/mtl/ShaderPermutationKey.h`, `src/ShaderGen/ShaderPermutationKey.cpp` | 新 PermutationKey |
| 1.7 | 定义 `MaterialPresetDef` | 结构体: `{preset_id, surface_type, material_category, name, mi_struct, mi_size, texture_slots[], fallback_surface_type, unique_feature_min_tier}` | 新建 `inc/hgl/mtl/MaterialPresetDef.h` | 预设定义结构体（含 MaterialCategory §5.1.1） |
| 1.8 | 实现 `DeviceQualityProfile` | GPU 检测逻辑: vendor/device → `{qualityTier, platformBackend, geometryFetchMode, featureMask}`。**复用**现有 `VKPhysicalDevice` 的 properties/features 查询 | 新建 `inc/hgl/mtl/DeviceQualityProfile.h`, 实现 `.cpp` | 自动档位检测 |
| 1.9 | 迁移 `PipelineInputMode` | 保留 `LegacyVABVBO`, `SSBOVertexInput`，删除 Hybrid/Auto；改为 `PlatformBackend` 驱动选择 | 修改 `RenderFlowDef.h` | |

**验证：** 新枚举可编译，`DeviceQualityProfile::Detect()` 在现有设备上正确返回档位。

---

#### Sprint 2：Descriptor Set Layout 重构 — 4-Set 固定布局

**目标：** 从 7 个语义 Set 收敛到 4 个固定 Set，统一全引擎。

> **⚠️ 保留约束**：现有 Instance-Rate ID 分发机制（TransformAssignmentBuffer / MaterialInstanceAssignmentBuffer）
> 及 Texture2DArray 纹理池**必须原样保留**（§6.4）。重构仅影响 Descriptor Set 的编号和布局，
> 不改变 SSBO 数据池和 instance-rate VAB 的工作方式。
> Set 1 binding 0 (L2W SSBO) 和 Set 2 binding 0 (MI SSBO) 的填充方仍由上述两个 AssignmentBuffer 负责。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 2.1 | 定义新 4-Set 布局 | Set 0: PerScene (SceneUBO, ShadowCascadeUBO, LightSSBO, ShadowMask, EnvCubeMap...)。Set 1: PerView (CameraUBO, FogUBO)。Set 2: PerMaterial (MI SSBO/UBO, textures, per-surface-type 布局)。Set 3: PerDraw (LocalToWorld SSBO, VertexData SSBO, IndexData SSBO, MeshletBuffer SSBO...) | 新建 `inc/hgl/mtl/DescriptorSetLayout.h` | 4-Set 完整定义 |
| 2.2 | 重构 `DescriptorSetType` | 从 7 个(`Unknow/RenderTarget/Camera/World/Global/PerFrame/PerMaterial`)合并为 4 个(`PerScene/PerView/PerMaterial/PerDraw`) | 修改 `DescriptorSetTypeDef.h`，全局搜索替换引用 | |
| 2.3 | 重写 `ResourceLayoutGenerator` | 根据新 4-Set 定义生成 `layout(set=N, binding=M)`。**删除**旧的语义名查找逻辑——改为固定 binding number 查表 | 重写 `src/ShaderGen/ResourceLayoutGenerator.cpp` | |
| 2.4 | 更新 `PipelineLayoutData` | 从 7 个 `VkDescriptorSetLayout` 改为 4 个。更新 `VkPipelineLayout` 创建逻辑 | 修改 `VKPipelineLayoutData.h/.cpp` | |
| 2.5 | 迁移旧材质的 Descriptor 引用 | 所有现有 `FixedDescriptorEntry[]` 数组 → 映射到新 Set/Binding 编号 | 修改所有 `M_*.cpp` 工厂文件 | |
| 2.6 | 定义 Terrain 专用 Set 2 | 当 `SurfaceType==Terrain` 时, Set 2 使用 §5.5 定义的 Terrain 专用 binding 0-7 | `DescriptorSetLayout.h` | dual-layout Set 2 |

**验证：** 所有现有 example 用新 4-Set Layout 编译运行，渲染结果与 Sprint 0 基线一致。

---

#### Sprint 3：Surface Function 架构 + Compositor 模板引擎

**目标：** 这是设计文档的**核心创新** — Surface Function 只写业务逻辑，Compositor 自动生成 main()。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 3.1 | 定义 `SurfaceInput` / `SurfaceOutput` | GLSL 结构体: SurfaceInput{worldPos, worldNormal, uv0, uv1, vertexColor, viewDir, screenPos}; SurfaceOutput{baseColor, normal, metallic, roughness, ao, emissive, alpha, ...} | 新建 `ShaderLibrary/common/surface_interface.glsl` | 公共接口 |
| 3.2 | 定义 `SurfaceOutputExt` | Special Surface 扩展: {subsurfaceColor, subsurfacePower, thickness, sheenColor, sheenRoughness, clearCoat, clearCoatRoughness, clearCoatNormal, anisotropy, anisotropyDirection} | `surface_interface.glsl` 追加 | |
| 3.3 | 编写 Standard Surface Function | `EvalSurface(SurfaceInput) → SurfaceOutput` + `EvalAlpha(SurfaceInput) → float`。**合并** TextureBlinnPhong + BasicLit + PBRColor3D 的采样逻辑。使用 `#if QUALITY_TIER >= N` 分支控制纹理采样数量 | 新建 `ShaderLibrary/surface/standard_surface.glsl` | 核心 Surface Function |
| 3.4 | 实现 Compositor VS 模板 | 前向不透明: `main_forward_opaque.vert.glsl` — MVP 变换 + 法线传递 + UV 传递。使用 `#if PLATFORM_SSBO` 分支选择顶点获取方式 | 新建 `ShaderLibrary/compositor/main_forward_opaque.vert.glsl` | |
| 3.5 | 实现 Compositor FS 模板 | 前向不透明: `main_forward_opaque.frag.glsl` — `#include` Surface Function → 调用 `EvalSurface()` → 调用 `EvalLighting()` → 输出 Color。**删除**旧的手写 main() | 新建 `ShaderLibrary/compositor/main_forward_opaque.frag.glsl` | |
| 3.6 | 实现 `EvalLighting()` 统一光照入口 | 合并现有 `ShaderLibrary/lighting/*.glsl` + `specular/*.glsl` + `ambient/*.glsl`。`#if QUALITY_TIER` 分支: 0=SimpleLambert, 1=HalfLambert+Phong, 2=BlinnPhong+IBL, 3-4=CookTorrance+IBL, 5=Full PBR+SSR | 新建 `ShaderLibrary/common/lighting.glsl`，**复用**现有 `pbr_functions.glsl`, `ggx.glsl` 等 | |
| 3.7 | 实现更多 Compositor 模板 | `main_forward_masked.frag.glsl` (+ discard), `main_forward_transparent.frag.glsl` (+ alpha blend), `main_forward_dither.frag.glsl`, `main_forward_a2c.frag.glsl`, `main_shadow_opaque.vert.glsl`, `main_shadow_masked.frag.glsl` | `ShaderLibrary/compositor/` | |
| 3.8 | 实现 `CompositorAssembler` | C++ 类: 输入 `(SurfaceType, BlendMode, PassType, QualityTier, PlatformBackend)` → 查表选择 VS/FS Compositor 模板 → 注入 `#define` + `#include "surface/xxx_surface.glsl"` → 生成完整 GLSL | 新建 `src/ShaderGen/CompositorAssembler.cpp/.h` | Compositor 核心 |
| 3.9 | 实现 `PresetShaderCompiler` | 遍历所有 `MaterialPresetDef` × 有效 `ShaderPermutationKey` 组合 → `CompositorAssembler` → glslang → SPV | 新建 `src/ShaderGen/PresetShaderCompiler.cpp/.h` | 离线编译器 |
| 3.10 | 实现 `SPVCache` | 键: `{preset_id, tier, shadow, flags, platform, pass_type}` → 值: `SPVData*`。**构建期**全量编译写入二进制文件，**运行时**只读查表 | 新建 `src/ShaderGen/SPVCache.cpp/.h` | SPV 缓存 |

**验证：** Standard Surface 通过 Compositor 生成的 SPV 渲染结果 == 旧 TextureBlinnPhong/BasicLit/PBRColor3D 的视觉等价。

---

#### Sprint 4：Reversed-Z + 深度管线统一

**目标：** 全管线切换 Reversed-Z + D32_SFLOAT + Infinite Far Plane。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 4.1 | 实现 Reversed-Z 投影矩阵 | `MakeInfiniteReversedZProj(fov, aspect, near)` — §2.10.2。**替换**现有 Camera 的投影矩阵计算 | 修改 `Camera.cpp` 或等价位置 | |
| 4.2 | 切换 DepthStencilState | 全局默认 `depthCompareOp = VK_COMPARE_OP_GREATER`, `depthClearValue = 0.0` | 修改 `VKPipelineData.cpp` 默认值, `VKRenderPass` clear 值 | |
| 4.3 | D32_SFLOAT 格式检测 | 运行时 `vkGetPhysicalDeviceFormatProperties(D32_SFLOAT)` → 不支持则降级 D24 | 修改 `DeviceQualityProfile` 或 `VKPhysicalDevice` | |
| 4.4 | 实现 `depth_utils.glsl` | `LinearizeDepth(d)=near/d`, `ReconstructWorldPos(ndc,depth)` | 新建 `ShaderLibrary/common/depth_utils.glsl` | |
| 4.5 | 天空 Pass 深度 | Sky material 输出 depth = 0.0（Reversed-Z 最远值） | 修改 SkyMinimal FS | |
| 4.6 | 全 Pass 验证 | Z-Prepass/Forward/Shadow 的 depthCompare 一致性检查 | 各 Compositor 模板 | |

**验证：** 远景物体不再 Z-fighting，近景精度不变，天空正确渲染在最远处。

---

#### Sprint 5：平台几何后端 — SSBO 顶点获取激活

**目标：** 激活已定义但未实现的 SSBO 顶点获取路径。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 5.1 | 实现全局 VertexDataBuffer SSBO | 单个大 SSBO 存储所有 mesh 的顶点数据，通过 offset 定位。**利用**现有 `VKStagedBuffer` + `VKRingBufferWrapper` | 新建 `VertexDataBufferManager.cpp/.h` | |
| 5.2 | 实现全局 IndexDataBuffer SSBO | 同上，索引数据 | 同上 | |
| 5.3 | 实现 `vertex_fetch_ssbo.glsl` | `#if PLATFORM_SSBO` 分支: `GetPosition() = VertexDataBuffer[vertexOffset + gl_VertexIndex].pos` | 新建 `ShaderLibrary/common/vertex_fetch_ssbo.glsl` | |
| 5.4 | 实现 `vertex_fetch_vbo.glsl` | `#if PLATFORM_VBO` 分支: 标准 `layout(location=N) in vec3 Position` | 新建 `ShaderLibrary/common/vertex_fetch_vbo.glsl` | |
| 5.5 | Compositor VS 模板集成 | `main_forward_opaque.vert.glsl` 使用 `#include "vertex_fetch_ssbo.glsl"` 或 `"vertex_fetch_vbo.glsl"` | 修改 Sprint 3 产出 | |
| 5.6 | Pipeline 创建分支 | SSBO 路径: `VkPipelineVertexInputStateCreateInfo` 为空(无 VAB) + Set 3 绑定 VertexData SSBO。VBO 路径: 标准 VIL | 修改 Pipeline 创建逻辑 | |
| 5.7 | Mesh 上传管理 | Mesh 加载时 → SSBO 分配 offset → 记录 `{vertexOffset, indexOffset, vertexCount, indexCount}` | | |

**验证：** PC (SSBO) 和 Android (VBO) 路径渲染结果一致。

---

#### Sprint 6：Forward 渲染完善 — Unlit + Standard 全通

**目标：** 所有 Unlit 和 Standard Surface 材质通过新 Compositor 系统渲染。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 6.1 | 迁移 Unlit 2D 材质 | PureColor2D / Texture2D / Text2D 适配新 Set Layout。这些保留独立 main() (不经过 Compositor) | 修改现有 M_*.cpp | |
| 6.2 | 迁移 Unlit 3D 材质 | PureColor3D / VertexColor3D / PaletteColor3D / Gizmo3D / Emissive3D / Billboard 适配新 Set Layout | 修改现有 M_*.cpp | |
| 6.3 | Standard Surface 全 Pass 验证 | 验证 `standard_surface.glsl` × 6 个 QualityTier × 5 个 BlendMode × Forward Pass = 所有变体正确渲染 | | |
| 6.4 | 实现 StandardColor Surface Function | 纯参数材质(无纹理)，`EvalSurface()` 直接从 MI 读取 base_color/metallic/roughness | 新建 `ShaderLibrary/surface/standard_color_surface.glsl` | |
| 6.5 | 实现 StandardVertexColor Surface | 顶点色 + 光照 | 新建 `ShaderLibrary/surface/standard_vertexcolor_surface.glsl` | |
| 6.6 | 删除旧 Shader 组合层 | 删除 `ComposedMaterialDef`, `MaterialLogicDef`, `ShaderLogic.h`, `ShaderCompositionBridge.cpp`, `ShaderComposition.h`。所有材质要么独立 main() (Unlit)，要么 Surface Function + Compositor | 删除文件 | |
| 6.7 | 删除旧 ShaderLibrary 模板引擎 | 删除 `ShaderLibrary/templates/*.tmpl`, `ShaderLibrary/recipes/`, inja JSON 驱动系统 | 删除文件 | |

**验证：** 所有 example 渲染正确。旧代码删除后无编译错误。

---

#### Sprint 7：阴影系统

**目标：** 实现 §3.6 的双层 ShadowMap 架构。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 7.1 | Shadow Pass Compositor | `main_shadow_opaque.vert.glsl` (MVP only), `main_shadow_masked.frag.glsl` (+ alpha test discard) | `ShaderLibrary/compositor/` | |
| 7.2 | Near Dynamic Cascade | 2 级 Cascade (0.5m~8m, 8m~30m)，每帧全量渲染。`ShadowCascadeUBO` 放入 Set 0 | 新建 `ShadowMapManager.cpp` | |
| 7.3 | Far Cached Cascade | 环形滚动 SM (30m~200m)，只更新 dirty tile。实现 Toroidal Scrolling + fract() UV 采样 | | |
| 7.4 | ShadowMask Compose Pass | Compute shader: Near+Far 距离混合 → ShadowMask(R)。考虑现有 `ObjectDynamicShadowPolicy` 枚举的 Capsule(G) + Contact(B) 通道 | | |
| 7.5 | PCF/PCSS 采样 | 实现现有 `ShadowReceive` 枚举中 PCF/PCSS 的 GLSL 实现 | `ShaderLibrary/common/shadow_sampling.glsl` | |
| 7.6 | 光照集成 | `EvalLighting()` 中采样 ShadowMask RT | 修改 `lighting.glsl` | |

**验证：** Forward 路径有正确的级联阴影，Near→Far 过渡平滑。

---

#### Sprint 8：HZB + 遮挡剔除

**目标：** 实现 §2.6-2.7 的 HZB 生成和 GPU 遮挡剔除。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 8.1 | HZB Downsample Compute | Depth RT → 逐级 min downsample (Reversed-Z)。现有设计文档 §2.6 已有 GLSL 代码 | 新建 `ShaderLibrary/pass/hzb_downsample.comp.glsl` | |
| 8.2 | HZB RT 管理 | R32F, log2(max(w,h)) 级 mip chain。通过 `ResourceAllocator` 分配 | | |
| 8.3 | Instance Cull (CPU fallback) | 视锥剔除 — **复用**现有 `Culler`/`VisibilityComponent` ECS 组件 | | |
| 8.4 | Instance Cull (GPU, SSBO 平台) | Compute shader: 视锥 + 粗 HZB → 输出可见 Instance list | | |

**验证：** GPU 剔除后 Draw Call 数显著减少，渲染结果无遗漏。

---

#### Sprint 9：VBuffer 渲染路径

**目标：** 实现 §8 的 Visibility Buffer 路径。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 9.1 | VBuffer ID Pass | `main_vbuffer_id.frag.glsl` — 输出 `{instanceId(16), materialPresetId(8), triangleId(8)}`。**SSBO 平台专属** | `ShaderLibrary/compositor/` | |
| 9.2 | Tile Classification Compute | 16×16 tile → 累计 SurfaceType mask → 生成 TileList per SurfaceType + DispatchArgs | | |
| 9.3 | VBuffer Resolve — 单一 Surface Tile | Compute shader: 解包 VBuffer → 重心插值 UV/Normal → `#include` Surface Function → `EvalSurface()` → `EvalLighting()` → 输出 LitColor | | |
| 9.4 | VBuffer Resolve — 混合 Tile | 通用路径: 分支处理多 SurfaceType | | |
| 9.5 | Forward / VBuffer 自动切换 | VBO 平台强制 Forward; SSBO 平台根据 `DeviceQualityProfile` 选择 | | |

**验证：** VBuffer 路径渲染结果 == Forward 路径（像素级对比）。

---

#### Sprint 10：Meshlet 管线

**目标：** 实现 §2.8 的 GPU-Driven Meshlet 渲染。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 10.1 | 集成 meshoptimizer | Mesh → meshlet 构建 + simplification + LOD DAG。离线工具 | `3rdpty/meshoptimizer/`, 新建 `src/Tools/MeshletBuilder` | |
| 10.2 | 定义 MeshletGPU 结构体 | `{vertexOffset, indexOffset, vertexCount, triangleCount, boundingSphere, normalCone, lodLevel, flags}` | `inc/hgl/graph/MeshletGPU.h` | |
| 10.3 | 引擎二进制格式 (.ulm) | 序列化 MeshletBuffer + LOD DAG + Flags | | |
| 10.4 | Meshlet LOD Select + Cull Compute | DAG 遍历 + Frustum + Cone + HZB → 输出 IndirectDraw commands | | |
| 10.5 | Two-Phase Occlusion | Phase 1: cull by last-frame HZB. Phase 2: re-cull by current HZB | | |
| 10.6 | `vkCmdDrawIndexedIndirectCount` 集成 | meshlet 粒度 Indirect Draw，从全局 SSBO 读取。**复用**现有 `IndirectDrawIndexedBuffer` | | |
| 10.7 | Terrain-Contact Dither | §2.8.5 toggleable Terrain-Contact Dither 路径 | | |
| 10.8 | VBO 平台回退 | 离散 LOD mesh 从 DAG 导出，CPU Frustum Cull + `vkCmdDrawIndexed` | | |

**验证：** GPU Drawcall 从 N 降至 < N/10（大场景），渲染无错。

---

#### Sprint 11：Terrain 256 层系统

**目标：** 实现 §5.5 的完整 Terrain 渲染架构。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 11.1 | 重写 Terrain Surface Function | `terrain_surface.glsl` — `EvalSurface()` 循环采样 TerrainLayerSSBO + Texture2DArray。**替换**旧 `M_TerrainGrid.cpp` 的硬编码 FS | `ShaderLibrary/surface/terrain_surface.glsl` | |
| 11.2 | TerrainLayerSSBO + MI_Terrain | §5.4 定义的 GPU 数据结构: `TerrainLayerParams[256]` (32B/layer) + `MI_Terrain` (32B global) | | |
| 11.3 | SplatMap 编码 | RGBA8 Texture2DArray, ceil(N/4) 层，每层存 4 个地形层权重 | | |
| 11.4 | Terrain VBuffer Resolve | 专用 Compute Kernel — per-pixel 多层 Dither 混合 | | |
| 11.5 | Terrain Forward Path | `EvalSurface` 循环采样 + weight threshold skip + QualityTier 层数限制 | | |
| 11.6 | Terrain LOD / Clipmap | 地形几何 LOD (quad-tree 或 clipmap) — 旧 TerrainGrid 的 VS 方案可作为基础扩展 | | |

**验证：** 256 层地形渲染无闪烁，SplatMap 权重正确混合。

---

#### Sprint 12：Material LOD + Special Surface

**目标：** 实现 §3.5 Material LOD 自动降级 + §5.3 特殊表面材质。

| # | 任务 | 具体操作 | 涉及文件 | 产出 |
|---|------|---------|---------|------|
| 12.1 | `CalcObjectLODTier()` | 屏幕空间面积估算 + 阈值表 + `importanceBias` 偏移 → `objectLODTier` | | |
| 12.2 | `EffectiveTier` 计算 | `min(deviceTier, objectLODTier, surfaceLODCap)` → 运行时选择 SPV 变体 | | |
| 12.3 | `ResolveSPVFallback()` | 根据 `fallback_surface_type` + `unique_feature_min_tier` 路由 SPV | | |
| 12.4 | Skin Surface Function | Ultra: 全 SSS + Detail Normal + 曲率 AO; High: 简化 SSS; Med/Low: fallback Standard | `surface/skin_surface.glsl` | |
| 12.5 | Hair Surface Function | Ultra: Marschner; High: Kajiya-Kay; Med: 单高光 PBR; Low: BlinnPhong | `surface/hair_surface.glsl` | |
| 12.6 | Cloth Surface Function | Sheen + Charlie Model; Med: 简化 wrap; Low: Standard | `surface/cloth_surface.glsl` | |
| 12.7 | ClearCoat Surface Function | High+: 双层 BRDF; Med: 单层近似; Low: 高 specular BlinnPhong | `surface/clearcoat_surface.glsl` | |
| 12.8 | Foliage Surface Function | High+: Thin Translucency + Wind; Med: Wrap + 简化 Wind; Low: 静态 AlphaTest | `surface/foliage_surface.glsl` | |

**验证：** 远处 Skin 材质无缝降级为 Standard，无视觉跳变。

---

#### Sprint 13：后处理管线

**目标：** 基本后处理链。

| # | 任务 | 具体操作 |
|---|------|---------|
| 13.1 | ToneMapping | Compute Pass, ACES/Filmic tone curve |
| 13.2 | Bloom | Downsample chain + Gaussian blur + upsample additive blend |
| 13.3 | FXAA | Single compute/FS pass, Low/Medium quality |
| 13.4 | TAA | Jittered projection + exponential history blend + neighborhood clamping |
| 13.5 | SSAO | GTAO 或 HBAO, Compute, 利用 HZB mip 加速采样 |
| 13.6 | Auto Exposure | Luminance histogram compute + temporal smooth |
| 13.7 | Fog | 内联到 `EvalLighting()` 或独立 Compute pass |

---

#### Sprint 14：Clustered Shading + 高级光照

**目标：** 多光源支持。

| # | 任务 | 具体操作 |
|---|------|---------|
| 14.1 | Cluster 空间划分 | 视锥 3D 网格 (X×Y×Z)，Compute 预计算 cluster bounds |
| 14.2 | Light Assignment Compute | 每个 cluster 分配 light list，输出 light index SSBO |
| 14.3 | `EvalLighting()` 集成 | 从 cluster light list 遍历灯光 |
| 14.4 | Capsule Shadow | CapsuleShadowData SSBO + per-pixel 解析遮挡 |
| 14.5 | Contact Shadow | 屏幕空间 ray-march, High+ |
| 14.6 | SSR | Hi-Z Ray March Compute, High+ |

---

### 14.3 关键代码映射表

以下列出设计文档各节与现有代码的精确对应、差距与动作：

| 设计文档节 | 现有代码 | 状态 | 动作 |
|-----------|---------|------|------|
| §1 设计目标 | — | ✅ 已确认 | — |
| §2.1 Forward Pass | `RenderStage::Forward_*`, `PipelineRenderPath::Forward` | 🔶 枚举存在，Pass 执行依赖 ECS | 注入 Compositor 模板 |
| §2.2 VBuffer Pass | `PipelineRenderPath::VBufferDeferred`, `RenderStage::VisibilityBuffer_Fill` | ❌ 仅枚举 | Sprint 9 新增 |
| §2.3 Z-Prepass | `RenderStage::EarlyZ_Solid/Masked` | 🔶 枚举存在 | Sprint 3 增加 Z-Prepass Compositor 模板 |
| §2.6 HZB | `RenderStage::HZB_Generation/Culling` | ❌ 仅枚举 | Sprint 8 新增 |
| §2.7 二阶段 Meshlet | — | ❌ | Sprint 10 新增 |
| §2.8 Meshlet 管线 | `PipelineTopology::MeshFS`, `ShaderModule::IsTask/IsMesh` | ❌ 仅枚举/flags | Sprint 10 新增 |
| §2.9 平台后端 | `PipelineInputMode::{LegacyVABVBO, SSBOVertexInput}` | 🔶 定义完成，SSBO 未激活 | Sprint 5 激活 |
| §2.10 Reversed-Z | — | ❌ | Sprint 4 新增 |
| §3.5 Material LOD | — | ❌ | Sprint 12 新增 |
| §3.6 阴影系统 | `ShadowReceive`, `GlobalDynamicShadowPolicy`, `ObjectDynamicShadowPolicy` 枚举 | ❌ 仅枚举 | Sprint 7 新增 |
| §4.1 SurfaceType | `MaterialPreset` (17 个旧枚举值) | 🔶 需重新映射 | Sprint 1 替换 |
| §4.2 QualityTier | 无(ShaderPermutationKey 只有 light/ambient/specular/shadow) | ❌ | Sprint 1 新增 |
| §5.1 预设材质表 | `MaterialPreset` 的 17 个工厂函数 | 🔶 存在但需重构 | Sprint 6 逐个迁移 |
| §5.2 Standard Surface | TextureBlinnPhong + BasicLit + PBRColor3D 分立 | 🔶 已有完整 GLSL | Sprint 3 合并 |
| §5.4-5.5 Terrain | `M_TerrainGrid` (简单 VS 生成 + 硬编码 FS) | 🔶 极简实现 | Sprint 11 重写 |
| §6.3 Descriptor Set | 7 个 Set, 语义驱动 | 🔶 需大改 | Sprint 2 重构 |
| **§6.4 Instance-Rate ID 分发** | **`TransformAssignmentBuffer.h`, `MaterialInstanceAssignmentBuffer.h`** | **✅ 完整实现** | **直接复用 — 无需修改** |
| **§6.4.4 Texture2DArray 纹理池** | **PBRColor3D `sampler2DArray` + `mi.texture_id`** | **✅ 完整实现** | **扩展至所有 Lit Surface** |
| §9 Compositor | ComposedMaterialDef + ShaderLogic + Bridge | 🔶 架构错误 | Sprint 3 替换 |
| §12 SPV Cache | 运行时编译(无缓存) | ❌ | Sprint 3.10 新增 |

---

### 14.4 推荐执行顺序与依赖图

```
Sprint 0 (清理)
    │
    ├─── Sprint 1 (类型定义) ──────────────┐
    │                                       │
    ├─── Sprint 4 (Reversed-Z) ←───────────┤
    │                                       │
    └─── Sprint 2 (Descriptor 重构) ───┐   │
                                        │   │
                                        ▼   ▼
                                Sprint 3 (Compositor + Surface Function)  ★★★ 关键路径
                                        │
                        ┌───────────────┼───────────────┐
                        │               │               │
                        ▼               ▼               ▼
                Sprint 5 (SSBO)  Sprint 6 (Forward)  Sprint 7 (Shadow)
                        │               │               │
                        └───────┬───────┘               │
                                │                       │
                                ▼                       │
                        Sprint 8 (HZB+Cull) ◄──────────┘
                                │
                        ┌───────┴───────┐
                        ▼               ▼
                Sprint 9 (VBuffer)  Sprint 10 (Meshlet)
                        │               │
                        └───────┬───────┘
                                ▼
                        Sprint 11 (Terrain)
                                │
                                ▼
                        Sprint 12 (Material LOD + Special Surface)
                                │
                                ▼
                        Sprint 13 (PostProcess)
                                │
                                ▼
                        Sprint 14 (Clustered + Advanced)
```

**关键路径**: Sprint 0 → 1 → 2 → 3 → 6 → 7 → 8 → 9/10 → 11 → 12

Sprint 4 (Reversed-Z) 和 Sprint 5 (SSBO) 可与 Sprint 3 并行开发。

---

### 14.5 现有代码复用率估算

| 模块 | 现有代码量(估) | 可复用 | 需重写 | 需删除 |
|------|-------------|--------|--------|--------|
| Vulkan 基础层 (`src/Vulkan/`) | ~15000 行 | 95% | 5% (Set Layout) | 0% |
| ECS 框架 (`inc/hgl/ecs/`) | ~5000 行 | 100% | 0% | 0% |
| 材质核心 (`VKMaterial*`) | ~3000 行 | 70% | 30% (MI 管理适配) | 0% |
| ShaderGen 组合层 | ~4000 行 | 0% | 0% | **100% 删除** |
| GLSL 模组 (`ShaderLibrary/`) | ~2000 行 | 60% (PBR/lighting) | 40% (重组) | 模板引擎删除 |
| 材质工厂 (`M_*.cpp`) | ~3000 行 | 30% (Unlit) | 70% (Lit → Surface) | 旧 Bridge 删除 |
| Pipeline 管理 | ~3000 行 | 90% | 10% | 0% |
| Buffer/Memory | ~4000 行 | 100% | 0% | 0% |
| **总计** | ~35000 行 | **~70%** | **~20%** | **~10%** |

> **结论：** 引擎底层基础设施（Vulkan 层、ECS、Buffer、Pipeline）非常完善，复用率极高。
> 主要工作集中在**中间层重构**（ShaderGen 组合 → Compositor，7-Set → 4-Set）和**上层新增**（阴影/HZB/Meshlet/VBuffer/PostProcess）。

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

## 附录 B：纹理双模式方案（传统纹理 / Texture2DArray 纹理阵列）

引擎同时支持**传统纹理（Mode A）**和 **Texture2DArray 纹理阵列（Mode B）**两种纹理绑定模式，
通过编译期 `#define TEXTURE_ARRAY 0/1` 切换。两种模式可在同一帧内并存（不同 DrawCall 使用不同 Pipeline 变体）。

### B.1 两种模式的工作原理

#### Mode A：传统纹理（`TEXTURE_ARRAY 0`）

1. **CPU 侧**：每个 MaterialInstance 持有独立纹理对象（`VkImageView` + `VkSampler`）
2. 每个 MI 拥有独立的 DescriptorSet，Set 2 binding 1-6 绑定各自的 `sampler2D`
3. 不同材质需切换 DescriptorSet → 适合材质种类少、无 GPU-Driven 批量合并的场景
4. **GPU 侧**：FS 直接 `texture(TextureAlbedo, uv)` 采样，无需 MI 中的 `tex_*` 索引字段

#### Mode B：纹理阵列（`TEXTURE_ARRAY 1`）

1. **CPU 侧**：MaterialInstanceAssignmentBuffer 收集当前帧所有可见 MI，对 MI 去重后写入 SSBO
2. 每个 MI 的纹理提交至 Texture2DArray（按分辨率分组），获得层索引 `texture_id`
3. `texture_id` 写入 MI 数据结构的 `tex_albedo`, `tex_normal` 等字段
4. 多个 MI 共享同一组 DescriptorSet（同一纹理阵列），支持大量材质合批绘制
5. **GPU 侧**：FS 通过 `GetMI()` 读取 MI → 使用 `mi.tex_albedo` 等字段从 `sampler2DArray` 采样

### B.2 各 SurfaceType 推荐模式

| SurfaceType | 推荐模式 | 说明 |
|-------------|----------|------|
| Standard (Texture/Color/VertexColor) | **Mode B** (纹理阵列) | 数量最多，合批收益最大 |
| Skin / Hair / Cloth / ClearCoat | **Mode B** (纹理阵列) | 上述 5 槽 + `ExtraArray0~2` 扩展槽；`SurfaceOutputExt` 额外纹理通过扩展槽索引 |
| Terrain | **Mode B** (纹理阵列, 专用) | 独立 `TerrainAlbedoArray` / `TerrainNormalArray` / `TerrainMRArray`，TerrainLayerSSBO 中 `tex_*` 索引（§5.5） |
| Unlit 2D/3D | **Mode A** (传统纹理) | 简单材质，数量少，无需纹理阵列开销 |
| Billboard / Sky / Gizmo3D | **Mode A** (传统纹理) | 特殊材质，各自独立绑定即可 |
| 任意 SurfaceType (调试/原型) | **Mode A** (传统纹理) | 开发期间快速迭代，跳过纹理阵列打包流程 |

> **注意**：推荐模式仅为默认策略。任何 SurfaceType 均可编译两套 Pipeline 变体（TEXTURE_ARRAY=0 和 1），
> 在运行时按场景需要选择。例如 Standard 材质在编辑器预览时可使用 Mode A 简化流程。

### B.3 分辨率分组策略（仅 Mode B）

Vulkan 要求 Texture2DArray 所有层分辨率一致。解决方案：
- 按分辨率分组创建多个 Array（如 512², 1024², 2048²）
- MI 中 `texture_id` 的高位编码 Array 组号，低位编码层索引
- 或使用 `VK_EXT_descriptor_indexing` 的 bindless 纹理数组（PC/High-end 设备）

> Mode A 不受此限制——每个 MI 的纹理分辨率可以各不相同。

### B.4 与 VBuffer 管线的兼容

两种模式均可与 VBuffer 管线配合使用：
- **Mode B**：VBuffer Resolve 阶段从 VBuffer 解包 MaterialInstanceID → 索引 MI SSBO → 读取 `tex_*` → 从纹理阵列采样
- **Mode A**：VBuffer Resolve 阶段需按 MaterialInstanceID 查找对应 DescriptorSet，逐 MI 发起采样（效率低于 Mode B）

因此 VBuffer 管线推荐优先使用 Mode B，Mode A 仅在 Forward 管线中效率最优。

## 附录 C：文件数量对比

| | 旧系统 | 新系统 |
|--|--------|--------|
| C++ 源文件 | ~40+ (.cpp/.h) | ~12 (.cpp/.h，含 CompositorAssembler) |
| GLSL 文件 | 0（全 C++ 字符串） | ~105 (.glsl 文件，含 compositor/ + surface/ + common/ + vbuffer/ + gpudrive/ + postprocess/ + debug/ + scene/) |
| 其中 Compositor 模板 | — | ~13 (compositor/*.glsl — 所有 SurfaceType 共用) |
| 其中 Surface Function | — | ~11 (surface/*.glsl — 每个 SurfaceType 各一份；Special Surface 低档位可 fallback 到 Standard SPV §3.5.6) |
| 编译期 Shader 变体 | 动态，不可预测 | ≤11 (SurfaceType) × ≤6 (QualityTier) × ≤4 (shadow+flags) × ≤3 (PassType per BlendMode) = ~792 最大（SPV fallback 复用后实际 ~550，Cinematic 仅 PC 编译） |
| 概念数 | FixedMaterialDef, ComposedMaterialDef, MaterialLogicDef, ShaderCompositionBridge, BuiltinHelpers, ShaderCreateInfo (5种), MaterialCreateConfig (3种), LightingModel enum... | MaterialPresetDef, MaterialInstance, MaterialCategory, **CompositorAssembler**, PresetShaderCompiler, SPVCache, DeviceQualityProfile, TextureSlotDef, BlendMode, PassType |
