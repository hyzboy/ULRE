# 渲染自省阶段二：Debug View 通道与图像量化分析

本文是「引擎内嵌渲染自省系统」第二阶段的**总体设计与实现原理**文档，覆盖：

- 为什么要在引擎里做 **Debug View（调试视图）通道**；
- Debug View 如何在**不侵入固定主着色器**的前提下接入现有材质领域体系；
- 世界坐标、法线、阴影覆盖率、帧间差异等指标各自的**数学原理**与**数据来源**；
- 引擎侧（C++ / HGL）与 MCP 侧（Python）的**职责边界**与**接口契约**。

第一阶段（Stage 1~4）已交付：`GpuQueryProfiler`（GPU 时间戳 + 管线统计）、
`RenderHealthLedger`（渲染健康账本）、`FrameDebugger`（按需抓帧）、
`DebugBridgeServer`（Windows 命名管道）、`tools/ulre-mcp-server`（MCP 桥）与
`SKILL_RENDER_DIAGNOSTICS.md`。本文描述的是**在其之上**的增量。

---

## 一、问题定义

排查「画面对不对」时，AI Agent 或自动化测试拿到的通常只有一张**最终颜色图**。
最终颜色是「几何 × 材质 × 光照 × 阴影 × 后处理」的乘积，任何一环出错都表现为
「画面奇怪」，无法定位。具体缺口：

| 缺口 | 后果 |
| --- | --- |
| 无法拿到**世界法线**通道 | 法线反向 / 法线贴图通道错位 / 面法线兜底失效无法判定 |
| 无法拿到**表面属性**（roughness / metallic / ao） | 无法判断「材质没生效」还是「光照不对」 |
| 无法拿到**相机矩阵** | 有了深度图也无法反算世界坐标（不知道 VP） |
| 没有**帧间差分** | 着色器/管线重构后无法自动判定「画面是否漂移」 |
| 只有图像没有**数值指标** | 视觉模型读图 token 成本高、精度低，无法做断言 |

第二阶段的目标：让 Agent 拿到**可断言的结构化数值**，而不是凭感觉看图。

---

## 二、总体架构与职责边界

```mermaid
flowchart TD
    subgraph Engine [ULRE 引擎 (C++ / HGL)]
        DVM[DebugViewMode 状态<br/>DebugViewController]
        DR[DebugAttribute 领域<br/>debug_attribute.frag]
        FD[FrameDebugger<br/>按需 RT/Depth 抓取 + 落盘]
        IPC[DebugBridgeServer<br/>命名管道 JSON-RPC]
        DVM -->|配方覆盖 MaterialData::SetRecipe| DR
        DR --> FD
        FD --> IPC
    end

    subgraph MCP [ulre-mcp-server (Python)]
        CL[pipe_client: 命名管道]
        AN[analyzer: 纯 Python 数值内核]
        CL --> AN
    end

    subgraph Agent [AI Agent]
        SK[SKILL_RENDER_DIAGNOSTICS.md]
        SK --> Agent2[诊断 Agent]
    end

    Agent2 -->|MCP tools| CL
    IPC -->|JSON + 抓取文件路径| CL
```

### 职责边界（唯一的分工口径）

| 层 | 负责 | 不负责 |
| --- | --- | --- |
| **C++ 引擎** | GPU 管线编排、调试通道渲染、Staging 回读、相机矩阵下发、把原始字节落盘 | 不做统计/比对等数值密集计算（避免给引擎引入图像算法） |
| **Python MCP** | 逐像素统计、直方图、矩阵反投影、帧差、热力图；把结果压成紧凑 JSON | 不接触 Vulkan，不重算渲染 |

理由：数值计算放在 Python 侧可以**零成本迭代**（改公式不需要重新编译引擎），
而原始像素只经过一次「引擎 → 文件」的搬运；Agent 最终拿到的是 KB 级 JSON，
不会把大图塞进上下文。

---

## 三、Debug View 的实现原理

### 3.1 为什么不能用「改主着色器」的方式

本项目已废除 ShaderGen，每个 `MaterialDomain × pass` 是一份**物理固定**的完整
GLSL 编译单元（`ShaderLibrary/domain/<domain>/*.frag`），其唯一的可变输入是
`generated/frag_options.glsl`（只有 `#define` 与少量 `#include` 行）。

因此「给所有材质额外输出一张法线图」有两条路：

