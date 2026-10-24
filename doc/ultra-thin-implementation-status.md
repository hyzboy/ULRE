# 超薄固定着色器架构 —— 实施现状与交接文档

> **用途**：把「Domain 即 Master Material」重构的**全部计划现状**固化下来，供新会话零上下文接续。
> **配套文档**：`doc/ultra-thin-fixed-shader-architecture.md`（架构设计正文，8 节，权威设计）。
> 本文只记录**进度、提交、验证结果、下一步动作、踩过的坑**，不重复设计论证。
> **最后更新**：阶段三之三完成后（分支 `feature/ultra-thin-stage3-material-instance`，HEAD = `addc99d58`）。**阶段三已完成（3/3），下一步是阶段四。**

---

## 0. 新会话开场怎么用

1. 先读本文 **§1 目标** 和 **§3 路线图**，确认自己在哪一阶段。
2. **阶段三已全部完成**（3.1 / 3.2 / 3.3，见 §4.2–§4.5）。**当前待做项是阶段四**，方案见 §6.5；§6.1–6.3 是阶段三之二的调研记录（已实施），§6.4 是阶段三之三的记录（已实施）。
3. 读 **§9 环境与坑**，避免重复踩已记录的坑（尤其是「门禁探针必须报 `domain` key」「模块数 82」「golden 无环境变量开关」）。
4. 动手前跑三条自检命令（见 §5.1），确认基线是绿的。

**铁律**：每个阶段/子阶段**独立分支 + 独立提交**；提交用 `git add` 显式列路径（禁 `-A`）；带 `Co-authored-by` trailer。

---

## 1. 架构目标

一句话：**上层开发者不存在「Material」这个概念。他只需要选模型是什么（Domain）、填好数值、塞好纹理。**

五条铁律：

| # | 铁律 | 现状 |
|---|------|------|
| 1 | Domain 就是未来的 Master Material，引擎硬编码，上层**不可**自定义、不可改 | ✅ 16 个领域契约已成体系 |
| 2 | 上层资产 = MaterialInstance：只能**选一个 Domain** + 配自己的资源与数值 | ✅ 阶段三完成（3/3）：资产只剩领域 + 槽名 + 采样器 + 顶点需求 + 渲染状态 |
| 3 | 最终废除 `*.material.toml` 这一中间层 | ⬜ 阶段四 |
| 4 | 统一函数签名的「多态包含契约」+ 空对象实现（`*_none.glsl`）来消灭条件编译，而不是一堆 `#if` / 宏 | ✅ 宏区仅剩 2 个 |
| 5 | 主表面着色器是**固定代码**；换光照/GI/天光/阴影只是 `#include` 不同文件；主着色器里只保留必须与 `discard`/输出同函数体的 alpha 三件套宏 | ✅ 阶段二达成 |

C++ 端只做三件事：**资产是否匹配 Domain 的校验**、**生成一个装满开关与选项的 `.h`**、**拼出少数几行 `#include`**。全部算法都不在 C++ 里。

---

## 2. 分支与提交链

| 顺序 | 分支 | 提交 | 内容 |
|---|---|---|---|
| 1 | `feature/ultra-thin-stage1-lighting-slots`（已并入） | `6fe3e0d6f` | 资产只填领域+值+图，算法模块名由领域契约兜底（早期阶段四） |
| 2 | 同上 | `2992bf73f` | 材质显式声明领域，Custom 从逃生门变成合法分类 |
| 3 | 同上 | `6ef579059` | 材质领域归零：13 个 Custom 材质全部领域化，领域数 5 → 16 |
| 4 | 同上 | `006d2b8e5` | **阶段一**：直接光拆分为漫反射 / 高光两条正交插槽 |
| 5 | `feature/ultra-thin-stage2-domain-shaders` | `277ad2a7c` | **阶段二之一**：`PipelineConfigEmitter` —— `#include` 那一半的单一真源 |
| 6 | 同上 | `69146a2e7` | **阶段二之二**：消灭主着色器最后三处条件编译（槽位升必需 + 空对象） |
| 7 | 同上 | `b21563928` | 阶段二收尾：路线图改为四阶段并与分支对齐（纯文档） |
| 8 | `feature/ultra-thin-stage3-material-instance` ← **当前** | `c7d1e45cb` | **阶段三之一**：顶点变换从资产推入领域契约 |
| 9 | 同上 | `20d36a0d0` | **阶段三之二**：纹理槽形态与 `material_data` 类型从资产推入领域契约 |
| 10 | 同上 | `addc99d58` | **阶段三之三**：保底链 5 个 builtin 材质定义代码化，不再依赖 TOML 文件 |

