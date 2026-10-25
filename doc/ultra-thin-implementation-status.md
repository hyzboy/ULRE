# 超薄固定着色器架构 —— 实施现状与交接文档

> 📌 **最新架构与演进终态导流（2026-10）**：
> 超薄固定着色器前六阶段已全部完成落地。后续**彻底废除 ShaderGen** 的终极方案与执行设计，
> 请参阅最新权威文档：**`doc/shadergen-elimination-domain-fixed-architecture.md`**（基于 MaterialDomain 的固定着色器方案）。
> 本文保留作为阶段一至阶段六的落地历史台账与环境坑参考。

> **用途**：把「Domain 即 Master Material」重构的**全部计划现状**固化下来，供新会话零上下文接续。
> **配套文档**：`doc/ultra-thin-fixed-shader-architecture.md`（架构设计正文，8 节，权威设计）。
> 本文只记录**进度、提交、验证结果、下一步动作、踩过的坑**，不重复设计论证。
> **最后更新**：阶段六收尾 + SPV 反射方向重审（分支 `feature/ultra-thin-stage5-shadergen-emission-cleanup`）。**四阶段全部完成，`.material.toml` 已废除；拼接顺序已有 golden 安全网（16 变体，含 3 种 mesh 发射模式）；四条 emission 通路全部走 `.tmpl`；mesh 侧拼接债经复查确认已清空，STL 残留清理判定为需整体迁移、刻意不做。2026-10-25 已在 ShaderGen 编译桥接中暴露 SPIR-V 反射并加入端到端门禁，并已接入**第一个真实消费者**：compute 模块烘焙的 push-constant 范围由反射结果决定（见 §10.3）；per-material descriptor-layout 假设经地面核查**没有靶子**，`ShaderDocument` 与 `GenericMaterialBuilder` 两个候选经面积实测**判定不做**。**

---

## 0. 新会话开场怎么用

1. 先读本文 **§1 目标** 和 **§3 路线图**，确认自己在哪一阶段。
2. **四阶段已全部完成**（一 / 二 / 三 3.1–3.3 / 四，见 §4.1–§4.6），阶段五（拼接清理前置安全网 + 小债，见 §4.7）与阶段六（ShadowCaster 骨架外置，见 §4.8）也已完成。阶段四已把资产降维为 `.mi.toml` 并废除 `.material.toml`，方案记录见 §6.5。
3. 读 **§9 环境与坑**，避免重复踩已记录的坑（尤其是「门禁探针必须报 `domain` key」「模块数常量 `kShaderCodeModuleCount`」「golden 无环境变量开关」「快照 case 必须自己装载 sampler 预设」「`alpha_test` 必须写在模板请求解析之前」）。
4. 动手前跑三条自检命令（见 §5.1），确认基线是绿的。
5. **剩余工作见 §10.4**（唯一入口：未完成 / 判定不做 / 守卫项三分）—— mesh 侧骨架经复查判定「已做到该做的程度」（剩余段数来自刻意的单一真源数据表），STL 残留清理经实测判定**必须整体迁移、刻意不做**。
   **另见 §10.3**：发行形态是否链接 ShaderGen 与开发期减化是两个独立问题；本轮只否决「反射替代 per-material descriptor layout」这一具体假设，不据此否决 SPV 反射用于精简其他编译期派生状态。反射接缝已通并已有一个真实消费者；其余候选（payload-row / runtime-row 标志、stage interface 对照、`ShaderDocument`、`GenericMaterialBuilder` 合并）已逐项审计，结论与理由见 §10.3。

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
| 17 | 同上 | `53a3629ef` | 补齐 `LineQuad` mesh 发射模式的快照覆盖（矩阵 14 → 16，纯测试夹具） |
| 18 | 同上 | `eeb325d36` | STL 残留改判：从「纯机械替换」改为需要整体迁移（纯文档） |
| 19 | 同上 | `0a1a07ace` | mesh 侧拼接债复查判定已清空，STL 清理确认为整体迁移（纯文档） |
| 20 | 同上 | `872ee3fd4` | 记录 SPV 反射核查结论与 `verify-release-form.ps1 -SkipBuild` 假 FAIL 陷阱（纯文档） |
| 21 | 同上 ← **当前** | `707178f17` | **反射落地**：`ShaderReflectionData` + `ReflectSPVData`/`GetSPVPushConstantEnd`，compute push-constant 范围改由反射决定；新增 `SPV.buffer-reference-reflection` 证明 BDA 不可由反射推断 |

当前工作区干净，仅有既存子模块脏项 `? CMScript` / `? res` / `? src/Tools/GLTFConvert`（**不是**本次改动，勿提交）。

累计 diff（`6ef579059..707178f17`，即上表第 3 行 → 第 21 行）：123 files, +6812 / −1102。

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
.\build\out\Windows_64_Debug\ShaderResourceSchemaRegressionGate.exe all      # 期望 58 PASS / exit=0
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