1. 改固定 `.frag` —— 被明确禁止（缓存身份、发行形态一致性门禁都绑在文本上）；
2. 换一个**领域** —— 领域是「表面契约」，本来就允许新增。

本方案选 2。

### 3.2 `DebugAttribute` 领域

新增领域 `MaterialDomain::DebugAttribute`，其特征：

- 顶点语义：`Position | Normal | TexCoord`（与 `StandardOpaque` 一致）；
- 片元入口：`ShaderLibrary/domain/debug_attribute/debug_attribute.frag`（固定单元）；
- 表面来源：`module/provider/material_source/material_source_debug_attribute.glsl`；
- 输出策略：复用 `output_policy_flat`（无光照，`baseColor` 直出）；
- 材质私有行：`GlobalSSBOType::PBRSurface`

**关键点**：`PBRSurface` 行的前 16 字节就是 `base_color`，与 PBR 材质**逐字节同构**。
所以切换到调试视图时**不需要搬动任何材质数据行**——同一个
`material_ssbo_binding`（`ssbo_type / ssbo_id / data_index`）直接被调试通道读取，
只是解读方式变了。

### 3.3 通道选择：用 `compile_defines` 做**编译期**分档

通道选择不引入运行时分支（`if (mode==...)` 会让所有通道的代码都进 SPIR-V），
而是走配方的 `compile_defines`：

```toml
# 等价于调试配方的语义
domain = "DebugAttribute"
defines = ["HGL_DEBUG_VIEW_WORLD_NORMAL"]
```

`MaterialRecipe::has_compile_defines` 为真时，`compile_defines` 直接进入
`ResolvedMaterial::compile_defines` → `DomainOptionsInput::definition_defines` →
`generated/frag_options.glsl` 的 `#define` 行。

因为 frag_options 文本**同时是 program 身份的一部分**，不同通道天然得到不同的
program / pipeline / 缓存项，切回原通道不会串味。

| 宏 | 通道内容 |
| --- | --- |
| `HGL_DEBUG_VIEW_WORLD_NORMAL` | `normalize(worldNormal) * 0.5 + 0.5` |
| `HGL_DEBUG_VIEW_BASE_COLOR` | PBR 行 `base_color.rgb` |
| `HGL_DEBUG_VIEW_SURFACE` | `(roughness, metallic, ao)` |
| `HGL_DEBUG_VIEW_EMISSIVE` | `emissive` |
| `HGL_DEBUG_VIEW_UV0` | `fract(uv0)` |
| 无宏（默认） | 世界法线 |

### 3.4 世界级开关：配方覆盖，而不是新建渲染管线

`MaterialData` 是本项目的「材质授权记录」（世界级旁表，键 = `EntityID`），
它提供唯一的授权入口 `SetRecipe()`，每次变更推进 `authored_generation`，
收集路径据此**重解析 program / pipeline / 材质行**。

`DebugViewController` 的做法：

```
RequestMode(mode)                    // 任意线程可调用（管道线程）
   ↓ 每帧一次
Apply(world)                         // 收集阶段之前
   ├─ mode == Off  → 逐实体还原（无覆盖的用 ClearRecipeOverride）
   └─ mode != Off  → 逐实体 SetRecipe(BuildDebugViewRecipe(mode, 原配方))
```

被覆盖的实体在首次进入调试视图时把**原配方（或「原本就没有覆盖」这一事实）**
存进控制器旁表；`Off` 时逐条还原。这样：

- **不新增 RT、不新增 Pass、不新增管线**：调试视图就是把现有几何用另一份
  材质再画一遍，复用全部间接绘制 / 实例化 / 顶点 ABI；
- 关闭时**零开销**：`Apply()` 只有一次原子比较，配方未变则直接返回；
- 可随时切换，`Debug View → 原视图 → Debug View` 任意往返。

> ⚠ 限制（必须写进文档，避免误判）：调试视图**只覆盖有 `MaterialData`
> 授权记录的图元**。天空、UI、调试线等引擎内部材质（无授权记录）不参与，
> 这是有意的——它们不是「被诊断的资产材质」。

---

## 四、数据来源与数学原理

### 4.1 世界坐标：不要渲染，直接反投影

调试视图**不输出世界坐标**。原因：主帧颜色附件是 8~10bit，把世界坐标压进去精度
只有厘米级；而世界坐标完全可以从**深度图 + 相机矩阵**精确反算。

引擎侧只需下发 `CameraInfo`（`inc/hgl/graph/CameraInfo.h`）中的：