当前工作区干净，仅有既存子模块脏项 `? CMScript` / `? res` / `? src/Tools/GLTFConvert`（**不是**本次改动，勿提交）。

累计 diff（`6ef579059..HEAD`）：64 files, +2806 / −471。

---

## 3. 四阶段路线图与状态

| 阶段 | 主题 | 分支 | 状态 |
|---|---|---|---|
| 一 | 直接光正交拆分（漫反射 × 高光）+ 空对象库 + 槽角色/能力扩展 | `feature/ultra-thin-stage1-lighting-slots` | ✅ 完成 |
| 二 | 管线配置头单一真源 + 消灭主着色器里所有算法相关 `#if` | `feature/ultra-thin-stage2-domain-shaders` | ✅ 完成 |
| 三 | **MaterialInstance 模型**：把「领域已确定的旋钮」逐个从资产契约上移到领域契约 | `feature/ultra-thin-stage3-material-instance` | ✅ 完成（3/3） |
| 四 | 资产降维为 `.mi.toml` + 废除 `.material.toml` + 清理旧 ShaderGen 拼接代码 | `feature/ultra-thin-stage4-cleanup-and-migration` | ⬜ 未开始 |

**阶段三的指导原则**：凡是「同一个 Domain 下所有材质取值必然相同」的字段，都从资产文件里删掉，改由领域契约给出规范值；资产若显式写了不同值，必须**被校验拒绝**（不是静默覆盖）。这既是减负，也是把「上层不该有的自由度」物理删除。

---

## 4. 已完成明细

### 4.1 阶段一（`006d2b8e5`）

- 直接光拆为两条正交轴：`DiffuseLightProvider` + `SpecularLightProvider`，取代原来的单条 `DirectLightProvider`（后者降级为 legacy）。
- 新增空对象库（`*_none.glsl`），用于「无此环节」的合法实现。
- `ShaderModuleSlotRole` 扩到 12 个有效 role + 哨兵 `SlotRoleCount`。
- `ShaderModuleCapability` 扩：`DirectDiffuse`（1<<11）/ `DirectSpecular`（1<<12）。
- 命中：门禁 54 → 55 PASS。

### 4.2 阶段二（`277ad2a7c` + `69146a2e7`）

**之一 —— `PipelineConfigEmitter`**
- 新增 `inc/hgl/mtl/PipelineConfigEmitter.h` + `src/ShaderGen/template/PipelineConfigEmitter.cpp`。
- 管线配置头（`HGL_USE_*`、采样器索引、纹理通道数、阴影 taps 等）的单一真源；`FragmentTemplateComposer` 瘦身 60 行。
- 门禁新增 `PC.pipeline-config-emit`。

**之二 —— 最后三处 `#if` 清零**
- `ForwardUnlitSlots` 与两个 shadow caster 槽表补 `NTBProvider`；`ForwardLitUnshadowedSlots` 补 `AmbientOcclusionProvider`。
- 新增 `ShaderLibrary/ntb/ntb_none.glsl`（NTB 空对象）。
- 删除 `material_surface.glsl` 两处、`forward_lighting.glsl` 一处的 `#if`。
- **`HGL_USE_NTB_PROVIDER` / `HGL_USE_AMBIENT_OCCLUSION_PROVIDER` 两个宏连同字段整删**（不是禁用，是删除）。
- 模块数 81 → **82**。

**收尾（`b21563928`）**：路线图文档改为四阶段并与实际分支对齐。

### 4.3 阶段三之一（`c7d1e45cb`）—— 顶点变换推入领域契约

**调研结论**：14 个 `.material.toml` 只有 4 种变换组合，且**完全由 domain 决定**：
- 默认 3D 配置 × 12
- 2D NDC（`UnlitTexture` / `Checker2D`）× 2
- Text2D 的 `None + PixelToLocal + OrthoViewport` × 2

**实现**：
| 文件 | 改动 |
|---|---|
| `inc/hgl/mtl/MaterialDomain.h` | `MaterialDomainContract` 加第 10 字段 `canonical_vertex_node_config`；`MaterialDomainValidationResult` 加 `TransformMismatch`；新增 `GetMaterialDomainCanonicalVertexNodeConfig()` |
| `inc/hgl/mtl/VertexShaderNodeConfig.h` | 新增 `IsSameVertexNodeConfig(a, b)` |
| `src/ShaderMaterialData/MaterialDomain.cpp` | 3 个非 3D 领域填规范值；校验器新增「3c. 顶点变换」段（详细报文，提示删除 `[transform]` 表） |
| `src/ShaderMaterialData/MaterialDefinitionFile.cpp` | `[transform]` 缺省时从契约兜底；Custom 保留结构体默认 |
| 14 个 `ShaderLibrary/material/*.material.toml` | **整块删除 `[transform]`**（98 行） |
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | 加 1 条负向探针（3D 领域自选 2D 变换必须被拒）+ 2 处正向断言 + ③b 段 Text2D 兜底断言 |

