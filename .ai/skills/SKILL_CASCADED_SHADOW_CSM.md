# SKILL_CASCADED_SHADOW_CSM.md

**适用：级联阴影（CSM）、动静分层阴影、静态滚动缓存、阴影 bias 与 PCF 调参、阴影故障排查**

本引擎的 CSM 不是"教科书 CSM"，而是一套**动静分层 + 静态滚动缓存**的定向光阴影实现。
它的每一步设计都对应一个已经踩过的线上缺陷，改动前请先读完对应章节，否则极易把
已经修好的问题重新引入（历史上"静态物件近距无阴影""相机一动静态阴影整体滑动"
"滚动更新退化成逐帧全量重绘"都是这么反复出现的）。

---

## 0. 文件地图

| 文件 | 职责 |
|------|------|
| `inc/hgl/graph/render/lighting/CascadedShadowController.h` | `CascadedShadowConfig`（配置）、`CascadeUpdateResult`（逐帧更新决策输出）、`ShadowDirtyRect`、控制器声明 |
| `src/SceneGraph/render/lighting/CascadedShadowController.cpp` | `CalculateSplitDistances`、`CalculateCascadeBounds`（拟合+锚定，**核心**）、`Update`（失效判定树+UBO 回写） |
| `inc/hgl/graph/ubo/ShadowInfo.h` | `kMaxShadowCascades=4`、`ShadowCascadeInfo`（176B）、`ShadowInfo`（832B）、`ShadowCascadeCacheState`（CPU 侧，不上传） |
| `ShaderLibrary/shadow/pcf_shadow.glsl` | `EvalCascadePCF`、`EvalCascadeShadowAt`、`EvalCascadeChain`、`EvalPCFShadow`（选级 + 动静合并） |
| `src/ShaderGen/template/FragmentTemplateComposer.cpp` | 发射 `HGL_SHADOW_PCF_POISSON_TAPS` 宏（仅当模板含 ShadowProvider 槽） |
| `example/Basic/CascadeShadowMap.cpp` | 参考用法：4 张 D32F 离屏 RT、`light_camera` 覆写矩阵、F1 之外只有 `[`/`]` 调 bias |
| `example/Basic/AlphaTestShadow.cpp` | alpha test 镂空阴影最小用例（masked vs fallback-opaque 对照）+ `DumpCascadeDepth` 级联深度读回报证工具 |
| `doc/alpha-test-shadow-masked-caster-fix-chain-2026-09-26.md` | ShadowCasterMasked 修复链全记录（7 层因果、两级寻址、取证方法） |
| `src/ecs/support/TestCSMIncrementalPass.cpp` | 契约测试（Test 1-4 渲染选项、Test 5A 重绘预算、Test 5B 矩阵恒定性+覆盖率、Test 5C 旋转恒定性、Test 6A-D 逐级联 bias） |
| `doc/shadow-ubo-inflight-overwrite.md` | 拖拽时阴影逐帧左右/远近跳：单份 ShadowInfo 被在途帧覆写，以及分槽修复 |

相关但不在本 SKILL 范围：`RenderPassRequest`（脏矩形/scissor 翻译）、`Mobility`（动静标志）、
`ECSContext::RenderTo`（渲染到离屏 RT）。

---

## 1. 硬约定（改任何一行矩阵前必须确认）

| 项 | 取值 |
|----|------|
| API / 手性 | Vulkan / **右手系** |
| 世界坐标 | **Z-up**（X 右，Y 前，Z 上） |
| 深度 | **Reversed-Z**，NDC z ∈ [0,1]（ZO） |
| 主相机投影 | X 不翻转、Y 翻转（`m00 > 0`，`m11 < 0`） |
| 阴影贴图 | 正交投影 `OrthoMatrixReversedZ`，RT 为 `OffscreenDepthOnly` + `PF_D32F` |
| 阴影比较 | `ShadowPCF` 采样器 `compare_op = "GreaterOrEqual"` ⇒ **值越大越靠近光源** |

`LookAtMatrix(eye, center, up)` 语义为 RH：相机前向映射到视空间 -Z。

**光空间基底的构造**（`CalculateCascadeBounds` 开头，固定不变）：

```cpp
light_forward   = normalize(light_dir);                 // 指向场景
light_up        = (0,0,1)，若 |dot(light_forward,up)| > 0.99 则改用 (0,1,0)
light_right     = normalize(cross(light_forward, light_up));
light_up_actual = cross(light_right, light_forward);
```

光空间 UV 的两轴是：**U = `light_right`**，**V = `-light_up_actual`**（Vulkan UV 向下）。
所以包围球中心的光空间坐标是：

```cpp
cx = dot(center,  light_right);
cy = dot(center, -light_up_actual);
```

---

## 2. 动静分层架构

`c0_dynamic_overlay = true` 时进入"动静分层"模式（`csm_params.y == 2`）：

```
CSM 0  ── 近距动态层（Movable 物件）── 每帧全量重绘，0.1m ~ split_distances[0]
CSM 1  ── 近+中距静态层（Static 物件）── 滚动缓存，0.1m ~ split_distances[1]   ┐
CSM 2  ── 远距静态层（Static 物件）  ── 滚动缓存，split[1] ~ split_distances[2] ├ 互相不重叠
CSM 3  ── 超远距静态层（Static 物件）── 滚动缓存，split[2] ~ split_distances[3] ┘
```

- **CSM 0 与 CSM 1 的覆盖区间必须重叠**（示例都从 `znear` 起），因为 CSM 0 只收动态物体、
  CSM 1 只收静态物体，二者的交集区由 shader 取"更暗者"合并。若让 CSM 1 从 `split[0]` 起，
  近距静态物件的阴影就会整块消失。
- **CSM 1..3 只收集 `Mobility::Static`**，因此场景不变时它们的贴图内容是稳定的，
  可以多帧累积（滚动缓存）。**CSM 0 不收静态物件**，所以它每帧全量重绘也不算浪费。
- 示例配置：`split_distances = {50, 50, 160, 300}`（CSM0/1 都是 50m，CSM2 到 160m，CSM3 到 300m）。

shader 侧合并（`pcf_shadow.glsl` 的 `EvalPCFShadow`）：

```glsl
static_shadow  = EvalCascadeChain(1u, cascade_count - 1u, worldPos, view_depth); // 静态链
dynamic_shadow = 1.0;                                                            // 默认受光
if (shadow.cascades[0].shadow_tex.x != 0u && view_depth <= dyn_far)
    dynamic_shadow = EvalCascadeShadowAt(0u, worldPos, dyn_edge);                // 动态层
return min(dynamic_shadow, static_shadow);                                       // 取更暗
```

动态层在 `dyn_far` 附近按 `blend_width * (dyn_far - dyn_near)` 的深度带 + UV 边界双向淡出，
避免 CSM 0 的方框边界出现硬边。

**选级依据**是"相机 → 片元"的前向深度 `dot(worldPos - camera.camera_world_pos, camera.view_line)`，
与每级 `cascade_params.y`（= `split_far`）比较，**不是**用 `shadow_vp` 反推。
因此 `cascade_params.x/y` 必须写真实切分距离，改动切分就要同步。

### 2.x 硬规则：**屏蔽级联 = 该深度区间无阴影数据**

`SetCascadeEnabled(c, false)` / `cascade_mask` 会把 `cascades[c].shadow_tex` 置 0，
CPU 侧跳过该级的渲染。shader 侧必须遵守：

1. **级联归属只由 `view_depth` 与 `cascade_params.y` 决定**，与"该级是否被屏蔽"无关：
   ```glsl
   uint selected = last_c;
   for (uint c = first_c; c <= last_c; ++c)
       if (view_depth <= shadow.cascades[c].cascade_params.y) { selected = c; break; }
   if (shadow.cascades[selected].shadow_tex.x == 0u)
       return 1.0;   // 该区间无数据 → 受光
   ```
2. **绝对禁止"跳过被屏蔽的级联、把该区间交给更远的级联"**。曾经写成
   `if (shadow_tex.x == 0u) continue;`（选级循环内），后果：
   关掉 CSM 1 后，20m 处的片元继续判 `20 <= split_far(CSM2)=160` → 命中 CSM 2，
   于是**近景（0.1–50m）被 CSM 2 那张粗粒度贴图接管** —— 现象即
   "CSM 2 的内容出现在 CSM 1 的范围"。因为两张贴图画的是同一批静态物件，
   交界处看起来还是**无缝**的，极易误判成"映射错位"或"包围盒漂移"。
3. **被屏蔽的级联不得影响相邻级**：交界带里取暗叠加前要判
   `cascades[selected+1].shadow_tex.x != 0u`，否则屏蔽一级会波及相邻级。
4. 反向情形天然正确：CSM 0（动态层）被屏蔽而 CSM 1 在时，
   `min(EvalCascadeShadowAt(0)/*1.0*/, static)` 仍得到静态阴影。

回归点：`TestCSMIncrementalPass` Test 7C 含契约字符串
`shadow.cascades[selected].shadow_tex.x == 0u`，删除该判断即失败。

### 2.y 交界带：`cascade_params.w` = **世界单位米**

- `cascade_params.z` = 比例（末级 `max_distance` 边缘淡出 + 动态层 CSM 0 边界淡出）。
- `cascade_params.w` = **交界带宽度（米，`config.blend_distance`）**，示例 1.5m。
- 交界带内**近级始终整强度参与、不做淡出**，只把远级结果 `min()` 进来 ——
  于是交界处表现为"叠加取暗"而不是"近级被远级顶替"。
  曾经让近级 `mix(1.0, shadow, fade_depth)` 淡出、远级只在近级"没数据"时介入，
  结果是交界处出现生硬的单向替代感。
- 带宽必须**小（1–2m）**：远级贴图更粗（PCF 半径折算到世界更大），
  带宽一大就会看到"近景换成远景贴图"。设置 0 表示硬切换。
- 历史坑：`.w` 曾被当作 flags 写（`c > 0 ? 1 : 0`），而 shader 把它当**比例**乘上
  `(split_far - split_near)` 用 → 1.0 表示"整个级联区间都是过渡带"，
  死旋钮 + 近级整段被远级顶替。**写 UBO 字段前先核对 `ShadowInfo.h` 的语义注释。**


---

## 3. 级联拟合与双轴锚定（本 SKILL 的核心）

### 3.0 两个有意识的设计决策（勿"顺手统一"）

| 决策 | 理由 | 代价（已接受） |
|------|------|----------------|
| **包围球 + 正交方框拟合**（而非光空间 AABB 收紧） | 朝向无关的 radius——锚定/缓存矩阵恒定性（§3.3、Test 5C 旋转恒定性）依赖它；AABB 随朝向变化，缓存命中率崩塌 | 纹素利用率损失 ~1.4-2×（方框对角覆盖球）。若未来做精度优化，须**同时**补偿锚定（AABB 中心也吸附），并重跑 Test 5B/5C |
| **手搓 4 张离屏 RT 而非复用 OffscreenWorld**（`depth_only`+`cull_mode`） | 阴影 pass 必须**复用主世界实体**（同一 ECSContext 的 collect/batch 数据），OffscreenWorld 是"另起一套实体集合"的机制，语义不匹配 | 4 张独立 D32F RT 的显存与 bindless 槽（§10 atlas 条目）。若未来统一，须先给 OffscreenWorld 加"引用外部实体"模式 |

