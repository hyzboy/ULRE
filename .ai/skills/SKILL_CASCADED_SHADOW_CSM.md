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

---

## 3. 级联拟合与双轴锚定（本 SKILL 的核心）

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

做法：把包围球中心在 `light_right` / `-light_up_actual` 两轴上吸附到 `lateral_step`
（`cache_lateral_anchor_step`，默认 2m）的整数倍。step 内中心完全静止 ⇒ 布局矩阵恒定，
texel 吸附后位移恒为 0 ⇒ 缓存整段有效；跨格时中心跳变 `step`，位移必然非零而触发重建。
收益实测：**CSM1/2/3 重绘次数 -92% / -86% / -66%**（`full=[600,33,32,34]`）。

**为什么必须用 `round` 而不是 `floor`**：`round` 每轴偏离 ≤ `step/2`，对角 ≤ `0.707*step`，
半径补 `0.708*step` 即可保覆盖；`floor` 的偏离范围是整个步长 `[0, step)`，半径要按
`1.414*step` 扩，代价几乎翻倍。

**代价**：`texel_size = 2*radius/map_size`，半径变大 ⇒ texel 变粗。`step=2m` 时近距级联
（radius 约 60m）约粗 12%，即用一点静态阴影精度换掉绝大部分重绘开销。

### 3.4 ⚠ 锚定基准必须是 `cx0`/`cy0`，不是 `cx`/`cy`

这是本模块历史上最隐蔽的一个缺陷，**改动第 8 步前务必读完**。

`snapped_cx = floor(cx/texel_size)*texel_size`，而 `cx` 本身已经是 `round(cx0/step)*step`
的粗格点。由于 `texel_size ≪ lateral_step`（约 `7e-3` vs `2`），

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
（`CameraSystem` 的 `custom_matrices` 分支会跳过常规矩阵推导）。

> **现状说明**：`cache_offset` / `cache_valid_rect` / `scroll_offset` 目前恒为
> `(0,0,0,0)` 与 `(0,0,W,H)`，即**环形寻址（Toroidal clipmap）管线尚未启用**，
> `pcf_shadow.glsl` 里的 `offset_uv`/`fract` 是恒等变换。滚动目前靠"整级重建"
> 实现而不是"条带搬移"。这是已知的后续工作，不是缺陷；改它必须同时改
> `cache_states_` 三处写入与 shader 的 UV 变换。

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

示例 `CascadeShadowMap.cpp` 用 `bias_world = -1.15f`，`[`/`]` 按 **0.05m** 步长调米数，
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
| 配置字段 | `CascadedShadowConfig::normal_offset_world`（默认 `0.0f`，示例 **0.35m**） |

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
`normal_offset_world = 0.35m` + `bias_world = -1.15m`。

**强度取值的量级参考**：acne 的深度误差量级 ≈ 一个纹素的世界尺寸（示例 CSM 0 半径约
188m / 1024 texel ⇒ ≈0.37m），故初值取 0.35m。

运行时 `-`（减小）/ `=`（增大）按 **0.05m** 步长微调（范围 `[0, 4]`）。改的是纯配置，
**不需要**重建级联缓存（与 §5.1 的 `[`/`]` 同理）。

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
| `bias_world` | 0 | **世界单位偏移（米）**，逐级自动换算，非 0 时压过上面两项（示例 **-1.15**） |
| `per_cascade_bias_scale[4]` | 全 1 | 逐级 bias 乘数（§5.1） |
| `normal_offset_world` | 0 | **法线偏移强度（米）**，按 `tan(θ)` 加权，0 = 关闭（示例 **0.35**，§5.2） |
| `pcf_radius` | 1.5 | PCF 采样半径（texel 倍数） |
| `darkness` | 0.12 | 全阴影时的最暗因子（示例 0.15） |
| `blend_width` | 0.05 | 级联边界混合带宽（UV 比例） |
| `cache_anchor_step` | 16.0 | **沿光轴**深度锚定步长（米）；0 = 禁用（缓存深度会随相机漂移） |
| `cache_lateral_anchor_step` | 2.0 | **横向**锚定步长（米）；0 = 禁用（每跨 texel 即整级重绘） |

