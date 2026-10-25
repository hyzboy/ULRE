# 「用静态 include 树取代注记驱动模块表」立项评估

> 2026-10-25。起因：超薄工程 + SPV 反射落地后，「削减 ShaderGen 80%」实际只兑现 1.15%（见
> `ultra-thin-implementation-status.md` §10.5）。剩余空间里唯一**看起来**能大幅削减的路径是
> 「废掉 `// @ulre` 注记表，改成手写静态 `#include` 树」。
>
> 本文用实测数据回答：这条路值不值得立项。**结论：不值得做整体重写；但从中能剥离出 ~146 行代码
> 的精确删除路径（约 128 代码行），可以不用立项直接做。**

---

## 0. 结论摘要

1. **「注记表」不是一个整体，它是 12 条互不相关的指令。** 逐条查消费者后：**7 条有真实消费者**（`name`
   / `slot` / `require` / `provides_capability` / `requires_capability` / `texture_reference` /
   `sampler_reference`），**1 条与 `#include` 重复**（`uses`），**1 条零实例**（`conflicts`），
   **3 条只写不读**（`provide <Semantic>` / `priority` / `kind`）。
2. **能删的是「重复 + 零实例」这两簇，约 146 文件行 / 128 代码行 ≈ 1.6%。** 前 7 条无法交给 include 树
   —— include 树只表达「谁包含谁」，表达不了「这个模块提供 `shadow_factor` 能力」「这个模块要读材质的
   哪张纹理」。
3. **整体重写不划算**：重写只能替代 `uses`（~55 代码行），其余 7 条注记必须在**新格式里重写一遍**，等于
   把 573 行 `ShaderCodeModuleFile.cpp` + 418 行 `ShaderCodeModuleRegistry.cpp` 推倒重来换 55 行。
4. **80% 依然不可达，而且与注记表无关。** 剩下 ~8000 行是组装逻辑
   （`GenericMaterialBuilder` 929 / `MaterialShaderEmitter` 675 / `ShaderCodeModuleFile` 573 /
   `FragmentTemplateComposer` 499 / `ShaderCodeModuleRegistry` 418），反射在定义上碰不到它们。

---

## 1. 命题拆解

「用静态 include 树取代注记驱动模块表」实际包含三件不同的事，混在一起谈就会得出错误结论：

| 子命题 | 内容 | 本文判定 |
|---|---|---|
| **A. 依赖边改由 `#include` 表达** | 删掉 `@ulre uses`，模块图的边直接取自正文 `#include` | ✅ 可行，省 ~55 代码行 |
| **B. 删掉零实例的注记** | `conflicts` 及其全部校验链路 | ✅ 已执行，实测省 **155** 代码行（估算 ~73，低估 65%） |
| **B'. 删掉只写不读的注记** | `provide` / `priority` / `kind` | ⚠ 可行但**建议保留**（意图文档 + 资产哈希位移） |
| **C. 整张模块表废掉** | 不再扫描解析 `.glsl`，不再有 `ShaderCodeModuleDefinition` | ❌ 不可行，见 §5 |

---

## 2. 实测：`ShaderLibrary` 的注记使用情况

口径：`ShaderLibrary/**/*.glsl` + `*.glsl.tmpl`，共 **89 个文件**，其中 **82 个带注记块**（7 个无注记，
例如纯工具片段）。

### 2.1 指令出现次数

| 指令 | 出现次数 | 备注 |
|---|---|---|
| `// @ulre begin` / `end` | 82 / 82 | 块边界 |
| `name` | 82 | 模块身份 |
| `kind` | 82 | Utility 56 / Position 10 / Transform 7 / Shared 6 / Surface 3 |
| `priority` | 82 | **74 个是 `0`**，非零只有 11 个（1 / 8 / 9 / 10） |
| `uses` | 82 条指令（分布在 50 个文件） | 每条指令**只带 1 个模块名**，故恰好 **82 条边** |
| `slot` | 34 | `material_source_provider` 10 / `diffuse_light_provider` 4 / `ambient_light_provider` 3 / `specular_light_provider` 3 / `shadow_provider` 3 / `surface_provider` 3 / `output_policy` 2 / `ntb_provider` 2 / `lighting_model` 2 / `ambient_occlusion_provider` 1 / `sky_provider` 1 |
| `require` | 32 | `GeometryAttribute Position Float 2 2` ×6、`Resource MaterialData` ×5、`ProducedSemantic WorldPosition` ×4 … |
| `provides_capability` | 17 | 能力位掩码（`direct_diffuse` / `shadow_factor` …） |
| `provide` | 16 | **语义**（`Normal` / `Position` / `UV0` / `Color` …） |
| `requires_capability` | 2 | 能力位掩码 |
| `texture_reference` | 9 | |
| `sampler_reference` | 10 | |
| `conflicts` | **0** | 全库**一次都没用过** |