- `ShaderResourceSchemaRegressionGate all`：**58 PASS / 0 FAIL，exit=0**（阶段六后原有 56 条；2026-10-25 新增 `SPV.reflection-api` 与 `SPV.buffer-reference-reflection`，均在 `reflection` 分组）
- 五个回归 exe（`SceneRenderTemplateResolverRegression` / `ShaderDocumentRegression` / `ShaderDocumentProductionRegression` / `TestRenderStrategyTable` / `TestMaterialVariantTable`）：**全 exit=0**
- `TestRenderStrategyTable` / `TestMaterialVariantTable`：**全 exit=0**（这两个覆盖保底链行为断言 —— 未知定义 ID ⇒ 换 `builtin/error_checkerboard_3d`、标记行不可得 ⇒ 降级 `builtin/checkerboard_3d`，是「内嵌定义可用」的最强证据）
- 实跑 `PBRSpheres` / `TextDraw`：`Loaded 9 material TOML definitions (0 errors) + 5 builtin definitions`、`Loaded 82 code modules`、`loaded 34 entries from program_manifest.pmf`、无 GLSL 编译错误
- **发行形态跨进程验收**（`scripts/ci/verify-release-form.ps1 -SkipConfigure`）：**5/5 PASS**，含 `[ OK ] prod tree has no ULRE.ShaderGen artifacts` 与 `[PASS] release form verified: baked cache resolved with zero generator symbols`。这一条是本仓唯一能证明「ShaderGen 真的不在发行包里」的证据 —— 它靠**链接失败**断言（探针只链 `ULRE::Runtime`），不是靠没有符号被优化掉。详见 `doc/shadergen-release-form-verification.md`
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
16. **`verify-release-form.ps1 -SkipBuild` 会给出假 FAIL**：它复用旧探针二进制，而运行时库 ABI 已随超薄六阶段变化 ⇒ 探针静默 `abort()`（MSVC 退出码 **3**、零 stdout/stderr）。看到 exit=3 且无输出就是 ABI 陈旧，**去掉 `-SkipBuild` 重建即 PASS**，不要去查清单或夹具。
    `-SkipConfigure` 是安全的（只重读 CMakeCache），`-SkipBake` 在有现成夹具时也是安全的（夹具本身会被探针校验）。

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
- **STL 残留**：`FragmentTemplateComposer.cpp` 与 `MeshShaderTemplate.cpp` / `MeshTemplateEmitter.h` 仍 `#include <fstream>` / `<map>` / `<string>` 并用 `std::string` / `std::vector`，违反本仓「禁用 STL」规范。
  **2026-10-25 复测口径**（`src/ShaderGen` + `src/ShaderMaterialData` + `inc/hgl/mtl`，共 116 个 `.cpp`/`.h`）：
  **35 个文件仍 include 禁列头，`std::` 出现 436 次**。分目录：`src/ShaderGen` 29 文件 / 283 处、
  `src/ShaderMaterialData` 7 文件 / 62 处、`inc/hgl/mtl` 19 文件 / 91 处。热点 Top6：
  `ShaderCodeModuleFile.cpp` 50、`FragmentTemplateComposer.cpp` 39、`MaterialShaderEmitter.cpp` 38、
  `ShaderStructureDump.cpp` 25、`MeshShaderTemplate.cpp` 24、`MaterialDefinitionFile.cpp` 31（`ShaderMaterialData` 侧）；
  公共头侧 `ShaderGenPhysicalDeviceProfileJson.h` 23、`MaterialRecipe.h` 20、`ShaderLibraryPath.h` 11。
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

### 10.3 SPV 反射与 ShaderGen 减化：descriptor-layout 假设已排除，其他候选继续审计

**背景**：超薄架构的原始动机之一是「大幅简化甚至删除 Shader 生成器」。建议方案是：用 `GLSLCompiler.dll` 的 `ParseSPV` 反射出资源绑定，替代 ShaderGen 的「资源预推断」，从而把 ShaderGen 压成薄装配器。

**范围限定**：以下核查只说明 SPV 反射不能替代「按材质生成 descriptor-set-layout」—— 仓库当前没有这类生成逻辑。这**不等于**「SPV 反射与 ShaderGen 减化无关」；是否能替代其他编译期派生状态，仍需逐项按真实消费者判断。

| 待证命题 | 核查方式 | 结果 |
|---|---|---|
| ShaderGen 仍在预推断描述符布局 | 全搜 `src/ShaderGen` + `inc/hgl/mtl` 的 `VkDescriptorSetLayoutCreateInfo` / `DescriptorSetLayoutBinding` / `CreateDescriptorSetLayout` | **0 命中** |
| 运行时需要 per-material 布局 | `src/Vulkan/pipeline/VKPipelineLayoutData.cpp` `CreateGlobalPipelineLayout()` | 只吃 `bindless_layout` 一个参数，函数体硬编码「Bindless(0) + 单条 `sizeof(RootAddresses)` 推送区」，**签名里没有任何材质输入** |
| `resource_layout_hash` 被运行时消费成绑定 | 全搜该字段 | 仅 `ShaderProgramManifest.cpp` 用于 manifest 行**自比对**；`ShaderProgramManager.cpp` 只校验 `baked_push_constant_size` |
| 「资源清单」是描述符表 | `inc/hgl/mtl/ShaderCodeResourceManifest.h` | 是 `code_module_names[64]` + `texture_references` + `sampler_references` + `stable_hash`，**不是布局** |

**关键因果**：描述符契约层已于 **2026-10-11（S1）删除**（`MaterialResourceManifestBuilder.h` 的头注释就是墓碑）。所以 `GenericMaterialBuilder.cpp` 里根本没有「预推断→布局」这一段可供反射替代 —— 反射只能**看见编译后的状态**，而 sampler 名 → 索引映射必须在编译**前**以 `#define <Name>Sampler N` 注入（`shader_type`/`includes` 依赖它）。`ParseSPV` 输出 set/binding/type，出不来 preset 名。