### 3.1 逐步流程

```
1) 级联切分
   split_near = (c==0 || (c==1 && c0_dynamic_overlay)) ? znear : splits[c-1]
   split_far  = splits[c]

2) 8 个视锥切片角点（split_near/split_far × tan(fovY/2) × aspect）

3) 包围球
   mid_dist     = (split_near + split_far) * 0.5
   sphere_center = cam.pos + cam_forward * mid_dist      ← 区间中点，不是视锥质心
   radius       = max(8 角点到 sphere_center 的距离) * 1.02

4) 【横向锚定补偿】c > 0 时：
   radius += lateral_anchor * 0.708

5) 【沿光轴锚定】
   along         = dot(sphere_center, light_forward)
   along_anchor  = floor(along / anchor_step) * anchor_step
   anchored_center = sphere_center - light_forward * (along - along_anchor)

6) 【横向锚定】
   cx0 = dot(anchored_center,  light_right)
   cy0 = dot(anchored_center, -light_up_actual)
   cx  = round(cx0 / lateral_anchor) * lateral_anchor      ← 必须 round
   cy  = round(cy0 / lateral_anchor) * lateral_anchor

7) texel 吸附
   texel_size = 2 * radius / map_size
   snapped_cx = floor(cx / texel_size) * texel_size
   snapped_cy = floor(cy / texel_size) * texel_size

8) 回到锚定点重建世界空间中心（⚠ 基准必须是 cx0/cy0，见 3.4）
   snapped_sphere_center = anchored_center
                         + (snapped_cx - cx0) * light_right
                         + (snapped_cy - cy0) * (-light_up_actual)

9) 光照相机与投影
   light_eye  = snapped_sphere_center - light_forward * (radius + caster_depth_margin)
   light_view = LookAtMatrix(light_eye, snapped_sphere_center, light_up_actual)
   正交窗口    = ±radius（left/right/bottom/top）
   znear      = 0.1
   zfar       = 2*radius + 2*caster_depth_margin + anchor_step
   light_proj = OrthoMatrixReversedZ(...)
```

输出回写：`out_snapped_center = (snapped_cx, snapped_cy, -radius, -radius)`、
`out_texel_size`、`out_along_anchor`。

### 3.2 为什么需要"沿光轴锚定"

静态级联的贴图是多帧累积的，**深度分量在缓存有效期内必须恒定**。否则每帧的
`shadow_vp` 会把缓存里的旧深度解释成另一个值：相机沿光轴每移动 1 个 texel 的横向
距离，深度就整体偏移 `1/map_size` 的完整深度范围，走几米就足以让接收者（地面）的
深度掉出缓存深度窗口 ⇒ **"远处地面不再接收阴影"**，且局部重建无法自愈。

沿光轴平移不改变 texel 对齐（不产生 xy 环形寻址位移），因此可以把它吸附到 `anchor_step`
（默认 16m）的整数倍：step 内深度矩阵完全一致，跨步时才整级联重建。
`cx/cy` 用的是与 `light_forward` 正交的两轴，所以沿光轴吸附不影响 texel 对齐。

### 3.3 为什么需要"横向锚定"

只沿光轴锚定是不够的：**相机横向只要移动 1 个 texel，包围球中心就整体平移 1 个 texel**，
缓存里的旧内容全部错位。实测 600 帧横向行走（0.083m/帧）会把静态级联打成**每 1.4 帧一次
整级全量重绘**（CSM1 435/600、CSM2 236/600、CSM3 101/600），"滚动更新"完全退化成"逐帧全量"。

做法：把包围球中心在 `light_right` / `-light_up_actual` 两轴上吸附到锚定格 `L` 的整数倍
（`cache_scroll_band_texels[]`，**texel 口径**，默认 `{0,16,16,32}`；c0 不参与）。格内中心
完全静止 ⇒ 布局矩阵恒定，texel 吸附后位移恒为 0 ⇒ 缓存整段有效；跨格时中心跳变 `L`，
位移必然非零。收益实测：**CSM1/2/3 重绘次数 -92% / -86% / -66%**（旧 2m 口径
`full=[600,33,32,34]`；现在 B=16/16/32 口径为 `full=[600,33,17,6]`——远景级联因世界步长
更大而进一步省）。

**步长为什么以 texel（而不是米）为主参数**：`L` 同时是 ①环形偏移的量子（`cache_offset`
是 `uvec`，滚动必须按整数 texel 走）②半径补偿量。两个约束同时成立 ⇒ **逐级 `L_c` 必须是
该级 texel 的整数倍**，所以给 B 而不是给米：

```
L_c = 2·B·r0_c / (M − 1.416·B)        radius += 0.708·L_c   ⇒  L_c / texel ≡ B
精度损失 = 1.416·B / (M − 1.416·B)     （与切片半径 r0、贴图分辨率 M 都无关，只看 B/M）
```

旧的世界米口径（共享 2.0m）折成 texel 是 c1 15.4 / c2 5.1 / c3 2.8 —— 远景级联的条带比
PCF 外扩（2 texel）还窄，"条带"会退化成"整条都是重叠带"。B 口径天然消除这个不对称。
M=1024：B=16 → 2.26%、B=32 → 4.63%、B=64 → 9.7%、B=M/8 → 21.5%；B ≥ M/1.416 ⇒ 退化
（代码 fail-safe 回"禁用锚定"）。**"贴图开到 4096 就能用 1/8 这种分数步长"不成立**：损失
只由 B/M 决定，大贴图的收益是同一个 B 对应更小的**世界**步长（陈旧带窄、滞后小）。

**为什么必须用 `round` 而不是 `floor`**：`round` 每轴偏离 ≤ `step/2`，对角 ≤ `0.707*step`，
半径补 `0.708*step` 即可保覆盖；`floor` 的偏离范围是整个步长 `[0, step)`，半径要按
`1.414*step` 扩，代价几乎翻倍。

**代价**：`texel_size = 2*radius/map_size`，半径变大 ⇒ texel 变粗，比例 = `1.416·B/(M−1.416·B)`
（B 口径下**与半径无关**，见上）⇒ 用一点静态阴影精度换掉绝大部分重绘开销。

### 3.4 ⚠ 锚定基准必须是 `cx0`/`cy0`，不是 `cx`/`cy`

这是本模块历史上最隐蔽的一个缺陷，**改动第 8 步前务必读完**。

`snapped_cx = floor(cx/texel_size)*texel_size`，而 `cx` 本身已经是 `round(cx0/step)*step`
的粗格点。由于 `texel_size ≪ L`（c1 约 `1.2e-1` vs `2.0`），

```
snapped_cx ≈ cx            ⇒   (snapped_cx - cx) ≈ 0
```

若第 8 步写成 `(snapped_cx - cx)`，横向吸附会被**完全抵消**，包围球中心仍跟随视锥
每帧连续移动（只是被 texel 量化）⇒ 光照矩阵每帧都变 ⇒ **缓存命中的帧里，旧深度被新矩阵
解读，静态阴影整体滑动**。这个缺陷在覆盖率类断言下完全看不出来（覆盖率依然 100%），
只有"矩阵恒定性"断言才能抓住它。

正确写法回到**原始中心**：

```cpp
snapped_sphere_center = anchored_center
                      + (snapped_cx - cx0) * light_right
                      + (snapped_cy - cy0) * (-light_up_actual);
```

效果：光照矩阵在一个锚定格内**完全恒定**（契约 Test 5B-1 实测 `max_delta = 0.000000`），
缓存命中时矩阵与生成该深度时的矩阵严格一致——这才是"静态滚动缓存"成立的前提。
格内视锥相对窗口最多漂移 `0.707*step`，由 3.1 第 4 步的半径补偿保证仍被覆盖。

### 3.5 `caster_depth_margin`

`caster_depth_margin`（默认 100，示例 120）从两侧扩展光照视锥：光源眼点后退
`radius + margin`，`zfar` 再加 `2*margin`。它负责容纳视锥外**背向光源**的投影物，
以及包围球下方的接收者（地面）。**不要因为"跑得动"就调小**：调小会让相机升高/俯仰时
地面深度超过 `zfar` 被裁掉，表现为"抬高相机后一片地面没有阴影"。

---

## 4. 更新决策树（`Update`）

```cpp
for c in 0..count-1:
    if (c == 0):
        need_full_update = true;  is_static_cache = false;      // 近距动态层：逐帧全量
        AddDirtyRect(全图);  scroll_offset = (0,0,0,0);  ++generation
    else:
        is_static_cache = true;
        if (valid == 0 || scene_revision 变 || along_anchor_[c] != along_anchor):
            全量重绘（首次 / 场景失效 / 沿光轴跨步）
        else:
            shift_x = round((snapped_cx - cache.snapped_origin.x) / texel_size);
            shift_y = round((snapped_cy - cache.snapped_origin.y) / texel_size);
            if (shift == (0,0)):  need_full_update = false;  ClearDirtyRects();  // 0 DrawCall
            else:                 need_full_update = true;   AddDirtyRect(全图); // 整级重建
```

`need_full_update == false && dirty_rect_count == 0` ⇒ **完全命中缓存，一个 DrawCall 都不发**
（相机静止时静态级联就是这个状态）。

示例侧（`RenderCSM`）据此选择三种渲染路径：

| 条件 | 请求 |
|------|------|
| `c == 0 || need_full_update` | `load_depth=false`、`use_scissor=false`、全图 |
| `dirty_rect_count > 0` | `load_depth=true`、`use_scissor=true` + 该脏矩形、`clear_scissor_depth=true` |
| 其余 | 什么都不做 |

`light_camera->custom_matrices = true` + `custom_view/custom_projection = res.*` 是必需步骤
（`CameraSystem` 的 `custom_matrices` 分支会跳过常规矩阵推导）。**`custom_view` 一律取
`res.light_view_draw`**（不是 `light_view`）：非零环形偏移时它把内容光栅化到物理贴图
坐标系，与读侧 `fract(uv + cache_offset*inv_map_size)` 对齐；偏移为 0 时两者逐位相同。

