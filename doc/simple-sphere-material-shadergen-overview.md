# SimpleSphere 到 ShaderGen 的材质与 Shader 工作链

## 1. 分析范围

分析入口：

```text
example/Basic/SimpleSphere.cpp
```

关联范围：

```text
inc/hgl/mtl
src/ShaderGen
ShaderLibrary
```

本文说明 `SimpleSphere` 如何声明一个 `Lit` 材质，以及这个声明如何经过材质定义、GLSL 模块、渲染模板和编译缓存，最终形成可用于绘制的 Shader 程序。

## 2. 总体结构

ULRE 的材质系统不是“一个材质文件对应一个完整 GLSL 文件”，而是一个数据驱动的 Shader 生成系统：

```text
SimpleSphere.cpp
    |
    +-- PBRSurfaceRow
    +-- MaterialRecipe
    +-- GeometryVertexFormat
    +-- Texture / Sampler resources
          |
          v
MaterialDefinitionRegistry
          |
          +-- ShaderLibrary/material/lit.material.toml
          |
          v
ShaderCodeModuleRegistry
          |
          +-- GLSL module dependency graph
          +-- texture reference manifest
          |
          v
SceneRenderTemplateResolver
          |
          +-- ForwardLit / ForwardLitShadowedAO template
          |
          v
MeshTemplateComposer
FragmentTemplateComposer
          |
          +-- ShaderDocument
          |
          v
MaterialShaderCompiler
          |
          +-- descriptor/resource schema
          +-- GLSL
          +-- SPIR-V
          +-- artifact cache
```

可以按职责分成四层：

| 层 | 位置 | 职责 |
| --- | --- | --- |
| 运行时使用层 | `example/Basic/SimpleSphere.cpp`、ECS、SceneGraph | 创建几何、材质实例、纹理和运行时资源绑定 |
| 公共合同层 | `inc/hgl/mtl` | 定义 Recipe、材质定义、Shader 模块、模板和资源 ABI |
| Shader 生成实现层 | `src/ShaderGen` | 解析、解析依赖、选择模板、生成 GLSL、编译和缓存 |
| 声明与实现资源层 | `ShaderLibrary` | `.material.toml` 材质定义和可组合 GLSL 模块 |

## 3. `SimpleSphere.cpp` 做了什么

### 3.1 创建 PBR 材质数据行

`InitMaterialDataSSBO()` 通过 `MaterialSSBOBufferRegistry` 获取：

```cpp
MaterialDataAccessor<PBRSurfaceRow>
```

并写入：

```text
base_color   = white
metallic     = 0.08
roughness    = 0.92
normal_scale = 0.35
```

`PBRSurfaceRow` 位于：

```text
inc/hgl/graph/ssbo/MaterialDataRows.h
```

它是 CPU 侧材质数据和 GLSL 侧材质读取逻辑之间的 ABI。生成的 Shader 会通过 `MTL_ROW(...)` 一类访问宏读取对应的数据行。

### 3.2 创建 `MaterialRecipe`

`InitMaterial()` 设置：

```text
recipe_name = "SimpleSphere.Lit"
mtl_def_id  = "Lit"
pipeline    = MakeSolid3DConfig()
material_ssbo_binding = PBRSurfaceRow 的 binding
```

其中 `mtl_def_id = "Lit"` 是整个 ShaderGen 链路的关键连接键。Recipe 本身不包含 GLSL，也不直接持有最终 Vulkan Shader 对象，它只是运行时材质实例的声明。

### 3.3 绑定纹理和采样器

示例加载：

```text
res/image/Brickwall/Albedo.Tex2D
res/image/Brickwall/Normal.Tex2D
res/image/Brickwall/Roughness.Tex2D
```

并使用材质语义绑定：

```text
base_color
normal
roughness
```

这些名称必须与 `lit.material.toml` 和材质 GLSL 中的资源声明保持一致。

### 3.4 创建几何

球体的顶点格式是：

```text
Position : VF_V3F
TexCoord : VF_V2HF
Normal   : VF_V2UN8
```

也就是三维位置、半精度 UV 和压缩法线。之后，几何和 Recipe 被包装成：

```text
PrimitiveAsset(sphere_geometry, &sphere_recipe, PrimitiveType::Triangles)
```

再由 `PrimitiveComponent` 绑定到 ECS 实体。

### 3.5 提交到 ECS 渲染系统

`PrimitiveComponent` 保存并连接：

```text
几何体
MaterialRecipe
纹理资源
材质 SSBO 数据资源
可见性
```

因此入口侧的关系是：

```text
SimpleSphereApp
    -> Entity
    -> PrimitiveComponent
    -> PrimitiveAsset
    -> MaterialRecipe
    -> MaterialDefinition / Shader Program
```

