# 材质 Shader 生成器重构工作计划

> **核心原则**
> - 每一步可以独立编译并用已有示例程序验证渲染结果
> - 硬编码材质（fallback/编辑器用）与 inja 模板材质并行存在，互不干扰
> - 渐进迁移，绝不整批切换
>
> **当前状态（已完成）**
> - ✅ `FixedMaterialDef.h`：`FixedDescriptorEntry` / `FixedVertexEntry` / `FixedMaterialDef` 数据结构
> - ✅ `ShaderPermutationKey`：4 轴（环境光/光照/高光/阴影）排列键及 `AppendGLSLDefines()`
> - ✅ `ShaderTemplateEngine`（inja）：模块加载、依赖解析、模板渲染框架（已有代码但注释掉）
> - ✅ `ShaderLibrary/`：templates/、modules/、recipes/、.mtl 文件树
> - ✅ `MaterialFileLoader`：现有 `.mtl` 文本文件解析器
> - ✅ `CompileFixedMaterial()`：从 FixedMaterialDef → MaterialCreateInfo 编译器（M0.1 完成）
> - ✅ `PermutationToRecipe.h`：ShaderPermutationKey → Recipe 模块名映射表（M0.3 完成）
> - ✅ `ShaderComposition.h` / `ShaderComposition_Examples.h`：合成型着色器框架（M0.4 完成）
> - ✅ `SHADER_COMPOSITION_ARCHITECTURE.md` / `HELPER_FUNCTION_AUTO_GENERATION.md`：架构设计文档（M0.4-0.5 参考）
> - ✅ `ComposedShaderGenerator.cpp`：完整 GLSL 生成实现（M0.5 ✓ 2026-02-24 完成）
>   - ✅ GenVertexInputStruct() / GenVSOutputStruct() / GenLightingOutputStruct()
>   - ✅ GenLayoutDeclarations() / GenCoordinateTransformFunctions()
>   - ✅ GenHelperFunctionLibrary() 及其所有子方法（GetLocalToWorld/GetNormalMatrix/GetNormal/GetPosition3D/GetMaterialInstance）
>   - ✅ GenOutputCompositionCode() / GenLightingCode()
>   - ✅ ComposeVertexShader() / ComposeFragmentShader() / ComposeGeometryShader()
>   - ✅ 已验证编译通过（ULRE.ShaderGen.lib）

---

## 阶段 0：基础设施补全（无需动现有材质，全部新增）

### 任务 0.1 — 实现 `CompileFixedMaterial()`

**目标**：可以用 `FixedMaterialDef` + `ShaderPermutationKey` 创建一个 `MaterialCreateInfo`。

**文件**
- 新增 `src/ShaderGen/MaterialCompiler.cpp`
- 新增 `inc/hgl/graph/mtl/MaterialCompiler.h`

**内容**

```cpp
/// MaterialCompiler.h
#pragma once
#include<hgl/graph/mtl/FixedMaterialDef.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/vk/VKDeviceAttribute.h>

namespace hgl::graph::mtl{
    MaterialCreateInfo *CompileFixedMaterial(
        const VulkanDevAttr *dev_attr,
        const FixedMaterialDef &def,
        const ShaderPermutationKey &key = ShaderPermutationKey{});
}
```

**实现要点**（`MaterialCompiler.cpp`）
1. 从 `def.descriptor_entries[N]` 直接按序填充 `MaterialDescriptorInfo`（无 `Resort()` 动态分配）
2. `key.AppendGLSLDefines(prefix)`，然后 `prefix + def.vert_glsl` 作为完整 GLSL 编译
3. 调用现有 `ShaderCreateInfo::CompileToSPV()` 流程（复用 glslang 封装）
4. 返回填充好的 `MaterialCreateInfo*`

**验证**
- 在现有任意示例中调用 `CompileFixedMaterial(dev_attr, MATERIAL_PURE_COLOR_DEF)` 并断言不为 nullptr
- 编译后不报 GLSL 错误

---

### 任务 0.2 — `inja` 子模块补全（`ShaderTemplateEngine` 解注释）

**目标**：让 `ShaderTemplateEngine` 可以编译进 `ULRE.ShaderGen`，单独测试 inja 渲染。

**文件**
- `src/ShaderGen/CMakeLists.txt`：把 `SHADER_TEMPLATE_ENGINE` 行去掉注释
- 确认 `3rdpty/inja` 子模块已 checkout（`git submodule update --init 3rdpty/inja`）

