# ShaderGen 模块隔离与复杂度削减设计

> 2026-08-31 定稿。基于对 `src/ShaderGen/` 全量扫描（47 源文件 + `inc/hgl/mtl/` 50 公共头 +
> `src/Tools/ShaderGen/` 4 工具）梳理的整体工作流、现状耦合分析、目录隔离方案，
> 以及对「目录隔离 ≠ 复杂度削减」这一认知修正的展开。
> 仅分析，未改动任何代码。配套方法论文档见 `shadergen-refactoring-methodology.md`。

---

## 一、背景与目标

ShaderGen 是 ULRE 的材质 → Shader 全链生成与编译模块（mesh shader 为唯一顶点路径，VS 已废弃）。
经过 T1-T3 / C1-C4 系列改造后，逻辑层已相当收敛（单一真源、表驱动、注入段数据化），
但**物理目录布局仍停留在演进早期形态**：实现文件挤在 `common/` 与根目录，公共头平铺在
`inc/hgl/mtl/`，模块边界无法从文件位置读出。

本文目标：

1. 完整梳理 ShaderGen 整体工作流（数据流全景）。
2. 定位现状耦合点（哪些模块"受其它部分影响"）。
3. 提出按功能模块独立目录（.h/.cpp 同目录）的隔离方案。
4. 修正认知：目录隔离只是复杂度削减的**前置条件**，真正的复杂度来自
   历史叠加 / 样板重复 / 隐式协议三类，必须三管齐下。

---

## 二、整体工作流（数据流全景）

```
┌─ 输入资产（ShaderLibrary/）─────────────────────────────────────────┐
│  70 个 .glsl 模块（全部带 @ulre 元数据）                            │
│  12 个材质 TOML + sampler.toml + mesh/*.glsl.tmpl 模板              │
└───────────────────────────────────────────────────────────────────┘
        │ LoadDirectory（懒加载单例，首次使用时扫描）
        ▼
┌─ 加载层 ───────────────────────────────────────────────────────────┐
│ MaterialDefinitionRegistry.cpp:456  GetGLSLCodeModuleRegistry()    │
│ MaterialDefinitionRegistry.cpp:430  GetMaterialDefinitionFileRegistry()│
└───────────────────────────────────────────────────────────────────┘
        ▼
┌─ 请求入口 ─────────────────────────────────────────────────────────┐
│ ShaderProgramManager::AcquireShaderProgram (SceneGraph)            │
│   → NormalizeRecipe → ResolveMaterialDefinitionForRequest          │
│   → CreateMaterialFromDefinition (MaterialDefinitionRegistry.h:80) │
└───────────────────────────────────────────────────────────────────┘
        ▼
┌─ BuildGenericMaterial（5 相位编排，GenericMaterialBuilder.cpp:658）─┐
│ P1 ResolvePurposeAndCoverage → purpose/coverage/varying/stage接口  │
│ P2 ResolveVertexABI          → position_format + s1_* 顶点输入     │
│                                 (BuildResolvedMaterialVertexABI)    │
│ P3 BuildResourceContract     → manifest + descriptors + 描述符契约 │
│ P4 GenerateStageSources      → ms（MeshShaderAssembler）            │
│                                 + fs（CompositorAssembler）         │
│ P5 FinalizeProgramLink       → stage key/哈希（含设备 profile）    │
└───────────────────────────────────────────────────────────────────┘
        ▼
┌─ CompileCompositorMaterial（7 步编译流水线，MaterialShaderCompiler.cpp:655）┐
│ S1 基础描述符契约 → S2 建 ShaderBuildContext → S3 描述符注册       │
│ （RegisterCanonicalDescriptors / CharQuadSSBOs / 私有数据槽）       │
│ S5 注入段组装（MaterialShaderEmitter 纯发射层）                    │
│ S6 ShaderResourceSchema 校验 + artifact 元数据                     │
│ S7 FinalizeShaderBuildContext → SPV 编译或缓存加载                 │
└───────────────────────────────────────────────────────────────────┘
        ▼
ShaderBuildContext（mesh + fragment SPV、描述符布局、schema）
        → ShaderProgramManager 缓存
```

**核心分工不变量（S2）**：`MaterialShaderCompiler.cpp` 是**求解层**（做决策——契约、描述符注册、
槽位合并），`MaterialShaderEmitter.cpp` 是**发射层**（纯函数、零决策、只把已解出的状态转成 GLSL 文本）。
`GenericMaterialBuilder` 是**行为保持的相位拆分**（原 390 行单函数拆 5 步，hash 输入序列逐字段不变）。

---

## 三、当前结构：物理布局 vs 逻辑模块