`SimpleSphere.cpp` 没有直接调用 `MaterialShaderCompiler`。Shader 解析、生成和编译在材质程序解析或首次使用时由下游渲染系统触发。

## 4. `ShaderLibrary/material/lit.material.toml`

`Lit` 定义是 SimpleSphere 的直接目标。它声明材质“需要什么”和“使用哪些 Shader provider”，而不是提供一份完整 Shader。

主要内容如下：

```text
材质 ID:
    Lit

顶点输入需求:
    Position
    UV0
    Normal

顶点到片元的 varying:
    emit_data_index_id
    emit_world_pos
    emit_world_normal
    emit_uv0

材质源码:
    material/pbr_surface_source.glsl

NTB 模块:
    ntb/ntb_tangent_vbo_normalmap.glsl

场景 UBO:
    CameraInfo
    SkyInfo

纹理声明:
    base_color
    roughness
    metallic
    occlusion
    opacity_mask
    normal

采样器预设:
    Trilinear
    Linear
```

`normal` 声明带有双通道配置，具体的法线贴图处理由材质源码和 NTB provider 协同完成。

## 5. `inc/hgl/mtl` 的结构

### 5.1 材质定义和 Recipe

```text
MaterialRecipe.h
MaterialDefinitionFile.h
MaterialDefinitionRegistry.h
```

负责：

- 描述运行时材质实例；
- 加载、查找、规范化 schema 3 材质定义；
- 将 Recipe 与选中的材质定义合并；
- 把材质源码和 NTB 模块注入渲染模板。

材质定义只有一个来源和一条统一的查询、生成链：

| 载体 | 用途 |
| --- | --- |
| `ShaderLibrary/material/*.material.toml` | 所有材质定义，包括 fallback、纯色、文本和错误棋盘格；解析器只接受 `schema = 3` |

`TryGetMaterialDefinitionByID` 只查询 `MaterialDefinitionFileRegistry`。fallback 直接使用 `builtin/pure_color` 文件定义；如果材质目录缺失，系统会显式报错，不再回退到 C++ 内嵌定义。

错误棋盘格定义为：

```text
builtin/checkerboard_2d
builtin/checkerboard_3d
```

2D 版本直接使用 `gl_FragCoord` 生成黑/灰固定像素网格，不依赖 UV 或材质数据。3D 版本使用世界位置做三平面棋盘格映射并进行固定方向光照：几何有 Normal 时使用顶点法线；没有 Normal 时用世界空间三角形位置导出的两条边 cross 重建面法线。当前两种颜色固定为黑色和灰色，后续可在这两个 provider 中扩展错误类型到颜色的映射。

### 5.2 GLSL 模块与资源清单

```text
ShaderCodeModule.h
ShaderCodeModuleFile.h
ShaderCodeModuleMetadata.h
ShaderCodeModuleRegistry.h
ShaderCodeModuleCapabilityResolver.h
ShaderCodeResourceManifest.h
```

这些类型把 ShaderLibrary 中的 GLSL 文件抽象成模块，记录：

- 模块名称和路径；
- 模块源码；
- 依赖关系；
- 能力声明；
- 纹理引用；
- 冲突和兼容性。

### 5.3 模板和阶段接口

```text
RenderTemplate.h
ResolvedRenderTemplate.h
SceneRenderTemplateResolver.h
MeshTemplateComposer.h
FragmentTemplateComposer.h
MaterialStageInterface.h
MaterialVertexVaryingConfig.h
VertexNodeConfig.h
VertexNodeConfigResolver.h
```

负责描述和解析：

- 顶点输入；
- 顶点变换节点；
- varying；
- 材质 provider；
- 光照 provider；
- 输出策略；
- 不同 Render Pass 的模板选择。

### 5.4 Descriptor 和资源合同

```text
DescriptorContract.h
DescriptorResourceCatalog.h
DescriptorSemantic.h
MaterialCoverageContract.h
MaterialOutputContract.h
ShaderResourceSchema.h
CanonicalShaderContract.h
ShaderLinkSpec.h
```

这部分保证 CPU 资源绑定、Shader 资源声明和阶段接口一致，主要检查：

- UBO、SSBO、纹理和 sampler 是否完整；
- descriptor 语义和布局是否一致；
- 顶点到片元的接口是否匹配；
- 材质输出是否满足模板要求。

### 5.5 编译和 Artifact

```text
ShaderBuildContext.h
MaterialShaderCompiler.h
ShaderProgramArtifactBuilder.h
ShaderArtifactContract.h
ShaderArtifactStore.h
ShaderProgramKey.h
ShaderKeyUtility.h
```

负责构建上下文、程序 key、GLSL/SPIR-V 产物、编译器和设备相关 Hash，以及磁盘缓存。