> **现状说明（S2 已启用环形寻址）**：`cache_offset` 不再恒 0——静态级联（1..N）跨格时
> 偏移按跨格量累加（`scroll_offset` 为 `uint32` 真源，写进 `casc.cache_offset` 与读侧
> shader 的 `fract(shadow_uv + cache_offset*inv_map_size)` 对齐）；`cache_valid_rect`
> 仍为 `(0,0,W,H)`（整张贴图恒有效：环形滚动下旧内容原地续用）。
>
> **写读坐标系的唯一契约（改任何一处前必读）**：非零偏移下写侧必须用
> `CascadeUpdateResult::light_view_draw`（= `light_view` 左乘光空间平移
> `(offset.x*texel, -offset.y*texel, 0)` —— **y 取负**是因为 `OrthoMatrixReversedZ`
> 用 `2/(bottom-top)`，V 轴向下），而不是 `light_view`；`EnvironmentSystem` 两条路径
> （全量/条带 scissor）都已改用 `light_view_draw`，偏移为 0 时它与 `light_view`
> 逐位相同（未滚动路径零变化）。三条同时成立的不变式：
> ① **物理不动**：固定世界点在物理贴图上的位置跨格前后不变（旧内容原地继续有效）；
> ② **写读一致**：写侧矩阵投出的 uv ≡ 读侧 `fract(uv + O*texel)`；
> ③ **整级重建必清偏移**（否则非零偏移下"整级重画"会漏掉尾部 `|O|` 条带——光栅器
> 没有环绕，内容落在 `[O, 1+O)` 被裁）。
>
> **条带可达性（改 B/M 时必算）**：`AppendWrappedStrip` 的"跨缝拆成两段"分支**只在
> `M % B != 0` 时可达**——偏移恒为 `B` 的整数倍，故 `M % B == 0` 时 `start+width ≤ M`
> 恒成立（默认 `{16,16,32}` 与 `M=1024` 即此情形，测试里用 `B=24` 才走到）。
> 该分支不是死代码：用户可配 `B`/`shadow_map_size` 使 `M % B != 0`。
>
> **静态缓存失效链（A3，已接线）**：决策树里 `scene_revision 变` 的信号源是
> `TransformSystem::SubmitTransformUpdates`——检出任何 Static transform 变更
> （LocalTRS/父子/Mobility）即递增 `ECSContext::static_scene_revision`，
> `EnvironmentSystem::RenderMainLightShadowPass` 比对消费并调用
> `InvalidateStaticCache()`（当帧 prepass 全量重建）。**注意**：`SetLocalPosition`
> 等 setter 没有同值短路——每帧重复 set 同值（如网格吸附逻辑）也会被判为
> 变更，把静态级联打成每帧全量重绘；调用方必须"值变了才 set"。运行时
> 新增/删除静态物体、替换其材质/贴图不走此链，需手动调
> `EnvironmentSystem::InvalidateMainLightStaticShadowCache()`。
> 另注意**深度锚点（`cache_anchor_step`）跨步也会整级重建并清零偏移**——它是滚动
> 偏移累积的最大杀手：偏移要连续几十次跨格不被重置，才可能落到"贴图末 B 纹素内"
> 从而走到跨缝拆分分支（测试子用例 (e) 就是为此把 `cache_anchor_step` 放到 4096）。

### 4.5 ShadowCasterMasked 数据链（alpha test 镂空阴影，2026-09-26 接线）

masked caster 的镂空阴影横跨 collect/batch/pipeline 三层，改其中任何一层前
先读本节（完整因果链见 `doc/alpha-test-shadow-masked-caster-fix-chain-2026-09-26.md`）：

1. **程序双槽**：阴影 pass 的 program 存 `MaterialComponent::shadow_program`
   （与 forward 槽独立），由 `ResolveShadowCasterProgram` 解析——模板分派按
   recipe 的 `alpha_test` 走 `ShadowCasterMasked`（片元采样 opacity_mask 并
   `HGLApplyAlpha` discard）。**阴影帧绝不代 forward 物化纹理行**——两条物化
   链会互踢纹理配置行；行未就绪（`valid==false`，首帧 prepass 早于主帧物化）
   时跳过本帧该 caster，并 bump `static_scene_revision` 触发下帧重画。跳过/失败
   路径（行未就绪 / 程序解析 / 几何 / 管线）统一走
   `RenderPrimitiveCollectSystem::AdvanceShadowRetry`（D9）：首次跳过告警一次
   （日志 `[RenderPrimitiveCollectSystem] shadow pass skip for '<名字>': <原因>`），
   连续 120 帧仍失败则报错一次并把 bump 降频为每 60 帧一次（限速自愈）；
   caster 成功画出后计数清零。
2. **两级寻址**（片元 `MTL_TEX(i)`）：`pc_root.addr_mtl_data_addrs` 指向
   **batch 行表**（`WriteBatchIndexRows` 每行 {payload_index,
   texture_reference_index}，`gl_InstanceIndex` = 行号）→
   `pc_root.addr_texture_references`（**纹理配置池基址**，即
   `material_texture_zero_row_gpu`）+ texref*stride = 配置行。任一地址为 0
   都静默 fallback 1.0 → **影子实心**。`texture_reference_base_addr` 必须
   与 4-ID 解析分支无关地幂等设置（曾在 resolved 分支漏设）。
3. **depth-only FS 剥除豁免**：`RenderPass::CreatePipeline` 对零颜色附件通道
   默认剥离片元 stage（不透明优化）——**判据只有一条**：normalize 后的 recipe
   `alpha_test`/`dither`（与 `masked ? ShadowCasterMasked : ShadowCasterOpaque`
   模板分派同源）。D8（2026-09-26）实测此前的两个程序级判据（SPIRV 扫描：常量
   写错 + 只在 stage 缓存命中分支运行；FinalGLSL 文本扫描：include 的
   alpha_compositor 不在文本里）**恒 false**，已整体删除，`ShaderProgram` 不再有
   `fragment_shader_required` 标志。**新增镂空类模板时必须确认 recipe 声明了
   `alpha_test`/`dither`**，否则深度图实心且无任何报错。
4. **pipeline 复用必须校验 program 身份，且身份禁用指针/句柄（D2，2026-09-26）**：
   shader 更新会生成新 program 对象，仅按 RenderPass 键控会永久复用旧 SPIRV——但
   身份**不能是 program 指针**（对象释放后新对象可落到同一地址 → 误判"同一个
   program"）。现态：`PrimitiveComponent::ResolvedRuntimePipeline{Pipeline*,
   ShaderProgramKey, has_program_key}` 单 map，复用校验比对 `ShaderProgramKey`
   digest；pipeline 缓存键 `FinalPipelineKey::shader_stages_hash` 用 **SPIRV 内容
   hash**（`VulkanDevice` 在 module 创建时登记、析构注销；查不到即 fail-fast 判键
   不完整，**禁止退回 `VkShaderModule` 句柄值**——句柄可被复用，两个不同 shader 会
   撞同一个键）。契约由 `TestCSMIncrementalPass` **Test 12**（7 checks，含 2 条禁复活）
   把守；根因与验证见 `doc/backlog.md` D2。
5. **主帧同步**：forward 管线的 alpha test 同语义（`forward_lit.glsl.tmpl` 的
   `#ifdef HGL_ALPHA_TEST HGLApplyAlpha(EvalAlpha(si, materialDataIndex))`）——
   本体与影子用同一 opacity_mask 槽。
6. **取证**：深度图直接读回（`AlphaTestShadow::DumpCascadeDepth`，graphics
   queue + CopyImageToBuffer + BMP）。判读：棋盘 cube 深度投影填充 ~57%=镂空、
   ~100%=实心、全空=片元被剥或全 discard。地面上看影子不如直接读深度图
   （地面纹理/环境光/透视压缩都会干扰判读）。
7. **后续工作**（TransformComponent 同值短路、级联重配置形态、拆分等）见
   `doc/backlog.md` **D 线**（D1/D8/D2 已完成 ✅）；完整修复因果链见
   `doc/alpha-test-shadow-masked-caster-fix-chain-2026-09-26.md`。

---

### 4.6 环形读侧的**接缝（seam）可达性**（S4，2026-09-26 量测）

读侧是 `fract(shadow_uv + cache_offset·texel)`：整张贴图构成一个环，`layout uv = 0/1`
是一条**跳变线（seam）**，其物理位置 = `cache_offset`（滚动时随偏移移动）。采样跨越
seam 会读到方框**对侧**的深度（世界距离 M 个 texel）⇒ 若可见接收者能贴到 seam，
贴图上就有一条 1~2 texel 宽的错影线。这就是"wrap 必须由 `cache_offset` 驱动、
未滚动时钳在边缘"的原始动机（Test 7C 钉住）。

**判据（Test 18 逐帧/逐级断言）**：视锥切片 8 个角点到方框边界的**最小余量**必须
大于采样可达半径

```
margin(texel)   = (1 − max|ndc_xy|)/2 · M        // ndc 由读侧 shadow_vp 投出
required(texel) = pcf_radius + 1.5m / texel_world   // 1.5 = SHADOW_NORMAL_OFFSET_MAX 硬上限
```

`1.5m/texel` 这一项是 normal-offset 的**悲观**可达半径：偏移量 = `clamp(strength·tanθ, 0, 1.5)`
（米），掠射面（θ→86°）会顶到 1.5m 上限；PCF 侧 Poisson 磁盘半径归一化到 1 ⇒ 最多
`1.0·pcf_radius` 个 texel。

**示例配置实测**（splits `{50,50,160,300}`、M=1024、B=`{0,16,16,32}`、pcf 1.5、
normal_offset 0.10m；4 相位 × 128 帧、2m/帧）

| 级联 | margin(texel) | required(texel) | slack |
|---|---|---|---|
| c1 | 23.2 | 13.0 | **1.8×** |
| c2 | 26.5 | 5.3 | 5.1× |
| c3 | 34.9 | 3.5 | 10.0× |

⇒ 安全但 **c1 只有 1.8 倍余量**（近景级联 texel 最细 ⇒ required 最大）。

**两条可迁移结论**：

1. **slack 与分辨率无关，只随级联世界半径 r0 变化**：`required ≈ 1.5·M/(2r0)`、
   `margin ≈ 0.13·M` ⇒ `slack ≈ 0.17·r0`。实测 M=256/512/1024/2048 的 slack =
   1.9/1.9/1.8/1.8×（Test 18 ② 扫描）。所以"把贴图开大"不会让 seam 更安全或更危险。
2. **配置约束：级联世界半径过小（r0 ≲ 6m）时 seam 变可达**（Test 18 ③ 用
   splits `{1,1,2,4}` 钉住该边界：margin 24~34 texel vs required 151~585 texel）。
   遇到这种配置要么下调 `normal_offset_world`、要么该级关横向滚动。**反过来也说明
   提高 `SHADOW_NORMAL_OFFSET_MAX`、加大 `pcf_radius`、把 c1 的切分距离调小都会
   吃掉余量**——Test 18 的源码契约已把 `SHADOW_NORMAL_OFFSET_MAX = 1.5` 钉住，
   改动它会直接失败并要求同步复核模型。

---

### 4.7 滚动缓存的**收益量化**与**整级↔条带对拍**（S5，2026-09-26）

**收益（示例 stats，1s 窗口平均）**：`[CSM Rolling Cache Stats] C1: 11 strips(band=16t
avg=0.29% off=(80,0) full=0) | C2: ... avg=0.10% | C3: ... avg=0.05%` ⇒ 滚动命中时
**每帧只重画贴图的 0.05~0.31%**（整级重建是 100%），`full` 计数同时区分"整级重建"
与"纯命中"（旧判据 `strips==0 ⇒ 100% Cached` 会把整级重建帧误报为命中）。

**对拍工具**（示例内，`CsmCacheDiff`；CPU 契约测不到 GPU 侧光栅化与 scissor 坐标系）：

```bash
CSM_CACHE_DIFF=1 CSM_AUTOWALK=24 ./CascadeShadowMap.exe   # 冻结动画 + 相机自动前进
# CSM_CACHE_DIFF_FREEZE=0 关冻结（用于自证"帧间内容在变"）
```

- **A** = 环形滚动帧（`offset≠0`）、**B** = `InvalidateMainLightStaticShadowCache`
  后的整级重建帧（`offset=0`）、**B2** = 紧接着的第二次整级重建。