### 2.2 判定表（核心，按消费者倒查）

`provided_capabilities` 与 `provide` 是**两件不同的事**：前者是能力位掩码，后者是语义列表。
之前的讨论把两者混为一谈，这里分开。

| 注记 | 落到哪个字段 | 真实消费者（`file:line`） | 判定 |
|---|---|---|---|
| `name` | `definition.name` | 渲染模板 root 名、manifest root、日志 | **真源** |
| `slot` | `slot_role` | `RenderTemplateResolve.cpp:53` `module->slot_role != root.role` → 模板按 role 选模块 | **真源（这才是选择机制）** |
| `require …` | `semantic_requirements[]` | `GenericMaterialBuilder.cpp` ×13、`MaterialShaderEmitter.cpp` ×2、`VertexABIBuilder.cpp` ×2（含 Position 分量数硬门） | **真源** |
| `provides_capability` | `provided_capabilities` | `RenderTemplateResolve.cpp:133-136`：对全图做 `required & ~provided` → `MissingModuleCapability` | **真源** |
| `requires_capability` | `required_capabilities` | 同上 | **真源** |
| `texture_reference` | `texture_reference_requirements[]` | `ShaderCodeResourceManifest.cpp:271` `ValidateShaderCodeResourceManifestTextureReferences` | **真源** |
| `sampler_reference` | `sampler_reference_requirements[]` | `ShaderCodeResourceManifest.cpp:309` `ValidateShaderCodeResourceManifestSamplerReferences` | **真源** |
| `uses` | `dependencies[]` | 闭包 DFS（`ShaderCodeResourceManifest.cpp:109-215`）+ `ShaderCodeModuleRegistry.cpp:261-313` 的一致性强制 | **与正文 `#include` 重复**（82 条边中 80 条有 `#include`，仅 2 条例外且已查明原因） |
| `provide <Semantic>` | `semantic_provides[]` | 全仓库**只有** `ShaderCodeModuleMetadata.cpp` 自校验 + `ShaderCodeModule.cpp` 哈希 | **只写不读** |
| `priority` | `priority` | 同上（只有 `AmbiguousProviderPriority`） | **只写不读** |
| `kind` | `kind` | 同上（只有 `IsProviderCandidate` 过滤候选） | **只写不读** |
| `conflicts` | `module_conflict_names[]` | `AreShaderCodeModulesConflicting` → `ResourceConflict` | **零实例** |

> **关键反直觉发现**：provider 的选择**不靠语义**，靠 `slot_role` + 渲染模板里写死的 root 名
> （`GenericMaterialBuilder.cpp:404-428`：`provider_roles[] = {MaterialSourceProvider, NTBProvider}`
> → `plan.render_template_request->FindModuleRoot(role)` → `FindByName(root->module_name)`）。
> 因此 `provide` / `priority` / `kind` 三个字段连同 `AmbiguousProviderPriority` 检查，保护的是一个
> **并不存在的选择机制** ——「选哪个 provider」由模板决定，「这个 provider 提供什么语义」从不参与判定。

---

## 3. 三个可删簇（行号 + 实测行数）

### 3.1 `conflicts`（零实例，最干净）

| 位置 | 行号 | 行数 |
|---|---|---|
| `ShaderCodeModuleRegistry.cpp` 冲突名解析块（`pending_module_conflicts` → `module_conflict_names`） | 316-339 | ~24 |
| `ShaderCodeModuleRegistry.cpp` incomplete 剔除环里的冲突半边（条件 369 + 循环 384-392） | 369, 384-392 | ~12 |
| `ShaderCodeModuleMetadata.cpp` `AreShaderCodeModulesConflicting` | 75-96 | ~20 |
| `ShaderCodeModuleMetadata.cpp` `module_conflict_names` 自校验（`SelfConflict` / `DuplicateConflict`） | 220-240 | ~21 |
| `ShaderCodeResourceManifest.cpp` 模块冲突对检查（`:197` 的 `AreShaderCodeModulesConflicting`） | 193-205 | ~6 |
| **合计** | | **~83 行** |