**验证**
- 执行 `cmake --build . --target ULRE.ShaderGen` 无错误
- 写一个不依赖 Vulkan 的单元测试：直接用 `ShaderTemplateEngine` 加载
  `ShaderLibrary/recipes/standard/metal.json` + 渲染到字符串，打印输出不为空

---

### 任务 0.3 — `ShaderPermutationKey → recipe JSON` 桥接层

**目标**：把 `ShaderPermutationKey` 中的枚举值映射到 recipe JSON 中的模块名称。

**文件**
- 新增 `inc/hgl/graph/mtl/PermutationToRecipe.h`

**内容（头文件 constexpr 表格，无 .cpp）**

```cpp
// 把 LightModel 枚举映射到 ShaderLibrary/modules/lighting/ 中的模块名
constexpr const char *LIGHT_MODEL_MODULE_NAMES[] = {
    nullptr,          // Unlit - 无模块
    "lambert",        // Lambert
    "blinn_phong",    // BlinnPhong
    "half_lambert",   // PBR_Lite → 暂用 half_lambert，后续换 pbr_lite
    "pbr_standard",   // PBR_Full
    nullptr           // CelShading - 待添加模块
};
constexpr const char *AMBIENT_MODEL_MODULE_NAMES[] = {
    nullptr,          // FlatColor - 无模块（纯色在 MI 里）
    nullptr,          // Hemisphere - 待添加模块
    "ibl_simple",     // IBL
    "ibl",            // IBL_SH - 暂用 full ibl
    nullptr           // MixedGI - 待添加模块
};
```

**验证**：编译通过即可（纯头文件）

---

## 阶段 0.5：合成驱动着色器系统（核心框架）

> **说明**：这个阶段是"新设计体系"的架构奠基。使开发者只写业务逻辑片段（VertexShaderBusiness + FragmentShaderBusiness），框架自动生成坐标变换、法线矩阵、光照计算、输出合成等通用部分。

### 任务 0.4 — 设计 `ComposedMaterialDef` 体系（新文件）

**目标**：引入高级着色器定义体系，代替低级 `FixedMaterialDef` 的繁琐 GLSL 编写。

**新增文件**
- `inc/hgl/graph/mtl/ShaderComposition.h` —— 核心数据结构和 `ComposedShaderGenerator` 骨架
- `inc/hgl/graph/mtl/ShaderComposition_Examples.h` —— PureColor3D、BasicLit、FXEmission 用例
- `SHADER_COMPOSITION_ARCHITECTURE.md` —— 详细设计文档（7 步生成流程、permutation 机制、与 ShaderTemplateEngine 协作）

**核心概念**
- `VertexShaderBusiness` / `FragmentShaderBusiness`：开发者只写业务逻辑片段（无需关心坐标变换、MVP 矩阵投影、光照计算）
- `ShaderOutputMode`：枚举（SingleRTAlphaBlend / SingleRTAdditive / DualRTDeferred）—— 框架根据此自动生成输出合成代码
- `ComposedShaderGenerator`：框架编译器，根据 `ComposedMaterialDef` + `ShaderPermutationKey` 自动生成完整 GLSL

**实现内容**
- `ComposedMaterialDef` 结构定义（业务代码 + 输出模式 + 光照启用标记）
- `GenVertexInputStruct()` / `GenVSOutputStruct()` —— 生成通用结构体
- `GenCoordinateTransformFunctions()` —— 生成坐标变换（L2W、NormalMatrix、VP 投影）
- `GenLightingCode()` —— 根据 key 生成光照计算（占位，实现延迟至 M2-M3）
- `GenOutputCompositionCode()` —— 根据 output_mode 生成最终 RT 写入逻辑

**验证**
- 编译 `ULRE.ShaderGen` 无错误
- 用 `ComposedShaderGenerator::ComposeVertexShader()` 生成一个 PureColor3D 的完整 VS 源码，验证包含：
  - `GetLocalToWorld()` 函数
  - `GetNormalMatrix()` 函数
  - `GetPosition3D()` 函数
  - `GetClipPosition()` 函数
  - 并且源码可以被 glslang 成功编译到 SPV

---

### 任务 0.5 — 辅助函数库自动生成机制（关键优化）

**目标**：框架根据 `ComposedMaterialDef` 自动生成 `GetLocalToWorld()`、`GetNormal()`、`GetMaterialInstance()` 等辅助函数，开发者无需从 MFGetPosition.h/MFCommon.h 中选择变体。

