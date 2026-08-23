# MeshShader 迁移收尾：VS/Tess/Geometry/VBO 遗留彻底删除

## 1. 背景

MeshShader 迁移（mesh shader 唯一顶点路径）推进到收尾阶段。前序工作（P1-P3、M7a/M7c）已完成
per-draw descriptor set 修复、VS 生成器废弃（GenerateVertexShader/VertexShaderAssembler 删除）、
回归门 VS→mesh 期望同步。本文记录**最后一轮**清理：ShaderStage 枚举/结构体/类的
Vertex/Tessellation/Geometry 遗留删除，stageFlags 精确化（kAllGraphicsOrMesh→kMeshFragment），
以及 VBO 输入类（VertexInput/VIA 系列）整体删除。

**原则**（用户零兼容偏好）：mesh 是唯一顶点路径，凡涉及 Vertex/Tessellation/Geometry stage 的
枚举值、结构体字段、专用类、条件分支全部删除，不留兼容层/标志。

## 2. ShaderStage 枚举清理（S1）

`inc/hgl/common/ShaderStageDef.h` 删除 8 个枚举值：

| 删除 | 原值 | 理由 |
|---|---|---|
| `Vertex` | `VK_SHADER_STAGE_VERTEX_BIT` | VS 已废弃 |
| `TessControl` | `TESSELLATION_CONTROL_BIT` | Tess 已废弃 |
| `TessEval` | `TESSELLATION_EVALUATION_BIT` | Tess 已废弃 |
| `Geometry` | `GEOMETRY_BIT` | Geometry 已废弃 |
| `VertexFragment` | `Vertex \| Fragment` | VS 组合 |
| `VertexGeometryFragment` | `Vertex \| Geometry \| Fragment` | VS/Geometry 组合 |
| `Tessellation` | `TessControl \| TessEval` | Tess 组合 |
| `kVertexOrMesh` | `VERTEX \| MESH` | VS 组合 |

保留：`Fragment`/`Compute`/`Task`/`Mesh`/`ClusterCulling`/`MeshFragment`/`TaskMesh`/
`TaskMeshFragment`/`AllGraphics`（后述删除）。

**删除 `AllGraphics` + `kAllGraphicsOrMesh` → `kMeshFragment`**：

`VK_SHADER_STAGE_ALL_GRAPHICS`（0x1F）含 VERTEX/TESS/GEOMETRY/FRAGMENT 位——这些 stage 已废弃，
descriptor/push constant 的 stageFlags 声明它们虽合法（Vulkan 允许超集），但零兼容原则不保留。
替换为精确值：

```cpp
constexpr uint32_t kMeshFragment =
    VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
```

全仓 25 处 `kAllGraphicsOrMesh` → `kMeshFragment` + 回归门 6 处手写 `VK_SHADER_STAGE_ALL_GRAPHICS`。

## 3. 结构体字段清理（S3）

| 结构体 | 删除 | 理由 |
|---|---|---|
| `ShaderLinkSpec` | `vertex_stage` 字段 | mesh 化后顶点 stage 只有 `mesh_stage`；`IsValid()` 简化为只查 mesh_stage |
| `ShaderProgramKey` | `vertex_stage_digest` 字段 | `BuildKey()` 只填 `mesh_stage_digest` |
| `ShaderProgramArtifactMetadata` | `vertex_stage_digest` → 改名 `mesh_stage_digest` | 语义准确（序列化顺序不变） |

**引擎侧连锁**（S4）：
- `ShaderArtifactStore.cpp`：`GetVertexStageKey(link)` helper 简化为直接 `link.mesh_stage`；
  3 处校验 `vertex_stage_digest` → `mesh_stage_digest`
- `ShaderProgramArtifactBuilder.cpp`：删 `is_mesh` 三元（恒 mesh），无条件 `link.mesh_stage`
- `MaterialShaderCompiler.cpp`：2 处 `has_mesh() ? Mesh : Vertex` 三元 → 无条件 `Mesh`；
  `SaveStageSPV` 用 `link.mesh_stage`
- `ShaderProgramManager.cpp`：cache_key 用 `stage == Mesh` + `link.mesh_stage`
- 5 处默认值 `ShaderStage::Vertex` → `Mesh` / `VertexFragment` → `MeshFragment`

## 4. 专用类清理（S2）

| 类/文件 | 处理 |
|---|---|
| `ShaderCreateInfoVertex`（.h/.cpp） | **删除**（VS 专用；mesh 化后顶点输入走 SSBO，无 VBO 输入） |
| `ShaderBuildContext::has_vertex()` / `GetVertexShader()` | 删除（mesh 化后恒 false/nullptr） |
| `VKShaderProgram::vertex_input` 成员 / `GetVertexInput()` | 删除（VBO 顶点输入布局不再使用） |
| `ReleaseVertexInput` 声明 | 删除 |

## 5. VBO 输入类整体删除

**删除 4 文件**：
- `inc/hgl/vk/VKVertexInput.h`（`VertexInput`/`VertexInputConfig` 类）
- `inc/hgl/vk/VKVertexInputAttribute.h`（转发头）
- `src/Vulkan/VKVertexInput.cpp`（`GetVertexInput`/`ReleaseVertexInput` 实现）
- `inc/hgl/common/VertexInputDef.h`（`VIA`/`VIAArray`/`VertexInputAttribute`）

**迁移**：4 个 stage 工具函数声明（`GetShaderStageName`/`GetShaderStageFlagBits`/
`GetShaderCountByBits`/`GetMaxShaderStage`，定义在 `VKShaderStage.cpp`）从 VertexInputDef.h
迁到 `ShaderStageDef.h`（同域，已 include vulkan.h）。

