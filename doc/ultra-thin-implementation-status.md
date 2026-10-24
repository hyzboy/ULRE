# 超薄固定着色器架构 —— 实施现状与交接文档

> **用途**：把「Domain 即 Master Material」重构的**全部计划现状**固化下来，供新会话零上下文接续。
> **配套文档**：`doc/ultra-thin-fixed-shader-architecture.md`（架构设计正文，8 节，权威设计）。
> 本文只记录**进度、提交、验证结果、下一步动作、踩过的坑**，不重复设计论证。
> **最后更新**：阶段六收尾复查后（分支 `feature/ultra-thin-stage5-shadergen-emission-cleanup`）。**四阶段全部完成，`.material.toml` 已废除；拼接顺序已有 golden 安全网（16 变体，含 3 种 mesh 发射模式）；四条 emission 通路全部走 `.tmpl`；mesh 侧拼接债经复查确认已清空，STL 残留清理判定为需整体迁移、刻意不做。**

---

## 0. 新会话开场怎么用

1. 先读本文 **§1 目标** 和 **§3 路线图**，确认自己在哪一阶段。
2. **四阶段已全部完成**（一 / 二 / 三 3.1–3.3 / 四，见 §4.1–§4.6），阶段五（拼接清理前置安全网 + 小债，见 §4.7）与阶段六（ShadowCaster 骨架外置，见 §4.8）也已完成。阶段四已把资产降维为 `.mi.toml` 并废除 `.material.toml`，方案记录见 §6.5。
3. 读 **§9 环境与坑**，避免重复踩已记录的坑（尤其是「门禁探针必须报 `domain` key」「模块数常量 `kShaderCodeModuleCount`」「golden 无环境变量开关」「快照 case 必须自己装载 sampler 预设」「`alpha_test` 必须写在模板请求解析之前」）。
4. 动手前跑三条自检命令（见 §5.1），确认基线是绿的。
5. **剩余工作见 §10.2** —— mesh 侧骨架经复查判定「已做到该做的程度」（剩余段数来自刻意的单一真源数据表），STL 残留清理经实测判定**必须整体迁移、刻意不做**。即当前**没有待办的技术债**，下一步动作需由用户指定（可选：STL 整体迁移、`inc/hgl/mtl/` 公共头清理、4 个 `nullptr` 领域补实现）。

**铁律**：每个阶段/子阶段**独立分支 + 独立提交**；提交用 `git add` 显式列路径（禁 `-A`）；带 `Co-authored-by` trailer。

---

## 1. 架构目标

一句话：**上层开发者不存在「Material」这个概念。他只需要选模型是什么（Domain）、填好数值、塞好纹理。**

五条铁律：

| # | 铁律 | 现状 |
|---|------|------|
| 1 | Domain 就是未来的 Master Material，引擎硬编码，上层**不可**自定义、不可改 | ✅ 16 个领域契约已成体系 |
| 2 | 上层资产 = MaterialInstance：只能**选一个 Domain** + 配自己的资源与数值 | ✅ 阶段四完成：资产只剩 `domain` + `[vertex] varyings` + `[resources]` |
| 3 | 最终废除 `*.material.toml` 这一中间层 | ✅ 阶段四完成：9 份资产全部迁为 `.mi.toml`，解析器不再识别旧后缀 |
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
| 11 | `feature/ultra-thin-stage4-cleanup-and-migration`（已并入） | `ec9fae16d` | **阶段四**：资产降维为 `.mi.toml`，废除 `.material.toml` |
| 12 | 同上 | `29b3febfe` | 阶段四文档收尾：回填提交号并对齐资产示例（纯文档） |
| 13 | `feature/ultra-thin-stage5-shadergen-emission-cleanup`（已并入） | `0fffcdba1` | **阶段五**：拼接清理前置安全网（emission 快照）+ 小债清理 |
| 14 | 同上 | `caf38cb34` | 阶段五文档收尾：回填提交号（纯文档） |
| 15 | 同上 | `83bac9349` | **阶段六**：ShadowCaster 内联骨架外置为 `shadow_caster.glsl.tmpl`（+ 补 masked 快照覆盖） |
| 16 | 同上 | `528531be8` | 阶段六文档收尾：新增 §4.8 并修正快照覆盖口径（纯文档） |
| 17 | 同上 ← **当前** | `53a3629ef` | 补齐 `LineQuad` mesh 发射模式的快照覆盖（矩阵 14 → 16，纯测试夹具） |
| 18 | 同上 | `eeb325d36` | STL 残留改判：从「纯机械替换」改为需要整体迁移（纯文档） |

当前工作区干净，仅有既存子模块脏项 `? CMScript` / `? res` / `? src/Tools/GLTFConvert`（**不是**本次改动，勿提交）。

累计 diff（`6ef579059..eeb325d36`，即上表第 3 行 → 第 18 行）：114 files, +6080 / −1085。

---

## 3. 六阶段路线图与状态