**新增文件**
- `HELPER_FUNCTION_AUTO_GENERATION.md` —— 完整设计（参数化工作表、算法伪代码、效果对比）

**核心扩展**（对 `ComposedShaderGenerator` 添加）
- `GenHelperFunctionLibrary(def, shader_stage)` —— 根据材质定义和 shader 阶段生成完整辅助函数库
  - `GenGetLocalToWorld()` —— 生成矩阵获取函数（自动选择 ByIndex/ByAssign/Fixed）
  - `GenGetNormalMatrix()` —— 生成法线矩阵推导
  - `GenGetNormalFunction()` —— 根据 VS/GS/FS 生成不同签名的 GetNormal()
  - `GenGetPositionFunctions()` —— 生成 GetPosition3D/GetClipPosition（自动处理坐标系）
  - `GenGetMaterialInstanceFunctions()` —— 生成 GetMaterialInstance/GetMI（自动选择 stage-specific 版本）

**关键特性**
- **VS 版本**：GetNormal() 接收参数、GetPosition3D() 使用顶点输入、生成 GetClipPosition()
- **GS 版本**：所有函数添加 vertex_index 参数、生成 HandoverMaterialInstanceID()
- **FS 版本**：GetPosition3D() 返回插值的世界坐标、GetNormal() 返回插值的世界法线
- **条件生成**：无 Normal 顶点属性 → 不生成 GetNormal(无参) 版本
- **零运行时成本**：所有函数均为 inline，最终 SPV 与手写等效

**验证**
- 使用 BasicLit 的 ComposedMaterialDef（含 Position/Normal/TexCoord 顶点输入、LocalToWorld/CameraInfo/MaterialInstanceData 描述符）
- 调用 `GenHelperFunctionLibrary(def, "VS")` 生成函数库，验证包含：
  - `mat4 GetLocalToWorld()`
  - `mat3 GetNormalMatrix()`
  - `vec3 GetNormal()` 和 `vec3 GetNormal(vec3)` 两个重载
  - `vec4 GetPosition3D()` 和 `vec4 GetClipPosition()`
  - `MaterialInstance GetMaterialInstance()` / `MaterialInstance GetMI()`
- 调用 `GenHelperFunctionLibrary(def, "FS")` 生成函数库，验证生成的函数签名符合 FS 阶段需求（玻不接受参数、返回插值值）
- 将生成的函数库和业务代码合并，编译到 SPV，验证着色器执行正确

**设计文件涉及**
- SHADER_COMPOSITION_ARCHITECTURE.md 更新 Step 3-4（加入函数库生成步骤）
- HELPER_FUNCTION_AUTO_GENERATION.md 提供完整参数化表和算法实现指南

---

## 阶段 1：第一个 FixedMaterialDef 硬编码材质（PureColor3D 迁移）

### 任务 1.1 — 定义 `PURE_COLOR_3D_DEF`（不替换，只新增）

**目标**：第一个真正用 `FixedMaterialDef` 描述的材质，用于验证 `CompileFixedMaterial()` 能跑通。

**文件**：`src/ShaderGen/3d/S_PureColor3D.h`（新增，内联于 M_PureColor3D.cpp 中使用）

```cpp
// 完整的 vert/frag GLSL 字符串（不是片段，有完整 layout 声明）
constexpr const char PURE_COLOR_3D_VERT_GLSL[] = R"(
#version 450
layout(set=..., binding=...) uniform CameraInfo { ... } camera;
...
void main() { ... }
)";
constexpr FixedVertexEntry PURE_COLOR_3D_VERTEX[] = { ... };
constexpr FixedDescriptorEntry PURE_COLOR_3D_DESCRIPTORS[] = { ... };
constexpr FixedMaterialDef PURE_COLOR_3D_DEF { "PureColor3D", ... };
```

**验证**：在示例 `TestPureColor3D`（或等价的简单场景）中临时调用
`CompileFixedMaterial(dev, PURE_COLOR_3D_DEF)` 代替原工厂函数，验证画面输出与原版一致。

---

### 任务 1.2 — 迁移 `CreatePureColor3D()` 工厂函数（接口不变）

**目标**：`M_PureColor3D.cpp` 内部切换到 `CompileFixedMaterial()`，对外接口 `CreatePureColor3D()` 保持不变。