**为什么变换属于 Domain 而不是资产**：变换链（PositionSource / PixelTransform / Projection / Depth / NormalTransform）决定的是「这份材质只能贴在哪类几何体上」。它是 Domain 的身份，不是实例的参数。资产一旦能自选，就等于允许「把 2D UI 材质挂到 3D 网格上」，而这正是校验器必须拒绝的错配。

### 4.4 阶段三之二（`20d36a0d0`）—— 纹理槽形态与 `material_data` 类型推入领域契约

**调研结论**：14 个 `.material.toml` 里，同一 Domain 下 `textures[].sampler` / `required` / `channels` 与 `material_data.type` **完全一致**（见 §6.1 实测表）；唯一不能上移的是 `[resources].samplers`（过滤设置），因为 `Text2D` 的 SDF 字形要 `Linear`、位图字形要 `Nearest`。

**实现**：
| 文件 | 改动 |
|---|---|
| `inc/hgl/mtl/MaterialDomain.h` | `MaterialDomainTextureSlot` 3 → 6 字段（`sampler` / `binding_required` / `channels`）；`MaterialDomainContract` 加第 11 字段 `material_data_type`；`MaterialDomainValidationResult` 加 `TextureSlotMismatch` / `MaterialDataTypeMismatch`；新增 `FindMaterialDomainTextureSlot()` / `GetMaterialDomainCanonicalMaterialDataType()` |
| `src/ShaderMaterialData/MaterialDomain.cpp` | 6 张槽表按新形态重写（`normal` 固定 `channels = 2`；`Sky.sky_cube` 是唯一的 `SamplerCube` 且 `must_declare = false` + `binding_required = true`）；16 个契约逐条填 `material_data_type`；校验器第 4 段重写为「白名单 + 形态」单遍扫描，新增第 5 段「材质数据类型」；加编译期 `static_assert` 守卫 surface 族 |
| `src/ShaderMaterialData/MaterialDefinitionFile.cpp` | `sampler` / `required` / `channels` 缺省时从契约兜底，槽名不在任何契约内则显式报错；新增 `declares_material_data` 标记，`[resources].material_data` 整块缺失时从契约兜底（显式非规范值保留给校验器报错）；`sampler` / `channels` 的类型错误现在也落键级诊断 |
| 9 个 `ShaderLibrary/material/*.material.toml` | 删除可上移字段：`lit` 的 6 个槽收成 `{ name = "..." }`；4 个无光材质删除 `material_data = { type = "EmissiveSurface" }`；`[resources].samplers` 全部保留 |
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | 加 4 条负向探针（自选采样器种类 / 自选绑定必要性 / 自选通道数 / 自选 `material_data` 类型）；③b 的 Text2D 探针从「无 `[transform]`」升级为「纹理只写 `name`」，同时断言变换、槽形态、数据类型三者都由契约兜底 |

**为什么槽形态属于 Domain**：同一领域内 `sampler` / `required` / `channels` 必然相同；资产重复声明唯一可能的后果是「同领域不同布局」，进而分叉 `layout_hash`、破坏合批。`channels` 更危险——它直接决定 shader 是否注入 `MTL_TEX_<NAME>_CHANNELS` 与 Z 分量还原逻辑，写错运行期不报错、只是数值全错。

**聚合初始化的坑**：`DomainContracts[]` 是聚合初始化，新增 `material_data_type` 后凡是没写到的条目都会被静默填成枚举 0 = `MeshDrawParams`，而 `IsGlobalSSBOType()` 会照样放行。因此加了编译期 `static_assert`，要求 16 个契约的该字段必须落在 surface 族（`PBRSurface` / `EmissiveSurface` / `TransmissionSurface`）内。**以后再往 `MaterialDomainContract` 加枚举字段，同样要加这种守卫。**

### 4.5 阶段三之三（`addc99d58`）—— 保底链材质定义代码化