## 6. `src/ShaderGen` 的实现流程

### 6.1 材质定义解析

目录：

```text
src/ShaderGen/material_definition/
```

主要文件：

```text
MaterialDefinitionFile.cpp
MaterialDefinitionRegistry.cpp
```

`MaterialDefinitionFile.cpp` 只解析普通 File 材质的 schema 3 TOML，将以下内容转换为内部定义：

- 材质 ID；
- transform 配置；
- material source；
- NTB module；
- vertex requirements；
- varying；
- UBO、纹理和 sampler；
- provider policy；
- 可选的顶点法线模式（例如棋盘格材质的 `OptionalFaceFallback`）。

所有材质，包括纯色、文本和棋盘格，均通过同一个 schema 3 文件解析、模板解析、资源合同和 SPIR-V cook 流程。

### 6.2 GLSL 模块注册和依赖解析

目录：

```text
src/ShaderGen/glsl_module/
```

主要过程：

1. 通过 `ShaderLibraryPath` 定位 ShaderLibrary；
2. 递归扫描 `.glsl` 文件；
3. 解析模块元数据；
4. 注册模块名称；
5. 建立依赖图；
6. 检查重复、冲突和能力；
7. 展开模块依赖；
8. 收集纹理引用；
9. 生成稳定的模块图 Hash。

纹理引用清单会与材质定义中的纹理声明交叉验证，以便在编译前发现资源名称或资源能力不一致。

### 6.3 顶点阶段生成

主要实现位于：

```text
src/ShaderGen/builder/
src/ShaderGen/meshgen/
src/ShaderGen/template/
```

顶点 Shader 由以下输入共同决定：

```text
GeometryVertexFormat
MaterialDefinition.vertex.requirements
MaterialDefinition.vertex.varyings
transform 配置
shader mode
```

对于 SimpleSphere，几何提供的 Position、UV0 和 Normal 正好满足 Lit 定义的要求。

### 6.4 渲染模板选择

`SceneRenderTemplateResolver` 根据场景模板请求、材质能力和渲染用途选择最终模板。Lit 路径会进入 Forward Lit 模板族；包含 NTB 时，实际路径可以解析到带阴影和 AO provider 的 `ForwardLitShadowedAO` 模板。

场景 provider 通常包括：

```text
Surface
Direct Light
Shadow
Ambient Light
Ambient Occlusion
Lighting Model
Output Policy
```

材质定义自身提供的两个重要能力槽位是：

```text
MaterialSourceProvider
NTBProvider
```

对于 Lit，它们分别指向：

```text
material/pbr_surface_source.glsl
ntb/ntb_tangent_vbo_normalmap.glsl
```

### 6.5 片元阶段生成

核心实现：

```text
src/ShaderGen/template/FragmentTemplateComposer.cpp
```

生成的片元 Shader 通常由以下部分组成：

```text
#version 和宏定义
descriptor/resource 声明
CameraInfo / SkyInfo UBO
surface interface
material source
NTB/normal mapping
direct / indirect lighting
shadow / AO provider
lighting model
alpha/compositor
fragment output
main()
```

因此 `pbr_surface_source.glsl` 只是材质属性来源模块，不是完整的 Lit fragment shader。

片元阶段只使用 native fragment template 和 `ShaderDocument` 组合；旧 Fragment assembler、输出 marker 替换和 fallback assembler 路径不再是可达生产路径。

### 6.6 编译和缓存

目录：

```text
src/ShaderGen/compile/
```

典型流程：

1. 校验材质定义和模块资源清单；
2. 构建 descriptor entries；
3. 创建 `ShaderBuildContext`；
4. 生成 mesh/vertex 和 fragment 文档；
5. 序列化为 GLSL；
6. 构建并校验 resource schema；
7. 计算程序和阶段 Hash；
8. 查询 `ShaderArtifactStore`；
9. 缓存命中时直接使用 SPIR-V；
10. 未命中时调用 GLSL 编译器并保存 GLSL、SPIR-V 和元数据。

Artifact 元数据会记录程序 key、模块图 Hash、接口 Hash、输出合同 Hash、阶段 digest、编译器 profile、设备目标和生成源码 digest。

## 7. ShaderLibrary 的模块分类

### 7.1 材质模块

```text
ShaderLibrary/material/
```

关键文件：

```text
lit.material.toml
pbr_surface_source.glsl
```

其它材质定义包括：

```text
pure_color.material.toml
checkerboard_2d.material.toml
checkerboard_3d.material.toml
text_2d_gpu.material.toml
text_2d_gpu_bitmap.material.toml
unlit_texture.material.toml
vertex_color.material.toml
vertex_luminance.material.toml
vertex_palette_color.material.toml
sky_minimal.material.toml
debug_normal_color.material.toml
```