**文件**：`src/ShaderGen/3d/M_PureColor3D.cpp`

```cpp
MaterialCreateInfo *CreatePureColor3D(const VulkanDevAttr *dev_attr, Material3DCreateConfig *cfg)
{
    return CompileFixedMaterial(dev_attr, PURE_COLOR_3D_DEF);
    // 原 MaterialPureColor3D 类整体删除
}
```

**验证**：运行示例，与迁移前视觉效果完全一致；删除 `class MaterialPureColor3D` 后代码仍编译通过。

---

### 任务 1.3 — 同样迁移 `VertexColor3D`、`Gizmo3D`（编辑器 / fallback 核心材质）

**目标**：这 3 个材质（PureColor3D、VertexColor3D、Gizmo3D）是编辑器必备的 fallback 材质，必须全部走硬编码 `FixedMaterialDef` 路线，永不依赖外部文件。

**文件**：各自的 `M_Xxx.cpp` 迁移方式同 1.2

**验证**：Gizmo3D 场景运行，显示网格坐标轴颜色正确。

---

## 阶段 2：inja 模板材质路线（BasicLit → 文件驱动）

### 任务 2.1 — 设计 `UberMaterial` 的 inja 模板（`forward_uber.frag.tmpl`）

**目标**：用 inja 模板生成支持 `ShaderPermutationKey` 4 轴的完整 fragment shader GLSL。

**文件**
- 新增 `ShaderLibrary/templates/forward_uber.frag.tmpl`

```jinja
#version 450

#define AMBIENT_MODEL {{ permutation.ambient }}
#define LIGHT_MODEL {{ permutation.light }}
#define SPECULAR_SPLIT {{ permutation.specular }}
#define SHADOW_MODE {{ permutation.shadow }}

// ... layout 声明 (由模板展开) ...

{% if permutation.light >= 2 %}
{{ module_lighting }}
{% endif %}

{% if permutation.ambient >= 2 %}
{{ module_ambient }}
{% endif %}

void main()
{
    MaterialInstance mi = GetMI();
    ...
}
```

**验证**：用 `ShaderTemplateEngine::Generate()` 渲染此模板，打印输出包含正确的 `#define` 行。

---

### 任务 2.2 — 定义 UberMaterial recipe（`recipes/uber/uber_3d.json`）

**目标**：新增一个 UberMaterial recipe JSON，每个质量等级对应一个 `ShaderPermutationKey` 预设。

**文件**：`ShaderLibrary/recipes/uber/uber_3d.json`

```json
{
  "name": "UberMaterial3D",
  "quality_levels": {
    "mobile_low":   { "permutation": { "ambient": 0, "light": 1, "specular": 0, "shadow": 0 } },
    "mobile_high":  { "permutation": { "ambient": 0, "light": 2, "specular": 0, "shadow": 0 } },
    "pc_medium":    { "permutation": { "ambient": 1, "light": 2, "specular": 0, "shadow": 1 } },
    "pc_high":      { "permutation": { "ambient": 2, "light": 4, "specular": 0, "shadow": 1 } },
    "pc_ultra":     { "permutation": { "ambient": 2, "light": 4, "specular": 1, "shadow": 2 } }
  }
}
```

**验证**：用 `engine.LoadRecipe()` 加载此文件，验证 JSON 解析不报错。

---

### 任务 2.3 — `TemplateBasedMaterialFactory`（通过 recipe 创建 MaterialCreateInfo）

**目标**：实现一个从 recipe JSON + quality_level → `MaterialCreateInfo*` 的完整路径。

**文件**
- 新增 `src/ShaderGen/TemplateBasedMaterialFactory.cpp`
- 新增 `inc/hgl/graph/mtl/TemplateBasedMaterialFactory.h`

```cpp
MaterialCreateInfo *CreateMaterialFromRecipe(
    const VulkanDevAttr *dev_attr,
    const AnsiString &recipe_path,
    const AnsiString &quality_level,
    const FixedMaterialDef &base_def);  // 提供描述符布局和顶点输入（这部分不能从 JSON 生成）
```

实现流程：
1. `ShaderTemplateEngine::LoadRecipe()` → `ShaderRecipe`
2. 从 recipe 中取 permutation → 构造 `ShaderPermutationKey`
3. `ShaderTemplateEngine::Generate(recipe)` → GLSL 字符串
4. 把 GLSL 字符串塞进 `base_def.frag_glsl`（或直接传给 glslang）
5. 调用 `CompileFixedMaterial()` 的内部编译路径