外加 `ShaderCodeModule.h` 的 `module_conflict_names` / `module_conflict_count` 两个字段与
`ShaderCodeModuleFile.cpp` 的 `pending_module_conflicts`。

> **口径修正（实施后才发现，务必注意）**：`ShaderCodeResourceManifest.cpp` 里还有 4 处
> `ResourceConflict`（`:37/:59/:73/:92`），此前写成「纹理/采样器引用的重复声明冲突」是**错的** ——
> `AddTextureReference` / `AddSamplerReference` 对同名条目是**合并**（`:41-51` / `:64-68`），
> `ResourceConflict` 只在**名称为空**或**数组容量溢出**时触发，与重复声明无关。**这 4 处不能删。**

⚠ **注意**：删除后必须确认没有「模块 A 与 B 同时进图会重复定义符号」的现实风险。当前 `conflicts`
实例为 0，说明这个风险从未被规避过 —— 若真有重复定义，glslang 会直接报错。所以删除是**把判据交给
编译器**，符合反射方向。

⚠ **实施中发现一处此前漏算的消费者**：`RenderTemplateResolve.cpp:116` 曾把
`BuildShaderCodeResourceManifest` 的 `ResourceConflict` 映射成
`RenderTemplateValidationError::ModuleConflict`，而 `ShaderDocumentRegression` 的
`SurfaceConflictsWithDirectSun` 夹具变体正是用 `@ulre conflicts diffuse/lambert` 触发它。
删掉模块冲突检查后这条路径**不可达**，故连带删除 `ModuleConflict` 枚举值、它的
`GetXxxErrorName` 分支和该夹具变体。**教训：`conflicts` 不是「零消费者」，而是「零生产实例 +
一个仅供夹具使用的消费者」。**

### 3.1a 实施结果（`conflicts` 全链已删除，实测）

| 文件 | 总行 | 代码行 |
|---|---|---|
| `ShaderCodeModuleMetadata.cpp` | −48 | −42 |
| `ShaderCodeModuleRegistry.cpp` | −43 | −39 |
| `ShaderCodeResourceManifest.cpp` | −13 | −12 |
| `ShaderCodeModuleFile.cpp` | −10 | −9 |
| `ShaderCodeModuleMetadata.h` | −7 | −5 |
| `ShaderCodeModule.h` | −5 | −2 |
| `ShaderCodeModuleFile.h` | −5 | −4 |
| `ShaderCodeModule.cpp` | −4 | −3 |
| `RenderTemplateResolve.cpp` | −3 | −3 |
| `RenderTemplate.h` | −1 | −1 |
| `RenderTemplate.cpp`（ShaderMaterialData） | −1 | −1 |
| **小计（`src/ShaderGen` + `inc/hgl/mtl` + `ShaderMaterialData`）** | **−140** | **−121** |
| 回归工装（`ShaderDocumentRegression` + 门禁） | −36 | −34 |
| **合计** | **−176** | **−155** |

比 §3.1 的 ~73 代码行估算高 ~65%，原因是估算时漏掉了 `ModuleConflict` 路径与多处
成对删掉的守卫行（`HasValidArray` 条目、哈希循环、`complete` 的 `&&` 链）。
校验：门禁 59 PASS / 0 FAIL；`ShaderDocumentRegression`、`ShaderDocumentProductionRegression`、
`SceneRenderTemplateResolverRegression`、`TestCSMIncrementalPass` 全部 exit=0，
`Loaded 82 code modules (0 errors)`。

### 3.2 `uses`（与 `#include` 重复）

| 位置 | 行号 | 行数 |
|---|---|---|
| `ShaderCodeModuleRegistry.cpp` `@ulre uses` ↔ `#include` 一致性强制块 | 261-313 | ~52 |
| `ShaderCodeModuleFile.cpp` `uses` 指令解析 | ~12 | 12 |
| **合计** | | **~64 行（~56 代码行）** |

改成从 `#include` 直接取边之后，`CollectShaderCodeModuleIncludePaths` 从「只能用来交叉校验」变成
「唯一的边来源」，**净收益 ~56 代码行**，且消灭 82 条重复声明与 `Undeclared code-module dependency`
这条错误路径（`ShaderCodeModuleRegistry.cpp:307` 的 `GLogError` 随之消失：不可能再出现「include 了
却没声明」的情况，因为它们本来就是同一个来源）。