| 阶段 | 主题 | 分支 | 状态 |
|---|---|---|---|
| 一 | 直接光正交拆分（漫反射 × 高光）+ 空对象库 + 槽角色/能力扩展 | `feature/ultra-thin-stage1-lighting-slots` | ✅ 完成 |
| 二 | 管线配置头单一真源 + 消灭主着色器里所有算法相关 `#if` | `feature/ultra-thin-stage2-domain-shaders` | ✅ 完成 |
| 三 | **MaterialInstance 模型**：把「领域已确定的旋钮」逐个从资产契约上移到领域契约 | `feature/ultra-thin-stage3-material-instance` | ✅ 完成（3/3） |
| 四 | 资产降维为 `.mi.toml` + 废除 `.material.toml` | `feature/ultra-thin-stage4-cleanup-and-migration` | ✅ 完成（迁移 + 废除） |
| 五 | **拼接清理前置安全网** + 小债清理（`DirectLightProvider`、模块计数常量） | `feature/ultra-thin-stage5-shadergen-emission-cleanup` | ✅ 完成（安全网就位） |
| 六 | **骨架外置**：ShadowCaster 内联骨架 → `fragment/shadow_caster.glsl.tmpl` | 同上 | ✅ 完成（见 §4.8；mesh 侧多段 `+=` 合并仍留 §10.2） |

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

### 4.6 阶段四（`feature/ultra-thin-stage4-cleanup-and-migration`）—— 资产降维与 `.material.toml` 废除

**资产最终形态**（`ShaderLibrary/material/lit.mi.toml` 全文，6 个纹理槽里最复杂的一份）：

```toml
schema = 4
id = "Lit"
domain = "StandardOpaque"

[vertex]
varyings = ["emit_data_index_id", "emit_world_pos", "emit_world_normal", "emit_uv0"]

[resources]
textures = [
    { name = "base_color" }, { name = "roughness" }, { name = "metallic" },
    { name = "occlusion" }, { name = "opacity_mask" }, { name = "normal" }
]
samplers = ["Trilinear", "Linear", "ShadowPCF"]
```

资产里**只剩三样**：选哪个领域、发哪些插值、声明哪些槽名与采样器。`name` / `provider_policy` / `[fragment]` / `[vertex] requirements` / `[transform]` / 槽形态（`sampler` / `required` / `channels`）/ `material_data.type` **全部由领域契约给出**。

**实现**：
| 文件 | 改动 |
|---|---|
| `inc/hgl/mtl/MaterialDomain.h` / `src/ShaderMaterialData/MaterialDomain.cpp` | 新增 `GetMaterialDomainVertexSemanticRequirements()`：把契约的 `required_vertex_semantic_mask` 翻译成 `ShaderCodeModuleSemanticRequirement` 数组（`AO` / `Rotation` 无正向映射，跳过） |
| `inc/hgl/mtl/MaterialRecipe.h` | `GetShaderCodeModuleSemanticFromVertexSemantic()` 补上漏掉的 `VertexSemantic::Size` → `ShaderCodeModuleSemantic::Size`；`IsMaterialVertexSemanticRequiredForVarying()` 从 `.cpp` 的匿名命名空间提升为**公开 inline**，成为「语义 ↔ varying」的唯一真源 |
| `src/ShaderGen/builder/GenericMaterialBuilder.cpp` | 删除本地那份 `IsVertexSemanticRequiredForVarying()`，改调 `MaterialRecipe.h` 的公开版本（深度 pass 重过滤行为不变） |
| `src/ShaderMaterialData/MaterialDefinitionFile.cpp` | `name` / `provider_policy` / `[fragment]` / `[vertex] requirements` 四项改为**领域材质可缺省**；缺省走契约兜底，非领域材质（Custom）缺任何一项都带**键级诊断**报错。`LoadDirectory()` 的扩展名过滤 `.material.toml` → `.mi.toml` |
| `src/ShaderMaterialData/BuiltinMaterialDefinitions.cpp` | 5 份内嵌源码同步降维（去掉 `provider_policy` / `[fragment]` / `requirements`；`name` 仅在确实不同于 `id` 时保留） |
| 9 个 `ShaderLibrary/material/*.mi.toml` | `git mv` 改名 + 全部重写为降维形态；`sky_cube` 保留 `[fragment] surface_module_name`（legacy 但仍在用） |
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | ① 库契约 case 的扩展名检查从「静默 `continue`」改为**正向断言**（任何非 `.mi.toml` 文件立刻红）；② 新增**正向推导探针** `DerivedVertexColor`（不写 name / provider_policy / fragment / requirements，断言四项全部由契约填对）；③ 新增**反向探针** `CustomNoPolicy`（Custom 缺 `provider_policy` 必须报 `key == "provider_policy"`）、`CustomNoRequirements`（缺 `requirements` 必须报 `key == "requirements"`）；④ 新增 **ABI 不变式探针** `DerivedKeepAbi` |
| `example/Environment/SkyCubeSphere.cpp` + 约 45 处文档引用 | `.material.toml` → `.mi.toml` |

**`[vertex] requirements` 为什么不从 varyings 推导（推翻了 §6.5 的建议）**：§6.5 曾建议「`requirements` 恒等于 varyings 所需属性，改成从 varyings 推导」。实施时先按这个思路做了交集，**门禁立刻红了**：

```
varying-trimmed material parse failed: InvalidValue key="domain" value="DebugNormal"
  detail="[vertex] requirements 缺少领域 DebugNormal 必需的顶点语义：Normal"
```

原因：`requirements` 是**顶点 ABI**（决定顶点布局与 `VertexABIBuilder` 的输入描述），`varyings` 只是**着色器发什么插值**。同领域必须同布局才能共享 layout / 合批；把不发 `emit_world_normal` 的材质裁掉 `Normal`，会让它与同领域其它材质的顶点布局分叉，领域契约校验**直接拒绝**。两者不是一回事，交集是错的。