**验证**：调用 `CreateMaterialFromRecipe(dev_attr, "recipes/uber/uber_3d.json", "mobile_low", UBER_BASE_DEF)`，能渲染出一个 Lambert 照亮的场景。

---

### 任务 2.4 — 把 `BasicLit` 迁移到 recipe 路线（可选硬编码 fallback）

**目标**：`CreateBasicLit()` 先尝试从文件加载 recipe，文件不存在时自动退回到硬编码 `FixedMaterialDef`。

**文件**：`src/ShaderGen/3d/M_BasicLit.cpp`

```cpp
MaterialCreateInfo *CreateBasicLit(const VulkanDevAttr *dev_attr, BasicLitMaterialCreateConfig *cfg)
{
    // 先尝试从文件加载
    if(FileExists("ShaderLibrary/recipes/uber/uber_3d.json"))
    {
        ShaderPermutationKey key = ConfigToPermutationKey(cfg);
        return CreateMaterialFromRecipe(dev_attr, "recipes/uber/uber_3d.json",
                                        PermutationKeyToQualityLevel(key), BASIC_LIT_BASE_DEF);
    }

    // Fallback：硬编码路径（文件丢失 / 编辑器模式）
    ShaderPermutationKey key = ConfigToPermutationKey(cfg);
    return CompileFixedMaterial(dev_attr, BASIC_LIT_HARDCODED_DEF, key);
}
```

**验证**：删除 recipe 文件后，场景仍可渲染（hardcode fallback 生效）；恢复文件后，两路输出视觉一致。

---

## 阶段 3：多光照模型 GLSL 模块补全

### 任务 3.1 — `cel_shading.glsl`（卡通渲染模块）

**文件**：`ShaderLibrary/modules/lighting/cel_shading.glsl`

```glsl
// Interface: GetDiffuseColor()
// Dependencies: GetAlbedo
vec3 GetDiffuseColor()
{
    float NdotL = max(dot(normalize(Normal), normalize(sky.sun_direction.xyz)), 0.0);
    float bands = floor(NdotL * 3.0) / 3.0;  // 3级卡通分档
    return GetAlbedo() * bands * sky.sun_color.rgb;
}
```

**验证**：创建一个 `CelShading` 排列的材质并渲染，法线边缘出现阶梯状色带。

---

### 任务 3.2 — `hemisphere_ambient.glsl`（半球环境光模块）

**文件**：`ShaderLibrary/modules/ambient/hemisphere.glsl`

```glsl
// Interface: GetAmbientColor()
// Requires: sky.base_sky_color, sky.base_ground_color
vec3 GetAmbientColor()
{
    float t = normalize(Normal).y * 0.5 + 0.5;
    return mix(sky.base_ground_color.rgb, sky.base_sky_color.rgb, t) * GetAlbedo();
}
```

**更新 `PermutationToRecipe.h`**：`AMBIENT_MODEL_MODULE_NAMES[1] = "hemisphere"`

**验证**：切换到 `AmbientModel::Hemisphere` 排列，天空和地面颜色过渡正确。

---

### 任务 3.3 — `pbr_lite.glsl`（简化 PBR 模块，不依赖 split-sum）

**文件**：`ShaderLibrary/modules/lighting/pbr_lite.glsl`

```glsl
// GGX diffuse + 近似 specular，不需要 BRDF LUT
// Interface: GetDiffuseColor(), GetSpecularColor()
// Dependencies: GetAlbedo
```

**更新 `LIGHT_MODEL_MODULE_NAMES[3] = "pbr_lite"`**

**验证**：PC medium 质量设置下渲染 PBR_Lite 效果，metallic/roughness 参数生效。

---

## 阶段 4：描述符集槽位精简（`DescriptorSetType` 从 9 → 4）

> ⚠️ 此阶段影响范围最广，必须在阶段 0-3 全部验证后再开始。

### 任务 4.1 — 评估当前枚举使用情况

**动作**：`grep -rn "DescriptorSetType::" inc/ src/` 统计每个枚举值的实际引用计数。

**验证**：生成报告，确认哪些值是零引用可以直接删除。

---

### 任务 4.2 — 添加 4 值枚举，保留旧值作为 deprecated alias

**文件**：`inc/hgl/vk/VKDescriptorSetType.h`

