# GUI 文本渲染：从 main() 到 vkCmdDrawMeshTasksEXT 全链路

## 总体架构

```
应用层 (example/GUI/)
  os_main() → RunFramework<TestApp>() → WorkManager::Run()
        │
框架层 (framework/)
  AppFramework (窗口/Vulkan设备/ECS初始化)
  WorkManager (主循环: Tick + Render)
        │
ECS 层 (ecs/)
  ECSContext → RenderGraph → 按执行阶段调度 System
  Text 管线四阶段:
    TextCollectSystem → TextBuildSystem → TextSyncSystem → TextRenderSystem
        │
渲染管线层 (TextRenderPipeline)
  RunCollect → RunBuild → RunSync → Render(cmd)
  三层 SSBO 数据模型 (AlignedStructureBuffer 管理):
    TextCharInfo (b14) / CharStyle (b15) / CharInstance (b16)
        │
图形抽象层 (graph/font + SceneGraph)
  TextLayout → TileFont → FontSource → FontBitmapDataSource (SDF/Bitmap 双路径)
        │
Vulkan 层
  vkCmdBindPipeline → vkCmdBindDescriptorSets → vkCmdDrawMeshTasksEXT
```

### SDF 双路径渲染

文本渲染支持两条路径，由 `FontSource::SetSDFEnabled(bool)` 切换：

| 路径 | 材质 | 采样器 | 着色器宏 | 说明 |
|------|------|--------|----------|------|
| SDF 距离场 | `text_2d_gpu.material.toml` | Linear | `TEXT_SDF_ENABLED` | 支持加粗/勾边/阴影特效，smoothstep 抗锯齿 |
| 原始位图 | `text_2d_gpu_bitmap.material.toml` | Nearest | — | 灰度蒙版直接调制，无特效 |

两条路径共用同一个 Fragment Shader 源 `text_source_gpu.glsl`，通过 `TEXT_SDF_ENABLED` 编译宏分流。

### AlignedStructureBuffer 管理 SSBO

`AlignedStructureBuffer<T>`（`inc/hgl/vk/AlignedStructureBuffer.h`）是 SSBO 的核心容器：
- CPU 侧紧密存储结构体数组（最优内存利用）
- `SyncToGPU()` 时自动处理 CPU→GPU 的对齐扩展（std430 对齐填充）
- 集成 `IGPUBuffer` 路径，支持 `StagedBuffer`/`ReBarBuffer` 双路由
- 提供 `GetData()`、`operator[]` 透明访问，`GetVkBuffer()`/`GetBufferInfo()` 用于描述符绑定

TextRenderPipeline 为三层 SSBO 各维护一个 `AlignedStructureBuffer`：
- `AlignedStructureBuffer<TextCharInfo>` → binding 14
- `AlignedStructureBuffer<CharStyle>` → binding 15
- `AlignedStructureBuffer<CharInstance>` → binding 16

---

## 一、程序启动与初始化

入口在 `example/GUI/TextDrawTest.cpp` 的 `os_main()`，通过 `RunFramework<TestApp>()` 模板函数启动。`AppFramework`（`inc/hgl/framework/AppFramework.h`）负责创建窗口、Vulkan 实例/设备、GraphicsContext 和 ECSContext（含系统注册）。

`TestApp::InitTextRenderable()` 创建 ECS 实体并添加 `TextComponent`：
- 从文件加载文本（如 `res/text/道德经.txt`）
- 创建 `FontSource`（如"微软雅黑"，24号）
- 若需 SDF 特效，调用 `fs->SetSDFEnabled(true)` 启用距离场
- 设置文本、字体、起始位置、字符样式（`CharStyle`，定义在 `TextCharSSBO.h`）

随后 `WorkManager::Run()`（`src/Work/WorkManager.cpp`）进入主循环，每帧调用 `ECSContext::Render()`。

---

## 二、主循环与帧驱动