| 逻辑模块 | 头文件位置 | 实现位置 | 关键依赖 |
|---|---|---|---|
| **GLSLCodeModule 家族**（读取/解析/注册/校验/哈希/取用） | `inc/hgl/mtl/GLSLCodeModule*.h`（5 头） | **全部挤在 `common/`** | 文件系统、hgl 容器、DescriptorSemantic、SSBO/TextureSlot 类型 |
| **材质定义**（TOML 解析 + 注册表） | `MaterialDefinitionFile.h`、`MaterialRecipe.h` | `common/MaterialDefinitionFile.cpp`（962 行） | toml 库 |
| **契约层**（描述符/输出/覆盖/阶段接口/语义/模块图） | `*Contract.h`、`ShaderSemanticRegistry.h` 等 | **散在根目录** | — |
| **求解编排** | `MaterialShaderCompiler.h`、`GenericMaterialBuilder.h` | 根目录 + `common/` | 契约层、mesh、compositor |
| **Mesh 生成器** | 内部头全在 `common/`（9 个） | `MeshShaderTemplate.cpp` | MaterialStageInterface、MeshShaderMode |
| **FS 组装** | `CompositorAssembler.h` | 根目录 | ShaderLibrary 文件 |
| **编译/产物** | `ShaderBuildContext.h`、`ShaderCreateInfo.h`、`ShaderArtifactStore.h` 等 | 根目录 | GLSLCompiler 插件 |
| **工具** | — | `src/Tools/ShaderGen/`（回归门 5,634 行） | 几乎全部内部头 |

### 现状的三个核心耦合问题

**问题 1：`common/` 是杂货铺，不是模块。**
`src/ShaderGen/common/` 25 个文件混装 4 个无关逻辑模块：GLSLCodeModule 家族（5 cpp）、
GenericMaterialBuilder、MaterialDefinitionFile、MeshShader 生成器、SamplerPreset、
ModuleResourceManifest、VertexBuilderCommon。改任何一个，同目录其它模块文件全部在
include 图上可见——物理目录不表达模块边界。

**问题 2：GLSLCodeModule 家族被"劫持"在注册表层。**
读取（LoadDirectory/解析）在 `common/GLSLCodeModule*.cpp`，但取用的单例
`GetGLSLCodeModuleRegistry()` 却定义在 `MaterialDefinitionRegistry.cpp:456`——生命周期、加载时机、
存储都挂在材质定义注册表上。想单独用 GLSL 模块（如给工具链离线解析），必须连带拉进
MaterialDefinitionRegistry 及其全部依赖。这是「这一部分受其它部分影响」的实锤。

**问题 3：头文件全平铺在 `inc/hgl/mtl/`，无层次。**
50 个公共头 + 4 个 contract/ 头全在一个目录，`#include <hgl/mtl/Xxx.h>` 无法表达
"契约类型 / 编译入口 / 模块图"的层次。回归门（Tools/ShaderGen）用
`#include "../../ShaderGen/common/MeshShaderAssembler.h"` 相对路径穿透——测试直接依赖实现文件的物理位置。

---

## 四、隔离方案（按模块独立目录，.h/.cpp 同目录）

```
src/ShaderGen/
├── glsl_module/          ← GLSLCodeModule 家族（完全独立）
│   ├── GLSLCodeModule.h / .cpp              （类型 + 语义注册表 + 哈希）
│   ├── GLSLCodeModuleFile.h / .cpp          （@ulre 元数据解析）
│   ├── GLSLCodeModuleRegistry.h / .cpp      （目录扫描 + 注册 + 依赖解析）
│   ├── GLSLCodeModuleMetadata.h / .cpp      （契约校验 / 环检测）
│   └── GLSLCodeModuleCapabilityResolver.h / .cpp（provider 图哈希 / 组合）
├── material_definition/  ← 材质 TOML 读取
│   ├── MaterialDefinitionFile.h / .cpp
│   └── MaterialRecipe.h
├── contract/             ← 契约层（纯类型 + 校验，无 I/O）
│   ├── CanonicalShaderContract.h/.cpp  DescriptorContract.h/.cpp
│   ├── MaterialOutputContract.h/.cpp   MaterialCoverageContract.h/.cpp
│   ├── MaterialStageInterface.h/.cpp   ShaderSemanticRegistry.h/.cpp
│   ├── ModuleResourceManifest.h/.cpp   ResolvedModuleGraphBuilder.h/.cpp
│   └── BindingTableBuilder.h/.cpp      MaterialBindingContract.h/.cpp
├── meshgen/              ← mesh shader 生成器
│   ├── MeshShaderAssembler.h  MeshShaderHeaderGen.h
│   ├── MeshShaderMode*.h      MeshShaderTemplate.h/.cpp
│   ├── MeshShaderVaryingGen.h MeshShaderVertexAdapter.h
│   └── VertexVaryingConfig.h  VertexBuilderCommon.h
├── compositor/           ← FS 组装
│   └── CompositorAssembler.h/.cpp
├── builder/              ← 求解编排（依赖上面全部）
│   ├── GenericMaterialBuilder.h/.cpp
│   ├── MaterialShaderCompiler.h/.cpp
│   ├── MaterialShaderEmitter.h/.cpp
│   └── DescriptorBuilderCommon.h / 3d/DefinitionDescriptorBuilder.h
├── compile/              ← 编译与产物
│   ├── GLSLCompiler.h/.cpp  TBuiltInResourceCompat.h
│   ├── ShaderBuildContext.h/.cpp  ShaderCreateInfo.h/.cpp
│   ├── ShaderArtifactStore.h/.cpp  ShaderProgramArtifactBuilder.h/.cpp
│   └── DescriptorSetLayoutAllocator.h/.cpp  ShaderStructureDump.h/.cpp
└── profile/              ← 设备能力（contract/* 固定头）
    └── contract/ShaderGen*.h  ShaderCompilerProfileAPI.h
```

