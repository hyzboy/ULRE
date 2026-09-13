# Lit 材质数据流全链路分析

> 分析入口：`example/Basic/SimpleSphere.cpp`（正向）与 `ShaderLibrary/material/lit.material.toml`（反向）。
> 分析日期：2026-09-13。基于分支 `RemoveTexture2DArray`（commit be1798181 之后）。

整个体系是一个三层结构，`.material.toml` 与 C++ 代码在中间汇合：

| 层 | 结构 | 含义 | 来源 |
|---|---|---|---|
| Layer 1 | `MaterialDefinition` | 材质**能做什么**（能力超集） | `lit.material.toml` 解析 |
| Layer 2 | `MaterialRecipe` | 这次渲染**要什么**（实例输入） | 应用代码（SimpleSphere） |
| Layer 3 | `MaterialDefinitionBuildRequest` | 构建上下文（recipe + 几何 + purpose + 模板） | 渲染系统组装 |

`recipe.mtl_def_id` 是两层之间**唯一的连接键**——示例代码里 `"Lit"` 这个字符串就是通往 toml 文件的全部线索。

## 一、正向：SimpleSphere.cpp → Recipe

`example/Basic/SimpleSphere.cpp` 的 `InitMaterial()`（第 107 行）只做了三件事：

1. `sphere_recipe.mtl_def_id = "Lit"`（第 116 行）——声明"我要 Lit 光照"；
2. `material_ssbo_binding = accessor.GetMaterialSSBOBinding()`——SSBO Arena 里一行 `PBRSurfaceRow`（base_color/metallic/roughness/normal_scale，32B，定义在 `inc/hgl/graph/ssbo/MaterialDataRows.h:20`）的句柄；
3. 通过 `PrimitiveComponent::SetMaterialTextureResource("base_color"/"normal"/"roughness", tex, sampler)` 挂纹理。

`MaterialRecipe` 结构本身（`inc/hgl/mtl/MaterialRecipe.h:480`）不含任何 GLSL 或 Vulkan 对象——它纯粹是声明式输入。

## 二、反向：lit.material.toml → MaterialDefinition

进程内第一次有代码碰材质注册表时（`ShaderProgramManager` 构造函数和 `GetMaterialDefinitionFileRegistry()`，`src/ShaderGen/material_definition/MaterialDefinitionRegistry.cpp:200`），会惰性扫描 `ShaderLibrary/material/` 下所有 `*.material.toml`，每个文件经 `ParseMaterialDefinitionFile`（`MaterialDefinitionFile.cpp:1176`，用 tomlpp 解析）变成一个 `MaterialDefinition`。

`lit.material.toml` 的每个字段去向：

| TOML 字段 | 去向 |
|---|---|
| `id = "Lit"` | `definition_id`，即 `mtl_def_id` 的查找主键 |
| `[transform]`（Vec3Position/Passthrough3D/World/WorldCameraVP） | `vertex_node_config`，决定 mesh 阶段的顶点变换节点图 |
| `[fragment].material_source_module = "material/pbr_surface_source.glsl"` | `material_source_module`，最终成为片元模板的 **MaterialSourceProvider** 槽位 |
| `[fragment].ntb_module = "ntb/ntb_tangent_vbo_normalmap.glsl"` | `ntb_module` → **NTBProvider** 槽位；**它的存在直接决定走 PBR 光照路径**（见第三节） |
| `[vertex].requirements = [Position, UV0, Normal]` | `vertex_semantic_requirements`，与几何顶点格式做能力匹配 |
| `[vertex].varyings = [emit_data_index_id, emit_world_pos, emit_world_normal, emit_uv0]` | `MaterialVertexVaryingConfig` → 决定 mesh/fragment 两阶段的插值接口 |
| `[resources].ubos = [CameraInfo, SkyInfo]` | `ubo_requirements` → 场景 UBO 描述符 |
| `[resources].textures`（6 个，normal 带 `channels = 2`） | `texture_declarations` → **逐名**生成 GLSL 纹理引用结构体（见第五节） |
| `[resources].samplers = [Trilinear, Linear]` | `sampler_names` → 生成 `#define TrilinearSampler <idx>` 之类的 bindless 采样器索引宏 |

解析带键白名单校验：任何不认识的键都是硬失败（`ValidateMaterialDefinitionKeys`），防止"作者以为配置生效实际被忽略"。

## 三、运行时触发：ECS → ShaderProgramManager

每帧 `RenderPrimitiveCollectSystem::ResolveMaterialProgramForPrimitive`（`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:387`）做脏检查后：