```cpp
enum class DescriptorSetType : uint8
{
    RenderTarget = 0,   // set=0: Viewport
    Camera       = 1,   // set=1: Camera + Sky
    PerFrame     = 2,   // set=2: L2W SSBO + MI SSBO
    PerMaterial  = 3,   // set=3: 材质纹理 + material-specific UBO

    // --- Backward compat aliases（同值）---
    World    = Camera,
    Static   = PerFrame,
    Global   = RenderTarget,
    Instance = PerMaterial,

    ENUM_CLASS_RANGE(RenderTarget, PerMaterial)
};
```

**验证**：所有现有代码编译通过（alias 保证无感替换）。

---

### 任务 4.3 — 逐步替换旧枚举值，删除 alias

每个文件单独提交，每次提交后运行示例验证。

**验证**：全场景渲染在精简后输出与精简前完全一致。

---

## 阶段 5：清理（阶段 0-4 全完成后）

### 任务 5.1 — 确认哪些材质保持硬编码（不可从文件加载）

以下材质**永久保持 `FixedMaterialDef` 硬编码**，文件丢失不影响：

| 材质 | 原因 |
|------|------|
| `PureColor3D` | 编辑器 Gizmo 底色 fallback |
| `VertexColor3D` | 编辑器骨骼可视化 |
| `Gizmo3D` | 编辑器辅助线 |
| `PureColor2D` | UI/编辑器 2D fallback |
| `VertexColor2D` | 编辑器 2D Gizmo |
| `SkyMinimal` | 天空盒必须始终可用 |

以下材质**迁移到 recipe/模板路线**，文件丢失时退回 hardcode fallback：

| 材质 | Recipe 文件 |
|------|------------|
| `BasicLit` | `recipes/uber/uber_3d.json` |
| `TextureBlinnPhong` | `recipes/uber/uber_3d.json` |
| `TerrainGrid` | `recipes/terrain/terrain_grid.json` |
| `Billboard2D` | `recipes/billboard/billboard.json` |

---

### 任务 5.2 — 删除 `Std3DMaterial` / `Std2DMaterial` 基类

**前提**：所有 `M_Xxx.cpp` 已迁移到 `CompileFixedMaterial()` 路线，不再继承 `StdXDMaterial`。

**文件删除**
- `src/ShaderGen/3d/Std3DMaterial.h/.cpp`
- `src/ShaderGen/2d/Std2DMaterial.h/.cpp`
- `inc/hgl/graph/mtl/Std2DMaterial.h`
- `inc/hgl/graph/mtl/Std3DMaterial.h`
- `inc/hgl/shadergen/ShaderCreateInfoVertex.h` 中 `AddAssignTransform()` / `AddAssignMaterialInstance()` 的公开 API（若无外部用户）

---

### 任务 5.3 — 删除 `ShaderCreateInfo` 公开 API（仅保留内部 `CompileGLSL()` 接口）

**目标**：`ShaderCreateInfo` 退化为 `MaterialCompiler` 的内部实现，不再是公开 API。

---

## 阶段 6：UberMaterial 深度集成（最终阶段）

### 任务 6.1 — `MaterialBatch` 中的 UberMaterial 检测与合并

**目标**：所有使用 `UberMaterial3D` 的 `RenderItem` 合并到一个 `MaterialBatch`，共享一个 `Pipeline`。

**文件**：`src/ecs/core/MaterialBatch.cpp`

**验证**：10000 个 Mesh 实例单帧只产生 1 个 `vkCmdDrawIndexedIndirect` 调用。

---

### 任务 6.2 — `UberTexturePool`（全局 Texture2DArray 管理）

**文件**
- 新增 `inc/hgl/graph/module/UberTexturePool.h`
- 新增 `src/SceneGraph/module/UberTexturePool.cpp`

**API**

```cpp
class UberTexturePool {
public:
    uint32_t AllocateLayer(Texture2D *src);   // 返回 layer 索引
    void     Release(uint32_t layer);
    Texture2DArray *GetBaseColorArray() const;
};
```

**验证**：100 张不同纹理写入 pool，采样结果逐张对比正确。

---

## 里程碑检查点（每个里程碑对应一次可演示 build）