**为什么必须做**：`builtin/pure_color`、`builtin/error_checkerboard_3d`、`builtin/checkerboard_3d`、`builtin/text_gpu`、`builtin/text_gpu_bitmap` 这五个 ID **就是保底链本身**（`inc/hgl/mtl/MaterialRecipe.h` 的 `kFallbackMaterialRules` 与 `src/ecs/support/text/TextRenderPipeline.cpp:425-426` 按字面量引用）。保底物一旦因为资产被误删 / 改名 / 写坏而解析不出来，错误处理就会**再失败一次** —— 保底物的可依赖性必须高于它所要兜底的东西。这五个材质在阶段三之一 / 之二之后也已没有任何可供作者调整的自由度。

**代码化的边界（关键设计决定）**：内嵌的仍是**完整 schema-4 源码文本**（`R"TOML(...)TOML"`），注册时走与 `.material.toml` **完全同一条**解析 + 领域校验管线。**没有**改成在 C++ 里手工拼 `MaterialDefinition` 结构体 —— 那样领域契约就得在第二处复述一遍（`material_source_module_name` / `ntb_module_name` / `vertex_node_config` / 纹理槽形态 / `material_data_type`…），正是本架构要消灭的重复。**代码化消灭的是「对磁盘文件的依赖」，不是「对校验管线的依赖」。**

**实现**：
| 文件 | 改动 |
|---|---|
| `inc/hgl/mtl/BuiltinMaterialDefinitions.h` | **新增**。`BuiltinMaterialDefinitionSource { source_label / definition_id / schema4_source }` + `GetBuiltinMaterialDefinitionCount()` / `GetBuiltinMaterialDefinitionSource(int)` / `FindBuiltinMaterialDefinitionSource(const char*)` |
| `src/ShaderMaterialData/BuiltinMaterialDefinitions.cpp` | **新增**。5 份 `constexpr const char[]` schema-4 源码 + `kBuiltinSources[]` 表（顺序即加载顺序） |
| `inc/hgl/mtl/MaterialDefinitionFile.h` | `MaterialDefinitionFileRegistry` 新增 `RegisterDefinitionSource(label, content, content_size)` |
| `src/ShaderMaterialData/MaterialDefinitionFile.cpp` | `LoadFile` 改为「读文件 → 委托 `RegisterDefinitionSource`」；解析 / 领域校验 / 重复 ID 检测只保留一份。日志键 `path`→`label`、`file=`→`source=` |
| `src/ShaderMaterialData/MaterialDefinitionRegistry.cpp` | `GetMaterialDefinitionFileRegistry()` 改为两段式：先注册 5 个内嵌（失败即 `GLogFatal` + `abort`，属引擎内部错误，不可降级），再扫资产目录（单文件解析失败仍 `GLogError` + 跳过）。空库判据从 `file_count == 0 \|\| registry.GetCount() == 0` 收窄为只看 `file_count`（内嵌保证 `GetCount() != 0`）。日志变为 `Loaded 9 material TOML definitions (0 errors) + 5 builtin definitions` |
| `src/ShaderMaterialData/CMakeLists.txt` | 加入两个新文件 |
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | `S.material-definition-file-schema` 改为复刻运行时两段式注册：断言内嵌数 == 5、**只喂内嵌源码的独立 registry 必须能解析全部 5 个保底 ID**、资产目录 `file_count == 9`、合并后 14 个 ID 全部可解析 |
| 5 个 `ShaderLibrary/material/*.material.toml` | **删除**（目录内剩 9 份资产定义 + 9 个 GLSL） |
| `doc/GUI_Text_Render_Pipeline.md` / `doc/simple-sphere-material-shadergen-overview.md` / `doc/terrain-rendering-implementation-plan.md` | 修正指向已删除文件的过期引用 |

**门禁新增的硬断言**：一个**从未接触过资产目录**的独立 registry 必须能解析这 5 个 ID。谁把它们挪回 `.material.toml`，这条立刻红 —— 这是防止架构回退的物理约束，不是风格偏好。

**`builtin/checkerboard_2d` 仍留在资产目录**：它是 `Checker2D` 领域但**不在**保底链上（只是调试用降级材质），所以按「只代码化保底物」的边界保留为文件，随阶段四一起迁走。

**注意**：这 5 个 ID **永远不能改**（默认 `builtin/pure_color`、错误 `builtin/error_checkerboard_3d`、降级 `builtin/checkerboard_3d`、文本 `builtin/text_gpu` / `builtin/text_gpu_bitmap`）。

---

## 5. 验证状态

### 5.1 自检命令（**必须在 `E:\ULRE` 仓库根运行**，`GLSLCompiler.dll` 在根目录）