- A 先按读侧映射 `phys=(layout+O) mod M` 拉回布局坐标系，再与 B 逐纹素比。
- 判读：`B vs B2` 必须 0（否则帧间内容在变、方法不可信）；差异再按
  **形状分类**（A缺 / A多 / 双方有几何）、**32×32 分布图**、**位移扫描**
  （x/y 各 ±24：最优位移 ≠ (0,0) ⇒ 错位；= (0,0) ⇒ 内容差异）判定；
  失败时落 `csm_cachediff_c<N>_{A,B,D}.bmp`（D=|A−B|×5，灰度=反 Z 深度）。

**2026-09-26 实测结论（未收敛）**：`B vs B2 = 0`；c2/c3 一致（1~2 texel、max|Δ|=1.25e-6）；
**c1 每轮不一致 400~1900 texel**（窗口 0.1~0.3%，max|Δ|≈0.4，集中在剪影处）。
已排除：动画伪影（从第 0 帧冻结可移动物体后差异不变）、整体错位（±24 扫描无命中）、
写侧平移量不准（`texel_size=2r/M` 与 ortho `∓radius` 严格一致 ⇒ 整数 texel 精确平移，
光栅化应平移不变）、两路径 caster 筛选不对称（全量路径对静态级联同为 `Static`）。
⇒ **根因待定位，最短路径是 RenderDoc 抓"条带帧 vs 整级重建帧"对比 draw/scissor 状态**
（见 backlog D5/S6）。在这条收敛前，别把"条带内容与整级重建等价"当作已证事实。

---

### 4.8 滚动缓存的**计数器**与两个读数陷阱（S5/S6 交界，2026-09-26）

**单调计数器**（控制器内累计、不重置；示例每秒打印一行 `[CSM Cache Counters]`）：

```
Upd=<Update 调用数> Inv=<InvalidateStaticCache 调用数> frames=<帧数> |
C1: fullC=<整级重建> stripC=<条带> hitC=<命中> | C2: ... | C3: ...
```

判据：`Upd == frames`（每帧恰好一次 Update）；`fullC + stripC + hitC == Upd`（每帧每级必落一支）；
`fullC ≥ Inv`（每次失效至少让每级各重建一次）；静置时 `stripC == 0 / hitC == frames`。

**陷阱 1：只显示"窗口最后一帧"的字段会骗你。** 旧 stats 的 `full=` 是末帧快照（60 帧里只有
2~5 帧整级重建，末帧多半是命中）⇒ 看上去像"失效了却从不重建"，实际 `fullC == Inv` 完全正常。
**凡"事件型"指标（整级重建/失效/跨格）一律用累计计数器差分，不要用最近一帧快照。**

**陷阱 2：环形偏移是 `uint32`，大数值是负数。** `off=(992,0)` = `-32`（一次反向跨格）、
`1008 = -16`、`976 = -48`；`off` 恒为 `band` 的整数倍 ⇒ 见到非整数倍（如 `922`）先怀疑读错行。

---

### 4.9 阴影 pass 的收集侧日志与 S6 差异排查（2026-09-26）

`CSM_PASS_LOG=1`（默认静默）打开两类逐 pass 日志：

```
[S6-PASS]    cascade=1 kind=full|strip mobility=0 off=(16,0) view_t=(..) rect=(0,0,16,1024) load_depth=1
[S6-COLLECT] shadow pass mobility=0 items=84 idsum=0x14820000 skipped(invisible=0 no_owner=0 no_transform=0)
```

`items`/`idsum` 是 `RenderPrimitiveCollectSystem` 收集末端统计的"产出图元数 + Σ(entity_index<<16|gen)"
⇒ **判断两帧是否收到同一批 caster**：相同 ⇒ 差异在光栅化侧；不同 ⇒ 差异在收集/剔除侧。

**读法要点**：
- `[S6-COLLECT]` 与紧随其后的 `[S6-PASS]`（或前一行）配对读——收集发生在 `RenderTo` 内部。
- `mobility=0` 是 Static（中远景静态缓存），`=1` 是 Movable（cascade 0 动态层）。
- `rect` 是**物理**坐标（含环形跨缝拆分）；`off` 是该级累加的环形偏移（uint32 ⇒ 大数=负数）。

**S6 已排除的两条**（都做了定量）：
1. 收集/剔除：条带帧 vs 整级帧 `items`/`idsum` 完全一致 ⇒ 不是 caster 集合问题。
2. 写侧矩阵：`P·(T·V)` 相对 `P·V` 的**位级残差 0.0001 纹素**；帧段内矩阵漂移 1.465e-07 相对
   （贴图边缘 0.0001 纹素）⇒ 都远不足以解释 ~3e2 纹素差异（见 Test 20）。

**S6 结论（结案）：原"对拍不一致"是诊断的相位假象，不是引擎缺陷。**
在相机**仍在行走**时读回 A，读到的深度图与随后读到的 `offset` 状态会跨帧（差一个跨格）
⇒ 整图被错误映射（签名：最优位移顶到扫描边缘且 `残余=0`＝纯整数错位）。
修正为"达到目标跨格数即停走 + 静置 5 帧再读回"后：**239 轮逐纹素 0 差异**（自查 B vs B2 也 0）；
再把每轮先行跨格数递增（1,2,3,…，封顶 70）扫过环形偏移 ⇒ **77 轮全 0 差异**，offset 覆盖
16→976（几乎整圈），其中最后若干轮累计 1120 texel ⇒ **已发生环形回绕**，回绕后同样逐纹素一致。
例程开关：`CacheDiff::pending/settle`（停走+静置）、`walk_target/walk_done`（逐轮递增跨格数）。

**⚠ 对拍/读回类诊断铁律**：读回 GPU 资源与读引擎状态必须在**状态静止**的同一样本上做，
否则行走进程中取样会得到假差异（本次差点据此误改引擎的清除/矩形逻辑）。

---

### 4.10 附件读回与落盘：下沉引擎 API + CM2D 写出（E1，2026-09-26 落地）

引擎侧新增 `inc/hgl/vk/VKTextureReadback.h` + `src/Vulkan/VKTextureReadback.cpp`（`hgl::graph`，已注册进
`src/Vulkan/CMakeLists.txt`）：

```cpp
bool ReadbackTexture(VulkanDevice *device, Texture *tex,
                     std::vector<uint8_t> &out_pixels, TextureReadbackInfo *out_info = nullptr);
bool ReadbackColorTarget(IRenderTarget *rt, std::vector<uint8_t> &out, uint32_t color_index = 0,
                         TextureReadbackInfo *out_info = nullptr);
bool ReadbackDepthTarget(IRenderTarget *rt, std::vector<uint8_t> &out, TextureReadbackInfo *out_info = nullptr);
```

- 内部：`vkQueueWaitIdle` 排空 → staging（`BufferAllocPolicy::Readback`）→ `TextureCmdBuffer`
  一次性 barrier（当前布局 → `TRANSFER_SRC_OPTIMAL`）→ `CopyImageToBuffer` → barrier 还原 →
  提交 + fence 等待 → map → 释放。**读回前后布局不变**；aspect 取 `Texture::GetAspect()`
  （纯深度格式不声明 STENCIL 位，与渲染侧规则一致）。
- 像素是**原始字节、行主序自上而下**（与 Vulkan 图像坐标一致 ⇒ 落盘无需翻转）；每像素字节数 =
  `GetStrideByFormat(格式)`；`TextureReadbackInfo` 回传 w/h/pixel_size/row_pitch/format/is_depth。
- 只用于**离线/诊断**（每次调用排空队列 + 新建 staging/cmd）⇒ 不进每帧热路径。

**配套引擎修复（关键）**：`RenderCmdBuffer::BeginRendering` / `EndRenderingPresent` 现在把每种转换的
`newLayout` 同步写回纹理跟踪（`Texture::SetImageLayout`）。此前 `EndRenderingPresent` 只在 barrier 里把
交换链颜色图转到 `PRESENT_SRC_KHR`，纹理上跟踪的值却停在 `SHADER_READ_ONLY_OPTIMAL` ⇒ 任何拿
`GetImageLayout()` 当 `oldLayout` 的代码都会触发校验层"非法转换"且拷贝不可信（原示例因此**硬编码**
`PRESENT_SRC_KHR`）。另：`VKBindlessTextureManager` 对不可采样布局（PRESENT_SRC/附件布局）回落
`SHADER_READ_ONLY_OPTIMAL` 注册——布局跟踪变准后不能把非法布局写进采样描述符。

**示例侧落盘改 CM2D**：`bitmap::SaveBitmapToTGA(&out, rgb, w, h, 3, 8)`（`hgl/2d/BitmapSave.h`）⇒
手写 BMP 头 + 底行翻转全删。CM2D 写的是 **UPPER_LEFT（行序自上而下）**，与读回顺序一致（BMP 是底行在前）。
示例只保留"取哪张图 + 怎么判读"：ATS `DumpCascadeDepth`/`DumpColorTarget`、CSM `ReadbackCascadeDepth`/`SaveDepthTga`。

**⚠ 已知缺口（读回交换链颜色图）**：present 之后交换链图**不再被 acquire**，从帧外提交它的布局转换违反
"presentable image 必须在 acquire 与 present 之间使用" ⇒ 校验层报
`vkQueueSubmit(): performs a layout transition on presentable VkImage ... has not been acquired`。
该 VUID **改造前就存在**（旧代码同样转这张图，且实测拷贝内容有效：D2/D3 契约数值与改造前逐位一致）。
彻底消除需帧内做（acquire 后 / present 前）或用 acquire+copy+present 的截图路径。

**E1 验收数据（与手写回读版本逐项相同 ⇒ 改的是通路不是数据）**：
`ATS_SELFCHECK=1` rc=0；`[D1-CONTRACT] c0 PASS bbox=112x58 filled=3740 填充率 57.6%`；
`[D3-CONTRACT] receive_shadow 18189 px / 反向 0 px`、`bias ×1000 600662 px`；
`[DepthDump] 1024x1024`、`[ColorDump] 1280x720 mean_lum=113.3/112.3/134.0`；
`CSM_CACHE_DIFF=1 CSM_AUTOWALK=24` 74 轮 `不一致=0`。
契约测试 **Test 21**（源码契约）：三入口 + `CopyImageToBuffer`/`vkQueueWaitIdle` + CMakeLists 注册 +
`Begin/EndRendering` 的 `SetImageLayout` 同步 + 示例零自研残留（`SaveMemoryToFile`/`vkCmdCopyImageToBuffer`
必须消失）。破坏验证：移除 `SetImageLayout(PRESENT_SRC_KHR)` ⇒ rc=21；移除 CMakeLists 注册 ⇒ rc=21；恢复 ⇒ 21 Passed。

**落盘命名约定（2026-09-26 用户裁定"走 A 方案 + 文件名写清楚宽高和格式"）**：诊断文件名一律
`<stem>_<W>x<H>_<tag>.<ext>`：