### 配套动作

1. **注册表单例下沉**：`GetGLSLCodeModuleRegistry()` 从 `MaterialDefinitionRegistry.cpp` 移入
   `glsl_module/GLSLCodeModuleRegistry.cpp`（连同 `GetShaderLibraryPath()` 调用一起）。材质注册表只保留
   自己的 `GetMaterialDefinitionFileRegistry()`。GLSL 模块加载时机与生命周期完全自治。
2. **include 策略**：模块内部用相对路径（`../contract/...`），模块对外只暴露该模块公开头；
   回归门改为 include 各模块目录头，不再穿透 `common/`。
3. **CMake**：`add_cm_library(ULRE.ShaderGen)` 的 SOURCE_GROUP 按新目录名（glsl_module / contract /
   meshgen / builder / compile…），物理目录即 VS 分组。
4. **ShaderLibrary 资产**（`.glsl` / `.toml` / `.tmpl`）保持独立资产目录不动——它本就是"数据"，
   隔离方案不碰它。

---

## 五、收益与风险

**收益**

- 改 GLSL 模块解析器（如加 `@ulre` 新指令）只重编该模块，不牵连 MaterialDefinitionFile / MeshShader。
- 工具链可单独链 glsl_module 做离线 @ulre 校验（回归门已证明该解析值得独立测试）。
- 依赖方向单向化：`glsl_module → 基础类型`；`builder → contract + meshgen + compositor + glsl_module`，
  消除 MaterialDefinitionRegistry 对 GLSL 模块的"持有"。

**风险与注意点**

- **头文件搬移破坏面小但存在**：外部仅 7 个文件 include `<hgl/mtl/*>`（SceneGraph 3、ecs 1、
  Vulkan 1、Tools 2），且集中在公共头；GLSLCodeModule 的 5 个头外部零依赖（仅回归门用）——迁移它最安全。
- **行为不变纪律**：纯搬移必须逐字节等价——不改函数体、不改 hash 输入序列、GLSL 输出不变；
  验证 = 回归门全 PASS（43 用例）+ 删缓存双跑 IDENTICAL。
- 回归门 5,634 行 include 要跟着改（约 25 个 hgl/mtl 头 + 4 个相对路径头），是主要机械工作量。
- `MeshShaderAssembler.h` 等 9 个 mesh 头建议保留 inline header 形态（纯文本发射器，无 .cpp 可搬，
  且回归门直接调用 `GenerateMeshShader` 做文本断言）——目录归位即可，不强行拆 .cpp。

---

## 六、认知修正：目录隔离 ≠ 复杂度削减

目录重排解决"耦合不可见"问题，但**不自动解决复杂度**——总行数、总逻辑、硬编码一处不少。
若只搬文件，结果大概率是"把 25 个文件从一个抽屉挪进 5 个抽屉"。要达成"降低复杂度 + 减硬编码"，
必须三管齐下。

### 依赖 DAG（先画图再定目录）

```
第 0 层  纯类型（无 .cpp）：MaterialRecipe / MeshShaderMode / Serialized*Entry /
          DescriptorSemantic / SSBOTypes / contract/ShaderGenContract 等
第 1 层  资产读取：glsl_module、material_definition、sampler_preset
          （都只依赖第 0 层 + hgl 基础库）
第 2 层  契约/解析：contract（ModuleResourceManifest、ResolvedModuleGraphBuilder
          —— 依赖第 1 层的 glsl_module registry）
第 3 层  生成器：meshgen、compositor（依赖第 2 层）
第 4 层  编排：builder（依赖 1+2+3）
第 5 层  产物/编译：compile（被第 4 层调用）
```