```
inverse_vp, view, projection, camera_world_pos, znear, zfar, use_reversed_z
```

MCP 侧对每个像素 `(px, py)`，深度 `d`（从深度 RT 读回，float32）：

```
u = (px + 0.5) / width
v = (py + 0.5) / height
ndc = (2u - 1, 2v - 1, d, 1)          // Vulkan RH_ZO：z ∈ [0,1]
h   = inverse_vp * ndc
world_rel = h.xyz / h.w                // 相机相对坐标（引擎是 Camera-Relative）
world     = world_rel + camera_world_pos
```

`use_reversed_z` 只影响**深度值语义的解读**（reversed-Z 时近平面 = 1），
反投影矩阵本身已经包含了该约定，因此公式无需分支。

### 4.2 法线的有效性检验

调试通道把世界法线编码为 `c = N * 0.5 + 0.5`，MCP 侧解码 `N = c * 2 - 1`。
在 8bit 量化下每分量误差 ≤ 1/255 ≈ 0.0039，模长误差上限约 0.014，因此
判据取 **0.05** 而不是 0.01：

| 指标 | 公式 | 判读 |
| --- | --- | --- |
| 单位化误差 | `epsilon = |‖N‖ - 1|` | `mean(epsilon) > 0.05` ⇒ 法线未单位化/被错误缩放 |
| 退化比例 | `‖N‖ < 0.1` 的像素占比 | 偏高 ⇒ 黑色区域被当成法线（未做零向量保护） |
| 朝向分布 | `Z > 0` / `Z < 0` 占比 | 地板/墙壁应各自占优；反了 ⇒ 坐标系或翻面错误 |
| 背向比例 | `dot(N, V) > 0` 占比（V 为指向相机） | 偏高 ⇒ 面片朝内 / 双面材质法线未翻转 |

> 几何法线（由世界坐标差分求得）与本通道法线做**逐像素夹角**统计，是「顶点法线
> 属性坏了」与「几何本身歪了」的判别器：夹角中位数大而几何法线自洽 ⇒ 顶点法线错。

### 4.3 亮度与动态范围

线性输入转感知亮度（ITU-R BT.709）：

```
Y = 0.2126 R + 0.7152 G + 0.0722 B
```

| 指标 | 意义 |
| --- | --- |
| `mean` / `stddev` | 整体曝光与对比度 |
| 分位数 `P05 / P50 / P95` | 抗极值；比 min/max 更能反映分布 |
| 死黑率 `Y < 0.005` | 过暗 / 未渲染 / 光照丢失 |
| 死白率 `Y > 0.995` | 过曝 / 光照强度爆炸 |
| `nan` / `inf` / `negative` 计数 | 浮点 RT 的数值污染 |

### 4.4 表面属性与阴影覆盖率

- **表面属性**：调试通道直接读出 `roughness / metallic / ao`，统计分布与
  「全 0 / 全 1」占比即可判定「材质参数没生效」。
- **阴影覆盖率**：读回 CSM 级联的**深度纹理**（`FrameDebugger` 已支持
  `RequestDepth`），按级联分块统计：
  - 有效深度占比（未清屏 = 有投射物）——过低 ⇒ 该级联没画进东西；
  - 深度直方图——判断投射物在级联视锥中的分布是否合理；
  - 与同一帧主视图的世界坐标点做「是否落在阴影贴图有效区」的采样比对，
    得到**阴影覆盖率**（受影像素占比）。

### 4.5 帧间对比

设 A、B 两帧为同尺寸 RGB 图：

```
MAE   = mean(|A - B|)
RMSE  = sqrt(mean((A - B)^2))
PSNR  = 20 * log10(1 / RMSE)                 // 以 1.0 为峰值
change_ratio = count(|A - B| > tau) / pixel_count
```

再加**结构相似性**（SSIM，11×11 高斯窗或 8×8 块）：

```
SSIM(x,y) = ((2μxμy + C1)(2σxy + C2)) / ((μx² + μy² + C1)(σx² + σy² + C2))
```

判读口径（写进 Skill，不让 Agent 自由发挥）：

| 场景 | 期望 |
| --- | --- |
| 同一场景、同一相机、无输入变化 | `MAE ≈ 0`，`SSIM = 1.0`（确定性的渲染应逐位一致） |
| 只改一个旋钮 | `change_ratio` 非零但局限在局部；`SSIM > 0.95` |
| 大面积闪烁/抖动 | `change_ratio` 高而 `MAE` 低 ⇒ 亚像素抖动或 TAA 残余 |
| 材质整体失效 | `MAE` 高、`SSIM` 低，且差异区域与几何轮廓一致 |

