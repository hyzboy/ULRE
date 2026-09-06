# Vertex/Texture 系列 SSBO 绑定 BDA 化重构计划

日期：2026-09
状态：待执行
分支：material-arena-bda（延续）
前置：材质数据 Arena+BDA 重构已完成（见 material-ssbo-arena-bda-refactor-plan-2026-09.md）

---

## 1. 目标与范围

**消灭**：Vertex 集 8 个 SSBO 绑定（set 4 整体）+ Material 集 `mtl_texture_layer_rows`（set 2 整体）——所有"随几何/材质切换而动"的动态绑定。

**不动**（用户明确约束）：
- Scene UBO（全场景共用、固定绑定，非复杂化来源）
- Bindless 纹理集（BDA 不作用于 image，永久保留）
- `mesh_draw_params` 绑定本身（必绑，升级为顶点基址的载体）
- l2w / l2w_index / mtl_data_addrs（本轮范围外）
- Text 的 CharInfo/Style/Instance b14/15/16（范围外）

**终态**：3 个 set（Scene / PerObject / Bindless），顶点与纹理表数据全部经 BDA 设备地址到达 shader。

## 2. 核心设计（探索已验证的事实）

### 2.1 MeshDrawParams 行扩 8 基址字段

- X-macro 单一真源：`inc/hgl/graph/ShaderBufferSources.h:61-67`（CPU struct / GLSL 字段名/类型 / 布局断言三同源）。
- 现状 6 字段 24B；追加 8 个 `uint64_t addr_position/uv/ntb/color/luminance/transform_id/size/index` → std430 uint64 8 对齐，行 24→88B 无 padding。
- 断言重写：`i*4` 连续性检查（:105-113）改为显式 offsetof 断言 + `sizeof == 88`。
- 行粒度 = 每 DrawBatch 一行（`WriteMeshDrawCommands`，PrimitiveBatchPipeline.cpp:506-581），shader 侧 `sbo_draw_params.rows[gl_DrawID]`（MeshTemplateEmitter.h:256）；直接绘制路径经 offset 视图取 row 0——**索引机制零改动**。
- 三个写入点：PrimitiveBatchPipeline（主路径）、TextRenderPipeline.cpp:826-857、LineRenderPipeline.cpp:690-715。

### 2.2 VDM 零改动 + 数组引用模式

现状 Vertex 集绑定整只大 VAB（offset=0, VK_WHOLE_SIZE，PipelineMaterialRenderer.cpp:129-180），shader 内以绝对顶点号寻址（`pc_vertex_index.vertex_base + VertexIndexID`）。BDA 化后：

- 基址 = VAB 大 buffer 的 `vkGetBufferDeviceAddress`（VDM 内每语义一条流一个大 buffer，所有几何共享子分配）。
- **数组下标算术原样保留**——`VertexPositionData(addr).data[vertex_base + i]`。区段偏移永远在数组下标里，不成为指针；仅 8 个基址需 16B 对齐（fail-fast 断言）。
- VDM 的 `AcquireVAB` 顶点号子分配、`GeometryDataVDM`、GeometryCreater 写入路径全部不动。

### 2.3 宏垫片迁移（s1 模块函数体零改动）

adapter（MeshShaderVertexAdapter.h）在 MeshDrawParams 之后发射：

```glsl
layout(buffer_reference, scalar, buffer_reference_align=16) buffer VertexPositionData { vec3 data[]; };
#define sbo_vertex_position (*VertexPositionData(pc_vertex_index.addr_position))
```

s1 模块只删各自的 `layout(set=VERTEX_SET, binding=..., std430) readonly buffer ... sbo_vertex_xxx;` 声明行——函数体 `sbo_vertex_position.data[...]` 逐字节不变。索引 SSBO（adapter 自声明的 `sbo_vertex_index`）同模式。

### 2.4 BDA 编码规范（沿用材质行验证过的约定）

- scalar 布局（引擎基线特性 scalarBlockLayout）；
- `buffer_reference_align=16` 承诺（数组引用模式保证承诺只挂在我们控制的基址上）；
- 基址创建时 fail-fast 断言 `%16==0`；
- 行结构/字段布局 static_assert 全覆盖纪律。

## 3. 工作包

### W0 基线（0.5 天）
全解决方案构建零错误；gate 41/41；用户示例走查基线。