1. `BuildResolvedRecipe` 从 PrimitiveComponent 收集有效 recipe；
2. `TryGetMaterialDefinitionByID("Lit")` 拿到定义；
3. `SelectCurrentSceneRenderTemplateRequest`（`src/SceneGraph/module/ShaderProgramManager.cpp:211`）做**路由决策**：Lit 有 `ntb_module` → 选 `ForwardLitShadowedAO` 模板，并填 `MakeIdentityForwardLitProfile()` 的场景槽位（`src/ShaderGen/template/SceneRenderTemplateResolver.cpp:5`）：
   - Surface → `surface/material_surface.glsl`
   - 直接光 → `lighting/direct_cook_torrance_pbr.glsl`
   - 阴影 → `shadow/identity.glsl`
   - 环境光 → `lighting/indirect_sky_ambient.glsl`
   - AO → `ao/identity.glsl`
   - 光照模型 → `lighting/forward_pbr.glsl`
   - 输出策略 → `compositor/forward_lighting.glsl`
4. `AppendMaterialRenderTemplateRoots` 把 toml 来的 `material/pbr_surface_source.glsl` 和 `ntb/ntb_tangent_vbo_normalmap.glsl` 作为两个**材质能力槽位**追加进模板请求；
5. `AcquireShaderProgram(request)`：先 `NormalizeRecipe`（把定义的默认渲染状态写回 recipe，之后 definition 退出参与），再 `BuildContextFromRequest` → `BuildGenericMaterial`。

## 四、ShaderGen 内部：BuildGenericMaterial 五阶段

`src/ShaderGen/builder/GenericMaterialBuilder.cpp:651`：

1. **ResolvePurposeAndCoverage**：校验模板请求，解析 coverage 合同（alpha test/dither）、按 purpose 裁剪 varying、构建 stage interface；
2. **ResolveVertexABI**：由几何的 `GeometryVertexFormat`（示例中 Position=V3F、TexCoord=V2HF、Normal=V2UN8）生成顶点输入 GLSL；
3. **BuildResourceContract**：从 `ShaderCodeModuleRegistry`（它惰性扫描整个 `ShaderLibrary/`，解析每个 `.glsl` 头部的 `// @ulre` 元数据块）取 provider 模块元数据，构建资源清单并**交叉验证**——`pbr_surface_source.glsl` 头里声明的 `texture_reference base_color/roughness/...` 必须被 toml 的 `texture_declarations` 覆盖，否则构建失败；
4. **GenerateStageSources**：`MeshTemplateComposer` 生成 mesh 阶段；`FragmentTemplateComposer::ComposeForwardLit`（`src/ShaderGen/template/FragmentTemplateComposer.cpp:488`）按模板拼装片元 `ShaderDocument`；
5. **FinalizeProgramLink** + `CompileMaterial`：对源码/接口/资源契约/编译器 profile 做哈希生成 stage key，交给 glslang 编译成 SPV（带磁盘缓存 `ShaderArtifactStore`，可用 `ULRE_SHADER_CACHE_MODE=readonly` 强制走离线 cook 产物）。

## 五、最终片元 shader 的组装形态

`ComposeForwardLit` 产出的不是内联代码，而是 `#version 450` + 宏 + 一串 `#include` + `main()`。关键对应关系：

- 宏区：`HGL_USE_MATERIAL_SOURCE_PROVIDER 1`、`HGL_USE_NTB_PROVIDER 1`、`HGL_USE_SCENE_LIGHTING 1`，以及 **`MTL_TEX_NORMAL_CHANNELS 2`——直接来自 toml 里 normal 纹理的 `channels = 2`**（BC5 双通道法线，Z 在 shader 还原）；
- 函数区 9 个槽位 include 中，`ForwardLit.MaterialSource` 就是 `#include "material/pbr_surface_source.glsl"`（toml 指定），`ForwardLit.NTB` 是 `ntb/ntb_tangent_vbo_normalmap.glsl`；
- `main()` 固定管线：装配 `SurfaceInput` → `EvalSurface(si, materialDataIndex)` → `BuildForwardLightingInput` → `EvalLighting` → 输出。

调用闭环在 GLSL 侧：

- `surface/material_surface.glsl` 的 `EvalSurface()` 调 `EvalMaterialSource()`——**这正是 `pbr_surface_source.glsl` 里实现的函数**；
- `lighting/forward_pbr.glsl` 的 `EvalLighting()` 做 Cook-Torrance 直接光 + 天空环境光合成，这就是 Lit 光照模型本体。

GLSL `#include` 的解析：`src/ShaderGen/compile/GLSLCompiler.cpp:277` 把 `ShaderLibrary/` 根和 `ShaderLibrary/common/` 作为 include 搜索路径传给 glslang 插件。