**关键点**：contract 依赖 glsl_module（不是反过来），所以 glsl_module 必须比 contract 更底层。

### CMake target 边界（否则隔离只是约定）

当前 `add_cm_library(ULRE.ShaderGen)` 把所有文件堆进一个库，SOURCE_GROUP 只是 VS 里分组好看——
编译器层面两个"模块"照样能互相 include 而零报错。要真正隔离，每个模块一个 CMake 目标
（OBJECT 或 STATIC），include 路径白名单化，跨模块必须显式 `target_link_libraries`。
"这一部分不受其它部分影响"由编译器强制，而非靠人自觉。

### 拆"混合职责文件"（真正降单模块复杂度）

`MaterialDefinitionRegistry.cpp`（511 行）混了 5 种职责：

| 职责 | 正确归属 |
|---|---|
| `GetGLSLCodeModuleRegistry()` 单例 | glsl_module |
| `GetMaterialDefinitionFileRegistry()` 单例 | material_definition |
| `GetNumericClassFromVkFormat` / `GetGLSLVertexInputType` | vertex_abi（或独立） |
| `BuildResolvedVertexABI`（s1_* 模块选择） | vertex_abi |
| `CreateMaterialFromDefinition` / `NormalizeRecipe` / `ResolveMaterialVertex*Config` | 入口编排 |

"单模块复杂度"靠拆职责降，不靠挪目录降。

---

## 七、硬编码与隐式协议清单（第 2、3 管）

### 去硬编码（按优先级）

1. **`GenerateMeshShader` 的 `shader_lib_path` 参数是死参数**——函数体全程未使用
   （`MeshShaderAssembler.h:47` 声明，正文只做 include 拼接）。直接删。
2. **varying 语义 → 类型/名字映射双份维护**：`MeshShaderVaryingGen.h` 手写一份
   `semantic → "flat uint" / "fragDataIndexID"`，`ShaderSemanticRegistry.cpp:53` 又一份
   `location=4` 注册表。漂移即 link 错误，收敛到 registry 单源。
3. **MeshDrawParams / TextCharInfo 等 GLSL↔CPU 结构双份手改**：GLSL 侧
   `MeshShaderVertexAdapter.h`、CPU 侧 `ShaderBufferSources.h`（有 `static_assert(sizeof==24)`，
   但只保大小不保字段顺序）。要么反射生成，要么字段级生成期校验。
4. `ms.reserve(3072)` 等魔法数。
5. 已收敛、保持不回退：`kCharQuadSSBOTable`、`kMeshIndexTableSpecs`、语义名 X-macro、
   `IsCharQuadMode` 等表驱动/单源化成果。

### 去隐式协议

1. **mesh 模式分派 3 处**（`MeshShaderAssembler.h` 容量 switch + `mode != CharQuad` 门控 +
   main 体 switch）——枚举与 `IsCharQuadMode` 已抽到 `MeshShaderMode.h`，但 Assembler 内 3 处判断还在。
   可做模式描述符表（每线程顶点数/图元数/是否走 vertex pipeline/发射函数指针），新加模式只加一行。
2. `MaterialDefinitionFile.cpp`（962 行）TOML 解析是"每字段一个 Parse* 函数"的样板——可用
   成员指针表驱动（`{"field", &Struct::field}`），与 `ResolveMaterialVertexVaryingConfig` 已用手法一致，
   只是尚未推广到解析层。
3. "结构体 GLSL 真源在 .glsl、CPU 布局在 .h、两侧手改"（CharQuad 的 `TextCharSSBO`）——
   最危险的隐式协议，应在生成期加跨侧校验。

---

## 八、落地顺序

```
① glsl_module 独立 + GetGLSLCodeModuleRegistry 单例迁回   ← 零外部依赖，最安全，先做
② contract + profile 归位（纯类型/校验，机械搬移）
③ 拆 MaterialDefinitionRegistry.cpp 的 5 职责
④ meshgen + compositor + vertex_abi 归位
⑤ builder + compile 归位（依赖最重，最后）
⑥ CMake 改多 target + 白名单 include
⑦ 回归门 include 更新 → 删缓存双跑 IDENTICAL 验证
```

每步满足"行为不变原则"（搬移逐字节等价，hash 不变）。

---

## 九、验证清单

1. VS 构建零错误。
2. 回归门全 PASS（`ShaderResourceSchemaRegressionGate.exe all`，43 用例）。
3. hash / 缓存稳定性：删缓存目录后两遍运行 IDENTICAL。
4. 示例渲染视觉确认（PBR / 文字 blend / 大气）。
5. 文档收尾：本设计文档 + 方法论文档同步更新。