每帧渲染流程：
1. **BeginManagedRenderFrame** — 获取 Swapchain 图像，`vkCmdBeginRendering`（Dynamic Rendering）
2. **ExecuteRenderGraphPasses** — 遍历 RenderGraph 各 Pass，按 ExecutionPhase 调度 System
3. **EndManagedRenderFrame** — 结束帧并提交

核心调度在 `ECSContext::Render()`（`src/ecs/core/Context.cpp`）和 `RenderGraph`（`src/ecs/core/RenderGraph.cpp`）中。

---

## 三、Text ECS 系统链（四阶段管线）

| 阶段 | 系统 | 职责 |
|------|------|------|
| RenderCollect | TextCollectSystem (`src/ecs/support/text/TextCollectSystem.cpp`) | 收集所有 TextComponent，按 FontSource 分组 |
| RenderBatch | TextBuildSystem (`src/ecs/support/text/TextBuildSystem.cpp`) | 排版 → 字形图集生成 → SSBO 数据准备 → 三层 SSBO 上传 |
| RenderBatch | TextSyncSystem (`src/ecs/support/text/TextSyncSystem.cpp`) | 清除变更标记 |
| RenderDrawSubmit | TextRenderSystem (`src/ecs/support/text/TextRenderSystem.cpp`) | 绑定管线/描述符 → DrawMeshTasks |

核心实现集中在 `TextRenderPipeline`（`src/ecs/support/text/TextRenderPipeline.cpp`）。

---

## 四、文本到网格（Text-to-Mesh）生成详解

这是整个链路的核心，分为三个子步骤：

### 4.1 字形注册与图集管理（含 SDF 距离场生成）

`TextLayout::StatChars()`（`src/SceneGraph/font/TextLayout.cpp`）遍历所有字符，通过 `FontSource`（`inc/hgl/graph/font/FontSource.h`）获取字符排版属性，然后由 `TileFont::Registry()`（`src/SceneGraph/font/TileFont.cpp`）将新字符的位图写入 `TileData`（`src/SceneGraph/tile/TileData.cpp`）的 2D 纹理图集（R8 单通道格式），并记录每个字符的 UV 坐标。

**SDF 距离场生成**（`src/SceneGraph/font/FontBitmapDataSource.cpp`）：

当 `FontSource::SetSDFEnabled(true)` 时，`GetCharBitmap()` 在获取灰度位图后执行 SDF 转换：

1. 将原始位图居中放入扩展缓冲（四周各扩展 `TEXT_SDF_SPREAD`（=8）像素，背景填 0）
2. 调用 `sdf_generate()`（`CMUtil/src/sdf.c`）生成距离场
3. 结果拷回位图（尺寸变为 `(w+2P) × (h+2P)`），metrics 偏移回退 `-P`
4. **注意**：`adv_x`/`adv_y` 保持原始值不变。SDF padding 仅为距离场计算预留空间，字形视觉扩展由 `extra_advance` 机制动态补偿

SDF 编码约定：内部 = 高值（255），外部 = 低值（0）。Shader 中解码为 `sdf = raw * 2.0 - 1.0`，字身边界 = 0。

### 4.2 排版（CharQuad Mesh Shader 模式）

`TextLayout::End()`（`src/SceneGraph/font/TextLayout.cpp`）调用 `sl_l2r()` 进行左到右排版。排版结果写入 `CharInstance` 数组（每字符 8B：pen_x/y, char_id, style_id）。

实际的网格生成由 `MeshShaderAssembler`（`src/ShaderGen/common/MeshShaderAssembler.h`，统一调度入口，CharQuad 主体在子模块 `MeshShaderModeCharQuad.h`）的 **CharQuad 模式**在 GPU 端完成：
- 每个 threadgroup 42 个线程（`max_invocations = 42`）
- 每线程处理 **1 个字符实例**，生成 **6 个顶点（2 个三角形 = 1 个 quad）**
- 顶点从三层 SSBO 读取数据，在 Mesh Shader 内计算像素坐标并做 NDC 变换
- 支持斜体剪切变形（`shear_factor = tan(italic)`）
- 支持 `CharStyle.scale` 缩放：quad 尺寸 = `metrics × scale`，绘制偏移同步缩放
- 支持 `CharStyle.rotation` 旋转（0/90/180/270）：绕 quad 中心旋转顶点，UV 同步旋转