结论：`requirements` 从**领域契约**推导（`required_vertex_semantic_mask`），且**不与 varyings 取交集**。`IsMaterialVertexSemanticRequiredForVarying()` 保留在 `MaterialRecipe.h`，只服务它本来的用途 —— ShaderGen 的**深度 pass** 重过滤（那里主表面着色器被替换，插值需求确实不同）。`DerivedKeepAbi` 探针就是这次拒绝的锚点：DebugNormal 材质即使不发 world normal，`Normal` 也必须留在需求里。

**为什么 `[transform]` 解析分支没有删**：资产与内嵌定义里确实已无任何 `[transform]` 声明，但 `Custom` 材质**仍然合法地**用它自选顶点通路（门禁 `S.material-definition-file-schema` 的主 fixture 就是 Custom + 显式 `[transform]`），而 `D.material-domain-contract` 有一条负向探针专门断言「3D 领域自选 2D 变换必须被拒」。删掉解析分支会同时废掉这两条路径。**保留**才是正确形态：Custom 可以选，领域材质选了就报错。

### 4.7 阶段五（`feature/ultra-thin-stage5-shadergen-emission-cleanup`）—— 拼接清理前置安全网

阶段四遗留的唯一实质项是「旧 ShaderGen 字符串拼接代码清理」。但 `FragmentTemplateComposer` / `GenericMaterialBuilder` 的**拼接顺序本身就是隐式契约**：`#include` 的先后、`#define` 在 `#include` 之前还是之后，都是当前可运行的 GLSL 的一部分，却没有任何断言看得见它们。先动拼接、再发现顺序变了，代价是逐帧对着黑屏猜。所以本阶段先造安全网，再清理小债；实际拼接迁移留作下一个可评审的独立改动（见 §10.2）。

**新增 emission 顺序快照**（`DumpShaderEmissionOrder`）：

| 文件 | 改动 |
|---|---|
| `inc/hgl/mtl/ShaderStructureDump.h` | 新增 `DumpShaderEmissionOrder(ctx, label)` 声明与设计说明：与 `DumpShaderStructure` **互补** —— 后者记录「有什么」（模块、varying、信号），前者记录「按什么顺序拼起来」 |
| `src/ShaderGen/compile/ShaderStructureDump.cpp` | 匿名命名空间新增 `CollapseWhitespace()` / `ClassifySkeletonLine()` / `AppendStageEmissionOrder()`；文件尾新增 `DumpShaderEmissionOrder()` |
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | 抽出 `CompareAgainstGolden()` 复用比对逻辑；快照矩阵从 5 个变体扩到 **12 个**；新增 case `SD.emission-order-golden` |
| `src/Tools/ShaderGen/golden/*.emission.txt` | 12 份新 golden（3.8–8.3 KB） |

**粒度选择（关键）**：快照**只记录骨架行** —— 预处理指令、顶层 `layout(...)`、顶层函数签名；函数体、注释、空行、缩进全部排除，行内连续空白折叠成单空格。这是刻意的：将来把骨架外化成 `.tmpl`、或把多段 `+=` 合并成单次 emit，只要**token 顺序不变**，golden 就不需要刷新。若 golden 每次重构都要刷，它就退化成一张废纸。

配套细节：
- **顶层判定在折叠空白之前**做（`*cursor` 不是空格/制表符才算顶层），否则缩进的函数体行会被误判成声明。
- `ClassifySkeletonLine()` 的启发式：`#` 开头 → `dir`；`layout` 前缀 → `layout`；其余要求「含空格且含 `(` 且空格在括号前」「首词不在语句关键字表内」「不以 `;` 结尾」三条同时成立才算 `func`（语句关键字表含 `if/else/for/while/do/switch/case/return/discard/struct/uniform/buffer/const/in/out/inout/flat/precise`）。

**12 变体矩阵覆盖全部 4 条 emission 通路**：`ForwardLit` / `ForwardUnlit` / `ShadowCaster` / `Sky`，并覆盖领域差异（带纹理 / 纯顶点 / 无顶点格式的 SSBO）。矩阵表 `kStructureDumpVariants[]`、`MakeStructureDumpProfile()`、`BuildStructureDumpVariant()` 提升到文件作用域，`SD.structure-dump-golden-pilot` 与 `SD.emission-order-golden` **共用同一张表**，避免两条快照覆盖范围悄悄分叉。

> ⚠️ **事后修正（阶段六）**：这里当时写「覆盖 ShadowCaster」是不完整的 —— 12 个变体里没有任何一个设置 `alpha_test`，所以 `ShadowCaster` 只覆盖了 opaque 分支，masked 分支（`requires_alpha == true`）**覆盖为 0**。阶段六已补 `lit-depth-masked` / `lit-shadow-masked`，矩阵 12 → 14。**教训：矩阵「包含某条通路」不等于「覆盖该通路的所有分支」，枚举分支时要把每条 `if` 都点名。**

**三个在造网过程中踩到并修掉的坑**：