```powershell
cd E:\ULRE
.\build\out\Windows_64_Debug\ShaderResourceSchemaRegressionGate.exe all      # 期望 55 PASS / exit=0
.\build\out\Windows_64_Debug\SceneRenderTemplateResolverRegression.exe        # 期望 exit=0
.\build\out\Windows_64_Debug\ShaderDocumentRegression.exe                     # 期望 exit=0
.\build\out\Windows_64_Debug\ShaderDocumentProductionRegression.exe           # 期望 exit=0
.\build\out\Windows_64_Debug\TestRenderStrategyTable.exe                      # 期望 exit=0（保底链行为）
.\build\out\Windows_64_Debug\TestMaterialVariantTable.exe                     # 期望 exit=0
```

构建单个目标：
```powershell
cmake --build build --config Debug --target ShaderResourceSchemaRegressionGate -- /m:2 /v:m
```
产物目录：`build\out\Windows_64_Debug\`

### 5.2 当前结果（全部绿，阶段三之三完成后复测）

- `ShaderResourceSchemaRegressionGate all`：**55 PASS / 0 FAIL，exit=0**（阶段三之三只在既有 case 内加断言，case 数不变）
- 三个回归 exe：**全 exit=0**
- `TestRenderStrategyTable` / `TestMaterialVariantTable`：**全 exit=0**（这两个覆盖保底链行为断言 —— 未知定义 ID ⇒ 换 `builtin/error_checkerboard_3d`、标记行不可得 ⇒ 降级 `builtin/checkerboard_3d`，是「内嵌定义可用」的最强证据）
- 实跑 `PBRSpheres` / `TextDraw`：`Loaded 9 material TOML definitions (0 errors) + 5 builtin definitions`、`Loaded 82 code modules`、无 GLSL 编译错误
- `src/Tools/ShaderGen/golden/` 下无 `.actual` 残留（无 golden 漂移）
- 实跑渲染 exe（脚本是会话局部产物，新会话请直接用下面命令）：
  ```powershell
  cd E:\ULRE
  $p = Start-Process .\build\out\Windows_64_Debug\PBRSpheres.exe -PassThru
  Start-Sleep -Seconds 20; Stop-Process -Id $p.Id
  ```

---

## 6. 阶段三的调研与方案记录（已全部实施）与下一步：阶段四

> ✅ **§6.1–6.3 已于 `20d36a0d0` 实施完成**（明细见 §4.4），**§6.4 已于 `addc99d58` 实施完成**（明细见 §4.5）。以下保留为当时的调研与方案记录，供阶段四迁移时对照。**下一步做 §6.5（阶段四）。**

### 6.1 逐材质实测统计（调研已完成，直接作为设计依据）

| 材质 | Domain | textures | samplers | material_data |
|---|---|---|---|---|
| `lit` | StandardOpaque | 6 槽 Sampler2DArray（`normal` 带 `channels = 2`），全 `required = false` | Trilinear / Linear / ShadowPCF | PBRSurface（缺省） |
| `unlit_texture` | UnlitTexture | `base_color` Sampler2DArray **required = true** | Trilinear | PBRSurface |
| `text_2d_gpu` | Text2D | `base_color` Sampler2DArray required | Linear | PBRSurface |
| `text_2d_gpu_bitmap` | Text2D | `base_color` Sampler2DArray required | **Nearest** | PBRSurface |
| `sky_cube` | Sky | `sky_cube` **SamplerCube** required | Trilinear | PBRSurface |
| `sky_minimal` | Sky | 无 | — | PBRSurface |
| `debug_normal_color` / `error_checkerboard_3d` / `pure_color` / `vertex_luminance` | DebugNormal / ErrorChecker3D / UnlitSolid / VertexLuminance | 无 | — | **EmissiveSurface** |
| `checkerboard_2d` / `checkerboard_3d` / `vertex_color` / `vertex_palette_color` | 各自 | 无 | — | 未声明 |

### 6.2 结论：可上移 vs 不可上移

**可上移（同一领域内完全一致）**
- `textures[].sampler`（StandardOpaque 全 `Sampler2DArray`；Sky 的 `sky_cube` 是 `SamplerCube`）
- `textures[].required`
- `textures[].channels`（`normal` 的 `channels = 2`）
- `material_data.type`（`GlobalSSBOType`：PBRSurface / EmissiveSurface）

**不可上移**
- `samplers[]`：**同一领域可以不同**（Text2D 的 SDF 用 `Linear`、Bitmap 用 `Nearest`）。必须留在资产上。

### 6.3 实现方案（照抄 `TransformMismatch` 模式）

1. `MaterialDomainTextureSlot`（`inc/hgl/mtl/MaterialDomain.h`）扩字段：`sampler` / `required` / `channels`；`MaterialDomainContract` 加 `material_data_type`。
2. `src/ShaderMaterialData/MaterialDomain.cpp` 的 `DomainContracts[]` 逐条填值（16 个领域）。
3. `src/ShaderMaterialData/MaterialDefinitionFile.cpp`：解析 `[resources].textures` 时，资产只写 `name` 即可，其余从契约兜底；若资产显式写了不一致的值 → 校验失败。
4. `MaterialDomainValidationResult` 新增枚举（如 `TextureSlotMismatch` / `MaterialDataTypeMismatch`）+ `GetMaterialDomainValidationResultName()` 加 case。
5. **注意**：校验失败必须把 `parse_error.key` 记成 `"domain"`，否则门禁报 "must be reported on key 'domain'"。
6. 14 个 TOML 删掉可上移的字段；**保留 `[resources].samplers`**。
7. 门禁：`D.material-domain-contract` 加正负向探针（例如「StandardOpaque 把 `normal` 的 channels 写成 4」「EmissiveSurface 领域的材质声明 PBRSurface」必须被拒）。
8. 观察点：`samplers` 参与 `GenericMaterialBuilder.cpp:978` 的结构哈希、经 `BuildSamplerMacros()` 生成 `#define <name>Sampler <idx>u`，改动需回归这四个 exe。

