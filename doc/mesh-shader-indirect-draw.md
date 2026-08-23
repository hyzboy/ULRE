# IndirectMeshDraw：mesh shader 模式下的间接绘制（multi-draw 合批）

## 1. 背景

mesh shader 迁移完成后，顶点数据统一走 SSBO（无 VBO 顶点输入），per-draw 段偏移
（index_base/vertex_base/is_indexed/total_vertices/viewport_height/first_instance）
经 **24B push constant** 传递——push constant 是 per-draw 状态，无法合批，
每 DrawBatch 一次 `vkCmdDrawMeshTasksEXT`。旧 VS+VBO 模式的
`vkCmdDrawIndexedIndirect` 合批能力在 mesh 化时丢失。

本文记录恢复方案：**per-draw 参数表 SSBO + gl_DrawID 查表 + 一条
`vkCmdDrawMeshTasksIndirectEXT` 提交整个材质批**。push constant 路径全链删除。

## 2. 核心契约

### 2.1 mesh_draw_params 参数表（PerObject 集 binding 13）

GLSL 声明由 `MeshShaderAssembler` 生成（`#version 460`——glslang 仅在 GLSL 4.60 起
在 mesh 阶段符号表声明 gl_DrawID，450 报 undeclared identifier）：

```glsl
struct MeshDrawParams { uint index_base, vertex_base, is_indexed, total_vertices;
                        float viewport_height; uint first_instance; };  // std430 24B
layout(set=MESH_DRAW_PARAMS_SET, binding=MESH_DRAW_PARAMS_BINDING, std430)
readonly buffer MeshDrawParamsData { MeshDrawParams rows[]; } sbo_draw_params;
MeshDrawParams pc_vertex_index;              // 全局可变（模块函数经 gl_InstanceIndex 宏引用）
void main() { pc_vertex_index = sbo_draw_params.rows[gl_DrawID]; ... }
```

- CPU 侧同构结构 `hgl::graph::mtl::MeshDrawParams`（ShaderBufferSources.h，
  static_assert 24B）；
- `DescriptorSemantic::MeshDrawParams` / `SSBOType::MeshDrawParams` /
  `SBS_MeshDrawParams` / `kPerObjectBindingMeshDrawParams=13`（固定名路径）；
- 所有 mesh 材质无条件持有该 descriptor（GenericMaterialBuilder 注入，
  stage flags 仅 MESH）。

### 2.2 命令语义（VkDrawMeshTasksIndirectCommandEXT）

每条命令 `{X=组数, Y=实例数, Z=1}`：

- **X** = CalcMeshGroupCount(is_lines, total_vertices)——Lines 每线程 1 线段
  （total/2，组 64），其余每线程 1 顶点（组 96，3 的倍数防跨组丢三角形）；
- **Y** = instance_count——**Y 轴即实例轴**（`gl_WorkGroupID.y` = 实例内序号，
  `first_instance + y` 对应 l2w_index_rows 行号）；
- **Z** 恒 1。

### 2.3 行序对齐（关键不变量）

- **每个 DrawBatch 一行参数**（`params_row` = draw 序号，含私有 VBO 几何）；
  mesh 命令**只写给 VDM DrawBatch**（与渲染器累积逻辑一致）；
- run（同 GeometryDataBuffer 连续段）内命令序与行序 **1:1**——同一 buffer 意味着
  整段同为 VDM 或同为私有；
- 渲染器在 **run 起点**（GeometryDataBuffer 切换处）按 `run 首行 × 24B` 的
  **offset 绑定**参数表；`gl_DrawID` 在每次 `vkCmdDrawMeshTasksIndirectEXT`
  调用内 0 起编号，`rows[gl_DrawID]` 恰好命中。

## 3. 四条渲染路径（全部走参数表，push constant 无消费者）

| 路径 | 参数行写入 | 绘制方式 |
|---|---|---|
| primitive VDM（合批主路径） | PrimitiveBatchPipeline::WriteMeshDrawCommands | 累积 → 一条 vkCmdDrawMeshTasksIndirectEXT/材质批 |
| primitive 私有 VBO | 同上 | 每 draw 绑参数表**本行 offset 视图** + DrawMeshTasks（gl_DrawID=0 → rows[0]） |
| Line（LineRenderPipeline） | RunBuild 收尾写 row 0（单 draw 非实例化） | 直接 DrawMeshTasks |
| Text（TextRenderPipeline，按字体） | ProcessInputs 在 draw_range 定稿后写 row 0 | 直接 DrawMeshTasks |

行写入全部在 **RenderBatch 阶段**（录制前），由 RenderBufferUploadSystem 上传
（barrier 已含 INDIRECT_COMMAND_READ_BIT，mesh tasks 同样适用）。
viewport_height 统一取渲染目标 `GetExtent().height`（不再从录制期 cmd viewport 读）。

## 4. 基础设施

- `IndirectMeshTaskBuffer : IndirectCommandBuffer<VkDrawMeshTasksIndirectCommandEXT>`
  （ReBAR/Staged Auto 策略复用；扩展函数无静态原型，draw 调用统一在
  `RenderCmdBuffer::DrawMeshTasksIndirect`——PFN 加载 + 无 MDI 逐条退化）；
- `PFN_vkCmdDrawMeshTasksIndirectEXT` 设备创建时加载（VKDeviceAttribute）；
- 无 MDI 设备**自动退化直接路径**（逐条 fallback 的 DrawID 恒 0，无法区分命令——
  语义不成立，宁可不合批）。

## 5. 已删除（本系列提交）

- `IndirectDrawBuffer` 类 + `VulkanDevice::CreateIndirectDrawBuffer` +
  `RenderCmdBuffer::DrawIndirect`（legacy VkDrawIndirectCommand 链路，mesh 化后死代码）；
- `MaterialBatch::icb_draw` / `WriteICB` / 渲染器 icb_draw 参数与累积分支；
- mesh shader 的 24B push constant（MeshPC/LinePushConstant/TextPC）及
  `VKPipelineLayoutData` 的 push constant range（pushConstantRangeCount=0）。

## 6. 限制与后续

- 跨材质批仍无法合并（pipeline/descriptor 切换）——远期
  VK_EXT_device_generated_commands；
- GPU 驱动方向（task shader 剔除 / `...IndirectCountEXT` + compute 写 count）未做，
  当前命令与参数行均为 CPU 侧生成；
- 合批条件：同 ShaderProgramPipelineKey + 同 GeometryDataBuffer（VDM 模式天然满足）
  + MDI 支持；
- gl_DrawID 依赖 GLSL 4.60 符号表（见 2.1）；GLSL_EXT_mesh_shader 规范确认
  gl_DrawID 为 vertex/task/mesh 三阶段合法输入（SPIR-V DrawIndex builtin）。

## 7. 验证

- ShaderResourceSchemaRegressionGate 43/43（新增 MeshDrawParams 语义全链注册）；
- PBRSpheres：日志确认 `mesh indirect flush engaged: first=0 count=10`（10 几何 ×
  每命令 10 实例，一条 multi-draw）；RenderDoc 中 sbo_draw_params 显示真实 buffer；
- AutoInstance/TextureQuad（私有 VAB 直接路径）、LineRenderTest（2216 线段恢复
  绘制）、DrawMultiLineText、RenderToTexture：VVL 开启下零 VUID；
- MaterialBatch 析构补 icb_mesh_tasks/mesh_draw_params_buffer 释放
  （修复 VUID-vkDestroyDevice-device-05137）。
