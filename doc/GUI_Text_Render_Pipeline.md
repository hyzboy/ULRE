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
        │
图形抽象层 (graph/font + SceneGraph)
  TextLayout → TextGeometry → TileFont → FontSource
        │
Vulkan 层
  vkCmdBindPipeline → vkCmdBindDescriptorSets → vkCmdDrawMeshTasksEXT
```

---

## 一、程序启动与初始化

入口在 `example/GUI/TextDrawTest.cpp` 的 `os_main()`，通过 `RunFramework<TestApp>()` 模板函数启动。`AppFramework`（`inc/hgl/framework/AppFramework.h`）负责创建窗口、Vulkan 实例/设备、GraphicsContext 和 ECSContext（含系统注册）。

`TestApp::InitTextRenderable()` 创建 ECS 实体并添加 `TextComponent`：
- 从文件加载文本（如 `res/text/道德经.txt`）
- 创建 `FontSource`（如"微软雅黑"，24号）
- 设置文本、字体、起始位置、字符样式

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
| RenderBatch | TextBuildSystem (`src/ecs/support/text/TextBuildSystem.cpp`) | 排版 → 字形图集生成 → 顶点数据 → SSBO 上传 |
| RenderBatch | TextSyncSystem (`src/ecs/support/text/TextSyncSystem.cpp`) | 清除变更标记 |
| RenderDrawSubmit | TextRenderSystem (`src/ecs/support/text/TextRenderSystem.cpp`) | 绑定管线/描述符 → DrawMeshTasks |

核心实现集中在 `TextRenderPipeline`（`src/ecs/support/TextRenderPipeline.cpp`，779 行）。

---

## 四、文本到网格（Text-to-Mesh）生成详解

这是整个链路的核心，分为三个子步骤：

### 4.1 字形注册与图集管理

`TextLayout::StatChars()`（`src/SceneGraph/font/TextLayout.cpp`）遍历所有字符，通过 `FontSource`（`inc/hgl/graph/font/FontSource.h`）获取字符排版属性，然后由 `TileFont::Registry()`（`src/SceneGraph/font/TileFont.cpp`）将新字符的位图写入 `TileData`（`src/SceneGraph/tile/TileData.cpp`）的 2D 纹理图集（R8 单通道格式），并记录每个字符的 UV 坐标。

### 4.2 顶点生成（排版 → 网格）

`TextLayout::End()`（`src/SceneGraph/font/TextLayout.cpp`）调用 `sl_l2r()` 进行左到右排版。每个可见字符生成 **6 个顶点（2 个三角形 = 1 个矩形）**：

- **Position**: `int16 x, int16 y`（像素坐标，`VK_FORMAT_R16G16_SINT`）
- **TexCoord**: `uint16 x, uint16 y`（UV 坐标，half float，`VK_FORMAT_R16G16_SFLOAT`）

### 4.3 写入 GPU 缓冲区

`TextGeometry`（`src/SceneGraph/font/TextGeometry.cpp`）将顶点数据写入 VAB（Vertex Attribute Buffer），即 GPU 端的 SSBO。Float 坐标先转换为 half float 再写入。

---

## 五、Vulkan 渲染与最终绘制

### 5.1 管线配置

使用 **Mesh Shader** 路径（`VK_EXT_mesh_shader`），没有传统顶点输入。管线配置：
- 无 `vkCmdBindVertexBuffers`，顶点从 SSBO 读取
- Mesh Shader + Fragment Shader
- Dynamic Rendering（无 VkRenderPass/VkFramebuffer）
- 材质定义在 `ShaderLibrary/material/text_2d.material.toml`

### 5.2 描述符绑定

| Set | 内容 |
|-----|------|
| Set 0 (Scene) | Camera/Viewport UBO |
| Set 1 (PerObject) | Position SSBO, UV SSBO, Index SSBO, mesh_draw_params SSBO |
| Set 2 (Material) | MaterialData, TextureLayer, DataIndex |
| Set 3 (Bindless) | 全局纹理数组 |

### 5.3 最终绘制调用

```cpp
// TextRenderPipeline::Render()
const uint32_t group_count = (total_vertices + 95u) / 96u;  // threadgroup = 96
cmd->DrawMeshTasks(group_count);
  └─ vkCmdDrawMeshTasksEXT(cmd_buf, group_count_x, 1, 1)
```

Mesh Shader（由 `MeshShaderAssembler`（`src/ShaderGen/common/MeshShaderAssembler.h`）生成）工作方式：
- 每个 threadgroup 96 线程，每线程处理 1 个顶点
- 从 SSBO 读取 Position/UV → 坐标变换（像素→NDC）→ 写入 `gl_MeshVerticesEXT`
- 每 3 个连续顶点组成 1 个三角形
- Fragment Shader（`ShaderLibrary/material/text_source.glsl`）采样字形图集 R 通道作为蒙版，乘以文字颜色得到最终输出

---

## 六、关键发现

1. **不使用 vkCmdDraw** — 实际调用的是 `vkCmdDrawMeshTasksEXT`，完全走 Mesh Shader 路径
2. **无传统顶点输入** — 没有 `vkCmdBindVertexBuffers`，所有顶点数据通过 SSBO 传入 Mesh Shader
3. **字形图集是 R8 单通道纹理** — Fragment Shader 用 R 通道作为字形蒙版
4. **每字符 6 顶点** — 标准 billboard quad 布局
5. **按字体批次合并绘制** — 同一 FontSource 的所有 TextComponent 合并为一次 DrawMeshTasks
6. **ECS 驱动架构** — TextComponent 是纯数据，TextRenderPipeline 管理所有运行时资源

---

## 七、关键文件索引

| 功能 | 文件路径 |
|------|---------|
| 示例入口 | `example/GUI/TextDrawTest.cpp` |
| 框架主循环 | `src/Work/WorkManager.cpp` |
| ECS 渲染调度 | `src/ecs/core/Context.cpp` |
| RenderGraph | `src/ecs/core/RenderGraph.cpp` |
| 默认系统注册 | `src/ecs/core/DefaultSystems.cpp` |
| TextComponent | `inc/hgl/ecs/components/TextComponent.h` |
| TextRenderPipeline（头） | `inc/hgl/ecs/support/TextRenderPipeline.h` |
| TextRenderPipeline（实现） | `src/ecs/support/TextRenderPipeline.cpp` |
| Text ECS 系统 | `src/ecs/support/text/Text{Collect,Build,Sync,Render}System.cpp` |
| TextLayout（排版） | `src/SceneGraph/font/TextLayout.cpp` |
| TextGeometry（网格） | `src/SceneGraph/font/TextGeometry.cpp` |
| TileFont（字形图集） | `src/SceneGraph/font/TileFont.cpp` |
| FontSource（字体源） | `inc/hgl/graph/font/FontSource.h` |
| RenderCmdBuffer | `inc/hgl/vk/VKCommandBuffer.h` |
| DrawMeshTasks 实现 | `src/Vulkan/VKCommandBufferRender.cpp` |
| MeshShader 生成 | `src/ShaderGen/common/MeshShaderAssembler.h` |
| Text2D 材质定义 | `ShaderLibrary/material/text_2d.material.toml` |
| Text 片段着色器 | `ShaderLibrary/material/text_source.glsl` |
| 顶点位置 SSBO 读取 | `ShaderLibrary/vertex/s1_position_vec2i.glsl` |
