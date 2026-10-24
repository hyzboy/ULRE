# Lit 材质数据流全链路分析

> **材质 provider 身份规范化**：材质只接受 schema 4；material-source、NTB、surface 只存 `@ulre name`，include 路径由注册表来源解析。Ambient 的**算法选择**（`ambient_module_name`）已上移到环境层（`EnvironmentSystem::SetAmbientProvider`），材质不再声明。

> **⚠ 2026-10-14 文卫生订正（B3）**：`PrimitiveComponent` 已删（A5）⇒ 文中"经 PrimitiveComponent 挂纹理 / BuildResolvedRecipe 从 PrimitiveComponent 收集"按**历史口径**读；现行 = `MaterialData` + `MaterialRecipe` + collect 侧求值（`RenderPrimitiveCollectSystem`）。
> 现行权威：`doc/ecs-layer-architecture-and-frame-flow.md`（ECS 层主档；§2.1 对象模型 / §5 组件面）＋ `doc/visibility-authority-chain.md`（可见性）＋ `doc/backlog.md` §2（逐条落地记录）。

> ⚠ **历史口径（2026-10-07 起）**：本文出现的 `SkyInfo` / `SkyInfo.h` / `addr_sky` / `SkyInfoRef` / `#define sky` / `EditSkyInfo` 已随 **sky 段整删** —— 两个外观字段（`base_sky_color` / `halo_intensity`）全仓无写者，取值搬成 `ShaderLibrary/sky/sky_atmosphere.glsl` 常量 `kBaseSkyColor` / `kHaloIntensity`（外观逐位不变）；世界表 `WorldAddresses` = **72 B / 9 字段**，无 `addr_sky`。相关段落读作当时形态。

> **订正（2026-09-28）**：相机 / 环境 / 地址表断言已按权威文档 `doc/world-addresses-and-camera-model-plan.md`（§1、§4、§7）就地修订，本文改动项：
> ① toml `[resources].ubos = [CameraInfo, SkyInfo]` **不再是"场景 UBO 描述符"**——Scene 集已整体退场，camera 经**相机行**（世界级数据，shader 用 `pc_root.camera_row` 索引）、sky 经**地址**（现为 `global_addresses.addr_sky`，批次 C2 后进世界表 `WorldAddresses`）消费；
> ② `MTL_ROW` 的基址来自 **`GlobalAddresses` SSBO（无绑定无集，`HGL_FRAME_SLOT_TOTAL` 帧槽 × 128B 步长）**，经 `pc_root.addr_global_addresses` 下发，不是"Scene 集里的 `GlobalAddressesInfo` UBO"；
> ③ `RootAddresses` push constant = **80B**（首字段 `addr_global_addresses`，末位 `camera_row` + `_pad_camera`）。

> 分析入口：`example/GettingStarted/SimpleSphere.cpp`（正向）与 `ShaderLibrary/material/lit.material.toml`（反向）。
> 分析日期：2026-09-13。基于历史分支 `RemoveTexture2DArray`（`git branch -a` 现仅在远端 `hyzgame/RemoveTexture2DArray` 存在，本地分支列表已无此分支）。
> 符号与 `path:line` 已按当前代码（描述符退役后的 BDA 终态）校正。

整个体系是一个三层结构，`.material.toml` 与 C++ 代码在中间汇合：

| 层 | 结构 | 含义 | 来源 |
|---|---|---|---|
| Layer 1 | `MaterialDefinition` | 材质**能做什么**（能力超集） | `lit.material.toml` 解析 |
| Layer 2 | `MaterialRecipe` | 这次渲染**要什么**（实例输入） | 应用代码（SimpleSphere） |
| Layer 3 | `MaterialDefinitionBuildRequest` | 构建上下文（recipe + 几何 + purpose + 模板） | 渲染系统组装 |

`recipe.mtl_def_id` 是两层之间**唯一的连接键**——示例代码里 `"Lit"` 这个字符串就是通往 toml 文件的全部线索。