发行构建是否排除 ShaderGen 是独立的打包/链接边界，与本节讨论的开发期代码减化**不作为同一目标或论据**；发行验收细节单列在 `doc/shadergen-release-form-verification.md`。

**本轮进展（2026-10-25）**：`src/ShaderGen/compile/GLSLCompiler.cpp` 已通过现有插件接口调用 `ParseSPV` + `CreateShaderFlat`，用 HGL `ValueArray<uint8>` 持有 flat 数据，并以 `ShaderFlatLoad` 产生零拷贝 view。随后新增 `ShaderReflectionData`，把 stage I/O、descriptor、push-constant 与 subpass-input 事实复制到有类型的 HGL 容器，支持 descriptor set/binding 查找与 push-constant end 计算；门禁在清空原 flat 缓冲区后继续断言提取数据，验证其独立生命周期。

**语义边界**：flat 反射格式当前给出 stage I/O、descriptor resources、push constants 与 subpass inputs；它没有记录 buffer-reference 类型/调用点的实际访问语义。`MaterialData` / `MTL_ROW` 在本引擎经 `RootAddresses` + `buffer_reference` 寻址，不是 descriptor。因此本步骤不从 SPV 反射猜测 `requires_recipe_payload_row` 或 `requires_recipe_runtime_rows`，也不移除原模块元数据判定。需先证明另有可靠的 SPIR-V 信号与运行时消费者等价，再讨论替换。

**第三步的安全落点**：`ShaderProgramManager::CreateComputeShaderModule` 编译真实 compute SPV 后，现调用 `GetSPVPushConstantEnd()` 取得反射范围；反射值必须与调用方的 CPU push-constant 大小相等，否则拒绝烘焙。compute manifest 记录反射值，而不是把调用参数不经核验地复制进去。CPU 大小仍保留为 host ABI 断言与 `VkPipelineLayout` 输入，避免改变 API/运行时布局行为。`PM.compute-release-cache` 现用真实 `entity_gpu_cull.glsl` 证实反射范围为 112B 并验证该值可烘焙/只读取回。

门禁 `SPV.reflection-api` 与 `SPV.buffer-reference-reflection` / CTest 分组 `reflection`：前者端到端验证 stage input/output、UBO set/binding、push constant 和提取结果独立生命周期；后者编译实际使用 `buffer_reference` 的 probe，证实当前 flat 反射只返回 push-constant 地址、不把该引用报告为 descriptor。当前完成的是反射事实提取及 compute manifest 值的反射校验；未据此猜测 `MTL_ROW`/`MTL_TEX` 语义，也未移除材质生成逻辑。

**本轮进展（2026-10-25，SPV 反射门 + Position 分量数硬门）**：

`SPV.generated-interface`（`reflection` 组）把结构快照矩阵的 **16 个变体全部真编译成 SPIR-V** 并反射，按 Vulkan 链接规则断言四件事：① 两 stage 都能编译；② 片元每个 stage input 在 mesh stage output 里有同 location、同 basetype/vec_size/col_count 的对应项；③ `ForwardColor` 用途必须写 ≥1 个颜色附件，深度/阴影用途必须一个都不写；④ 两 stage 的 `GetPushConstantEnd()` 都 == `RootAddresses` 的 88 字节。断言全部取自二进制，不看生成器怎么描述自己。

**门上线第一天就抓到一类真实缺陷**（这正是反射该接管的位置）：

- Stage 1 的 `s1_position_*` 由 **`VkFormat`** 选（`VertexABIBuilder.cpp` 的 `switch(out_position_format)` 与 `MeshModeDescriptor.h:142 ResolveStandardStage1Input` 两份重复逻辑）；
- Stage 2 的 `s2_*` 由 **`position_mapping`** 选（`VertexNodeConfigResolver.h:14`），**从不看 `VkFormat`**；
- 两者对 Position 分量数的选择是两条互不相见的路径。不一致时生成的是 `vec4(vec3, 0.0, 1.0)` 这类五分量构造，**glslang 直接拒绝**，且报错指向 `s2` 模块 —— 定位成本极高。快照矩阵里 3 个变体就是这种状态（`unlit-texture-forward` / `checkerboard-2d-forward` / `vertex-color-lines-quad-2d`）。
- `.glsl` 模块上的 `// @ulre require GeometryAttribute Position Float N N` 注记**一直在被解析和校验格式**（`ShaderCodeModuleFile.cpp:196/567`、`ShaderCodeModuleMetadata.cpp:33`），但**从未与真实顶点格式比对过**（唯一消费者是 `Resource MaterialData` 检查，见 `MaterialShaderEmitter.cpp:137-170`）。

修法是把一致性做成**发射期硬门**，而不是再加一条事后校验：`GetPositionMappingComponentCount(mode)`（`Passthrough3D`→3，其余 lift→2）+ `VertexNodeConfigResolver::CheckPositionComponentCount(...)`，材质路径（`VertexABIBuilder`）与 mesh 路径（`MeshTemplateEmitter`）共用同一道门 → **不可能再产出分量数矛盾的解码模块**。

**规模 80% 削减的正靶子（分层模型，后续轮次按此判断，勿再回到「ShaderGen 不用动」）**：