**extra_advance 机制**：

当 `CharStyle` 的 `bold_px` 或 `outline_px` > 0 时，字形视觉尺寸扩展，需要增加字符间距避免重叠。`TextRenderPipeline::BuildDrawStyle()` 自动计算：

```cpp
const float extra = 2.0f * (char_style->bold_px + char_style->outline_px);
out_style.extra_advance_x = extra;
out_style.extra_advance_y = extra;
```

`TextLayout::sl_l2r()` 在字符定位和换行逻辑中追加 `extra_advance`，普通文本（bold=0, outline=0）不受影响。

### 4.3 GPU 缓冲区（三层 SSBO 模型）

`TextRenderPipeline` 通过三个 `AlignedStructureBuffer` 将数据上传至 GPU SSBO：

**TextCharInfo（binding 14，16B/字符，std430）**：

```cpp
struct TextCharInfo {
    int16_t  offset_x;    // 字形绘制偏移 X
    int16_t  offset_y;    // 字形绘制偏移 Y
    uint16_t metrics_w;   // 字形像素宽度
    uint16_t metrics_h;   // 字形像素高度
    uint16_t uv_left;     // half-float 图集 UV
    uint16_t uv_top;
    uint16_t uv_right;
    uint16_t uv_bottom;
};  // 16 bytes
```

**CharStyle（binding 15，40B/样式，std430）**：

```cpp
struct CharStyle {
    uint32_t text_color;        // packed RGBA8 字符颜色
    uint32_t outline_color;     // packed RGBA8 勾边颜色
    uint32_t shadow_color;      // packed RGBA8 阴影颜色
    uint32_t flags;             // bit0 = shadow_enabled
    float    italic;            // 斜切角度（弧度）
    float    bold_px;           // 加粗宽度（像素），0=关闭
    float    outline_px;        // 勾边宽度（像素），0=关闭，钳制 <= TEXT_SDF_SPREAD
    uint32_t shadow_uv_offset;  // packed half2 (du, dv) 阴影 UV 偏移
    float    scale;             // 缩放因子（1.0=原始大小）
    int32_t  rotation;          // 旋转角度（0/90/180/270）
};  // 40 bytes
```

CharStyle 定义在 `inc/hgl/graph/font/TextCharSSBO.h`，**CPU/GPU 共用同一布局**。CPU 侧通过 `TextRenderPipeline` 做 `Color4ub → packed uint32` 转换后直接写入。

**CharInstance（binding 16，8B/实例，std430）**：

```cpp
struct CharInstance {
    int16_t  pen_x;       // 屏幕 X 坐标
    int16_t  pen_y;       // 屏幕 Y 坐标
    uint16_t char_id;     // 索引 TextCharInfo SSBO
    uint16_t style_id;    // 索引 CharStyle SSBO
};  // 8 bytes
```

---

## 五、Vulkan 渲染与最终绘制

### 5.1 管线配置

使用 **Mesh Shader** 路径（`VK_EXT_mesh_shader`），没有传统顶点输入。管线配置：
- 无 `vkCmdBindVertexBuffers`，顶点从 SSBO 读取
- Mesh Shader + Fragment Shader
- Dynamic Rendering（无 VkRenderPass/VkFramebuffer）
- 材质定义：
  - SDF 路径：`ShaderLibrary/material/text_2d_gpu.material.toml`（`blend = "Transparent"`，`defines = ["TEXT_SDF_ENABLED"]`）
  - 位图路径：`ShaderLibrary/material/text_2d_gpu_bitmap.material.toml`（`blend = "Transparent"`，无 SDF 宏）