| 里程碑 | 完成条件 | 对应任务 | 关键文件 | 状态 |
|--------|---------|---------|---------|------|
| **M0 基础设施** | CompileFixedMaterial() + inja 编译通过 + PermutationToRecipe 映射表 | 0.1, 0.2, 0.3 | MaterialCompiler.{h,cpp}，PermutationToRecipe.h | ✅ 完成 |
| **M0.5 合成框架** | ComposedShaderGenerator 框架完成；辅助函数库自动生成机制可用 | 0.4, 0.5 | ShaderComposition.h，ShaderComposition_Examples.h，ComposedShaderGenerator.cpp | ✅ 完成 |
| **M1 硬编码材质** | 6 个编辑器 fallback 材质全部走 FixedMaterialDef；删除对应 class MaterialXxx | 1.1, 1.2, 1.3 | S_PureColor3D.h，S_VertexColor3D.h，S_Gizmo3D.h 等 | ⏳ 进行中 |
| **M2 文件驱动材质** | CreateMaterialFromRecipe() 可以从 recipe JSON 生成 BasicLit Lambert 效果 | 2.1–2.3 | TemplateBasedMaterialFactory.{h,cpp}，forward_uber.frag.tmpl，recipes/uber/uber_3d.json | ⏳ 待开始 |
| **M3 质量预设** | BasicLit 支持 5 种质量等级（mobile_low ~ pc_ultra），文件丢失自动 fallback | 2.4 | M_BasicLit.cpp（fallback 路由逻辑） | ⏳ 待开始 |
| **M4 光照模块** | 卡通渲染 + 半球环境光 + PBR_Lite 三个新模块全部可用 | 3.1–3.3 | ShaderLibrary/modules/lighting/cel_shading.glsl，hemisphere.glsl，pbr_lite.glsl | ⏳ 待开始 |
| **M5 描述符精简** | DescriptorSetType 精简为 4 值；旧 9 值枚举变为 alias；全场景渲染正确 | 4.1–4.3 | VKDescriptorSetType.h（精简后），相关 binding 代码 | ⏳ 待开始 |
| **M6 清理旧代码** | 旧 Std3DMaterial / Std2DMaterial 删除；ShaderCreateInfo 仅内部使用 | 5.2–5.3 | 文件删除 + MaterialCompiler.cpp 内部使用 | ⏳ 待开始 |
| **M7 性能优化** | 10000 Mesh 只产生 1 DrawCall；纹理 pool 正常工作 | 6.1–6.2 | MaterialBatch.cpp，UberTexturePool.{h,cpp} | ⏳ 待开始 |

---

## 关键设计原则回顾

### 从 FixedMaterialDef 到 ComposedMaterialDef 的演进

```
┌────────────────────────────────────────────────────────────┐
│ 阶段 M0-M1：硬编码基础                                      │
│                                                            │
│ 开发者 → 手写完整 GLSL（坐标变换、法线、光照、输出合成）   │
│       → 填充 FixedMaterialDef                             │
│       → CompileFixedMaterial() → SPV                      │
└────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────┐
│ 阶段 M0.5：抽象革新                                         │
│                                                            │
│ 开发者 → 只写业务逻辑片段：                                │
│          · VertexShaderBusiness(vi) → 顶点变换            │
│          · FragmentShaderBusiness(vso) → 颜色/法线        │
│       → 框架自动生成 Complete GLSL：                      │
│          · 坐标变换函数库（GetLocalToWorld等）            │
│          · 法线矩阵函数（GetNormalMatrix等）             │
│          · 光照计算（根据 ShaderPermutationKey）          │
│          · 输出合成（根据 ShaderOutputMode）             │
│       → ComposedShaderGenerator → SPV                     │
└────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────┐
│ 阶段 M2-M4：模板驱动                                        │
│                                                            │
│ 开发者 → 通过 recipe JSON 选择质量等级                    │
│       → ShaderTemplateEngine 加载 .glsl 模块             │
│       → Inja 模板渲染 + ComposedShaderGenerator 拼接       │
│       → TemplateBasedMaterialFactory → SPV               │
└────────────────────────────────────────────────────────────┘
```

### M0.5 对整个系统的核心改进

| 维度 | 改进 |
|------|------|
| **开发复杂度** | ↓80%：无需手工管理坐标变换、法线矩阵、光照调用 |
| **可维护性** | ↑ 修改坐标系/光照算法一次，自动适用所有材质 |
| **可扩展性** | ↑ 添加新光照模式无需改业务代码，仅需添加光照函数 |
| **性能成本** | = 0：所有辅助函数均 inline，编译后 SPV 完全相同 |
| **学习曲线** | ↓：API 统一（GetLocalToWorld/GetNormal/GetMI），无选择困扰 |