| 层 | 内容 | 反射能否替代 | 归属 |
|---|---|---|---|
| **A. 编译前必须已知的输入** | `#define NameSampler N` / 纹理引用表 / `s1_position_*` 选择 / `s2_*` 选择 / `layout(location=N) out` 附件编号 / `RootAddresses` 字段布局 | **不能**（反射只能看见已写进去的东西） | 保留，最终沉进静态 `.tmpl` |
| **B. 编译后的验证（现在靠 C++ 人肉模拟）** | stage 间 varying 同 location 同类型 / push constant 末端 == CPU 侧 / 用途与附件数 / 手写契约断言 | **能，且应当** | 改成反射断言后删掉 C++ 侧那份 |
| **C. 生成物里的纯自述结构** | `ShaderDocument` 的 AST 校验 | 部分能 | 见 §10.4 的判定 |

**别再重复的错误结论**：曾把上述核查总结为「**建议：不再为减行数而动 `src/ShaderGen`**」。那句话把主目标从「削减 80%」偷换成「别碰它」，已作废 —— 「发行版不含 ShaderGen」（`ULRE_ENABLE_SHADERGEN=OFF`）是打包边界，与开发期维护成本是两个独立目标。

**面积实测**（真实代码行，剔除空行与纯注释行）：`src/ShaderGen` = **42 文件 / 9762 总行 / 7765 代码行 / 909 注释行**。
Top5：`GenericMaterialBuilder.cpp` 929、`MaterialShaderEmitter.cpp` 675、`ShaderCodeModuleFile.cpp` 573、`FragmentTemplateComposer.cpp` 499、`ShaderCodeModuleRegistry.cpp` 418。
`GenericMaterialBuilder.cpp` 已是 20 个全可达具名函数（最大 `ResolvePurposeAndCoverage` 141 行），**无重复逻辑、无死码**。`CanonicalContractWriter.h` 是唯一自引用单元。

**候选审计结果**：
- **payload-row / runtime-row 标志**（`MaterialShaderCompiler` Step 6）—— **不能由反射替代**：`ParseSPV` 只能看见编译后状态，而这两个标志决定**是否发射** `MTL_ROW`/`MTL_TEX` 声明与 BDA 布局，属于编译**前**输入，构成循环依赖。语义边界段已给出实测证据（buffer-reference probe 零 descriptor）。
- **mesh→fragment stage interface 对照** —— 反射确实给出每 stage 的 in/out 列表，但要判定「mesh 输出被 fragment 消费」还需要 location/type 匹配规则，而当前 emitter 在拼接期生成的就是同一张表；换成「先编译、再反射、再比对」只会把一次性拼接检查变成两遍编译，净增复杂度。**改为不做**。
- **`ShaderDocument`**（173 代码行）—— 面积 = `src/ShaderGen` 全量 **2.6%**，对产品体积与运行时**零影响**；删除代价是丢掉「Version 必须首块 / 单一 MainBody / 拒空块」三条结构性校验（16 变体 golden 只管拼接顺序，不管这些）。`SerializeFragment` / `GetSerializedHash` 仍被回归与门禁使用。**判定风险大于收益，刻意不做**。
- **合并 `GenericMaterialBuilder` 为「薄装配器」**（929 代码行）—— 它已是 20 个全可达具名函数（最大 `ResolvePurposeAndCoverage` 141 行），无重复逻辑、无死码；其职责是装配（purpose/coverage、顶点 ABI、mesh 发射模式、模块图、include 闭包哈希、program 链接），不是字符串拼接 —— 拼接已在阶段五/六收敛到 `.tmpl` + 单一真源数据表。**没有可合并的对象**。
- **新增 flat 反射字段以承载 BDA 指针类型** —— 属于「为能力而能力」：唯一潜在消费者（payload-row 标志）是编译前输入，反射结果无法反哺，故**不扩展** flat 格式，避免为没有消费者的事实付出格式版本成本。

**已证伪的具体方向**：
- 用反射推导 per-material descriptor-set-layout —— 当前 Vulkan pipeline layout 是全局固定布局，没有 per-material 布局输出；
- 把 `MaterialResourceManifestBuilder` / 资源清单当作「伪预推断」删除 —— 它是真实的**编译前输入**；
- 从 varyings 反推 `[vertex] requirements`；删除 `[transform]` 解析分支；**局部** STL 迁移；把 mesh 侧数据表外置成 `.tmpl`。

---

### 10.4 未完成计划清单（2026-10-25 整理，唯一入口）

**一句话**：超薄六阶段 + 反射落地已全部完成；**「削减 80%」的正靶子已按 §10.3 的 A/B/C 分层重排为 P1→P4**（B 层换成反射断言 → C 层删自述校验 → A 层外置成 `.tmpl` → 模块 registry 交给 glslang `#include`）。原来那张「只剩 1 项 STL 迁移 + 1 项审计」的清单把主目标漏掉了，已改写。**P1 的 B 层删除已完成并提交（-94 代码行）**，P2 判定不做，P3 判定收益过低，P4 已完成立项评估（§10.6，结论：不做整体重写；从中剥离的两项精确删除中 `conflicts` 全链已落地 −155 代码行，`uses` 推边待做），剩余空间见 §10.5。

#### A. 未完成（Open）