- Mesh Shader 模式：`CharQuad`，`max_invocations = 42`
- `blend = "Transparent"` 启用 alpha 混合（`VK_BLEND_FACTOR_SRC_ALPHA` / `ONE_MINUS_SRC_ALPHA`），使 SDF smoothstep 抗锯齿边缘和阴影/勾边效果正确与背景混合

### 5.2 描述符绑定

| Set | 内容 |
|-----|------|
| Set 0 (Scene) | Camera/Viewport UBO (`ViewportInfo`) |
| Set 1 (PerObject) | b14: TextCharInfo SSBO, b15: CharStyle SSBO, b16: CharInstance SSBO, mesh_draw_params SSBO |
| Set 2 (Material) | MaterialData, TextureLayer, DataIndex |
| Set 3 (Bindless) | 全局纹理数组 |

### 5.3 最终绘制调用

```cpp
// TextRenderPipeline::Render()
// CharQuad 模式：每线程 1 字符 → 6 顶点，threadgroup = 42
const uint32_t group_count = (total_chars + 41u) / 42u;
cmd->DrawMeshTasks(group_count);
  └─ vkCmdDrawMeshTasksEXT(cmd_buf, group_count_x, 1, 1)
```

Mesh Shader（由 `MeshShaderAssembler`（`src/ShaderGen/common/MeshShaderAssembler.h`，CharQuad 主体在 `MeshShaderModeCharQuad.h`）CharQuad 模式生成）工作方式：
- 每个 threadgroup 42 线程，每线程处理 1 个字符实例
- 从三层 SSBO 读取 CharInstance → TextCharInfo → CharStyle
- 计算 quad 像素坐标：`metrics × scale` 缩放 + 斜体剪切 + rotation 绕中心旋转 → `viewport.ortho_matrix` 变换到 NDC
- 生成 6 个顶点（2 三角形）+ UV/颜色/样式ID varying
- Fragment Shader（`ShaderLibrary/material/text_source_gpu.glsl`）：
  - **SDF 路径**（`TEXT_SDF_ENABLED`）：解码距离场 `sdf = raw * 2.0 - 1.0`，调用 `EvalTextStyleEffects()` 合成加粗/勾边/阴影效果
  - **位图路径**：直接灰度采样调制文字颜色

---

## 六、SDF 字体特效

SDF 特效在 `text_source_gpu.glsl` 的 `EvalTextStyleEffects()` 函数中实现，通过 CharStyle SSBO 读取样式参数。

### CharStyle 40B 布局详解

| 偏移 | 大小 | 字段 | 说明 |
|------|------|------|------|
| 0 | 4B | `text_color` | packed RGBA8 字符颜色 |
| 4 | 4B | `outline_color` | packed RGBA8 勾边颜色 |
| 8 | 4B | `shadow_color` | packed RGBA8 阴影颜色，默认黑 |
| 12 | 4B | `flags` | bit0 = shadow_enabled |
| 16 | 4B | `italic` | 斜切角度（弧度），0=正体 |
| 20 | 4B | `bold_px` | 加粗像素，0=关闭 |
| 24 | 4B | `outline_px` | 勾边像素，0=关闭，钳制 ≤ TEXT_SDF_SPREAD(8) |
| 28 | 4B | `shadow_uv_offset` | packed half2 UV 偏移 |
| 32 | 4B | `scale` | 缩放因子（1.0=原始大小） |
| 36 | 4B | `rotation` | 旋转角度（0/90/180/270） |

### 缩放（Scale）