两个 `anchor_step` 只用在中远景静态级联（`c > 0`），CSM 0 传 `0.0f`（它本来每帧全量重绘）。

**改 `cache_lateral_anchor_step` 的影响面**：步长变大 ⇒ 重绘更少，但半径补偿 `0.708*step`
同步变大 ⇒ texel 更粗（`step=10m` 时近距级联精度会明显劣化，`Test 5B-2` 就用这个值做压力测试）。

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
| **Test 5B-2** | **冻结窗口覆盖率**：薄切片 + `lateral_step=10m` 压力配置，8 位置 × 16 朝向共 128 帧，用**冻结矩阵**判定角点 NDC | `[CSM-COVERAGE] lateral_step=10m frames=128 fail=0 worst_ndc=[...]` |
| **Test 5C** | **原地旋转下的矩阵恒定性**：相机位置固定、`viewDirection` 绕圈 240 帧，缓存命中帧的 `light_proj*light_view` 必须与最近重绘帧逐元素相同；同时断言扫描确实产生命中帧、且拟合半径不随朝向变化（半径若随朝向变 ⇒ 贴图被逐帧缩放） | `[CSM-SPIN] frames=240 hits=347 max_delta=0.000000 max_radius_delta=0.000061m fail=0` |
| **Test 6A** | 逐级联 bias 回归：默认配置（`bias_world=0`、scale 全 1）必须让 4 级写同一个 `bias`，且 `depth_range` 全为正、各级确实不同 | `[CSM-BIAS] default normalized=[...] depth_range=[346 349 619 953]m` |
| **Test 6B** | `per_cascade_bias_scale=[1 2 3 0.5]` 必须逐级写进 `shadow_params.x`，且 `.y/.z` 不被带偏 | `[CSM-BIAS] per-cascade scale=[1 2 3 0.5] -> normalized=[...]` |
| **Test 6C** | `bias_world=-1.15` 时归一化值必须**逐级不同**，但 `normalized × depth_range` 必须**恒定**且等于 `bias_world` | `[CSM-BIAS] bias_world=-1.15m ... world=[-1.1500..-1.1500]m (constant)` |
| **Test 6D** | `bias_world != 0` 必须压过 `per_cascade_bias_scale`（优先级契约） | `Test 6D Passed` |
| **Test 7A** | `normal_offset_world` 逐级写进 `shadow_params.w`：默认配置（结构体默认 0）必须逐级为 0（不擅自改变历史行为）；显式配置后逐级同值且镜像到单级回退字段 | `[CSM-NORMAL-OFFSET] default strength=0.00m ...` / `configured strength=0.35m on [0.350000 ×4] (mirror=0.350000)` |
| **Test 7B** | **正交性**：`bias_world` 与 `normal_offset_world` 同时开，逐级世界 bias 仍恒为 `bias_world`、`w` 仍等于配置值（各写不同分量，互不干扰） | `Test 7B Passed` |
| **Test 7C** | **shader 源码契约**（读真实文件，8 项 `Contains`）：编译期宏 / 偏移辅助函数 / 宏守卫 / `shadow_params.w` 读取 / `sin_theta / cos_theta` / 背光早退 / `tan` 上限 clamp / `EvalPCFShadowAt(sample_pos, surface.worldPos)` 选级分离 | `Test 7C Passed: shader source contract holds (8 checks, 18104 bytes)` |

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
| 静态级联**每帧全量重绘** | `[CSM Rolling Cache Stats]`、`cache_lateral_anchor_step` | 横向锚定被禁用 / `=0` / 被 3.4 的缺陷抵消 |
| **远处地面**不再接收阴影 | `along_anchor_` 是否在变、`cache_anchor_step` | 沿光轴锚定失效 ⇒ 缓存旧深度被新矩阵解释 |
| 相机抬高/俯仰后**一片地面**无阴影 | `caster_depth_margin` | `zfar` 不够，地面深度被裁 |
| 阴影**边缘一圈没有阴影** | `worst_ndc`、半径补偿 | `0.708*step` 补偿缺失或不匹配 `round`/`floor` 选择 |
| 接触点**漏光 / peter-panning** | `bias` 符号 | 背面渲染下 bias 取了正值（§5） |
| 陡峭表面**条纹**（acne） | `normal_offset_world`、`bias` 绝对值、`pcf_radius` | 先开法线偏移（§5.2）；仍不干净才是缺 slope-scaled bias（§5 末尾） |
| 法线偏移调大后**接触点反而断开** | `normal_offset_world`、`bias` 符号 | 法线偏移推过头（`tan` 在近掠射角权重很大）⇒ 收小强度，或把 `|bias_world|` 往贴合方向补一点 |
| 法线偏移**看起来完全没生效** | 生成后的 GLSL 里是否有 `#define HGL_SHADOW_NORMAL_OFFSET 1` | 模板没带 ShadowProvider 槽（宏不会被发射）或 `shadow_normal_offset = 0`；或 `shadow_params.w` 仍是 0（CPU 侧没接通） |
| 半影出现**光晕** | `bias` 负值过大 | 收一点（更贴合方向） |
| **启动几帧**阴影闪现 | — | 尚无 warm-up 流程（见 §10） |
| 拖拽时阴影**一帧左一帧右 / 一帧近一帧远**，静止后正常；RenderDoc 截帧永远正常 | 不是拟合公式。先确认 `ShadowInfo` 是否又变回单份 UBO | 在途主帧还在读 binding 5 时，CPU 覆写了同一块 `ShadowInfo`。修复与禁令见 `doc/shadow-ubo-inflight-overwrite.md`。不要用每帧 `WaitFence()` 全槽排空来压症状 |
| 静态阴影能渲染但**读到就没了** | 是否每帧都发了静态级的 DrawCall | 静态级被错误地也当成了逐帧层 |