| # | 项 | 实测范围 | 为什么还开着 | 开工前置 | 风险 |
|---|---|---|---|---|---|
| P1 | **把 B 层（编译后人肉验证）换成反射断言** | ✅ 已在 `SPV.generated-interface` 完成 mesh↔fragment 接口、用途↔附件数、push-constant 末端三组；✅ 并已删除 `ShaderCodeModuleMetadata.cpp` 的依赖满足性推断（107 行 / 代码行 −95） | **已完成**。删除依据与保留项见 §10.5「本轮实际兑现」 | — | 已完成 |
| P2 | **C 层：`ShaderDocument` 的自述校验** | `Serialize` 4 条诊断（Version 首块 / 单一 MainBody / 拒空块 / 块序单调） | 见下方 B 表判定：**保留校验，不删** | — | — |
| P3 | **A 层外置**：把仍在 C++ 里的静态 GLSL 文本沉进 `.tmpl` | 实测后**大幅缩水**，见下方 §10.5 | 静态骨架（mesh 3 个 + fragment 4 个 `.tmpl`）**已经外置完毕**；剩下的 `GenericMaterialBuilder.cpp` 929 代码行里只有 **3** 条 `+= "` 语句 | emission golden 16 变体 + `SPV.generated-interface` | 低（但收益也低） |
| P4 | **用 glslang `#include` 取代依赖预解析** | ✅ 已完成立项评估，见 **§10.6**：只剩 ~45 行（`CollectShaderCodeModuleIncludePaths`），且从中剥离出 `uses` 改推边（~56 代码行）+ 删 `conflicts` 全链（**已落地 −155 代码行**）两项 | 评估已证明「整张注记表换成 include 树」可行但**不划算**（1650 行重写换 ~56 行），因此降级为两项精确删除而非重写 | `uses` 推边前先修那 2 个无 `#include` 的 `surface_interface` 反例 | 低 |
| P5 | **STL 整体迁移** | 3 目录 116 文件；35 文件仍 include 禁列头；`std::` 436 处（`src/ShaderGen` 29/283、`src/ShaderMaterialData` 7/62、`inc/hgl/mtl` 19/91） | 局部替换会造出「一半 HGL 一半 STL」，边界反复转换更糟 | ① HGL 侧补 `ValueArray` 迭代能力与 `hgl::sort`（当前**无等价物**：`ValueArray` 无 `begin/end`/`operator[]`，内部自己调 `std::sort`）② `GetShaderLibraryPath()` 返回 `std::string`，改签名要动 **11 文件 / 26 调用点** | **高**：跨 CMCore 公共库 + 公共头 |
| P6 | **manifest 运行时消费者审计**（反射方向收尾） | `ShaderProgramManifest` / `ShaderCodeResourceManifest` 的全部运行时读取点 | 唯一没逐行取证的候选：是否还有「编译后事实可被反射替代」的派生值 | 无 | **低**：纯只读审计，产出「结项」证据 |
| P7 | **拼接收敛为单次 emit**（可选、纯风格） | `FragmentTemplateComposer` / `GenericMaterialBuilder` 的多段 `+=` | 无外部行为差异，低价值 | emission golden 16 变体已在网（做则**必须零位移**） | 低 |

#### B. 判定不做（设计决策，附依据）

| 项 | 判定依据 |
|---|---|
| 删除 `ShaderDocument`（173 代码行） | 面积仅占 `src/ShaderGen` 2.6%，对体积/运行时零影响。**逐条复核后新增两条依据**：① `ShaderDocument::GetBlockOrder` 是**发射期**排序真源（`MeshTemplateEmitter.h:189/204`、`MaterialShaderEmitter.cpp:762`、`DocumentFragmentBuilder.cpp:47` 都拿它排序），删 AST 等于重写三条 emission 通路；② 唯一可删的是 4 条校验，而其中「拒空块」glslang **抓不到**（空块只会在别处炸成行号无关的报错），且报错文本里带着 `source.stage/module/path` —— 删它换来 149 行、赔上定位能力，净亏 |
| 合并 `GenericMaterialBuilder` 为薄装配器（929 行） | 已是 20 个全可达具名函数，无重复逻辑、无死码；职责是装配而非拼接 |
| 用反射推断 `requires_recipe_payload_row` / `requires_recipe_runtime_rows` | 标志决定**是否发射** `MTL_ROW`/`MTL_TEX`，属编译**前**输入 → 循环依赖；`SPV.buffer-reference-reflection` 已实测 BDA 零 descriptor |
| 用反射做 mesh→fragment stage interface 对照 | 反射能给出 in/out 列表，但判定「被消费」仍需 location/type 匹配规则，等于把一次拼接检查换成两遍编译 |
| 扩展 flat 反射格式承载 BDA 指针类型 | 唯一潜在消费者（payload-row 标志）不存在 → 为无消费者的事实付版本成本 |
| 继续外置 mesh 侧骨架 | 剩余每文件仅 3–6 行静态行，外置只多一次模板查找，净可读性损失 |
| 局部 STL 替换 | 见 P1：依赖链第一条就断 |
| 4 个 `nullptr` 领域（`Foliage` / `Terrain` / `HumanSkin` / `UI2D` 部分） | 刻意保留的扩展位，只有契约骨架，**不是 bug** |

#### C. 守卫项（改动后必跑，不是待办）

- `ShaderResourceSchemaRegressionGate all` → **59 PASS / 0 FAIL / exit=0**（`cycle_first`/`cycle_second` 用例已改指向 `BuildShaderCodeResourceManifest` 的真实判据）
- CTest 门禁组 → **9 条**（含 `reflection` 分组）
- **3 个回归 exe**（`SceneRenderTemplateResolverRegression` / `ShaderDocumentRegression` / `ShaderDocumentProductionRegression`）→ 全 exit=0；触及保底链时另跑 `TestRenderStrategyTable` / `TestMaterialVariantTable`
- 触及 `ULRE::Runtime` 成员列表、manifest 后端、`ShaderArtifactStore` 磁盘布局或烘焙值时，必须重跑 `verify-release-form.ps1`（**不要**用 `-SkipBuild`，会给出假 FAIL）
- 任何 emitted GLSL 变化必须显式声明为 golden 位移并升 `SHADERGEN_CODEGEN_GENERATION`
- **编译失败必须能看到 glslang 原文**：`SPV.generated-interface` 内含一条自检，故意编译非法 GLSL，要求 `CompileFinalGLSLToSPVWithDiagnostic` 既返回 `nullptr` 又写出非空日志（见 §10.3）