1. **`SamplerPresetLibrary` 是进程级单例，由 `materialization` 组或各 App 启动时装载**。只跑 `pipeline` 组时预设没装，`MaterialShaderEmitter.cpp:115` 的 `GetIndex(name)` 未命中，`#define TrilinearSampler 2u` / `LinearSampler` / `ShadowPCFSampler` **被静默省略** —— GLSL 照样生成，只是少了三行 `#define`。于是 `pipeline` 组跑出来的 golden 与 `all` 组跑出来的不一致。修法：新增 `EnsureSamplerPresetsLoaded()`，`GetCount()==0` 时自行装载 `ShaderLibrary/sampler.toml`，两个快照 case 开头都调用。**教训：快照门禁必须自己拥有前置条件，否则 golden 本身就不可信。**
2. **`DebugNormalColor` 是无光调试材质**，在 `ForwardLitShadowedAO` 下 `ResolveSceneRenderTemplateRequest` 直接失败；矩阵里改用 `ForwardUnlit`。
3. `GetShaderLibraryPath()` 返回 `const char*`，直接 `+ "/sampler.toml"` 会 `C2678`；必须先构造 `std::string`。

**小债清理**：

| 项 | 处理 |
|---|---|
| `ShaderModuleSlotRole::DirectLightProvider`（阶段一已被 `DiffuseLightProvider` + `SpecularLightProvider` 取代） | 从 `inc/hgl/mtl/RenderTemplate.h` 枚举与 `src/ShaderMaterialData/RenderTemplate.cpp` 名称映射 switch 中删除。安全性已确认：`ShaderCodeModuleFile.cpp` 按 `Unknown+1 .. SlotRoleCount` 遍历，删值不影响枚举边界 |
| 模块总数 `82` 硬编码两处 | 收敛为 `constexpr int kShaderCodeModuleCount = 82;`（放在 `struct GateResult` 之后）。**刻意不从实现侧推导** —— 门禁要断言一个独立持有的期望值，用被测实现算出来的期望值等于没断言 |

---

### 4.8 阶段六 —— ShadowCaster 骨架外置

阶段五的判断是「拼接顺序本身是隐式契约」，所以先造网再动刀。阶段六就是用那张网做的第一刀：把 `FragmentTemplateComposer` 里**最后一段内联 GLSL 骨架** `ComposeShadow()`（约 134 行）彻底删掉，改由 `ShaderLibrary/fragment/shadow_caster.glsl.tmpl` 承载。至此 4 条 emission 通路（`ForwardLit` / `ForwardUnlit` / `ShadowCaster` / `Sky`）**全部走 `.tmpl`**，不再有「某一族骨架写在 C++ 里」的特例。

**为什么先补覆盖（关键顺序）**：9 个示例 `.mi.toml` 与 5 个 builtin 定义**没有任何一个**设置 `alpha_test`，而 `ComposeShadow()` 的 masked 分支（`requires_alpha == true`）只在那时进入 —— 也就是说这一刀砍下去，快照矩阵对 masked 分支的覆盖是 **0**。于是先扩矩阵再删代码。

| 文件 | 改动 |
|---|---|
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | `StructureDumpVariant` 增 `bool alpha_test`；`kStructureDumpVariants[]` 12 → **14**（新增 `lit-depth-masked` / `lit-shadow-masked`）；`BuildStructureDumpVariant()` 在**模板请求解析之前**写 `alpha_test = true` + `alpha_cutoff = 0.5f` |
| `src/Tools/ShaderGen/golden/lit-depth-masked.txt` / `lit-shadow-masked.txt` | 2 份新结构快照 |
| `src/Tools/ShaderGen/golden/lit-depth-masked.emission.txt` / `lit-shadow-masked.emission.txt` | 2 份新 emission 快照 |

**`alpha_test` 必须写在 `ResolveSceneRenderTemplateRequest()` 之前**：`MaterialCoverageContract.cpp:112` 的 `requires_alpha_evaluation = (mode != None)` 是从 `ResolvedMaterialRenderState{alpha_test, alpha_cutoff, dither}` 经 `ResolveMaterialRenderState()` 推出来的，契约在**模板请求解析那一刻**读取渲染状态。写在后面等于没写 —— 这条踩过。

**外置后的结构**（`shadow_caster.glsl.tmpl`，7 个占位符）：

```
{{defines}}
{{alpha_resources}}
{{fragment_inputs}}
{{output_declarations}}
{{code_modules}}
{{alpha_includes}}
{{main_body}}
```

配套改动：