## 一、正向：SimpleSphere.cpp → Recipe

`example/GettingStarted/SimpleSphere.cpp` 的 `InitMaterial()`（第 107 行）只做了三件事：

1. `sphere_recipe.mtl_def_id = "Lit"`（第 116 行）——声明"我要 Lit 光照"；
2. `material_ssbo_binding = accessor.GetGlobalSSBOBinding()`——全局 SSBO Arena 里一行 `PBRSurfaceRow`（base_color/metallic/roughness/normal_scale/fresnel，32B，定义在 `inc/hgl/graph/ssbo/MaterialDataRows.h:20`）的句柄，类型为 `GlobalSSBOBinding{ssbo_type, ssbo_id, data_index}`（`inc/hgl/graph/ssbo/GlobalSSBOTypes.h:64`；取法 `inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:67`）；
3. 通过 `PrimitiveComponent::SetMaterialTextureResource("base_color"/"normal"/"roughness", tex, sampler)` 挂纹理。

`MaterialRecipe` 结构本身（`inc/hgl/mtl/MaterialRecipe.h:499`）不含任何 GLSL 或 Vulkan 对象——它纯粹是声明式输入。

## 二、反向：lit.material.toml → MaterialDefinition

进程内第一次有代码碰材质注册表时（`ShaderProgramManager` 构造函数和 `GetMaterialDefinitionFileRegistry()`，`src/ShaderMaterialData/MaterialDefinitionRegistry.cpp:184`），会惰性扫描 `ShaderLibrary/material/` 下所有 `*.material.toml`，每个文件经 `ParseMaterialDefinitionFile`（`src/ShaderMaterialData/MaterialDefinitionFile.cpp:1211`，用 tomlpp 解析）变成一个 `MaterialDefinition`。

`lit.material.toml` 的每个字段去向：

| TOML 字段 | 去向 |
|---|---|
| `id = "Lit"` | `definition_id`，即 `mtl_def_id` 的查找主键 |
| `[transform]`（Vec3Position/Passthrough3D/World/WorldCameraVP） | `vertex_node_config`，决定 mesh 阶段的顶点变换节点图 |
| `[fragment].material_source_module_name = "pbr_surface_source"` | `material_source_module_name`，按注册名成为片元模板的 **MaterialSourceProvider** 槽位 |
| `[fragment].ntb_module_name = "ntb_tangent_vbo_normalmap"` | `ntb_module_name` → **NTBProvider** 槽位；**它的存在直接决定走 PBR 光照路径**（见第三节） |
| `[vertex].requirements = [Position, UV0, Normal]` | `vertex_semantic_requirements`，与几何顶点格式做能力匹配 |
| `[vertex].varyings = [emit_data_index_id, emit_world_pos, emit_world_normal, emit_uv0]` | `MaterialVertexVaryingConfig` → 决定 mesh/fragment 两阶段的插值接口 |
| `[resources].ubos = [CameraInfo, SkyInfo]` | `ubo_requirements` → **场景数据需求**（不再是描述符集绑定：Scene 集已整体退场）——camera 由相机行 `cameras[pc_root.camera_row]` 提供、sky 由 `SkyInfoRef(global_addresses.addr_sky)` 解引用（C2 后为 `world.addr_sky`） |
| `[resources].textures`（6 个，normal 带 `channels = 2`） | `texture_declarations` → **逐名**生成 GLSL 纹理引用结构体（见第五节） |
| `[resources].samplers = [Trilinear, Linear, ShadowMap, ShadowPCF]` | `sampler_names` → 生成 `#define TrilinearSampler <idx>` 之类的 bindless 采样器索引宏（`BuildSamplerMacros`，`src/ShaderGen/compile/MaterialShaderEmitter.cpp:112`） |

解析带键白名单校验：任何不认识的键都是硬失败（`ValidateMaterialDefinitionKeys`），防止"作者以为配置生效实际被忽略"。

## 三、运行时触发：ECS → ShaderProgramManager