### 6.4 阶段三之三 —— 保底链代码化（✅ 已于 `addc99d58` 实施，见 §4.5）

- 把 `builtin/*` 保底材质**代码化**（`pure_color` / `error_checkerboard_3d` / `checkerboard_3d` / `text_gpu` / `text_gpu_bitmap`），不再依赖 TOML 文件。
- 回退链**id 绝不能改**：默认 `builtin/pure_color`、错误 `builtin/error_checkerboard_3d`、降级 `builtin/checkerboard_3d`、文本硬编码 `builtin/text_gpu` / `builtin/text_gpu_bitmap`（`TextRenderPipeline.cpp:425-426`）。
- 此时 `MaterialInstance` = `{domain_id, texture_bindings, numeric_values, samplers, render_state}`，C++ 只校验 + 发配置头。

### 6.5 阶段四 —— 收尾（分支 `feature/ultra-thin-stage4-cleanup-and-migration`）

- 资产降维为 `.mi.toml`（只写 Domain 名 + 资源 + 数值）。
- 废除 `.material.toml` 与 `MaterialDefinitionFile` 里已无意义的解析分支。
- 清理旧 ShaderGen 字符串拼接代码。
- **顺手清 3 处过期文档**（见 §10）。

**阶段四开工时值得顺手做的一件事**：`[vertex] requirements` 现在是**冗余声明** —— 它恒等于「所选 varyings 所需的顶点属性」∪「领域 `vertex_node_config` 所需属性」。9 份资产里没有一份能给出反例（同一领域内 `requirements` 全部相同，且都由 varyings 唯一决定）。把它改成**从 varyings 推导**（而非从领域契约上移），既删掉一个可以写错且不会报错的字段，也避免「领域固定 requirements」带来的过严约束（例如某 `StandardOpaque` 材质不发 `emit_world_normal` 时本不该强制要求 `Normal` 属性）。

---

## 7. 关键代码地图（全部「单一真源」位置）

| 关注点 | 文件 |
|---|---|
| 领域契约（16 个）唯一真源 | `inc/hgl/mtl/MaterialDomain.h` / `src/ShaderMaterialData/MaterialDomain.cpp` |
| 顶点变换枚举与比较 | `inc/hgl/mtl/VertexShaderNodeConfig.h` |
| 资产 TOML 解析与兜底 | `src/ShaderMaterialData/MaterialDefinitionFile.cpp` |
| 保底链内嵌定义（5 个 builtin，不依赖磁盘） | `inc/hgl/mtl/BuiltinMaterialDefinitions.h` / `src/ShaderMaterialData/BuiltinMaterialDefinitions.cpp` |
| 保底链规则表（ID 硬编码处） | `inc/hgl/mtl/MaterialRecipe.h` 的 `kFallbackMaterialRules` / `GetFallbackMaterialDefinitionID()` |
| 槽表（12 role） | `src/ShaderMaterialData/RenderTemplate.cpp` |
| 模板 profile | `src/ShaderMaterialData/SceneRenderTemplateResolver.cpp` |
| 模板选路（depth / lit / unlit / sky） | `src/ShaderMaterialData/MaterialDefinitionRegistry.cpp:128-138` |
| 三个固定主着色器 `.tmpl` | `ShaderLibrary/fragment/forward_lit.glsl.tmpl` / `forward_unlit.glsl.tmpl` / `sky.glsl.tmpl` |
| 管线配置头（`#include` 一半） | `inc/hgl/mtl/PipelineConfigEmitter.h` / `src/ShaderGen/template/PipelineConfigEmitter.cpp` |
| 能力名解析静态表 | `src/ShaderGen/glsl_module/ShaderCodeModuleFile.cpp:152`（`ParseCapabilityMask()`，**新增能力名必须同时加行**） |
| 门禁 | `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` |
| golden 结构快照 | `src/Tools/ShaderGen/golden/<slug>.txt` |
| 设计正文 | `doc/ultra-thin-fixed-shader-architecture.md` |