| tag / ext | 含义 | 说明 |
|---|---|---|
| `_f32.raw` | **裸 float32 全精度** | 零转换（就是回读到的 GPU 字节），行紧排、小端；numpy：`np.fromfile("cascade_depth_c0_1024x1024_f32.raw", dtype="<f4").reshape(1024,1024)` |
| `_r8.tga` | 8bit **单通道**灰度可视化副本 | CM2D `channels=1` ⇒ image_type=3 / 8bpp（1MB/1024²，比 3 通道省 2/3） |
| `_<引擎格式名>.raw` | 颜色附件原生打包 | 本管线是 `A2BGR10UN`（10bit/通道打包进 32bit），零转换 |
| `_<引擎格式名>_low8x3.tga` | 颜色的低 3 字节截断视图 | 只有人眼看形态的用途，**不是**精确数据 |

**⚠ 颜色目标不是 8bit**：交换链颜色图实测 `VK_FORMAT_A2B10G10R10_UNORM_PACK32` ⇒ 旧 8bit BMP/灰度
分析一直在**截断低 2bit**（D3 契约数值因此不变、仍可用，但精确分析必须读 `.raw`）。这也解释了为什么
"低 3 字节均值"能当亮度代理：与通道序无关，且截断对三通道是等比例的。

**独立验证（Python 复算，完全不经过 C++ 代码）**：
- `cascade_depth_c0_1024x1024_f32.raw`（4194304B）复算 = filled **3740** / bbox **112x58** / **57.6%**
  ⇒ 与 D1 契约值逐项相同；
- `cascade_depth_c0_1024x1024_r8.tga` 头解析 = type 3 / 1024x1024 / 8bpp / upper-left，且与 f32 量化
  **逐像素 0 不一致**；
- `ats_d3_A_receive_on_1280x720_A2BGR10UN.raw`（3686400B）复算 mean_lum = **113.3** = 日志值
  ⇒ `.raw` 与判定用的是同一份字节。

CSM 侧 `csm_cachediff_c*_{A,B,D}` 同样落 `.raw`（D 标 `f32x5`）+ `_r8.tga`；该分支**只在"平坦区真有差异"
时触发**，S6 结案后差异恒 0 ⇒ 本会话未能实测触发（历史上有真实差异时它确实产出过 `csm_cachediff_*.bmp`）。

---

## 5. 背面渲染与 bias 极性

**阴影贴图渲染模型背面**（`req.cull_mode = CullMode::Front`）：贴图里存的是物体背光侧
的深度，比正面更远，从而避开自身共面 acne。引擎**不做隐式推断**，必须显式声明剔除正面。

受光判定：`ref = light_ndc.z + bias`，采样器 `compare_op = GreaterOrEqual`，
reversed-Z 下"值越大越靠近光源"。因此：

| bias | 效果 |
|------|------|
| 正 | 阴影朝光源方向推 ⇒ 接触点/地面被判为受光 ⇒ **漏光、peter-panning** |
| 负 | 接收者深度朝物体背面方向拉 ⇒ **阴影贴合、接触点变实**；过大会出现半影光晕 |
| `0` | 背面渲染下明显漏光 |

背面渲染的贴合量约等于遮挡体沿光轴的厚度在光空间里的占比，所以取负值。

### 5.1 逐级联 bias（为什么一个 `bias` 不够）

`bias` 是**归一化深度**偏移，它的世界效果 = `bias × 该级联的深度范围`。而各级联的深度
范围差异极大（示例实测 `[346 349 619 953]m`，远近相差 **2.75 倍**）：同一个归一化 bias
在超远距级联上会放大成 2.75 倍的世界偏移 ⇒ **按近景调好的"贴合"取值套到远景就变成
半影光晕，反之远景调好了近景又会漏光**。

`CascadedShadowConfig` 提供两种修正方式，按需选一：

| 字段 | 语义 | 说明 |
|------|------|------|
| `bias_world` | **世界单位偏移（米）**，非 0 时生效 | 逐级按各自深度范围自动换算 `bias_c = bias_world / depth_range_c`；全场景世界偏移恒定，只剩一个直觉旋钮。**推荐用法**，且会压过 `per_cascade_bias_scale` |
| `per_cascade_bias_scale[4]` | 逐级乘数，默认全 1 | 精细微调用，`bias_c = config_.bias * scale[c]`。`bias_world != 0` 时被忽略 |
| `bias` | 归一化基准值 | 仅在 `bias_world == 0` 时生效（历史行为，控制器默认 `0.002f`） |

解析结果每帧由 `Update()` 写进 `CascadeUpdateResult`，便于上层换算/打印：

- `depth_range` = 该级正交投影的深度范围（`zfar - znear`，米）
- `resolved_bias` = 该级真正写入 UBO 的归一化值，即 `cascades[c].shadow_params.x`

示例 `CascadeShadowMap.cpp` 用 `bias_world = -0.20f`，`[`/`]` 按 **0.05m** 步长调米数，
并打印逐级归一化值与逐级深度范围，可直接看到"归一化不同、世界偏移相同"。

> 注意：`CascadeUpdateResult` 只有 `sphere_radius`/`depth_range` 这类**纯读**数据可用于
> 外部换算。**不要在 `TuneShadowBias()` 之类的调参回调里再调一次 `csm_controller.Update()`
> 取数**——`Update()` 会提交 `cache_states_[c].snapped_origin`/`along_anchor`/`generation`，
> 额外的调用会让下一帧以为"贴图已按新中心重绘"，实际贴图里还是旧内容 ⇒ 静态阴影整体滑动。

**世界偏移折算**：`世界偏移 = bias × depth_range`，`depth_range = 2*radius + 2*margin + anchor_step`。
`radius` 可用 `CascadeUpdateResult::sphere_radius` 取（它等于 `texel_size * map_size * 0.5`，
已包含横向锚定的半径补偿，但**不含** margin 与 anchor_step 部分）。

运行时 `[`（更贴合）/ `]`（更漏光）现场微调。改 bias **不需要**重建级联缓存
（只改接收者侧比较基准，滚动缓存里的深度完全有效）。

> 已知缺口：引擎**没有** `vkCmdSetDepthBias`（`VKPipelineResolver` 的动态状态列表缺
> `VK_DYNAMIC_STATE_DEPTH_BIAS`），所以没有 slope-scaled 硬件 bias。"阴影在陡峭表面上
> 出现条纹"这类问题只能靠调大 `pcf_radius` 或整体 bias 缓解（这部分已由 §5.2 的
> 法线偏移补上，见下）。

### 5.2 法线偏移（normal-offset）

固定深度 bias 与入射角**无关**：为掠射面（法线与光线夹角 θ→90°）调到能压住 acne，所有
正面就一起被推离遮挡体 ⇒ peter-panning。而 acne 的深度误差本身正比于 `tan(θ)`（表面在
一个纹素跨度内沿光轴的高度变化），所以缺的是**按角度加权**的偏移。

实现集中在 `ShaderLibrary/shadow/pcf_shadow.glsl`：

| 项 | 值 / 位置 |
|----|-----------|
| 强度 | `shadow.cascades[0].shadow_params.w`（**世界单位米**，0 = 关闭） |
| 编译期总开关 | `HGL_SHADOW_NORMAL_OFFSET`（0/1，由 ShaderGen 注入，见 §6） |
| 公式 | `offset = min(strength * (sin_theta / cos_theta), SHADOW_NORMAL_OFFSET_MAX)`；`sample_pos = world_pos + N * offset` |
| 上限 | `SHADOW_NORMAL_OFFSET_MAX = 1.5`（米，挡 `tan` 在掠射角发散） |
| 早退 | `strength <= 0` / 法线零长 / `cos_theta <= 1.0e-3`（背光面本就在阴影里，偏移无意义且 `tan` 发散） |
| 配置字段 | `CascadedShadowConfig::normal_offset_world`（默认 `0.0f`，示例 **0.10m**） |

四条硬约束：

1. **只用几何法线**（`SurfaceInput::worldNormal`），不要用着色法线。normal map 的扰动只改
   外观不改轮廓，拿它偏移会在法线花纹上抖出噪点。
2. **光方向**取 `sky.sun_direction`（它**指向光源**，正是公式要的 `L`）。若某场景把 CSM 光
   与 `sun_direction` 驱动成两个不同方向，把强度置 0 或关掉宏。
3. **只在采样时偏移，选级必须用真实位置**：`EvalPCFShadowAt(worldPos, selectPos)` 的双入参
   就是为此而拆的。`view_depth` 与 `split_far` 的比较若用偏移后的位置，片元会被推过 split
   边界 ⇒ 级联接缝会闪出错误的一级。
4. **UBO ABI 零改动**：`ShadowCascadeInfo` 布局（`static_assert(== 176)`）不变，强度借用原本
   unused 的 `shadow_params.w`。控制器把**同一个值写进每一级**（逐级不同会让将来"按级调法线
   偏移"的尝试拿到错值且无法解释），shader 读级联 0。

**与 bias 的分工与调参顺序**：两者互补而非替代。先把 `normal_offset_world` 调到掠射面无
acne，再把 `|bias_world|` 往回收（bias 越大越漏光、越小越贴合）。示例配
`normal_offset_world = 0.10m` + `bias_world = -0.20m`。

**强度取值的量级参考**：acne 的深度误差量级 ≈ 一个纹素的世界尺寸（示例 CSM 0 半径约
188m / 1024 texel ⇒ ≈0.37m），经实机微调确立 0.10m（消掠射角 acne 且接触点不悬浮）。

运行时 `-`（减小）/ `=`（增大）按 **0.05m** 步长微调（范围 `[0, 4]`）。改的是纯配置，
**不需要**重建级联缓存（与 §5.1 的 `[`/`]` 同理）。

---

### 5.3 接收侧旋钮：`receive_shadow` / `bias_multiplier`（D3 落地，2026-09-26）

`ShadowComponent` 的两个接收侧旋钮是**逐图元**着色决策（同一批次里不同物件可以不同），
所以它们不跟材质、也不跟批次走，而是随 **per-draw 行**到达片元着色器。

| 项 | 值 / 位置 |
|----|-----------|
| 载体 | `MaterialInstanceAddresses`（per-draw 行，`inc/hgl/graph/ShaderBufferSources.h`）：**8B → 16B**，新增 `shadow_flags`（bit0 = 不接收）与 `shadow_bias_multiplier` |
| 唯一真源 | `HGL_MATERIAL_INSTANCE_ADDRESSES_FIELD_LIST`（X 列表）：CPU struct 与 GLSL struct 发射**同源遍历**，新增/改名/调序字段只改一处，没有手写漂移面 |
| 写入端 | `PrimitiveBatchPipeline` 逐图元读 `CanReceiveShadow()` / `GetBiasMultiplier()` 写行 |
| 读取端 | `pcf_shadow.glsl` 的 `GetShadowReceiveParams(data_index)` → `GetShadowFactor(surface, data_index)` |
| 零值语义 | 行内倍率 `0` = "不调节"（回落 1.0）；未挂载 `ShadowComponent` 的行全零 = 接收 + 倍率 1.0，与接线**前逐字节一致** |
| 不接收 | `shadow_flags` bit0 → `GetShadowFactor` 直接返回 `1.0`（**不采样**，零成本早退） |
| 倍率 | 逐级乘进 `EvalCascadePCF` 的深度 bias（`shadow_params.x * bias_scale`）与 §5.2 的法线偏移强度；沿 `EvalPCFShadowAt → EvalCascadeChain → EvalCascadeShadowAt → EvalCascadePCF` 逐点穿线 |