| 文件 | 改动 |
|---|---|
| `inc/hgl/mtl/ShaderConfig.h` | `ShaderConfigProfile` 新增 `ShadowCaster`（现 4 值：`None` / `ForwardLit` / `ForwardUnlit` / `ShadowCaster`）。头注释明确：**四个 profile 互斥**，宏集合只由模板族决定 |
| `src/ShaderGen/contract/ShaderConfig.cpp` | `EmitShaderConfigDefines` 成为 `#define HGL_COVERAGE_ONLY 1` 的**单一真源**（此前是 `ComposeShadow` 手写）；`ShadowCaster` 分支提前返回，跳过 provider 旗标 / 纹理通道 / PCF 宏；纹理通道循环与 PCF 块加 `profile != ShadowCaster` 守卫 |
| `src/ShaderMaterialData/RenderTemplate.cpp` | `GetRenderTemplateFragmentSourceFile()` 补 `ShadowCasterOpaque` / `ShadowCasterMasked` → `"shadow_caster.glsl.tmpl"`，四族齐了；`nullptr` 语义收窄为「未注册」 |
| `src/ShaderGen/template/FragmentTemplateComposer.cpp` | 删除 `ComposeShadow` 与 4 个随之失效的 helper（`AddTemplateBlock` / `AppendDocumentBlocks` / `AppendFragmentInputDeclarations` / `AppendOutputDeclarations`），852 → 646 行；新增 `IsShadowCasterTemplate` / `RequiresAlphaEvaluation` / `BuildShadowAlphaIncludeText` / `BuildShadowMainBodyText`；`ComposeFromFragmentTemplate` 增 `is_shadow` 分支（输出附件允许整型、alpha 资源/`#include`/`main` 体三槽） |
| `inc/hgl/mtl/contract/ShaderGenProfileTargetVersion.h` | `SHADERGEN_CODEGEN_GENERATION` 13 → **14**（见下方「唯一的行为变化」） |
| `src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp` | `D2.shader-config-emit` 增子用例 ⑥，钉死 ShadowCaster 的规范宏文本（首跑就抓到纹理通道与 PCF 宏泄漏） |

**唯一的行为变化（已接受并写进 generation changelog）**：旧 `ComposeShadow` 把 `HGL_COVERAGE_ONLY` / `HGL_ALPHA_*` 作为 `Define` 块**放在材质注入块之前**；新路径是单一 `MainBody` 块，**在注入之后** —— 与 forward 模板一致。宏集合与文本逐字节相同，只有位置变了。4 份 shadow emission golden 因此位移（`lit-depth-only` 的三行 define 从 L52-54 移到 L59-61 / L62-64），**文件字节数不变**（4203 / 6541）。

**为什么不去保留旧顺序**：那需要在材质注入路径（`MaterialShaderEmitter::EmitMaterialInjection`）上加一个 `pending_prefix` 钩子，让 `.tmpl` 路径把 define 前插 —— 新机制，且要动 forward 模板那条已验证的路径。forward 模板本来就把 define 放在注入之后，所以新顺序才是**一致的那个**。

**两个易错的实现细节**：
- **NTB 必须排在 surface 之前**：`material_surface.glsl` 的定义体无条件调用 `GetNTB`，顺序反了直接编译失败。
- **`materialDataIndex` vs `0u`**：`BuildShadowMainBodyText` 按 `coverage_contract->required_semantics & GetInterStageSemanticMask(InterStageSemantic::DataIndexID)` 决定是走 `fragment_inputs` 的接线还是手写零初始化 `si`。

---

## 5. 验证状态

### 5.1 自检命令（**必须在 `E:\ULRE` 仓库根运行**，`GLSLCompiler.dll` 在根目录）