## 六、运行时数据如何接上 shader

`MaterialShaderEmitter.cpp:120`（`BuildMaterialSSBODeclarations`）在编译期生成两段 GLSL 注入文档：

- `struct PBRSurface {...}` + `layout(buffer_reference, scalar) buffer PBRSurfaceRow {...}`（成员表来自 C++ 侧 `GetMaterialSSBOStructGLSL(PBRSurface)`，与 `MaterialDataRows.h` 的 32B 布局一一对应），以及 `#define MTL_ROW(i) PBRSurfaceRow(...addr_mtl_data_addrs.values[(i)].payload_address)`——通过 push constant 持有的 **BDA 地址**解引用材质数据行；
- `buffer MaterialTextureReferencesRef { uvec2 tex_base_color; uvec2 tex_roughness; uvec2 tex_metallic; uvec2 tex_occlusion; uvec2 tex_opacity_mask; uvec2 tex_normal; }` + `#define MTL_TEX(i)`——**这个结构体的字段是逐名照抄 toml `[resources].textures` 的**。

于是运行时：

- SimpleSphere 写入的 `PBRSurfaceRow` 被 shader 通过 `MTL_ROW(source_input.dataIndex)` 读出（`material_data.metallic` 等）；
- `SetMaterialTextureResource` 挂的纹理在运行期物化为 bindless 描述符索引 + array layer（uvec2），shader 经 `SampleOptional(MTL_TEX(i).tex_base_color, TrilinearSampler, uv, vec4(1.0))` → `bindless_tex[handle-1]` 采样；
- 未绑定的槽由 `SampleOptional` 的 fallback 参数（1.0）保底，等价于乘性中性值。

## 七、整体链路图

```
SimpleSphere.cpp                          lit.material.toml
  sphere_recipe.mtl_def_id="Lit"  ──查找──▶ MaterialDefinitionFileRegistry
  PBRSurfaceRow(base_color…)              (ParseMaterialDefinitionFile)
  SetMaterialTextureResource(…)                  │ id/transform/fragment/vertex/resources
      │                                          ▼
      ▼                                   MaterialDefinition (Layer 1)
PrimitiveComponent ──每帧──▶ RenderPrimitiveCollectSystem
      │ BuildResolvedRecipe → MaterialRecipe (Layer 2)
      ▼
SelectCurrentSceneRenderTemplateRequest
  (有 ntb_module → ForwardLitShadowedAO + IdentityForwardLitProfile)
      ▼
ShaderProgramManager::AcquireShaderProgram
      │ NormalizeRecipe (Layer 2 合并 Layer 1 默认值)
      ▼
MaterialDefinitionBuildRequest (Layer 3: recipe + GVF + purpose + RenderTemplateRequest)
      ▼
BuildGenericMaterial (ShaderGen)
  ├─ Phase1 ResolvePurposeAndCoverage (coverage/varying/stage-interface)
  ├─ Phase2 ResolveVertexABI (GeometryVertexFormat)
  ├─ Phase3 BuildResourceContract (manifest + descriptors + 交叉验证)
  ├─ Phase4 MeshTemplateComposer + FragmentTemplateComposer
  │      └─ #include 槽位: sky/direct/indirect/shadow/ao/lighting/
  │                        material_source★/ntb★/surface   (★ 来自 toml)
  └─ Phase5 MaterialShaderEmitter (MTL_ROW/MTL_TEX/BDA) → glslang → SPV → ShaderModule
```

## 八、结论

`SimpleSphere.cpp` 只声明 `"Lit"` 这个字符串和一行 32B 的 PBR 数据；`lit.material.toml` 在首次访问时被解析成 `MaterialDefinition` 提供能力声明（材质源模块、法线模块、纹理声明、varying）；每帧渲染收集系统把两者合成 Layer 3 构建请求，`BuildGenericMaterial` 按"场景模板槽位 + 材质能力槽位"拼出由 `#include` 和 BDA 宏组成的完整 GLSL 文档，最后 glslang 编译为 SPV——toml 的每一个字段（包括 `channels = 2` 这样的细节）都能在最终 shader 里找到确切落点。

## 附：调试入口

- 设 `ULRE_DUMP_GLSL=1`（Debug 构建，`GenericMaterialBuilder.cpp:590`）可导出拼装后的最终 mesh/fs GLSL；
- `src/Tools/ShaderGen/ShaderCooker.cpp` 可离线 cook 全变体供发布形态使用；
- `ULRE_SHADER_CACHE_MODE=readonly` 强制只读缓存（miss 即失败），用于验证 cook 覆盖完整性。