⚠ **两个反例必须先处理**（实测 82 条 `uses` 边里只有这 2 条没有对应 `#include`）：

- `ShaderLibrary/material/sky_cube_surface.glsl:9` 声明 `uses surface_interface`，正文只有 `#include "common/bindless_textures.glsl"`
- `ShaderLibrary/surface/sky_minimal_surface.glsl:8` 声明 `uses surface_interface`，正文只有 `#include "sky/sky_atmosphere.glsl"`

**已查明原因（不是历史残留）**：这两个都是 **Surface 模块**，正文用 `SurfaceInput` / `SurfaceOutput`，
而这两个类型由 `fragment/sky.glsl.tmpl:4` 的 `#include "common/surface_interface.glsl"` 提供 —— 也就是
**依赖由模板满足，不由模块自己满足**。所以 `uses` 在这里承担的是「声明，而非包含」。

**影响评估：几乎为零。** `common/surface_interface.glsl` 的注记只有
`kind Shared` + `uses descriptor_macros`，**没有** `texture_reference` / `sampler_reference` /
`require` / `*_capability`。因此丢掉这条边不会丢任何校验。

**建议的修法（1 行 × 2 文件，零风险）**：给这两个文件各补一行
`#include "common/surface_interface.glsl"`。该文件自带 `#ifndef SURFACE_INTERFACE_GLSL` 守卫
（`surface_interface.glsl:10-11`），与模板的 include 重复不会产生重复定义。补完之后 82 条边
**100% 可被 include 树重建**，`uses` 即可安全删除。

（探索性的备选：若不想动 GLSL，也可在「`#include` 推边」实现里保留一条特例：`Surface` 类模块隐式
依赖 `surface_interface`。但这等于把一个特例写进代码，正是本轮想消灭的东西 —— 不推荐。）

### 3.3 `provide` / `priority` / `kind`（只写不读）

| 位置 | 行号 | 行数 |
|---|---|---|
| `ShaderCodeModuleMetadata.cpp` `AmbiguousProviderPriority` 双循环 | 263-306 | 44 |
| `ShaderCodeModuleMetadata.cpp` `IsProviderCandidate` | 54-60 | 7 |
| **合计** | | **~51 行** |

⚠ 这一簇**建议保留、只加注释标注「只写不读」**，理由：它是一份**意图文档**（`priority=10` 的阴影实现
暗示「以后要按优先级选」），删掉省 51 行但会让后来者重新踩一遍「以为它有用」的坑。若坚持删，须同时
把 `provide` / `priority` / `kind` 三条注记从 82 个文件里清掉（约 180 行资产），那是**资产迁移**而不是
代码简化，需要单独评估 `ShaderArtifactStore` 的烘焙哈希位移。

---

## 4. 汇总：可删总量

（代码行按上一轮实测的「代码行/文件行 ≈ 0.88」折算。）

| 簇 | 文件行 | 代码行 | 资产改动 | 风险 | 状态 |
|---|---|---|---|---|---|
| `conflicts` | −176 | **−155** | 0 | 低 | ✅ 已执行（实测比估算高 65%，见 §3.1a） |
| `uses` | ~64 | **~56** | +2 行 `#include`，−82 行 `uses` | 中（glslang include 深度需复核） | 待执行 |
| `provide`/`priority`/`kind` | ~51 | ~45（建议保留） | ~180 | 中（哈希位移） | 建议不做 |
| **合计（A+B，建议执行）** | **−240** | **−211** | | | A 已落地 −155，B 待执行 ~56 |

实测修正：`conflicts` 一簇实际就是 **−155 代码行**，估算 ~73 偏低 65%——既高估了「删得干净」的
程度，也漏算了 `ModuleConflict` 那条路径。剩余可执行项只有 `uses` 的 ~56 代码行。

按 8071 代码行的分母：已落地 **−249 代码行 ≈ 3.1%**；若再做 `uses` 则 ≈ **3.8%**；
若连第三簇一起删则 ≈ **4.3%**。**距离 80% 仍是两个数量级。**
（`conflicts` 在全仓 13 个文件合计 −155，其中 `src/ShaderGen` 本身占 −106 → 8071 → 7965。）

---

## 5. 为什么不建议「整张模块表废掉」（子命题 C）