**诊断手段**：

```
[CSM Rolling Cache Stats] Cam=(...) | C1=n strips | C2=... | C3=... | Mid/Far Status: ...
```
`strips == 0` 表示完全命中（0 DrawCall）；静止时这条应当持续为 0。

---

## 10. 已知缺口 / 后续工作

| 主题 | 现状 |
|------|------|
| ~~逐级联独立 bias~~ | 已实现（§5.1：`bias_world` 世界单位 + `per_cascade_bias_scale`） |
| Slope-scaled / 硬件 depth bias | 引擎无 `vkCmdSetDepthBias`，动态状态列表缺 `VK_DYNAMIC_STATE_DEPTH_BIAS` |
| ~~Normal-offset shadow mapping~~ | 已实现（§5.2：`normal_offset_world` + `HGL_SHADOW_NORMAL_OFFSET`，`tan(θ)` 加权） |
| 首帧 warm-up | 未实现，启动前几帧会出现阴影闪现 |
| 环形寻址（Toroidal clipmap） | `cache_offset`/`scroll_offset` 恒 0，滚动靠整级重建而非条带搬移 |
| caster/receiver 标志位 | 只有 `Mobility` 动/静二分，没有"投射/接收"独立标志 |
| 静态级联分辨率与更新频率解耦 | 静态级联被迫跟动态级联同分辨率同 `caster_depth_margin` |
| 单张 shadow atlas | 目前 4 张独立 D32F RT，无 atlas 合并 |
| 逐物体脏追踪 | 脏粒度是"整级联"，不是"受影响的物体集合" |
| ~~ShadowInfo 单份 UBO 被在途帧覆写~~ | 已分槽（`kShadowUboRing`，按下标 = acquired image）。`MarkDirty` 不再写 shadow GPU。见 `doc/shadow-ubo-inflight-overwrite.md` |

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
- [ ] 动过 `castShadow` 相关逻辑？确认静态级**只在** `need_full_update || dirty_rect_count > 0`
      时发 DrawCall。
- [ ] 源文件必须是**无 BOM UTF-8**（MSVC 未设 `/utf-8`；用 PowerShell 改文件时
      `Set-Content` 会加 BOM，必须用 `[System.IO.File]::WriteAllText(..., UTF8Encoding($false))`）。