### 槽位与渲染模板（当前形态）

```
ForwardLitSlots[11]      : Sky → Diffuse → Specular → Ambient → Shadow → AO
                           → LightingModel → MaterialSource → NTB → OutputPolicy → Surface
ForwardLitUnshadowedSlots[10] : 同上，无 Shadow
ForwardUnlitSlots[4]     : OutputPolicy / MaterialSource / NTB / Surface
ShadowCasterOpaqueSlots[4] / ShadowCasterMaskedSlots[4] : MaterialSource(可选) / …
SkySlots[2]              : SkyProvider / Surface（刻意无 OutputPolicy）
MaxRenderTemplateModuleRoots = 16；Templates[] = 7 个；SurfaceProvider 恒排末位
```

> `SurfaceProvider` 必须末位：它调用 `EvalMaterialSource` / `EvalMaterialAlpha` / `GetNTB`，需要前面所有 provider 已求值。
> `AddModule` 对同一 role 返回 `false`（**不允许覆盖**）。

### 统一接口契约（模板方法名不可改）

- `PassEnvironmentInput{ geom, surface, dataIndex }` → `vec4 EvalPassEnvironment(...)`（`common/pass_environment_interface.glsl`）
  - 字段刻意叫 `geom` 不叫 `surface`：GLSL 结构体按**位置**构造，同名会穿错顺序（历史真实踩过）。
- `LightingInput` + `EvalDiffuseLight` / `EvalSpecularLight` / `EvalIndirectLighting` / `EvalLighting`
- `NTBSpace{ N, T, B }` + `NTBInput{ surface, dataIndex, normalScale }` → `NTBSpace GetNTB(NTBInput)`
- `EvalSurface(SurfaceInput si, uint dataIndex)` + `EvalAlpha`（`surface/material_surface.glsl`）
- 数学辅助：`ULRE_LIT_D_GGX` / `ULRE_LIT_G_Smith` / `ULRE_LIT_F_Schlick`

---

## 8. 门禁（`ShaderResourceSchemaRegressionGate`）关键点

Case 名与职责：

| Case | 作用 |
|---|---|
| `D.material-domain-contract` | ① 契约表自检 + 引用模块存在性；② 合法材质必须通过；③ 未声明 domain = Custom 豁免；③ 领域规范模块兜底正向；③b 非 3D 领域变换兜底；④ 负向探针表 `DomainViolationProbe probes[]`；⑤ 无光表面族正向 |
| `D2.shader-config-emit` | 校验配置头规范文本（ForwardLit 全开） |
| `PC.pipeline-config-emit` | `PipelineConfigEmitter` 输出 |
| `MI.module-include-declaration-contract` | Registry 加载期强制 `#include ⊆ @ulre uses` |
| `SD.structure-dump-golden-pilot` | 读 `golden/<slug>.txt` 比对；不匹配写 `<slug>.txt.actual` |

要点：

1. **负向探针断言 `parse_error.key == "domain"`**。任何新增的领域校验失败都必须记成 `"domain"`。
2. **模块总数硬编码 `82`，出现在两处**（`ShaderResourceSchemaRegressionGate.cpp` 约 `:6013` 与 `:6020`）。每次增删 `.glsl` 都要改。
3. `require_slot_module(name, slot_name)` 断言块约 `:6015`。
4. **golden 没有环境变量开关**。更新方式：`Copy-Item *.actual *.txt` 然后删除 `.actual`。首跑 golden 缺失会自动生成并 PASS。
5. 门禁注册表在文件尾部（约 `:8515` 起）：`run_glsl / run_interface / run_descriptor / run_cache / run_materialization / run_pipeline / run_module_invariants`。

---

## 9. 环境与工具坑（务必先看，能省半小时）