---

### 10.5 「削减 80%」的实测天花板（2026-10-25，勿再凭印象估算）

**先看总量趋势**（口径：`src/ShaderGen` 下全部 `.cpp`/`.h`；总行数 / 代码行＝总行 − 空行 − 整行注释）：

| 提交 | 文件 | 总行 | 代码行 |
|---|---|---|---|
| `006d2b8e5`（超薄工程开始前） | 41 | 9617 | 7690 |
| `277ad2a7c`（超薄第一个提交） | 42 | 9729 | 7782 |
| `HEAD`（本次删依赖推断前） | 44 | 10234 | 8166 |
| 当前工作区（已删依赖推断） | 44 | 10128 | **8071** |

整个超薄工程**没有让 ShaderGen 变小**，反而 +548 代码行 —— 因为它做的是「单一真源 / 契约化 / 骨架外置」，同时新增了契约代码。**「大幅减化 80%」目前只兑现了 1.15%**（详见下方「本轮实际兑现」）。

**逐文件实测（代码行 = 总行 − 空行 − 整行注释）**：

| 文件 | 代码行 | `+= "` 发射行 | 字符串字面量 | 真实职责 |
|---|---|---|---|---|
| `GenericMaterialBuilder.cpp` | 929 | **3** | 81 | 组装：purpose/coverage、顶点 ABI、mesh 发射模式、模块图、include 闭包哈希、链接 |
| `MaterialShaderEmitter.cpp` | 675 | 56 | 113 | 唯一大量发射文本的文件（`#define`、layout、资源表） |
| `ShaderCodeModuleFile.cpp` | 573 | 0 | 71 | `// @ulre` 注记解析器（12 种指令）+ include 路径收集 |
| `FragmentTemplateComposer.cpp` | 499 | 13 | 80 | `.tmpl` 填槽 + 模块拼接 |
| `ShaderCodeModuleRegistry.cpp` | 418 | 0 | 15 | 模块清册、查找、依赖闭包 |
| `ShaderCodeModuleMetadata.cpp` | 370 → **276** | 0 | 4 | 模块表的**纯校验**（依赖满足性推断已删，见下） |

**结论一（推翻 P3 的前提）**：`GenericMaterialBuilder.cpp` 929 行里只有 3 条文本发射语句 —— 它是**逻辑**，不是文本。把它「外置成 `.tmpl`」无处可外置。留存的静态骨架其实早已外置完（`ShaderLibrary/mesh/*.glsl.tmpl` 3 个 + `fragment/*.glsl.tmpl` 4 个）。

**结论二（推翻 P4 的前提）**：glslang 的 `DirStackFileIncluder` 只替代 `#include` 的**解析**；`ShaderCodeModuleFile.cpp` 的 573 行是**注记语义解析**，glslang 不碰。可删的是 `CollectShaderCodeModuleIncludePaths` 一段，约 45 行。

**结论三（这才是真实可删清单）**：

| 目标 | 层 | 可删行数 | 替代物 | 现状 |
|---|---|---|---|---|
| `ShaderCodeModuleMetadata.cpp` 的依赖满足性推断（`FindModuleIndex` / `ValidateDependencyVisit` / `MissingDependency` / `DependencyCycle` / `MissingConflictTarget`） | B | 估 ~150，实删 107 行（代码行 −95） | 「拼出来的 GLSL 能编译」+ registry 的 incomplete 剔除环 + `BuildShaderCodeResourceManifest` 的 DFS | ✅ **已完成** |
| `CollectShaderCodeModuleIncludePaths` | A | ~45 / 573 | glslang `#include` | 待做 |
| `MaterialShaderEmitter.cpp` 静态文本 | A | ~100 / 675 | `.tmpl` | 待做（性价比低） |
| `ShaderDocument` 校验 | C | 0 | — | **已判定不做**（见 B 表） |
| **合计** | | **~256 ≈ 2.9%** | | |

> **与 §10.6 的对账**：本表按「P3/P4 旧任务」分解（include 路径收集 45 + 静态文本外置 100 + 已删 107），
> §10.6 按「注记消费者」分解（`conflicts` + `uses`）。两者**不可相加**：
> `CollectShaderCodeModuleIncludePaths` 在 `uses` 推边落地后会被复用而不是删除（§10.6-A 把它当唯一边来源），
> 所以 §10.6 的口径才是更保守、更可执行的那个。注意 §10.6-B（`conflicts`）**实测删掉了 −155 代码行**，
> 高于本表与评估文档原先的 ~73 估算，因此本表合计 ~256 也需相应上修。

**本轮实际兑现**：`src/ShaderGen` 代码行 **8166 → 7965（−201，−2.5%）**，总行 10234 → 10008。分两批：① 换反射断言 −95（8071）；② 删 `conflicts` 全链 −106（7965；全仓同批 13 文件合计 −155）。删除的判据（三个都是既有的、更早生效的权威判据，删掉的只是它们的 C++ 复述）：