`CharStyle.scale` 控制字符缩放，CPU/GPU 两侧联动：
- **排版侧**（`TextLayout::sl_l2r`）：字符 advance、空格/制表符宽度、换行行距全部乘 `scale`，保证放大后字符不重叠
- **GPU 侧**（`MeshShaderModeCharQuad.h`）：quad 尺寸 = `metrics × scale`，绘制偏移（offset_x/y）同步缩放，基线校正使用 `char_height × scale`（viewport_height 传基础字符高度）
- **图集位图保持原始字号光栅化，放大不重新采样**：SDF 路径下边缘由 smoothstep 抗锯齿保证平滑，这是 SDF 的核心优势；位图路径放大后会模糊
- `TextRenderPipeline::BuildDrawStyle()` 将 `CharStyle.scale` 透传到 `TextDrawStyle.scale`

### 旋转（Rotation）

`CharStyle.rotation` 支持 0/90/180/270 四档：
- GPU 侧绕 quad 中心旋转顶点（旋转中心 = 未加斜体的原始 quad 中心），斜体 shear 叠加在旋转结果上
- UV 同步旋转（90°: (u,v)→(1-v,u)；180°: (u,v)→(1-u,1-v)；270°: (u,v)→(v,1-u)）
- 排版 advance 不随旋转调整（旋转 90° 的字符仍占原始宽度）

### 加粗（Bold）

字身边界外扩 `bold_px` 像素。在 smoothstep 阈值上内移：

```glsl
const float du = 2.0 / TEXT_SDF_SPREAD;  // 每像素对应的 sdf 单位
const float sm = fwidth(sdf);             // 抗锯齿带宽
const float body_a = smoothstep(-sm, sm, sdf + bold_px * du);
```

### 勾边（Outline）

边界外扩 `outline_px` 像素，使用 over 合成与字身叠加。`outline_px = 0` 时强制归零，避免 over 合成重复叠加：

```glsl
float outline_a = smoothstep(-sm, sm, sdf + outline_px * du);
if (outline_px <= 0.0) outline_a = 0.0;
```

### 阴影（Shadow）

偏移采样同一距离场，配合渐变淡出效果：

1. **偏移采样**：UV 减去 `shadow_uv_offset`（`unpackHalf2x16` 解包），得到阴影位置的距离场值
2. **阴影基础覆盖率**：与加粗类似，`smoothstep(-shadow_sm, shadow_sm, shadow_sdf + bold_px * du)`，阴影跟随加粗
3. **渐变淡出**：利用原始 SDF 距离值调制阴影强度，距离字形越远阴影越淡：
   ```glsl
   const float fade = smoothstep(-3.0 * du, 1.0 * du, sdf);
   shadow_a = shadow_base_a * fade;
   ```

### 合成顺序

预乘 alpha over 合成，从底到顶：
1. **阴影层**（最底）：`shadow_color × shadow_a`
2. **勾边层**（中间）：`outline_color × outline_a × (1 - body_a)`
3. **字身层**（最顶）：`text_color × body_a`

```glsl
// top = body over outline
top_rgb = textColor.rgb * body_a + outline_color.rgb * outline_a * (1 - body_a)
top_a   = body_a + outline_a * (1 - body_a)
// final = shadow over top
out_rgb = top_rgb + shadow_color.rgb * shadow_a * (1 - top_a)
out_alpha = textColor.a * (top_a + shadow_a * (1 - top_a))
```

### SDF 编码约定

- 距离场内部 = 高值（255），外部 = 低值（0）
- Shader 中解码：`sdf = rawSample * 2.0 - 1.0`，字身边界 = 0，内部 > 0，外部 < 0
- `TEXT_SDF_SPREAD = 8`（由 `FontSource.h` 定义），表示 ±4 像素的传播范围

---

## 七、关键发现