1. **门禁/回归 exe 必须在 `E:\ULRE` 仓库根运行**（`GLSLCompiler.dll` 在根目录）。
2. 材质 TOML 缺 `[fragment]` 表或 `[vertex] requirements` → `ParseDefinition` **静默 `return false`，无任何诊断**。新建测试材质必须两样都写全。
3. `hgl/type/ArrayList.h` **不存在**；HGL 集合是 `ValueArray` / `ManagedArray` / `PtrArray`。
4. `FNV1a.h` 在 `CMCoreType/inc/hgl/util/hash/FNV1a.h`（**不在** `CMCore/`）。
5. 项目**禁止 STL**，用 HGL 库（详见仓库 `.ai/skills/SKILL_HGL_TYPES_REFERENCE.md`）；日志用 `GLogInfo` / `OBJECT_LOGGER` + `LogInfo`，禁 `printf` / `std::cout`。
6. **PowerShell 无 heredoc**：多行提交信息用
   `$msg = @'…'@` → `Out-File -Encoding utf8NoBOM _cmsg.txt` → `git commit -F _cmsg.txt` → `Remove-Item _cmsg.txt`。
7. PowerShell `-replace` 的替换串里 `\r\n` 会变成字面量。要保行尾请用 `[System.IO.File]::ReadAllText` + `[regex]::Replace` + `WriteAllText`，或 `.Replace()`。
8. `Select-String -Pattern ''` 报错；`[regex]::Match` 第二参必须是 int（用 `(?m)` 内联标志，不要传 `'Multiline'`）。
9. 仓库有病理深目录 `CMAudio\Plug-Ins\Audio.FluidSynth\...`，`Get-ChildItem -Recurse` 会路径过长/超时。
10. 全量 `ctest` 约 1925 秒，基线 **183/184**（`BidirectionalMapStressTest` Timeout 为既存失败，不是回归）。
11. `MaterialErrorFallback` / `DualWorldShadows` 退出期崩溃（`exit=-1073741819`）是**既存问题**，与本次重构无关。
12. 日志里 `Failed to find vkGetDeviceProcAddr in layer ...amdvlk64.dll` 是既存噪声，不是错误。
13. 提交必须 `git add` 显式列路径（禁 `-A`），带
    `Co-authored-by: Copilot App <223556219+Copilot@users.noreply.github.com>`。

---

## 10. 已知技术债 / 待清理

### 10.1 文档过期（3 处，阶段四顺手清）

| 文件 | 行 | 过期内容 |
|---|---|---|
| `doc/ENVIRONMENT_SYSTEM.md` | `:244` | 说 AO 调用由 `HGL_USE_AMBIENT_OCCLUSION_PROVIDER` 守卫 —— 宏已删除 |
| `doc/material-lit-recipe-to-shadergen-dataflow.md` | `:92` | 宏区仍列 `HGL_USE_NTB_PROVIDER 1` —— 已删 |
| `doc/ShaderGen_ComposableTemplate_TechnicalDesign.md` | `:369` | 同上 |

### 10.2 结构性遗留

- 4 个领域（`Foliage` / `Terrain` / `HumanSkin` / `UI2D` 部分）reference 为 `nullptr`，只有契约骨架、无专属实现。属**刻意保留的扩展位**，不是 bug。
- `DirectLightProvider`（legacy role）仍在枚举里，阶段一已由 `DiffuseLightProvider` + `SpecularLightProvider` 取代，待阶段四删除。
- 主着色器仍保留 alpha 三件套宏（`HGL_ALPHA_TEST` / `HGL_ALPHA_DITHER` / `HGL_ALPHA_OUTPUT`）—— **这是刻意设计**，因为它们必须与 `discard` / 输出同函数体，无法外移。
- 模块总数 `82` 硬编码在两处，尚未收敛成常量。

---

## 11. 每个子阶段的完成定义（DoD）

- [ ] `ShaderResourceSchemaRegressionGate all` → 55 PASS / exit=0（若新增 case 则数量随之增加并同步文档）
- [ ] `SceneRenderTemplateResolverRegression` / `ShaderDocumentRegression` / `ShaderDocumentProductionRegression` → 全 exit=0
- [ ] `TestRenderStrategyTable` / `TestMaterialVariantTable` → 全 exit=0（保底链行为断言，改动材质定义/回退规则时必跑）
- [ ] 实跑 `PBRSpheres` + `TextDraw`：0 errors、模块数正确、无 GLSL 编译错误
- [ ] `git status` 只剩既存子模块脏项
- [ ] 更新 `doc/ultra-thin-fixed-shader-architecture.md` 的「阶段进展」条目
- [ ] 独立提交，提交信息带 `feat(ultra-thin):` / `docs(ultra-thin):` 前缀