四条硬约束：

1. **只影响接收者，不影响投射者**：`receive_shadow` 管的是"这个物件**自己**算不算影子"；
   想让它不投影子用 `cast_shadow`（走 caster 收集，另一条链）。
2. **选级必须用真实位置**：倍率只进 `EvalPCFShadowAt(worldPos, selectPos, bias_scale)` 的
   采样与偏移，**不能**改 `selectPos`——否则片元被推过 split 边界，级联接缝会闪出错误的一级
   （同 §5.2 约束 3）。
3. **不要下沉到材质**：它是逐图元数据（`ShadowComponent`），不是材质业务数据也不是逐批共享量；
   写进材质会让同材质的不同图元无法各自开关。
4. **行结构改宽必须自动校验**：行大小断言要由字段列表**推导**（遍历求 sizeof），
   别手写 `sizeof(MaterialInstanceAddresses) == 16`——加字段就漏改。

**取证（两条命令给出结论）**：

```
ATS_SELFCHECK=1 ./build/out/Windows_64_Debug/AlphaTestShadow.exe              # D1 契约 + D3 三帧逐像素对照
ATS_SELFCHECK=1 ATS_D3_NOKNOB=1 ./build/out/Windows_64_Debug/AlphaTestShadow.exe   # 对照组：不拨旋钮 ⇒ 必须 0 px 变化
```

必须**两条都过**才说明差异来自旋钮（对照组是噪声底断言）。实测：
`[D3-CONTRACT] receive_shadow PASS: 受影→未受影(变红) 18189 px / 未受影→受影 0 px`、
`bias_multiplier PASS: 倍率×1000 后外观变化 600662 px`、对照组 `0 px`。

> **口径坑**：判据必须按**通道**（"是否变红"），不能用亮度。`AlphaTestShadow` 的地面是
> 饱和红（R≈198,G≈1,B≈31），影子压上去偏灰蓝——红的 RGB 均值**反而低于**灰影，
> 用"变亮/变暗"会得出反方向结论。
> 另：交换链颜色图提交时点的真实布局是 `PRESENT_SRC_KHR`，而
> `Texture2D::GetImageLayout()` 停在 `SHADER_READ_ONLY_OPTIMAL`；拿它当 oldLayout
> 会被校验层判非法转换（拷贝内容不可信）。

---

## 6. Poisson PCF 开关

`ShaderLibrary/shadow/pcf_shadow.glsl` 顶部：

```glsl
#ifndef HGL_SHADOW_PCF_POISSON_TAPS
#define HGL_SHADOW_PCF_POISSON_TAPS 16     // 默认 16，上限 32；0 = 退回 3x3
#endif
```

- `> 0`：走 32 点 Poisson 磁盘（取前 N 个），并用 `ShadowPoissonPhase(gl_FragCoord.xy)`
  做每像素相位旋转 ⇒ 采样点错开，**既软化阴影又显著抑制重复图案**。
- `== 0`：退回 3×3 矩形采样。
- 由 `FragmentTemplateComposer` 在生成 GLSL 时发射（`#define` 落在 `{{defines}}` 块，
  早于 `{{module_includes}}`，所以 `pcf_shadow.glsl` 的 `#ifndef` 会取到该值）；
  输入字段 `ComposerInput::shadow_pcf_poisson_taps`（`FragmentTemplateComposer.h`，默认 16）。
  **只有模板真的带 ShadowProvider 槽时才发射该宏。**

同一位置还会发射法线偏移的总开关（见 §5.2）：

```glsl
#define HGL_SHADOW_NORMAL_OFFSET 1        // 0 = 整段法线偏移代码不参与编译
```

输入字段 `ComposerInput::shadow_normal_offset`（默认 1，合法值 0/1）。**宏管"有没有这段
代码"，`shadow_params.w` 管"用多大劲"**——两者都到位才算接通：只在运行时把强度调大、而宏
是 0，等于没有这段代码。

开启 Poisson 是本模块历史上"近距离静态物件不再丢失阴影"的最后一环（配合 3.4 的矩阵修复）。

---

## 7. 配置速查（`CascadedShadowConfig`）

| 字段 | 默认 | 说明 |
|------|------|------|
| `cascade_count` | 4 | 1..`kMaxShadowCascades`(4) |
| `split_lambda` | 0.85 | Practical split 权重（0 线性，1 对数） |
| `max_distance` | 250 | 阴影最远距离（决定 `far_z`） |
| `split_distances[]` | {15,45,100,250} | 每级 `split_far`（示例：{50,50,160,300}） |
| `use_custom_splits` | true | 优先用自定义切分 |
| `c0_dynamic_overlay` | false | **动静分层开关**（示例开） |
| `shadow_map_size` | 1024 | 贴图边长 |
| `caster_depth_margin` | 100 | 光源视锥沿 -Z 余量（示例 120） |
| `bias` | 0.002 | 归一化深度 bias；仅 `bias_world == 0` 时生效 |
| `bias_world` | 0 | **世界单位偏移（米）**，逐级自动换算，非 0 时压过上面两项（示例 **-0.20**） |
| `per_cascade_bias_scale[4]` | 全 1 | 逐级 bias 乘数（§5.1） |
| `normal_offset_world` | 0 | **法线偏移强度（米）**，按 `tan(θ)` 加权，0 = 关闭（示例 **0.10**，§5.2） |
| `pcf_radius` | 1.5 | PCF 采样半径（texel 倍数） |
| `darkness` | 0.12 | 全阴影时的最暗因子（示例 0.15） |
| `blend_width` | 0.05 | 比例：末级 `max_distance` 边缘淡出带 + 动态层 CSM 0 边界淡出带（占本级深度区间） |
| `blend_distance` | 1.5 | **相邻级联交界带宽度（世界单位米）**，写进 `cascade_params.w`；只做取暗叠加、近级不做淡出；0 = 硬切换（§2.y） |
| `cache_anchor_step` | 16.0 | **沿光轴**深度锚定步长（米）；0 = 禁用（缓存深度会随相机漂移） |
| `cache_scroll_band_texels[4]` | `{0,16,16,32}` | **横向**锚定步长（**texel**，逐级）；世界步长 `L_c=2·B·r0/(M−1.416·B)` 与半径补偿由 B 派生；0 = 该级禁用。c0（动态层）恒为 0 |

两个 `anchor_step` 只用在中远景静态级联（`c > 0`），CSM 0 传 `0.0f`（它本来每帧全量重绘）。

**改 `cache_scroll_band_texels` 的影响面**：B 变大 ⇒ 世界步长变大 ⇒ 重绘更少，但半径补偿
`0.708·L` 同步变大 ⇒ texel 更粗（比例 `1.416·B/(M−1.416·B)`，与分辨率无关；`Test 5B-2`
用 B=600 做"补偿与半径同量级"的压力测试）。**不要用"图的比例"（1/8、1/4）当更新间隔**：
1/8 图 = B=128 ⇒ 精度损失 21.5%（近景级联尤其明显）；1/4 的正确用途是**上限阈值**——
偏移超过 1/4 图时放弃条带、直接整级重建（覆盖瞬移/传送，待 S3 落地）。

---

## 8. 契约测试

`src/ecs/support/TestCSMIncrementalPass.cpp`（target 名 = 文件名，输出到
`build/out/Windows_<arch>_<cfg>/TestCSMIncrementalPass.exe`，必须从仓库根运行）：