1. **registry 的 incomplete 剔除环**（`ShaderCodeModuleRegistry.cpp`）已经用 `FindByName` 把「依赖名解析不到」的模块整个移出注册表 → 等价于 `MissingDependency`；
2. **`BuildShaderCodeResourceManifest` 的 DFS**（`ShaderCodeResourceManifest.cpp:110-215`）返回 `UnknownCodeModule` / `CodeModuleCycle`，且**阻断编译** → 等价于缺失依赖 + 依赖环；
3. **`SPV.generated-interface` 对 16 个变体做真实编译** → 兜住前两者漏掉的任何装配错误。

保留项（删不得）：`AmbiguousProviderPriority`（同优先级 provider 歧义，无第二判据；注：provider 实际由 `slot_role` + 模板 root 名选定，本检查保护的是**意图**而非现行机制，见 §10.6）。
`AreShaderCodeModulesConflicting` 曾因「`ShaderCodeResourceManifest.cpp:197` 用它报 `ResourceConflict`」而被判为删不得，
**该判断已作废**：`ResourceConflict` 在 `AddTextureReference`/`AddSamplerReference` 里只在**名称为空**或**容量溢出**时触发，
同名为**合并**，与模块冲突无关；模块冲突检查已连同 `ModuleConflict` 错误路径在 §10.6-B 中删除。
回归门的 `cycle_first`/`cycle_second` 用例已改指向真实判据：断言 `BuildShaderCodeResourceManifest` 对环返回 `CodeModuleCycle`、对缺失依赖返回 `UnknownCodeModule`（用例数不变，仍 59 PASS）。

**天花板结论**：按当前架构，**反射能合法替代的 B 层只有那 94 行（已兑现）**；其余 ~8000 行是「编译前必须已知的输入」与「组装逻辑」，反射在定义上无法替代（它只能看见已经写进去的东西）。因此 **80% 削减在当前架构下不可达**；换命题（「把注记驱动的模块表整体换成静态 include 树」）已单独立项评估，结论见 §10.6 —— **不立项**。
从中剥离出的两项精确删除，`conflicts` 全链（实测 −155 代码行）**已落地**，`uses` 推边（~56 代码行）待做。

---

### 10.6 「注记表 → 静态 include 树」立项评估（2026-10-25）

全文见 [`doc/annotation-vs-include-tree-assessment.md`](annotation-vs-include-tree-assessment.md)。此处只放可执行结论。

**核心事实**：`// @ulre` 不是一张表，是 12 条互不相关的指令。逐条倒查消费者后（`ShaderLibrary` 89 文件 / 82 带注记）：

| 判定 | 指令 | 依据 |
|---|---|---|
| **真源（删不得）** | `name`、`slot`（34）、`require`（32）、`provides_capability`（17）、`requires_capability`（2）、`texture_reference`（9）、`sampler_reference`（10） | `RenderTemplateResolve.cpp:53` 用 `slot_role` 选模块；`:133-136` 用能力位做全图闭合（`MissingModuleCapability`）；`ShaderCodeResourceManifest.cpp:271/309` 校验纹理/采样器引用；`require` 含 Position 分量数硬门 |
| **与 `#include` 重复** | `uses`（82 条指令 / 82 条边 / 50 个文件） | 精确核对：80 条边有对应 `#include`；`uses` 指向不存在模块 0；`#include` 指向非模块 0；`#include` 到的模块未在 `uses` 声明 0；仅 **2** 条边无 `#include`（`material/sky_cube_surface.glsl`、`surface/sky_minimal_surface.glsl` 的 `surface_interface`，原因已查明：依赖由 `fragment/sky.glsl.tmpl:4` 满足） |
| **零实例** | `conflicts` | 全库 **0** 次 |
| **只写不读** | `provide <Semantic>`（16）、`priority`（82，其中 74 个为 0）、`kind`（82） | 消费者只有 `AmbiguousProviderPriority` 自校验 + 定义哈希；provider 实际由 `slot_role` + 模板 root 名选定 |

**评估结论**：

1. **子命题 C（整张模块表废掉）不可行**：include 树只表达「谁包含谁」，表达不了节点属性（`slot` / 能力位 / 纹理引用 / 语义要求）。这些属性要么搬进新格式（换语法的同一张表），要么硬编码进模板（搬家不是删除）。
2. **子命题 C 不划算**：重写 `ShaderCodeModuleFile.cpp`(573) + `ShaderCodeModuleRegistry.cpp`(418) + `ShaderCodeModuleMetadata.cpp`(276) + `ShaderCodeModule.h`(~400) ≈ **1650 行**，只换回 `uses` 那 **~56 代码行**。
3. **可做的是子命题 A + B**（A 已落地 −155，B 待执行 ~56，合计 ~211 代码行 ≈ 3.1%，不构成「立项」量级）：

| 簇 | 位置 | 行数 | 风险 |
|---|---|---|---|
| `conflicts` 全链（零实例） | `Registry.cpp:316-339`(24) + `:369,384-392`(12)、`Metadata.cpp:75-96`(20) + `:220-240`(21)、`Manifest.cpp:193-205`(6) + 字段 + 解析 | **−155 代码行（实测）✅ 已删** | 低。判据交给 glslang。实施时发现漏算 `ModuleConflict` 路径，已连带删除 |
| `uses` 改由 `#include` 推边 | `Registry.cpp:261-313`(52) + `File.cpp` 解析(~12)，`CollectShaderCodeModuleIncludePaths` 转为唯一边来源 | **~56 代码行** | 中。前置：给 2 个 Surface 模块各补一行 `#include "common/surface_interface.glsl"`（已查明原因，零风险） |
| `provide`/`priority`/`kind` | `Metadata.cpp:263-306`(44) + `:54-60`(7) | ~51 | **建议保留**：它是意图文档；删则需同步清掉 82 个文件里 ~180 行资产，引起哈希位移 |