### W1 BDA 基础设施（1-1.5 天）——双写共存，零行为变化
| # | 任务 | 锚点 |
|---|---|---|
| W1a | CreateVAB/CreateIBO usage 补 `SHADER_DEVICE_ADDRESS_BIT`；分配 flag `VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT` 贯通（CreateMemory 加参数或仿 CreateArenaBuffer 专用路径）；设备地址 %16 断言 | VKDeviceBuffer.cpp:102-155/194-224、VKMemory.cpp:8-26、VKDeviceStagedBuffer.cpp:110-139 |
| W1b | MeshDrawParams X-macro +8 字段 + 断言重写 | ShaderBufferSources.h:61-114 |
| W1c | 三个写入点补填基址（经 DrawBatch→GeometryDataVDM→vdm->GetVAB(i)→设备地址） | PrimitiveBatchPipeline.cpp:526-542、Text:826-857、Line:690-715 |

验收：全示例零变化，gate 41/41。

### W2 s1 模块 BDA 化（2-3 天）——描述符暂存未用
| # | 任务 |
|---|---|
| W2a | MeshShaderVertexAdapter：发射 buffer_reference 结构 + `sbo_vertex_index` 垫片宏（改造 :30-31） |
| W2b | 13 个声明模块换垫片（Position×3/UV×2/NTB×4/Color/PaletteIndex/Luminance/TransformID/Size），函数体不动 |
| W2c | line_quad.glsl.tmpl + MeshShaderModeLineQuad/VertexPassthrough 直读点核查（垫片自动覆盖） |
| W2d | `SHADERGEN_CODEGEN_GENERATION` +1 |

验收：全示例渲染逐项一致；VertexLoaderConsistency ctest 通过。

### W3 Vertex 集退场（1.5-2 天）
- ShaderGen 删除链：DefinitionDescriptorBuilder.h:80-113（need_* 推导整块）→ PushVertexResource → MaterialShaderCompiler VertexGeometry case（:514-525）→ ShaderBuildContext::AddSSBOVertex*（:164-174）→ Catalog 8 行（:60-68）+ :261 static_assert → SBS_Vertex* 8 行。
- 枚举/宏：VertexBinding 枚举 + :69-71 锚点断言、宏表 158-170、`DescriptorSetType::Vertex`；`DescriptorMacroGen --emit` 重生成（交叉校验需两侧同步）。
- 运行时：PipelineMaterialRenderer.cpp:129-180 绑定块 + :369-380 `ssbo_vertex_input` + vertex_mp 池、LineRenderPipeline.cpp:279-320、RDBS:908-969。
- 5 个 golden 重生成。
- 验收：全示例 + gate 41/41。

### W4 texture_layer_rows 消灭 + Material 集退场（1.5-2 天）
- UnlitTexture TOML 挂 `material_data`；texture_source.glsl 改读行尾；TextureQuad/TextureRect 每材质 1 行 accessor。
- Text：每字体一行 EmissiveSurfaceRow（accessor 挂 RenderResources——Gizmo 修复模式），图集句柄→`tex_base_color`；删 `texture_layer_buffer`。
- 删除：SBS_MaterialTextureLayerRows、Push/AppendMaterialTextureLayerDescriptors、`uses_texture_layer_table`、RDBS MaterialTextureLayerTable 分支 + `resolve_recipe_batch_struct_ssbo_id`、FS 发射块、Collect 遗留域写。
- `DescriptorSetType::Material` 删除（**Bindless 3→2 重编号**，缓存换代吸收）；MATERIAL_SET 宏、VKCommandBufferRender 特判、Text material_mp、StructuredBufferAccessor 默认集类型。
- gate AD 用例冲突载体重做。
- 验收：TextureQuad/Rect + GUI 五例 + gate。

### W5（可选延伸）合批解锁（0.5-1 天）
`need_buffer_switch` 几何切换 flush 放宽（基址已逐行携带）→ 不同几何合入同一 indirect run。验收：混几何示例 + RenderDoc draw 数对比。

## 4. 风险登记

| # | 风险 | 缓解 |
|---|---|---|
| R1 | VAB 分配 flag 贯通 StagedBuffer 路径（上传路径回归） | W1 验收含全示例走查；上传与 flag 正交 |
| R2 | 基址 16 对齐（桌面存储缓冲普遍满足但规范不保证） | 创建时 fail-fast 断言 |
| R3 | Material 枚举重编号序列化面 | SPV 缓存换代 + golden 重生成吸收；TOML 无 set_type 字段 |
| R4 | gate AD 冲突载体丢失 | 改用其它类型冲突或删断言 |
| R5 | Line 管线独立绑定路径 | W3 明确迁移（其 shader 经垫片自动 BDA 化） |
| R6 | TDR/调试性（BDA 常态风险） | 基址断言 + null 行约定 + ULRE_ARENA_DEBUG 诊断 |

## 5. 总量
6-9 天（W0-W4 必做约 6-8 天，W5 可选）。