```powershell
cd E:\ULRE
.\build\out\Windows_64_Debug\ShaderResourceSchemaRegressionGate.exe all      # 期望 56 PASS / exit=0
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

### 5.2 当前结果（全部绿，阶段六完成后复测）

- `ShaderResourceSchemaRegressionGate all`：**56 PASS / 0 FAIL，exit=0**（阶段四未增删 case；阶段五新增 `SD.emission-order-golden`，55 → 56；阶段六只在 `D2.shader-config-emit` 内增子用例 ⑥，case 总数不变）
- 五个回归 exe（`SceneRenderTemplateResolverRegression` / `ShaderDocumentRegression` / `ShaderDocumentProductionRegression` / `TestRenderStrategyTable` / `TestMaterialVariantTable`）：**全 exit=0**
- `TestRenderStrategyTable` / `TestMaterialVariantTable`：**全 exit=0**（这两个覆盖保底链行为断言 —— 未知定义 ID ⇒ 换 `builtin/error_checkerboard_3d`、标记行不可得 ⇒ 降级 `builtin/checkerboard_3d`，是「内嵌定义可用」的最强证据）
- 实跑 `PBRSpheres` / `TextDraw`：`Loaded 9 material TOML definitions (0 errors) + 5 builtin definitions`、`Loaded 82 code modules`、`loaded 34 entries from program_manifest.pmf`、无 GLSL 编译错误
- `src/Tools/ShaderGen/golden/` 下无 `.actual` 残留（无 golden 漂移）。该目录现在有两类快照，各 **16** 份、按同一张变体矩阵一一对应：`<slug>.txt`（结构快照）与 `<slug>.emission.txt`（**拼接顺序快照**）。阶段六把矩阵从 12 扩到 14（新增 `lit-depth-masked` / `lit-shadow-masked`），并刷新了 2 份 shadow emission golden（define 位置变化，字节数不变）；阶段六收尾再扩到 16（新增 `vertex-color-lines-quad` / `vertex-color-lines-quad-2d`，补上此前**零覆盖**的 `LineQuad` mesh 发射模式，见 §5.3）
- 全量 `cmake --build build --config Debug` 除一个**既存无关**失败外全绿：`TexConvCore.vcxproj` 的 `LNK1104: cannot open file '..\..\..\..\out\Windows_64_Release\TexImage.lib'`（Debug 目标链 Release 库，与本次重构无关）
- 实跑渲染 exe（脚本是会话局部产物，新会话请直接用下面命令）：
  ```powershell
  cd E:\ULRE
  $p = Start-Process .\build\out\Windows_64_Debug\PBRSpheres.exe -PassThru
  Start-Sleep -Seconds 20; Stop-Process -Id $p.Id
  ```

### 5.3 快照矩阵的 mesh 发射模式覆盖（阶段六收尾补齐）

`SD.emission-order-golden` 的原始设计目标是「给 mesh 侧骨架外置当安全网」（见 §10.2），
但阶段五建网时矩阵只从 **fragment** 维度枚举（四条 emission 通路 × 领域差异），
mesh 侧只被顺带覆盖了 `VertexPassthrough` 与 `CharQuad`：

| mesh 发射模式 | 选择依据 | 补齐前的变体 |
|---|---|---|
| `VertexPassthrough` | 默认（`primitive_type` 非 `Lines`） | 13 个 |
| `CharQuad` | `definition.mesh_shader_mode` 显式指定 | `text-gpu-charquad` |
| `LineQuad` | **只能**由 `primitive_type == Lines` 推出 | **0 个** |

`LineQuad` 恰恰是三者里槽位逻辑最复杂的一个（`line_quad.glsl.tmpl` 的
`{{endpoint_read}}` 按 `position_format` 分叉），却完全没有网 —— 与阶段六
masked `ShadowCaster` 是同一类漏，属于「改坏了也没人会报」。

补齐方式（只动夹具，不动被测代码）：

| 文件 | 改动 |
|---|---|
| `StructureDumpVariant` | 追加 `PrimitiveType primitive_type = Triangles` 与 `bool vec2_position = false`（追加在末尾，前 14 个变体的位置初始化不受影响） |
| `kStructureDumpGeometry2D` | 新增：`Position` 用 `VF_V2F`。共享表恒为 `VF_V3F`，不换表就永远走不到 vec2 分叉 |
| `kStructureDumpVariants[]` | 14 → **16**：`vertex-color-lines-quad`（vec3）、`vertex-color-lines-quad-2d`（vec2） |
| `BuildStructureDumpVariant()` | 把 `variant.primitive_type` 写进 `MaterialDefinitionBuildRequest`，几何表按 `vec2_position` 二选一 |

两份 golden 的差异已实测确认覆盖真实分叉，不是冗余副本：

- 与 `vertex-color-forward`（`VertexPassthrough`）比：`layout(triangles, max_vertices = 256, max_primitives = 128)` vs `192 / 64`，`fragVertexColor[256]` vs `[192]`，且 vertex-input 的 `#ifdef` 分支结构不同 —— 拓扑解析器确实换成了 `ResolveLineQuadTopology`（4× / 2×）。
- vec3 与 vec2 变体互比：`#include "vertex/s1_position_vec3.glsl"` vs `s1_position_vec2.glsl` —— `position_format` 管线确实被走通了。

> ⚠️ **仍未覆盖**：`{{endpoint_read}}` 的两段**函数体**文本不在快照粒度内（发射顺序快照只看骨架行），
> 且夹具是 `defer_finalize = true`（不编译 SPV），所以那两段 GLSL 的正确性只靠实跑
> `Lines` 渲染（`LineRenderPipeline`）保证。补网时不要误以为它已被 golden 锁住。

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

### 6.5 阶段四 —— 收尾（✅ 已实施，见 §4.6）

- 资产降维为 `.mi.toml`（只写 Domain 名 + 资源 + 数值）。✅
- 废除 `.material.toml` 与 `MaterialDefinitionFile` 里已无意义的解析分支。✅
- 清理旧 ShaderGen 字符串拼接代码。⬜ **未做**，见 §10.2 —— 它是独立的、范围较大的重构，与资产迁移无耦合，混进本阶段会让 diff 无法审阅。
- 顺手清 3 处过期文档（见 §10）。✅

**关于 `[vertex] requirements` 从 varyings 推导的建议 —— 已评估并否决**，理由见 §4.6「为什么不从 varyings 推导」：它是顶点 ABI 而非插值需求，交集会让同领域材质的顶点布局分叉，领域契约校验会直接拒绝。**这条建议不要重新尝试**；现在的形态是从领域契约推导，且不与 varyings 取交集。

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
| `D2.shader-config-emit` | 校验配置头规范文本（ForwardLit 全开；⑥ 子用例钉死 `ShadowCaster` 只出 `HGL_COVERAGE_ONLY` + alpha 三件套，纹理通道/PCF 宏不得泄漏） |
| `PC.pipeline-config-emit` | `PipelineConfigEmitter` 输出 |
| `MI.module-include-declaration-contract` | Registry 加载期强制 `#include ⊆ @ulre uses` |
| `SD.structure-dump-golden-pilot` | 读 `golden/<slug>.txt` 比对（16 变体矩阵）；不匹配写 `<slug>.txt.actual` |
| `SD.emission-order-golden` | 读 `golden/<slug>.emission.txt` 比对**拼接顺序骨架**（同一张 16 变体矩阵）；不匹配写 `<slug>.emission.txt.actual` |

要点：