1. **使用 `vkCmdDrawMeshTasksEXT`** — CharQuad Mesh Shader 模式，每线程 1 字符实例生成 6 顶点 2 三角形
2. **三层 SSBO 数据模型** — TextCharInfo (b14, 16B) / CharStyle (b15, 40B) / CharInstance (b16, 8B)，取代旧的 Position/UV/Index SSBO
3. **SDF 双路径渲染** — SDF 距离场（Linear 采样 + smoothstep 特效）与原始位图（Nearest 采样），通过 `TEXT_SDF_ENABLED` 编译宏切换
4. **CharStyle CPU/GPU 统一定义** — `TextCharSSBO.h` 中 40B std430 布局，CPU 直接写入 packed 数据，GPU 直接读取，无需转换层
5. **SDF 字体特效** — 加粗、勾边、阴影，全部在 Fragment Shader 端通过 smoothstep + over 合成实现
6. **extra_advance 排版间距机制** — bold/outline 时自动增加字符间距 `2.0 × (bold_px + outline_px)`，普通文本不受影响
7. **AlignedStructureBuffer 管理 SSBO 生命周期** — CPU 紧密存储 → GPU 自动对齐，`SyncToGPU()` 一次性上传
8. **Transparent blend 模式** — 启用 alpha 混合，支持 SDF 平滑边缘和半透明特效效果
9. **ECS 驱动架构** — TextComponent 是纯数据，TextRenderPipeline 管理所有运行时资源
10. **scale/rotation 字符变换** — `CharStyle` 内置缩放（排版 advance 与 quad 尺寸联动缩放）与四档旋转（0/90/180/270，顶点 + UV 同步旋转），SDF 放大不重新采样仍保持边缘平滑

---

## 八、关键文件索引

| 功能 | 文件路径 |
|------|---------|
| 示例入口 | `example/GUI/TextDrawTest.cpp` |
| SDF 特效演示范例 | `example/GUI/SDFTextEffects.cpp` |
| 框架主循环 | `src/Work/WorkManager.cpp` |
| ECS 渲染调度 | `src/ecs/core/Context.cpp` |
| RenderGraph | `src/ecs/core/RenderGraph.cpp` |
| 默认系统注册 | `src/ecs/core/DefaultSystems.cpp` |
| TextComponent | `inc/hgl/ecs/components/TextComponent.h` |
| TextRenderPipeline（头） | `inc/hgl/ecs/support/TextRenderPipeline.h` |
| TextRenderPipeline（实现） | `src/ecs/support/text/TextRenderPipeline.cpp` |
| Text ECS 系统 | `src/ecs/support/text/Text{Collect,Build,Sync,Render}System.cpp` |
| TextLayout（排版） | `src/SceneGraph/font/TextLayout.cpp` |
| TextLayout（头，TextDrawStyle/extra_advance） | `inc/hgl/graph/font/TextLayout.h` |
| CharStyle/TextCharInfo/CharInstance SSBO 定义 | `inc/hgl/graph/font/TextCharSSBO.h` |
| TileFont（字形图集） | `src/SceneGraph/font/TileFont.cpp` |
| FontSource（字体源） | `inc/hgl/graph/font/FontSource.h` |
| FontBitmapDataSource（SDF 距离场生成） | `src/SceneGraph/font/FontBitmapDataSource.cpp` |
| SDF 距离场算法（CMUtil） | `CMUtil/src/sdf.c` |
| AlignedStructureBuffer（SSBO 容器） | `inc/hgl/vk/AlignedStructureBuffer.h` |
| RenderCmdBuffer | `inc/hgl/vk/VKCommandBuffer.h` |
| DrawMeshTasks 实现 | `src/Vulkan/VKCommandBufferRender.cpp` |
| MeshShader 生成（CharQuad 模式） | `src/ShaderGen/common/MeshShaderAssembler.h`（调度）+ `MeshShaderModeCharQuad.h`（CharQuad 主体） |
| SDF 材质定义 | `ShaderLibrary/material/text_2d_gpu.material.toml` |
| 位图材质定义 | `ShaderLibrary/material/text_2d_gpu_bitmap.material.toml` |
| 文本 Fragment 着色器（SDF/Bitmap 双路径） | `ShaderLibrary/material/text_source_gpu.glsl` |