差异热力图（`heatmap`）用于人眼复核，不作为断言依据。

---

## 五、接口契约

### 5.1 引擎侧（命名管道 JSON-RPC）

| method | 说明 |
| --- | --- |
| `get_render_health` | 健康账本（既有） |
| `get_pass_metrics` | Pass 级 GPU 时间与管线统计（既有） |
| `get_camera_matrices` | `inverse_vp / view / projection / camera_world_pos / znear / zfar / reversed_z` |
| `set_debug_view` | 设置调试通道（`off / world_normal / base_color / surface / emissive / uv0`），返回生效模式 |
| `get_debug_view` | 读当前调试通道 |
| `capture_frame` | 登记一次抓帧请求（颜色 / 深度 / 指定命名 RT），返回请求 id |
| `get_last_capture` | 返回最近一次完成的抓帧：尺寸、格式、逐通道统计、**落盘文件路径** |

抓帧的**落盘约定**：

```
<dump_dir>/<name>_<frame_id>.<ext>.raw     // 原始字节（无转换）
<dump_dir>/<name>_<frame_id>.<ext>.json    // 元数据：宽高、VkFormat、pixel_size、
                                           // 相机矩阵快照、调试通道、时间戳
```

管道**只传路径与统计**，绝不传像素（避免 4KB 缓冲与 base64 膨胀）。

### 5.2 MCP 侧工具

| tool | 输入 | 输出（要点） |
| --- | --- | --- |
| `get_camera_matrices` | — | 矩阵数组 |
| `set_debug_view` / `get_debug_view` | `mode` | 生效模式 |
| `capture_frame` / `get_last_capture` | `targets[]` | 抓取清单与文件路径 |
| `analyze_luminance` | `path`, `channels`, `layout` | BT.709 亮度统计、死黑/死白、NaN/Inf |
| `analyze_normals` | `path`, `layout` | 单位化误差、退化比例、半球分布 |
| `analyze_surface` | `path` | roughness / metallic / ao 分布与退化判定 |
| `analyze_depth` | `path`, `reversed_z` | 深度直方图、有效像素占比 |
| `reconstruct_world_pos` | `depth_path`, `inverse_vp` 或直接取引擎 | 点云范围、采样点、深度回代误差 |
| `compare_frames` | `path_a`, `path_b`, `tau` | MAE / RMSE / PSNR / SSIM / change_ratio |

`layout` 说明像素排列（`rgba8` / `bgra8` / `rgb10a2` / `r32f`），因为引擎回读的是
**原生字节**，格式由 `.json` 元数据给出，MCP 侧按元数据解码，不猜。

---

## 六、分阶段实施

| 阶段 | 内容 | 交付物 |
| --- | --- | --- |
| 1 | 本文档 | `doc/debug-analysis-and-view-modes.md` |
| 2 | `DebugAttribute` 领域 + 调试配方 + 世界级控制器 | `debug_attribute.frag`、`material_source_debug_attribute.glsl`、`DebugViewMode.h`、`DebugViewRecipe.h/.cpp`、`DebugViewController.h/.cpp` |
| 3 | 抓帧落盘 + 相机矩阵 + 管道方法 | `FrameDebugger` 扩展、`DebugBridgeServer` 方法、抓帧泵 |
| 4 | Python 数值内核 + MCP 工具 | `tools/ulre-mcp-server/analyzer.py`、`mcp_server.py` |
| 5 | Skill 升级 + 端到端自检 | `SKILL_RENDER_DIAGNOSTICS.md`、`selftest.py` |

---

## 七、约束与红线

1. **零 STL**：引擎侧新增代码只用 HGL 类型（`AnsiString` / `ValueArray` /
   `UnorderedMap` / `Mutex` / `io::FileOutputStream` 等）。
2. **坐标契约不变**：Vulkan / RH / Z-up / ZO，`m00 > 0, m11 < 0`。任何调试通道
   都不得引入第二套投影符号。
3. **关闭即零开销**：调试视图关闭时不产生额外 draw、不分配 RT、不做回读。
4. **不改固定主着色器**：所有调试成像都通过「新增领域 + 新增模块」实现。
5. **不向 Agent 灌大图**：返回值是数值 + 文件路径；大图只在本地磁盘。