它们使用相同的 ShaderGen 框架，但提供不同的材质 source、顶点需求、输出策略或模板能力。

### 7.2 通用接口模块

```text
ShaderLibrary/common/
```

包括：

```text
descriptor_macros.glsl
bindless_textures.glsl
material_source_interface.glsl
surface_interface.glsl
lighting_interface.glsl
ntb_interface.glsl
l2w_ssbo.glsl
alpha_compositor.glsl
```

这些模块提供材质输入输出结构、surface 接口、纹理访问、SSBO/BDA 访问和 alpha 合成等公共合同。

### 7.3 顶点模块

```text
ShaderLibrary/vertex/
```

模块名大致按阶段划分：

```text
s1_* : 顶点属性读取
s2_* : 坐标提升、转换或透传
s3_* : 世界/相机变换和裁剪空间输出
```

SimpleSphere 相关模块包括：

```text
s1_position_vec3.glsl
s1_uv_rg16f.glsl
s1_ntb_rg8.glsl
s2_passthrough3d.glsl
s3_world_camera_vp.glsl
```

### 7.4 NTB、光照和合成模块

```text
ShaderLibrary/ntb/
ShaderLibrary/lighting/
ShaderLibrary/compositor/
ShaderLibrary/surface/
```

Lit 路径的主要模块包括：

```text
ntb/ntb_tangent_vbo_normalmap.glsl
lighting/direct_cook_torrance_pbr.glsl
lighting/indirect_sky_ambient.glsl
lighting/forward_pbr.glsl
compositor/forward_lighting.glsl
surface/material_surface.glsl
```

它们分别参与法线贴图、直接光照、天空环境光、PBR 光照模型、光照合成和最终 surface 输出。

### 7.5 UBO、Sampler 和特殊模式

```text
ShaderLibrary/ubo/
ShaderLibrary/sampler.toml
ShaderLibrary/mesh/
```

UBO 模块包括 `camera_info.glsl` 和 `sky_info.glsl`。`sampler.toml` 定义 `Trilinear`、`Linear` 等采样器预设。`mesh/` 下的字符四边形和线四边形模板服务于其它特殊绘制模式，不是球体路径的核心。

## 8. Lit/PBR 数据流

### 8.1 材质 SSBO

```text
PBRSurfaceRow
    |
    v
MaterialSSBOBufferRegistry
    |
    v
material SSBO binding / data index
    |
    v
MTL_ROW(...)
    |
    v
pbr_surface_source.glsl
    |
    v
MaterialSourceOutput
```

### 8.2 纹理资源

```text
TextureManager::LoadTexture2D
    |
    v
PrimitiveComponent::SetMaterialTextureResource
    |
    v
base_color / normal / roughness
    |
    v
descriptor / bindless texture table
    |
    v
MTL_TEX(...)
```

### 8.3 最终片元逻辑

```text
PBRSurfaceRow 默认值
    + base_color 纹理
    + roughness 纹理
    + normal 纹理
          |
          v
MaterialSourceOutput
          |
          v
NTB / normal mapping
          |
          v
Forward PBR lighting
          |
          v
alpha / output compositor
          |
          v
framebuffer
```

## 9. 关键设计结论

1. `.material.toml` 描述材质能力和资源需求，`.glsl` 实现可复用模块，`MaterialRecipe` 提供本次运行时实例数据。
2. `mtl_def_id = "Lit"` 是 SimpleSphere 与 `lit.material.toml` 之间的连接键。
3. `pbr_surface_source.glsl` 不是完整 Shader，而是被 Forward Lit 模板插入的 Material Source provider。
4. 材质资源名称是跨层合同，例如 `base_color` 必须在 C++、TOML、GLSL 和资源清单中一致。
5. 几何顶点格式必须满足材质定义的 vertex requirements；SimpleSphere 的 Position、UV0、Normal 与 Lit 定义匹配。
6. 生成结果受模块图、接口合同、设备 profile 和源码 digest 共同影响，因此 Shader 是可缓存的构建产物，而不是简单的运行时字符串。
7. SimpleSphere 只提交“几何 + Lit Recipe + PBR 数据 + 纹理资源”，真正的 Shader 组合、资源校验、GLSL 生成和 SPIR-V 编译发生在下游 ShaderGen/渲染管线中。

## 10. 相关文档

仓库中已有更细的专项资料：

```text
doc/material-lit-recipe-to-shadergen-dataflow.md
doc/simple-sphere-ecs-render-chain.md
doc/ShaderGen_ComposableTemplate_TechnicalDesign.md
doc/ShaderGen_MaterialGLSL_DesignAnalysis.md
```