假设真的把 `ShaderCodeModuleDefinition` + registry 全部删掉，只留手写 `#include` 树：

1. **`slot` 无处安放**。`RenderTemplateResolve.cpp:53` 需要 `module->slot_role` 判断「这个模块能不能填
   模板的这个 role」。要么在模板里写死文件名（那就把 34 个 `slot` 声明换成 34 处模板硬编码，是搬家
   不是删除），要么在文件名/目录名里编码（等于换个语法的注记表）。
2. **`provides_capability` 无处安放**。`MissingModuleCapability` 是**整图**能力闭合检查
   （`RenderTemplateResolve.cpp:120-140` 明确注释「必须基于整张依赖图，不能只看 roots」）。include 树
   只给边，不给节点属性。
3. **`require` / `texture_reference` / `sampler_reference` 无处安放**。这三者都是「节点属性」，且有真实
   消费者。`require GeometryAttribute Position Float 2 2` 就是上一轮抓到的 Position 分量数双重真源 bug
   的判据来源，删不得。
4. **代价**：推倒 `ShaderCodeModuleFile.cpp`(573) + `ShaderCodeModuleRegistry.cpp`(418) +
   `ShaderCodeModuleMetadata.cpp`(276) + 整个 `ShaderCodeModule.h`(约 400) ≈ **1650 行重写**，换回
   **~56 代码行**（只有 `uses` 那一段真正被 include 树替代）。
5. **收益为零**：重写后的「静态 include 树 + 若干新格式属性声明」与现在的「注记块 + 解析器」在体量上
   是同一个数量级。**换命题不等于减化。**

---

## 6. 最终结论

1. **不立项**做整体重写。它只能吃掉 `uses` 的 ~56 代码行，代价是 ~1650 行重写 + 3 个回归门大改。
2. **不需要立项，直接做**的是 §3.1 + §3.2，判据交给编译器与 include 树，风险可控，跟「让编译器和
   反射当权威」这条主线完全一致。其中 **§3.1 `conflicts` 已执行完毕（−155 代码行，实测）**，
   §3.2 `uses` 的 ~56 代码行仍待执行。
3. **80% 削减在当前架构下结构性不可达**。无论注记表怎么改，`src/ShaderGen` 的主体是
   **组装逻辑**：929 + 675 + 573 + 499 + 418 = 3094 行（38%），它们决定「往 GLSL 里写什么」，
   而反射只能看见「已经写进去的东西」。想要真正的数量级削减，唯一的方向是**减少需要组装的变体**
   ——也就是继续走超薄路线把变体推给领域契约与模板，而不是改注记格式。

---

## 7. 复现方法（下次不要凭印象重估）

```powershell
# 注记普查（按指令计数，含 slot/kind/priority 取值分布）
# 注意：`uses` 是 82 条指令分布在 50 个文件，每条只带 1 个模块名 → 恰好 82 条边
python -c "import re,pathlib,collections; ..."   # 正则 ^\s*//\s*@ulre\s+(\w+)\s*(.*)$

# uses ↔ #include 精确交叉核对（必须建 name→path 映射，不能拿字符串直接比）
#   期望：80/82 匹配（2 条例外即两个 Surface 模块）、0 指向不存在模块、
#         0 include 到非模块文件、0 include 未在 uses 声明

# 元数据字段的消费者倒查（排除回归门）
Get-ChildItem src,inc -Recurse -Include *.cpp,*.h |
  Select-String -Pattern 'semantic_provide' |
  Where-Object { $_.Path -notmatch 'ShaderResourceSchemaRegressionGate' }

# 总量口径（务必用 git ls-tree 逐文件，不要用 Get-ChildItem 单层 glob）
git ls-tree -r --name-only HEAD src/ShaderGen | Select-String '\.(cpp|h)$'
```

**口径教训（已犯两次，勿再犯）**：

1. `@ulre uses` 是**指令数**（82）不是**文件数**（50），也不是**边数上限**（每条只带 1 个名字 → 边数 = 指令数 = 82）。
   先前写成「82 个文件 / ~250 条边」是错的。
2. `Get-ChildItem -Recurse -Include *.cpp,*.h` 在 `src/ShaderGen` 上会漏掉子目录层级的差异，导致 §10.5
   里曾经写错三个总量数字（8565 / 8662 / 9080）。正确口径见 `ultra-thin-implementation-status.md` §10.5 的表。