每帧 `RenderPrimitiveCollectSystem::ResolveMaterialProgramForPrimitive`（`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:389`）做脏检查后：

1. `BuildResolvedRecipe` 从 PrimitiveComponent 收集有效 recipe；
2. `TryGetMaterialDefinitionByID("Lit")` 拿到定义；
3. `SelectCurrentSceneRenderTemplateRequest`（`src/SceneGraph/module/ShaderProgramManager.cpp`）做**路由决策**——判定只有一份实现：`mtl::SelectSceneRenderTemplateRequest`（`src/ShaderMaterialData/MaterialDefinitionRegistry.cpp`，运行时与文档生产回归工具共用），SceneGraph 侧仅转发。Lit 有 `ntb_module_name` → 选 `ForwardLitShadowedAO` 模板，并填 `MakeIdentityForwardLitProfile()` 的场景槽位（`src/ShaderMaterialData/SceneRenderTemplateResolver.cpp`）：
   - Surface → `surface/material_surface.glsl`
   - 直接光 → `lighting/direct_cook_torrance_pbr.glsl`
   - 天光（SkyProvider）→ `sky/sky_atmosphere.glsl`
   - 阴影 → `shadow/pcf_shadow.glsl`
   - 环境光 → `lighting/indirect_sky_ambient.glsl`（**环境层**选定 provider 时被覆盖，如 `EnvironmentSystem::SetAmbientProvider("indirect_ibl")`；只写注册名，include 路径由注册表来源解析）
   - AO → `ao/identity.glsl`
   - 光照模型 → `lighting/forward_pbr.glsl`
   - 输出策略 → `compositor/forward_lighting.glsl`
4. `AppendMaterialRenderTemplateRoots` 把 toml 来的 `pbr_surface_source` 和 `ntb_tangent_vbo_normalmap` 作为两个 **name-only 材质能力槽位**追加进模板请求；Sky 模板的 surface 名字覆盖也集中在此，运行时与文档生产共用；
5. `AcquireShaderProgram(request)`：先 `NormalizeRecipe`（把定义的默认渲染状态写回 recipe，之后 definition 退出参与），再 `BuildContextFromRequest` → `BuildGenericMaterial`。

## 四、ShaderGen 内部：BuildGenericMaterial 五阶段

`src/ShaderGen/builder/GenericMaterialBuilder.cpp:702`：

1. **ResolvePurposeAndCoverage**：校验模板请求，按注册文件来源补齐规范 include 路径并校验名字/槽位/路径一致性，再用 canonical request 计算模板身份；解析 coverage 合同（alpha test/dither）、按 purpose 裁剪 varying、构建 stage interface；
2. **ResolveVertexABI**：由几何的 `GeometryVertexFormat`（示例中 Position=V3F、TexCoord=V2HF、Normal=V2UN8）生成顶点输入 GLSL；
3. **BuildResourceContract**：从 `ShaderCodeModuleRegistry`（它惰性扫描整个 `ShaderLibrary/`，解析每个 `.glsl` 头部的 `// @ulre` 元数据块）取 provider 模块元数据，构建资源清单并**交叉验证**——`pbr_surface_source.glsl` 头里声明的 `texture_reference base_color/roughness/...` 必须被 toml 的 `texture_declarations` 覆盖，否则构建失败；
4. **GenerateStageSources**：`MeshTemplateComposer` 生成 mesh 阶段；`FragmentTemplateComposer::Compose`（`src/ShaderGen/template/FragmentTemplateComposer.cpp:614`）按模板 slots 拼装片元 `ShaderDocument`；
5. **FinalizeProgramLink** + `CompileMaterial`：对源码/接口/资源契约/编译器 profile 做哈希生成 stage key，交给 glslang 编译成 SPV（带磁盘缓存 `ShaderArtifactStore`，可用 `ULRE_SHADER_CACHE_MODE=readonly` 强制走离线 cook 产物）。

## 五、最终片元 shader 的组装形态