1. **负向探针断言 `parse_error.key == "domain"`**。任何新增的领域校验失败都必须记成 `"domain"`。
2. **模块总数收敛为单个常量 `kShaderCodeModuleCount = 82`**（`struct GateResult` 之后）。每次增删 `.glsl` 都要改这一处。**不要**改成从 `ShaderCodeModuleRegistry` 反推 —— 门禁的期望值必须独立持有，否则断言自证。
3. `require_slot_module(name, slot_name)` 断言块约 `:6015`。
4. **golden 没有环境变量开关**。更新方式：`Copy-Item *.actual *.txt` 然后删除 `.actual`。首跑 golden 缺失会自动生成并 PASS。
5. **快照 case 必须自己装载前置条件**：`EnsureSamplerPresetsLoaded()` 在 `SamplerPresetLibrary` 为空时装载 `ShaderLibrary/sampler.toml`。不装的话采样器 `#define` 会被静默省略，只跑 `pipeline` 组与跑 `all` 组得到的 golden 不一致（详见 §4.7）。
6. **emission 快照的粒度是骨架行**（预处理指令 / 顶层 `layout` / 顶层函数签名），行内空白折叠、缩进与函数体排除。只要重构不改 token 顺序，就不该需要刷新 emission golden —— 需要刷新就说明顺序真的变了，这正是它要抓的东西。**但「顺序变了但宏集合没变」是合法的**：阶段六把 `ComposeShadow` 的 define 块从注入前移到注入后（与 forward 模板一致），golden 位移、字节数不变，此时应接受位移 + 升 `SHADERGEN_CODEGEN_GENERATION`，而不是为了 golden 不变去改拼接顺序。
7. 门禁注册表在文件尾部（约 `:8515` 起）：`run_glsl / run_interface / run_descriptor / run_cache / run_materialization / run_pipeline / run_module_invariants`。

---

## 9. 环境与工具坑（务必先看，能省半小时）

1. **门禁/回归 exe 必须在 `E:\ULRE` 仓库根运行**（`GLSLCompiler.dll` 在根目录）。
2. 材质 TOML 的 `name` / `provider_policy` / `[fragment]` / `[vertex] requirements` 四项对**领域材质**已可缺省（走契约兜底）；对**非领域材质（Custom）**仍必须写全，缺任何一项都会带**键级诊断**报错（`parse_error.key` = 缺的那个键）。历史上这里是静默 `return false`、无任何诊断 —— 已于阶段四修掉。
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
14. **快照变体里改渲染状态必须写在 `ResolveSceneRenderTemplateRequest()` 之前**：`MaterialCoverageContract.cpp:112` 的 `requires_alpha_evaluation` 是从 `ResolvedMaterialRenderState` 推出来的，契约在模板请求解析那一刻就读走了。写晚了等于没写，golden 会安静地覆盖错分支（阶段六补 masked 覆盖时踩过）。
15. `ApplyFragmentTemplateSlot` 的吞行规则：替换 `{{key}}` 后若紧跟一个 `\n` 就删掉它。所以**空值独占一行时整行消失** —— 写 `.tmpl` 时靠这个实现可选段。

---

## 10. 已知技术债 / 待清理

### 10.1 文档过期（✅ 阶段四已全部清掉）

`doc/ENVIRONMENT_SYSTEM.md:244`、`doc/material-lit-recipe-to-shadergen-dataflow.md:92`、`doc/ShaderGen_ComposableTemplate_TechnicalDesign.md:369` 三处「宏已被删除但文档还在提」的条目已修正。全仓 `.material.toml` 的引用也已统一改为 `.mi.toml`，仅 `doc/backlog.md` 与 `doc/future/` 保留为历史记录（**刻意不改**）。

### 10.2 结构性遗留

- ~~mesh 侧骨架外化成 `.tmpl`~~ → **复查后判定：已经做到该做的程度，剩余部分不该外置**（fragment 侧已于阶段六收口，见 §4.8）。
  原先按「内嵌 `\n` 数量」排序（`MeshVaryingGen` 47 段 > `VertexAdapter` 34 > `HeaderGen` 22）挑目标，实际读文件后发现**这个度量选错了对象**——段数多的原因是 GLSL 片段被放进了**数据表**，而不是 `ms +=` 拼接代码：

  | 文件 | 实际形态 | 为何不该外置 |
  |---|---|---|
  | `MeshShaderVaryingGen.h` | `MeshVaryingWriteRule { semantic, color_source, const char *glsl }` 规则表 | 该表同时是「发射什么」与「模式**能**写什么」（`IsMeshVaryingWritable` 硬门）的**唯一真源**，正是为消灭「描述符清单 + 发射器 if 链」双表漂移而建（`LineQuad` 漏写 `WorldPosition` 就是漂移产物）。搬到 `.tmpl` 会把语义↔GLSL 的邻接关系拆开，反而重造一个需要第二套查找的真源 |
  | `MeshShaderVertexAdapter.h` / `MeshShaderHeaderGen.h` 的 `pc_root`、`MeshDrawParams` 块 | 遍历 `kMeshDrawParamsField*` / `kRootAddressesField*` 的 X 列表生成 | 字段名/类型是**与 CPU struct 同源**的 X 宏单一真源（`ShaderBufferSources.h`）。写进 `.tmpl` 就是把字段清单抄第二份，改字段只改一处的前提直接破掉 |
  | 三个 `MeshShaderMode*.h` | 无条件静态骨架 | **阶段三已外置完毕**（`line_quad.glsl.tmpl` 等） |

  剩余真正无条件的静态行每个文件只有 3–6 行短 `+=`（如 `#version 460`、6 条 `#extension`、`#define gl_InstanceIndex`），外置只增加一次模板查找、不减少任何 C++ 分支，是净可读性损失。
  `MeshTemplateEmitter` 的多段拼接已收敛为「按模式选表 + 拼接」，其残余分支（`mode == VertexPassthrough || mode == LineQuad`，E2）属模式语义而非字符串拼接。
  结论：**mesh 侧字符串拼接债已清空**；`FragmentTemplateComposer`（角色化 `ResolvedInclude`）与 `GenericMaterialBuilder` 的多段 `+=` 合并成单次 emit 仍是可选优化（纯风格，低价值），留作独立提交。