| 测试 | 契约 | 期望输出 |
|------|------|----------|
| Test 1 | `RenderPassRequest` 默认值 | `Test 1 Passed` |
| Test 2 | `RenderPassOptions` 配置 | `Test 2 Passed` |
| Test 3 | `Mobility` 与 `CullMode` 枚举和 `VK_CULL_MODE_*` 数值对齐 | `Test 3 Passed` |
| Test 4 | 脏矩形 → `RenderPassRequest`（scissor）翻译 | `Test 4 Passed` |
| **Test 5A** | 重绘预算：600 帧横向行走，c0 必须 600/600，c1..3 允许 ≤10% 全量 | `[CSM-CACHE] full=[600,33,32,34] band=[0,0,0,0]` |
| **Test 5B-1** | **矩阵恒定性**：固定朝向平移 400 帧，缓存命中帧与最近一次重绘的 `light_proj*light_view` 逐元素差 ≤ `1e-3`，且 CSM1/2/3 都至少命中一次 | `[CSM-COHERENCE] frames=400 hits=[377,377,380] max_delta=0.000000 ... fail=0` |
| **Test 5B-2** | **冻结窗口覆盖率**：薄切片 + B=600 texel（≈10.6m，补偿与半径同量级）压力配置，8 位置 × 16 朝向共 128 帧，用**冻结矩阵**判定角点 NDC | `[CSM-COVERAGE] band=600texel(~10.6m) frames=128 fail=0 worst_ndc=[...]` |
| **Test 5C** | **原地旋转下的矩阵恒定性**：相机位置固定、`viewDirection` 绕圈 240 帧，缓存命中帧的 `light_proj*light_view` 必须与最近重绘帧逐元素相同；同时断言扫描确实产生命中帧、且拟合半径不随朝向变化（半径若随朝向变 ⇒ 贴图被逐帧缩放） | `[CSM-SPIN] frames=240 hits=347 max_delta=0.000000 max_radius_delta=0.000061m fail=0` |
| **Test 6A** | 逐级联 bias 回归：默认配置（`bias_world=0`、scale 全 1）必须让 4 级写同一个 `bias`，且 `depth_range` 全为正、各级确实不同 | `[CSM-BIAS] default normalized=[...] depth_range=[346 349 619 953]m` |
| **Test 6B** | `per_cascade_bias_scale=[1 2 3 0.5]` 必须逐级写进 `shadow_params.x`，且 `.y/.z` 不被带偏 | `[CSM-BIAS] per-cascade scale=[1 2 3 0.5] -> normalized=[...]` |
| **Test 6C** | `bias_world=-1.15` 时归一化值必须**逐级不同**，但 `normalized × depth_range` 必须**恒定**且等于 `bias_world` | `[CSM-BIAS] bias_world=-1.15m ... world=[-1.1500..-1.1500]m (constant)` |
| **Test 6D** | `bias_world != 0` 必须压过 `per_cascade_bias_scale`（优先级契约） | `Test 6D Passed` |
| **Test 7A** | `normal_offset_world` 逐级写进 `shadow_params.w`：默认配置（结构体默认 0）必须逐级为 0（不擅自改变历史行为）；显式配置后逐级同值且镜像到单级回退字段 | `[CSM-NORMAL-OFFSET] default strength=0.00m ...` / `configured strength=0.35m on [0.350000 ×4] (mirror=0.350000)` |
| **Test 7B** | **正交性**：`bias_world` 与 `normal_offset_world` 同时开，逐级世界 bias 仍恒为 `bias_world`、`w` 仍等于配置值（各写不同分量，互不干扰） | `Test 7B Passed` |
| **Test 7C** | **shader 源码契约**（读真实文件，10 项 `Contains`）：编译期宏 / 偏移辅助函数 / 宏守卫 / `shadow_params.w` 读取 / `sin_theta / cos_theta` / 背光早退 / `tan` 上限 clamp / `EvalPCFShadowAt(sample_pos, surface.worldPos, receive_params.bias_multiplier)` 选级分离 / **`shadow.cascades[selected].shadow_tex.x == 0u` 屏蔽级联不降级（§2.x）** / **`cascade_params.w` 交界带取自控制器写入字段（§2.y）** | `Test 7C Passed: shader source contract holds (10 checks, 18970 bytes)` |
| **Test 11** | masked caster 链源码契约（10 条 needle，见 §4.5） | `Test 11 Passed: ...` |
| **Test 12** | pipeline 键内容化（SPIRV 内容 hash 登记/消费/注销 + 2 条禁复活） | `Test 12 Passed: ... (7 checks)` |
| **Test 13** | 阴影跳过路径告警与收敛（一次性告警/上限/降频/清零 + 2 条禁刷屏） | `Test 13 Passed: ... (7 checks)` |
| **Test 15** | **静态物件运行期写入留痕**（11 源码 + 5 行为）：八条写入路径都留痕、同值写也告警且只告警一次、未 arm（无静态变更）不告警、`Movable` 不告警（§D4） | `Test 15 Passed: ... (11+5 checks)` |
| **Test 16** | **横向锚定步长是 texel 口径**（S1）：独立量测 `texel(B)` 与 `texel(0)` 反解 `L`，断言 `L/texel ≡ B`（整数 texel 量子）、精度损失 `= 1.416·B/(M−1.416B)`、c0 不受 B 影响、`B ≥ M/1.416` 退化时 fail-safe 回禁用、B 单调；源码 needle 2 条 + 禁复活世界米字段 | `[CSM-BAND] B={0,16,16,32} texel(bare)=[0.03819 ...] loss=[2.26% 2.26% 4.63%]` |
| **Test 17** | **环形滚动的坐标契约**（S2，主用例 3 条纯 CPU 不变式 + 子用例 (e) 跨缝）：① 物理不动（同一世界点物理 uv 跨格不变）② 写读一致（`vp_draw` 投出的 uv ≡ `fract(uv+O·texel)`）③ 整级重建必清偏移 ④ 新暴露内容必须落在 `dirty_rects` 并集内（探针由当前框几何生成、是否"新暴露"由**上一帧矩阵**独立判定）⑤ 面积契约（纯滚动帧沿主轴各段宽度之和 == 该轴位移量，咬住 1 纹素截短/多画）⑥ 非整步回落（`cache_anchor_step=0` 档）+ 空跑守卫（跨格/覆盖检查/命中帧/反向跨格计数） | `[CSM-SCROLL] frames=400 crossings=[349,207,44] 条带覆盖检查=1602 次 命中帧=1135 反向跨格=268 次` + `[CSM-SEAM] B=24 M=1024 覆盖检查=2577 次 跨缝拆分=8 次 反向跨格=297 次` |
| **Test 18** | **环形读侧的接缝可达性**（S4）：视锥切片 8 角点到方框边界的余量 `(1−max\|ndc\|)/2·M` 必须 > `pcf_radius + 1.5m/texel_world`；只统计 **wrap 真生效**（`cache_offset≠0`）的帧并要求每级 ≥4 帧（防空跑）；② 分辨率扫描 M=256..2048 断言 slack 不随 M 恶化；③ 极窄级联（splits `{1,1,2,4}`）必须**越过**边界（证明断言非恒真）；④ shader 源码契约 5 正 + 2 禁复活（`fract(shadow_uv)`、`HGL_SHADOW_TOROIDAL`），并钉住 `SHADOW_NORMAL_OFFSET_MAX = 1.5` | `[CSM-SEAM-MARGIN] margin=[23.2,26.5,34.9] required=[13.0,5.3,3.5] slack=[1.8x,5.1x,10.0x] wrap_frames=[459,381,308]` |：行结构唯一真源 X 列表 + 行大小自动推导 + 发射端遍历列表 + 写入端取 `CanReceiveShadow/GetBiasMultiplier` + 片元端 `GetShadowReceiveParams`/不接收早退/倍率乘进 bias 与法线偏移 + 3 条**禁复活** needle（`sizeof(MaterialInstanceAddresses) == 8`、发射端 `uint payload_index` 手写、`EvalPCFShadowAt(sample_pos, surface.worldPos)` 无倍率调用） | `Test 14 Passed: shadow receive-side knob contract holds (22 checks)` |

### 写这类断言的两个硬要求

1. **5B-1 必须固定朝向、只平移相机**。包围半径来自 `max(8 角点到 sphere_center 距离)`，
   朝向固定时该几何量只随位置平移 ⇒ 半径恒定，矩阵恒定性才有意义。**若同时改朝向，
   半径会变 ⇒ 矩阵必然变，与锚定是否生效无关，断言会误报。**
2. **5B-2 必须用"冻结矩阵"而不是当前帧矩阵**判定覆盖率。贴图里的深度是按
   **该级最近一次重绘时**的矩阵写进去的，冻结窗口只对那一刻的半径/窗口有意义。
3. **5C 必须固定相机位置、只旋转朝向**。旋转改变的只是切片朝向；配合横向锚定，拟合半径
   在这个测试配置里实测 **span 完全为 0**（`radius=[60.120..60.120]`），所以"矩阵逐元素恒定"
   是有意义且可达成的强断言。若把相机一起平移，`along` 跨格会触发重绘、断言失去意义。

### 反证有牙性（必做）

写完断言必须故意破坏实现、确认它**会失败**，否则那只是"打印"不是"测试"。
两个已验证的反证：

| 破坏方式 | 结果 |
|----------|------|
| `radius += lateral_anchor * 0.708f` → `* 0.0f` | 5B-2 `fail=1865`、`worst_ndc` 涨到 `4.02`、`EXIT=1` |
| 3.1 第 8 步 `(snapped_cx - cx0)` → `(snapped_cx - cx)` | 5B-1 `fail=1032`、`max_delta=0.0328`、`EXIT=1` |
| `shadow_params.x` 写 `update_res.resolved_bias` → 写 `config_.bias` | 6B 停在 cascade 1（`-0.002 != -0.004`）、`EXIT=6`；6A 仍绿（回归契约本就该绿） |
| 6C 的换算分母 `update_res.depth_range` → `out_updates[0].depth_range` | 6C 报"normalized bias identical on every cascade"、`EXIT=6`；6B 仍绿 |
| 6D 的 `bias_world / depth_range` 再乘 `per_cascade_bias_scale[c]` | 6D 报 cascade 1 world `-5.6 != -0.8`、`EXIT=6`；6B/6C 仍绿 |
| `shadow_params.w` 写死 `0.0f`（丢掉 `config_.normal_offset_world`） | 7A 报 cascade 0 `w = 0.000000, expected 0.350000`、`EXIT=7` |
| `normal_offset_world` 的结构体默认值 `0.0f` → `0.35f` | 7A 默认分支报 `w = 0.350000, expected 0`、`EXIT=7`（**它守的是"历史行为不被悄悄改掉"**） |
| `shadow_params.w` 写成 `(bias_world == 0) ? normal_offset_world : 0`（两特性互斥） | 7B 报 `cascade 0 normal offset 0.000000 lost while bias_world is active`、`EXIT=7`；7A 仍绿 |
| shader 删掉 `#define HGL_SHADOW_NORMAL_OFFSET` | 7C 报 `compile-time switch is missing`、`EXIT=7` |
| shader 的 `sin_theta / cos_theta` → `1.0` | 7C 报 `offset is no longer tan(theta) weighted`、`EXIT=7` |
| shader 的 `EvalPCFShadowAt(sample_pos, surface.worldPos)` → `(sample_pos, sample_pos)` | 7C 报 `offset must not leak into cascade selection`、`EXIT=7` |
| 5C 把 `cache_anchor_step` 从 `16` 改成 `0`（关掉沿光轴锚定） | **5C 报 `light matrix drifted on 219 cache-hit frames while rotating in place (max delta 0.010181)`、`EXIT=1`**；此时横向锚定仍生效所以仍会"命中缓存"，但矩阵随相机连续滑动 ⇒ 静态阴影在拖拽中整体滑动。**这是真实的配置陷阱**，见 todo `anchor-zero-slide` |

（注意：第一行反证**不会**让 5B-1 失败，第二行反证**不会**让 5B-2 失败——两个契约各管一段，
这正是它们必须并存的原因。）

---

## 9. 故障诊断表