`Compose` 产出的不是内联代码，而是以手写模板 `ShaderLibrary/fragment/forward_lit.glsl.tmpl`（含 `main()`）为骨架、按 `{{slot}}` 占位符填入后的 `#version 450` + 宏 + 一串 `#include`。关键对应关系：

- 宏区：`HGL_USE_MATERIAL_SOURCE_PROVIDER 1`、`HGL_USE_NTB_PROVIDER 1`、`HGL_USE_SCENE_LIGHTING 1`，以及 **`MTL_TEX_NORMAL_CHANNELS 2`——直接来自 toml 里 normal 纹理的 `channels = 2`**（BC5 双通道法线，Z 在 shader 还原）；
- 函数区槽位 include 由 `ShaderModuleSlotRole` 定位：`MaterialSourceProvider` 即 `#include "material/pbr_surface_source.glsl"`，`NTBProvider` 即 `ntb/ntb_tangent_vbo_normalmap.glsl`。TOML 指定的是注册名，路径来自注册来源，不再按文件 basename 猜名字；
- `main()` 固定管线：装配 `SurfaceInput` → `EvalSurface(si, materialDataIndex)` → `BuildForwardLightingInput` → `EvalLighting` → 输出。

调用闭环在 GLSL 侧：

- `surface/material_surface.glsl` 的 `EvalSurface()` 调 `EvalMaterialSource()`——**这正是 `pbr_surface_source.glsl` 里实现的函数**；
- `lighting/forward_pbr.glsl` 的 `EvalLighting()` 做 Cook-Torrance 直接光 + 天空环境光合成，这就是 Lit 光照模型本体。

GLSL `#include` 的解析：`src/ShaderGen/compile/GLSLCompiler.cpp` 只向插件传递一个
ShaderLibrary 根目录。所有 include 都写库根相对路径（如 `common/descriptor_macros.glsl`），
嵌套 include 也不搜索包含文件所在目录、当前工作目录或隐式的 `common/`。

## 六、运行时数据如何接上 shader

`src/ShaderGen/compile/MaterialShaderEmitter.cpp:197`（`BuildMaterialSSBODeclarations`）在编译期生成两段 GLSL 注入文档：

- `struct PBRSurfaceData {...}` + `layout(buffer_reference, scalar, buffer_reference_align=16) buffer PBRSurfaceRow {...}`（成员表来自 C++ 侧 `ssbo::GetGlobalSSBOStructGLSL(GlobalSSBOType::PBRSurface)`、行名来自 `ssbo::GetGlobalSSBORowName()`，与 `MaterialDataRows.h` 的 32B 布局一一对应；`inc/hgl/graph/ssbo/MaterialSSBOLayout.h:65`/`:41`），以及 `#define MTL_ROW(i) PBRSurfaceRow(global_addresses.addr_pbr_surface + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].payload_index) * uint64_t(32))`——旧式 `...values[(i)].payload_address` 写法已退役，行号字段现为 `payload_index`，基址来自 **`GlobalAddresses` SSBO（无绑定无集）** 的 `addr_pbr_surface`（`inc/hgl/graph/ubo/GlobalAddresses.h:31`）——该表基址经 `pc_root.addr_global_addresses`（`RootAddresses` 首字段，push constant 共 **80B**）下发，行号经 `pc_root.addr_mtl_data_addrs` 的 **BDA 地址**解引用；
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
  → mtl::SelectSceneRenderTemplateRequest (唯一实现,ShaderGen)
  (有 ntb_module_name → ForwardLitShadowedAO + IdentityForwardLitProfile
   环境层 ambient provider 覆盖 AmbientLightProvider 槽)
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

- `channels.shadergen = true`（Debug 构建；读取点 `src/ShaderGen/compile/MaterialShaderEmitter.cpp:77`）可导出拼装后的最终 mesh/fs GLSL；
- `ULRE_SHADER_CACHE_MODE=readonly` 强制只读缓存（miss 即失败），用于验证缓存覆盖完整性。