- **STL 残留**：`FragmentTemplateComposer.cpp` 与 `MeshShaderTemplate.cpp` / `MeshTemplateEmitter.h` 仍 `#include <fstream>` / `<map>` / `<string>` 并用 `std::string` / `std::vector`，违反本仓「禁用 STL」规范（全量口径：`src/ShaderGen` + `src/ShaderMaterialData` + `inc/hgl/mtl` 共 **40 个文件 / 247 处**）。
  前提条件已满足：mesh 侧三种发射模式都有 emission golden 兜底（§5.3），替换 `std::string` 累加器若改变任何拼接顺序会立刻变红。
  **但它不是「纯机械替换」** —— 实测最小目标 `MeshShaderTemplate.cpp` 就卡在第一个依赖上：
  | 依赖 | 实情 | 后果 |
  |---|---|---|
  | `GetShaderLibraryPath()`（`inc/hgl/mtl/ShaderLibraryPath.h`） | inline 公共头，**返回 `std::string`**，内部还用 `std::vector<std::string>` + `_wgetenv` | 改返回类型要动 **11 个文件 / 26 个调用点**；不改则累加器只能在 `AnsiString` / `std::string` 之间来回转换，`<string>` 仍留着 |
  | `std::sort`（`MeshTemplateEmitter.h:341`） | HGL 无 `hgl::sort`（`CMCoreType` / `CMCore` 里只有 `ValueArray` 等容器，且它们内部自己就调 `std::sort`） | 需手写稳定插入排序或引入新机制 |
  | `std::vector<PendingBlock>` | `hgl::ValueArray` 只有 `Add/Get/Set/GetCount/GetData`，**没有 `begin/end`/`operator[]`** | 遍历与排序都要改写 |

  结论：**局部改会把仓里搞得更不一致**（一半 HGL 一半 STL，边界处来回转换），
  所以要么整体做（含 `ShaderLibraryPath.h` 的公共签名迁移），要么不做。
  **刻意不做**，留作独立、可评审的迁移；`inc/hgl/mtl/` 的公共头 STL 清理另需专门方案（`MaterialRecipe.h` 一个文件就 17 处）。
- 4 个领域（`Foliage` / `Terrain` / `HumanSkin` / `UI2D` 部分）reference 为 `nullptr`，只有契约骨架、无专属实现。属**刻意保留的扩展位**，不是 bug。
- 主着色器仍保留 alpha 三件套宏（`HGL_ALPHA_TEST` / `HGL_ALPHA_DITHER` / `HGL_ALPHA_OUTPUT`）—— **这是刻意设计**，因为它们必须与 `discard` / 输出同函数体，无法外移。
- ~~`DirectLightProvider`（legacy role）仍在枚举里~~ → **阶段五已删除**（枚举 + 名称映射 switch）。
- ~~模块总数 `82` 硬编码在两处~~ → **阶段五已收敛**为 `kShaderCodeModuleCount`（门禁内单一常量）。

---

## 11. 每个子阶段的完成定义（DoD）

- [x] `ShaderResourceSchemaRegressionGate all` → 56 PASS / exit=0（若新增 case 则数量随之增加并同步文档）
- [x] `SceneRenderTemplateResolverRegression` / `ShaderDocumentRegression` / `ShaderDocumentProductionRegression` → 全 exit=0
- [x] `TestRenderStrategyTable` / `TestMaterialVariantTable` → 全 exit=0（保底链行为断言，改动材质定义/回退规则时必跑）
- [x] 实跑 `PBRSpheres` + `TextDraw`：0 errors、模块数正确、无 GLSL 编译错误
- [x] 实跑 `LineRender`（`PrimitiveType::Lines` → 真正走 `LineQuad` mesh 发射模式）：0 validation error、SPV 缓存全命中、持续出帧。**这条不能用 golden 代替**——`{{endpoint_read}}` 的两个函数体文本不在快照粒度内，且夹具是 `defer_finalize`（不编译 SPV），只有实跑能证明两条 `position_format` 分支都产出合法 GLSL
- [x] `git status` 只剩既存子模块脏项
- [x] **拼接顺序有 golden 覆盖**：`SD.emission-order-golden` **16** 变体全绿，覆盖 `ForwardLit` / `ForwardUnlit` / `ShadowCaster` / `Sky` 四条 emission 通路，`ShadowCaster` 的 **opaque 与 masked 两条分支都有覆盖**（阶段五建网、阶段六补 masked），三种 mesh 发射模式（`VertexPassthrough` / `CharQuad` / `LineQuad`）全部有覆盖（阶段六收尾补 `LineQuad`，见 §5.3）
- [x] 更新 `doc/ultra-thin-fixed-shader-architecture.md` 的「阶段进展」条目
- [x] 独立提交，提交信息带 `feat(ultra-thin):` / `docs(ultra-thin):` 前缀