| 症状 | 首查 | 常见根因 |
|------|------|----------|
| 静态物件**近距**没有阴影 | `c0_dynamic_overlay` 与 `split_distances[1]` | CSM 1 的 `split_near` 不是 `znear`；或 CSM 0 收不到静态物件而 CSM 1 不覆盖近距 |
| 相机移动时静态阴影**整体滑动** | 3.1 第 8 步的基准 | 用了 `cx` 而不是 `cx0`（§3.4） |
| 静态级联**每帧全量重绘** | `[CSM Rolling Cache Stats]`、`cache_scroll_band_texels` | 横向锚定被禁用 / 该级 `B=0` / 被 3.4 的缺陷抵消；或某 Static transform 每帧被重复 set 同值（A3 链每帧失效，搜 `invalidating static cascade` 日志定位调用方） |
| **远处地面**不再接收阴影 | `along_anchor_` 是否在变、`cache_anchor_step` | 沿光轴锚定失效 ⇒ 缓存旧深度被新矩阵解释 |
| 相机抬高/俯仰后**一片地面**无阴影 | `caster_depth_margin` | `zfar` 不够，地面深度被裁 |
| 阴影**边缘一圈没有阴影** | `worst_ndc`、半径补偿 | `0.708·L` 补偿缺失或不匹配 `round`/`floor` 选择（L 由 `cache_scroll_band_texels` 派生） |
| 相机移动时静态阴影**整体滑动一个步长** | `cache_offset`、`light_view_draw` | 偏移累加方向错（`O += shift`，不是 `-= shift`）或写侧平移 y 符号错（应为 `-offset.y*texel`）；跑 Test 17 ①（物理不动）/②（写读一致）定位 |
| 静态阴影**整片消失/落后一段** | `need_full_update` 与 `cache_offset` | 非零偏移下走了整级重画（偏移未清零）⇒ 光栅器无环绕、尾部 `|O|` 条带被裁；查 Test 17 ③ |
| 对拍报告"条带区与整级重建**内容不一致**"（整图错位、平坦区也差异、最优位移顶到扫描边缘且残余=0） | 示例对拍：`CSM_CACHE_DIFF=1 CSM_AUTOWALK=24`（§4.7） | **诊断相位假象**，不是引擎缺陷：行走中读回 A 会使"读回的图"与"随后读到的 `offset`"跨帧 ⇒ 整图错映射。已修（停走 + 静置 5 帧再读回）；修正后 239 轮逐纹素 0 差异。若再见此签名，先查取样是否在状态静止下进行 |
| 贴图**边缘一条 1~2 texel 宽亮/暗线**（pcf 半径外扩后显现） | `AppendWrappedStrip` | 跨缝条带未拆成两段或被截短（`M % B != 0` 时可达）；跑 Test 17 子用例 (e) 与面积契约⑤ |
| 贴图上一条**随相机移动的 1~2 texel 错影线**（在阴影区内，不在贴图几何边缘） | Test 18 的 margin/required | seam 真实可达：级联世界半径过小（r0 ≲ 6m）或 `normal_offset`/`pcf_radius`/切分距离调整吃掉了余量；按 §4.6 的 slack 公式核算，必要时下调 `normal_offset_world` 或该级关横向滚动 |
| 相机移动时静态级联**频繁整级重建** | `along_anchor`（深度锚点） | `cache_anchor_step` 太小 ⇒ 每 `step` 米跨一次就整级重建并清零偏移（会同时抹掉滚动收益） |
| 接触点**漏光 / peter-panning** | `bias` 符号 | 背面渲染下 bias 取了正值（§5） |
| 陡峭表面**条纹**（acne） | `normal_offset_world`、`bias` 绝对值、`pcf_radius` | 先开法线偏移（§5.2）；仍不干净才是缺 slope-scaled bias（§5 末尾） |
| 法线偏移调大后**接触点反而断开** | `normal_offset_world`、`bias` 符号 | 法线偏移推过头（`tan` 在近掠射角权重很大）⇒ 收小强度，或把 `|bias_world|` 往贴合方向补一点 |
| 法线偏移**看起来完全没生效** | 生成后的 GLSL 里是否有 `#define HGL_SHADOW_NORMAL_OFFSET 1` | 模板没带 ShadowProvider 槽（宏不会被发射）或 `shadow_normal_offset = 0`；或 `shadow_params.w` 仍是 0（CPU 侧没接通） |
| 半影出现**光晕** | `bias` 负值过大 | 收一点（更贴合方向） |
| **启动几帧**阴影闪现 | — | 尚无 warm-up 流程（见 §10） |
| 拖拽时阴影**一帧左一帧右 / 一帧近一帧远**，静止后正常；RenderDoc 截帧永远正常 | 不是拟合公式。先确认 `ShadowInfo` 是否又变回单份 UBO | 在途主帧还在读 binding 5 时，CPU 覆写了同一块 `ShadowInfo`。修复与禁令见 `doc/shadow-ubo-inflight-overwrite.md`。不要用每帧 `WaitFence()` 全槽排空来压症状 |
| 静态阴影能渲染但**读到就没了** | 是否每帧都发了静态级的 DrawCall | 静态级被错误地也当成了逐帧层 |
| **alpha test 物体的阴影是实心的**（本体镂空正常） | 物件的 recipe 是否声明了 `alpha_test`/`dither`（depth-only FS 保留的**唯一**判据，D8 后程序级扫描已删） | 片元含 discard 却被 depth-only 快速路径剥掉；或 `batch.texture_reference_base_addr`=0（MTL_TEX 解引用 0 → fallback 1.0）。**取证**：`AlphaTestShadow` 第 45 帧自动判读 c0 的 `[D1-CONTRACT]` 行（**包围盒内**填充率 57.6%=镂空、~100%=实心、全空=未进深度图）；`ATS_SELFCHECK=1` 时以退出码给出结论（0/1）。判据链本身由 `TestCSMIncrementalPass` Test 11 源码契约把守 |
| **masked 物体在深度图里缺失**（影子不出现或固化消失） | collect 日志 `shadow pass skip for '<名字>': <原因>`（首帧起）或 `persisted 120 frames`（持续失败） | 首帧 resolve/行未就绪失败 + 静态缓存固化。跳过路径已带告警与收敛（D9：120 帧内每帧 bump 重画，之后降频到每 60 帧一次并报错）；手动调 `InvalidateMainLightStaticShadowCache()` 立即重画 |
| 静态级联**每帧**全量重绘（日志刷 `invalidating static cascade`、滚动缓存不再 `100% Cached`） | `[TransformComponent] 运行期写入 Static transform（<setter>｜实体 '<名字>'）` 告警 | **运行期写了 Static 物体**：静态段是"写一次用很久"的常驻区，任何一次写入都会整段重写静态矩阵 + 全部静态级联失效（D4 实测：每帧同值写 ⇒ 15s 内 872 次失效）。会动的对象在创建期 `SetMobility(Mobility::Movable)`（每帧 ring 写 + 动态级联）；编辑期一次性调整可忽略。告警每组件只报一次（D4/A′） |
| `receive_shadow` / `bias_multiplier` **拨了没反应**（物件照样受影、倍率看不出变化） | 该图元的 per-draw 行是否真的写进去（`MaterialInstanceAddresses::shadow_flags` / `shadow_bias_multiplier`，§5.3） | ① 物件**没挂** `ShadowComponent` ⇒ 行全零 = 引擎默认（接收 + 倍率 1.0），这是有意行为不是 bug；② 该图元画的 recipe 没走 `pcf_shadow.glsl` 的 `GetShadowFactor`（例如 `identity.glsl` 占位恒 1.0）；③ 行结构改了但发射端没跟上——看生成后的 GLSL 里 `struct MaterialInstanceAddresses` 是否含新字段（Test 14 已把守）；④ 倍率在**不自投影**的场景里本来就看不出变化（无 acne 可消），用极端倍率（×1000）验证接线 |
| 想验证"接收侧旋钮真的通了" | `ATS_SELFCHECK=1 AlphaTestShadow.exe` + `ATS_SELFCHECK=1 ATS_D3_NOKNOB=1 ...` | 两条都要过：前者 `[D3-CONTRACT] receive_shadow PASS`（受影→未受影 18189 px）、`bias_multiplier PASS`（600662 px）；后者对照组必须 **0 px**（噪声底），否则任何差异都不能归因给旋钮。判据必须按**通道**（是否变红）而非亮度，理由见 §5.3 口径坑 |

**诊断手段**：

```
[CSM Rolling Cache Stats] Cam=(x,y,z) | C1=n strips(band=16t avg=0.29% off=(80,0) full=0) | C2=... | C3=... | Mid/Far Status: ...
```

`strips==0 && full==0` 表示窗口内完全命中（0 DrawCall）；`avg` 是窗口平均每帧重画占比
（滚动方案收益，整级重建为 100%）；`full` 是窗口内**真实**整级重建次数（来自单调计数器差分，
不是末帧快照——见 §4.8）。`full>0` 说明窗口内有整级重建（横向锚定跨格、深度锚点跨步、
静态场景 revision 变更，或有人调了 `InvalidateMainLightStaticShadowCache`）。静置不动时这条应
持续 `0 strips / 0 full / 100% Cached`。GPU 侧"整级 vs 条带"对拍见 §4.7，计数器见 §4.8。

---

## 10. 已知缺口 / 后续工作

| 主题 | 现状 |
|------|------|
| ~~逐级联独立 bias~~ | 已实现（§5.1：`bias_world` 世界单位 + `per_cascade_bias_scale`） |
| Slope-scaled / 硬件 depth bias | 引擎无 `vkCmdSetDepthBias`，动态状态列表缺 `VK_DYNAMIC_STATE_DEPTH_BIAS` |
| ~~Normal-offset shadow mapping~~ | 已实现（§5.2：`normal_offset_world` + `HGL_SHADOW_NORMAL_OFFSET`，`tan(θ)` 加权） |
| 首帧 warm-up | 未实现，启动前几帧会出现阴影闪现 |
| 环形寻址（Toroidal clipmap） | ~~未启用~~ → **S2 已启用**：偏移按跨格量累加（`scroll_offset` → `casc.cache_offset`）、写侧改用 `light_view_draw`、只重画新暴露条带（含跨缝拆分）。**S5 已接 stats + 收益量化 + 对拍工具**（§4.7：每帧重画 0.05~0.31%）；但**对拍发现"条带重画内容 ≠ 整级重建内容"（c1 每轮 400~1900 texel，剪影处）且根因未定** ⇒ 见 backlog D5/S6 |
| caster/receiver 标志位 | 只有 `Mobility` 动/静二分，没有"投射/接收"独立标志 |
| 静态级联分辨率与更新频率解耦 | 静态级联被迫跟动态级联同分辨率同 `caster_depth_margin` |
| 单张 shadow atlas | 目前 4 张独立 D32F RT，无 atlas 合并 |
| 逐物体脏追踪 | 脏粒度是"整级联"，不是"受影响的物体集合" |
| ~~ShadowInfo 单份 UBO 被在途帧覆写~~ | 已分槽（`kShadowUboRing`，按下标 = acquired image）。`MarkDirty` 不再写 shadow GPU。见 `doc/shadow-ubo-inflight-overwrite.md` |
| ~~ShadowCaster 专用程序未接线~~ | 已实现（A1 + A1-4）：阴影 pass 走 `MaterialComponent` 双槽（`shadow_program`，`ShadowCasterOpaque/Masked` 模板）；masked caster 的 opacity_mask 采样链完整（纹理引用行由 forward 物化链持有，深度镂空经深度图读回验证）。链路细节见 §4.5 |
| ~~静态缓存失效链缺失~~ | 已接线（A3）：TransformSystem 检出 Static 变更 → `static_scene_revision` → EnvironmentSystem 消费失效。剩余缺口：新增/删除静态物体与材质/贴图替换需手动调 `InvalidateMainLightStaticShadowCache()`；`SetLocalPosition` 等无同值短路 |

---

## 11. 修改本模块的检查清单

- [ ] 动过 `CalculateCascadeBounds` 的任何一步？先跑 `TestCSMIncrementalPass`，**5A/5B-1/5B-2 全绿**。
- [ ] 动过半径 / 锚定步长？做一次"故意破坏"反证，确认 5B-1 或 5B-2 能失败（§8）。
- [ ] 动过矩阵？必须**目视确认静态阴影在相机移动时不再滑动**（自动化断言只能证明矩阵恒定，
      不能证明画面正确）。
- [ ] 动过 `CullMode` / `bias`？确认接触点的贴合与漏光（`[`/`]` 微调）。
- [ ] 动过 `normal_offset_world` 或 `pcf_shadow.glsl` 的偏移代码？跑 `TestCSMIncrementalPass`，
      **7A/7B/7C 全绿**（7C 读真实 shader 源码，改完 shader 不必重新编译测试）；改了 shader 的
      辅助函数签名/结构，别忘了同步 7C 的 `Contains` 契约串。
- [ ] 给接收者加新的采样位置扰动（如再叠一层 offset）？确认**选级仍用未偏移的位置**
      （`EvalPCFShadowAt(worldPos, selectPos)` 的第二个入参）。
- [ ] 动过切分距离？同时确认 shader 选级用的 `cascade_params.x/y` 与新距离一致。
- [ ] 用了 `SetCascadeEnabled(false)` / `cascade_mask`？症状"关掉 CSM 1 后中远景还是出现 CSM 2/3 的内容"、
      "交界处像单向替代"时**先查 `EvalCascadeChain` 的选级循环**：必须"先按 `view_depth` 定级、再判
      `shadow_tex.x == 0` 返回受光"，绝不能 `continue` 跳过被屏蔽级（§2.x）。顺手确认
      `cascade_params.w` 没被当成 flags 写（§2.y）。
- [ ] 动过 `castShadow` 相关逻辑？确认静态级**只在** `need_full_update || dirty_rect_count > 0`
      时发 DrawCall。
- [ ] 源文件必须是**无 BOM UTF-8**（MSVC 未设 `/utf-8`；用 PowerShell 改文件时
      `Set-Content` 会加 BOM，必须用 `[System.IO.File]::WriteAllText(..., UTF8Encoding($false))`）。