4. **累计天花板修正**：已兑现 95（B 层）+ 155（`conflicts`，全文件口径）≈ **250 代码行**；按 `src/ShaderGen` 口径为 **−201（−2.5%）**，按全部触碰文件口径约 **3.1%**。仍与 80% 差两个数量级。工程已结项，见 `doc/shadergen-reduction-closure-report.md`。

**实施教训（供下次估算用）**：`conflicts` 一簇估算 ~73 代码行、实际删掉 −155，**低估 65%**。
两个原因：① 成对守卫行（`HasValidArray` 条目、哈希循环、`complete` 的 `&&` 链）在估算时被漏算；
② 「零实例」不等于「零消费者」—— `RenderTemplateResolve.cpp:116` 把 `ResourceConflict` 映射成
`ModuleConflict`，并由 `ShaderDocumentRegression` 的夹具变体触发，这条路径是实施时才发现的。
**今后凡判定某特性「可删」，必须同时倒查 `ShaderCodeResourceManifestError` /
`RenderTemplateValidationError` 这类错误枚举的映射链，而不只是 grep 字段名。**

**下一步动作**（工程已结项，以下按结项报告 §5 的判定标注）：

- [x] §10.6-B：删 `conflicts` 全链 —— 实际 −155 代码行，门禁 59 PASS / 0 FAIL，4 个回归 exe 全 exit=0
- [ ] ~~§10.6-A：`uses` 改由 `#include` 推边（~56 代码行）~~ —— **已判定不做**（收益 0.7%，需动 2 个资产文件并复核 glslang include 深度上限）
- [ ] ~~§10.4 P4：`CollectShaderCodeModuleIncludePaths` → glslang `#include`~~ —— **已判定不做**（`uses` 不做后属重复投入）
- [ ] ~~§10.6-C：给 `provide`/`priority`/`kind` 加「只写不读」注释~~ —— **已判定不做**（工程结项，不再做纯文档性改动）

> **结项**：本工程结论见 **`doc/shadergen-reduction-closure-report.md`**。
> 一句话：SPV 反射能替代的 B 层已替代完（95 代码行）；相对引入反射前的基线，ShaderGen 代码量是 **+274 行（+3.6%）**。
> 80% 的目标在当前架构下结构性不可达，真正的出口是「减少需要装配的变体数量」，应另立命题。

**结构性断言（勿再重复论证）**：`GenericMaterialBuilder`(929) + `MaterialShaderEmitter`(675) + `ShaderCodeModuleFile`(573) + `FragmentTemplateComposer`(499) + `ShaderCodeModuleRegistry`(418) = **3094 行（38%）是组装逻辑** —— 它们决定「往 GLSL 里写什么」，反射只能看见「已经写进去的东西」。想真正数量级削减，唯一方向是**减少需要组装的变体**（继续超薄化，把变体推给领域契约与模板），而不是改注记格式。

---

## 11. 每个子阶段的完成定义（DoD）

- [x] `ShaderResourceSchemaRegressionGate all` → **59 PASS / 0 FAIL / exit=0**（含 `reflection` 分组；新增 case 时数量随之增加并同步文档）
- [x] `SceneRenderTemplateResolverRegression` / `ShaderDocumentRegression` / `ShaderDocumentProductionRegression` → 全 exit=0
- [x] `TestRenderStrategyTable` / `TestMaterialVariantTable` → 全 exit=0（保底链行为断言，改动材质定义/回退规则时必跑）
- [x] 实跑 `PBRSpheres` + `TextDraw`：0 errors、模块数正确、无 GLSL 编译错误
- [x] 实跑 `LineRender`（`PrimitiveType::Lines` → 真正走 `LineQuad` mesh 发射模式）：0 validation error、SPV 缓存全命中、持续出帧。**这条不能用 golden 代替**——`{{endpoint_read}}` 的两个函数体文本不在快照粒度内，且夹具是 `defer_finalize`（不编译 SPV），只有实跑能证明两条 `position_format` 分支都产出合法 GLSL
- [x] `git status` 只剩既存子模块脏项
- [x] **拼接顺序有 golden 覆盖**：`SD.emission-order-golden` **16** 变体全绿，覆盖 `ForwardLit` / `ForwardUnlit` / `ShadowCaster` / `Sky` 四条 emission 通路，`ShadowCaster` 的 **opaque 与 masked 两条分支都有覆盖**（阶段五建网、阶段六补 masked），三种 mesh 发射模式（`VertexPassthrough` / `CharQuad` / `LineQuad`）全部有覆盖（阶段六收尾补 `LineQuad`，见 §5.3）
- [x] 更新 `doc/ultra-thin-fixed-shader-architecture.md` 的「阶段进展」条目
- [x] **发行形态跨进程验收 PASS**：`verify-release-form.ps1` 5/5（烘焙 → 探针只链 `ULRE::Runtime` 链接 → 只读运行），证明零生成器符号。任何触碰 `ULRE::Runtime` 成员列表、清单后端、`ShaderArtifactStore` 磁盘布局的改动都必须重跑，见 `doc/shadergen-release-form-verification.md` §6
- [x] 独立提交，提交信息带 `feat(ultra-thin):` / `docs(ultra-thin):` 前缀