**清理 8 处 include**：
- `VKShaderStage.cpp`：VKVertexInputAttribute.h → ShaderStageDef.h
- `ShaderProgramManager.cpp`/`VKShaderProgram.cpp`：VKVertexInput.h → ShaderStageDef.h
- `VKShaderModule.cpp`/`VKDeviceMaterial.cpp`/`VKPipelineData.cpp`/`VertexDataManager.cpp`/
  `PrimitiveBatchPipeline.cpp`：删多余 VKVertexInputFormat.h include（未实际使用）
- `RenderContext.h`：删 VKVertexInput.h + VKVertexInputFormat.h include
- `VertexAttrib.cpp`：删 `GetVulkanFormat(const VertexInputAttribute*)`（无调用者）
- `VK.h`：删 `VertexInput`/`VertexInputAttribute` 前向声明

**保留**（SSBO 路径仍用，非 VBO 输入类）：
- `VKVertexInputFormat.h`（`VertexInputFormat`/`VIF`——GeometryVertexFormat 兼容性判断用）
- `VKVertexAttribBuffer.h`（`VertexAttribBuffer`/`VAB`——顶点属性 SSBO 缓冲）
- `VKVABList.h`（`VABList`）
- `VertexAttrib.h`/`VertexAttribDataAccess.h`/`VertexAttribDef.h`（`VAType`/`VABaseType`/属性数据访问）

**CMake**：`src/Vulkan/CMakeLists.txt` VK_VERTEX_INPUT_FILES 删 3 项（VKVertexInput.h/
VKVertexInputAttribute.h/VKVertexInput.cpp）。

## 6. VKShaderStage.cpp / VKPipelineResolver.cpp 收尾

- `VKShaderStage.cpp`：`shader_stage_name_list` 删 4 个废弃条目（Vertex/TessControl/
  TeseEval[原拼写错误]/Geometry），保留 Fragment/Compute/Task/Mesh/Raygen 等通用映射
- `VKPipelineResolver.cpp`：删 `has_tessellation_shader` 检查（恒 false 死逻辑——
  引擎无 tess shader），`pTessellationState` 直接置 nullptr

## 7. 回归门同步（S5）

回归门 20 处 VS→mesh：
- `stage.stage = ShaderStage::Vertex` ×4 → `Mesh`（ShaderStageKey 构造）
- `program_link.vertex_stage` ×6 → `mesh_stage`
- `metadata.vertex_stage_digest` ×3 → `mesh_stage_digest`
- `program_key.vertex_stage_digest` ×2 → `mesh_stage_digest`
- entry point `{Vertex, "main.vs"}` → `{Mesh, "main.msh"}`
- depth/masked 变体守卫 `has(Vertex)` → `has(Mesh)`，`safe_vs` → `safe_ms`

**V1 断言语义陷阱**：depth 变体的 mesh 守卫从 Vertex 改 Mesh 时，原「`!= npos` 含则 mismatch」
（期望 depth 不含 fragDataIndexID）**误写成 `lacks`（不含则 mismatch）**——语义反转导致误报。
修复：改回 `contains`。depth 的 `ResolveMaterialVertexVaryingConfig`（MaterialDefinitionRegistry.cpp）
在 depth_purpose 时 `emit_data_index_id=false`（除非 requires_alpha_evaluation），mesh 源按设计
不含 fragDataIndexID。

**W1 引擎遗漏**：`DescriptorContract.cpp` 生成 `MaterialDataIndexTable` 时手写
`VK_SHADER_STAGE_ALL_GRAPHICS`——kMeshFragment 改名（perl 只替换 `kAllGraphicsOrMesh` 字面量）
漏了这处手写常量 → 回归门 `stage_flags == kMeshFragment` 精确断言 FAIL → 改 `kMeshFragment`。
教训：**全局改名要 grep 手写常量**（`VK_SHADER_STAGE_ALL_GRAPHICS`/`VK_SHADER_STAGE_VERTEX_BIT`），
不能只替换命名字面量。

## 8. 提交链

```
76444eb5a 删 GenericMaterialBuilder 的 VS 死分支 + vs 三元/include
d4dcdaf0e 清除 VertexShaderAssembler/VertexVaryingConfig
823096561 修复部分VertexStage遗留代码（ShaderArtifactStore 缓存 bug + 回归门崩溃 + 脏日志）
8ec44f788 清理ShaderStage中的Vertex部分（S1-S5 + 本文档前身）
0be3c9fe1 [OK]废弃旧的kAllGraphicsOrMesh（kMeshFragment 精确化 + push constant VUID）
d0ffa47e4 清理误提交的 CMakeCache.txt 构建缓存 + gitignore
cfdc76d55 gitignore 追加 .cmake/ 与 .kateproject.build
0d92362ae 修复kMeshFragment改名遗漏与V1输出契约断言语义反转
f1272b106 删除VertexInputAttribute/VertexInputAttributeArray/VertexInputConfig/VertexInput（VBO 输入类）
```

## 9. 遗留/注意

- `GLSLCodeModuleFile.cpp` 的 stage 字符串解析（`// @ulre stage Vertex` 等）保留——
  通用模块元数据工具，非引擎顶点路径；shader 库当前无 `@ulre stage` 声明
- `glsl2spv.cpp` 工具自己的 `VK_SHADER_STAGE_ALL_GRAPHICS = 0x1F` 枚举保留（编译器工具枚举）
- `VertexInputMode` 枚举（VertexShaderNodeConfig.h）保留——SSBO 顶点数据读取模式
- 缓存格式变化（metadata 字段改名 + ProgramKey 删字段）→ 旧 shader 缓存失效，运行重建
- CMake 改动需 VS 重新 configure
