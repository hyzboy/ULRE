# ShaderGen 削减工程 · 结项报告

> 状态：**结项（目标未达成，结论已定）**
> 范围：`src/ShaderGen` / `src/ShaderMaterialData` / `inc/hgl/mtl` / `src/Tools/ShaderGen`
> 依据文档：`doc/ultra-thin-implementation-status.md`（过程台账）、`doc/annotation-vs-include-tree-assessment.md`（注记表改写立项评估）
> 计口径：**代码行 = 总行 − 空行 − 整行注释**，文件集用 `git ls-tree -r --name-only <ref> src/ShaderGen`（`*.cpp` / `*.h`）

---

## 0. 结论摘要

原命题是：**用 SPV 反射取代 ShaderGen 的编译前推断，以削减 ≥80% 的 ShaderGen 代码量。**

实测结论：

1. **80% 不可达，且不是执行不力，而是结构性不可达。** 反射只能看见「已经写进着色器的东西」，
   而 ShaderGen 的绝大部分代码在做**编译前就必须决定的事**（采样器宏、纹理表、顶点布局、`layout(location=N)` 出参序号、push-constant 地址）。
   这类输入定义上无法由反射反推。
2. **两批删除合计 −250 代码行**（`src/ShaderGen` 内部 −201，其余在 `inc/hgl/mtl` / `ShaderMaterialData` / 回归工装）。
   相对峰值 8071 行约 **3.1%**；若只按 `src/ShaderGen` 口径算则是 **−2.5%**。两者都对，差别只在统计范围，见 §2 的脚注。
3. **相对「引入反射之前」的基线，ShaderGen 反而多了 274 行（+3.6%）。**
   那 250 行是删掉自己后续添加的一部分，不是净减少。
4. 反射并没有白做——它买到的是**正确性**（三组编译后真实校验 + 一条不再需要人肉模拟的判据），但那不是「削减」。

**建议**：本命题结项。真正能带来数量级削减的命题是 **「减少需要装配的变体数量」**，与注记格式、与反射都无关，应另立。

---

## 1. 判定标准

为了让「削减了多少」可验证、不可自我表扬，本次用三条硬口径：

| 口径 | 定义 | 为什么这么定 |
|---|---|---|
| 度量单位 | **代码行**（总行 − 空行 − 整行注释） | 总行会被「补注释」污染；文件数会被「拆文件」污染 |
| 分母 | 反射落地后的峰值 **8071** 代码行 | 与「削减」语义一致：删的是自己加进来的部分 |
| 参照 | 同时给出「相对引入反射前基线 7691」的净值 | 防止把「先加后删」讲成「净减少」 |

---

## 2. 实测台账

同一脚本、同一文件集口径复测四个里程碑：

| 里程碑 | 提交 | 文件 | 总行 | 代码行 | 相对基线 |
|---|---|---|---|---|---|
| 超薄改造前 | `006d2b8e5` | 41 | 9617 | **7691** | 基线 |
| 反射落地后峰值 | `59a266251` | 44 | 10234 | **8166** | **+475（+6.2%）** |
| B 层删除（依赖满足性推断交给权威判据） | `439a6a517` | 44 | 10127 | **8071** | +380 |
| 删 `conflicts` 全链 | `61935e865` | 44 | 10008 | **7965** | **+274（+3.6%）** |

三段变化拆开看：

- **加**：超薄六阶段 + 反射 API + 门禁，7691 → 8166，**+475**。这是必要投入（拿到了 .tmpl 外置、资产迁移、反射接口、59 条门禁用例）。
- **删**：8166 → 8071 → 7965，**−201**。
  （早前文档把第一段记为 −94，本次统一脚本复测为 −95，差 1 行来自尾行计数方式；本文统一用复测值。）
- **净**：7691 → 7965，**+274**。

> **口径脚注**：两批删除的统计范围不同，别把它们当成同一个分母上的两个数。
> B 层删除只发生在一个文件（`ShaderCodeModuleMetadata.cpp`），因此 −95 就是 `src/ShaderGen` 内的净减；
> `conflicts` 全链跨 13 个文件，**全文件口径 −155、其中 `src/ShaderGen` 内 −106**。
> 于是：`src/ShaderGen` 口径合计 **−201（−2.5%）**；全部触碰文件口径合计 **−250（相对峰值约 3.1%）**。
> 在「削减 80%」的原命题下，正确的读法只有一个：**投入 475 行换回 201 行，净额 +274（+3.6%）——分母没有变小，反而变大了。**

---

## 3. 为什么 80% 结构性不可达

### 3.1 A/B/C 分层：只有 B 层是反射的定义域

| 层 | 内容 | 反射能否替代 | 实测行数 |
|---|---|---|---|
| **A. 编译前必须已知的输入** | 采样器 `#define`、纹理表、`s1_position_*`、`s2_*`、`layout(location=N)` 出参序号、`RootAddresses` | **不能**（反射只能读出已经写进去的结果） | 主体 |
| **B. 编译后人肉验证** | C++ 里对「依赖是否满足、接口是否闭合、能力位是否齐」的模拟推断 | **能，且已做** | 已删 95 |
| **C. 生成物里的纯自述结构** | 模块表自带的元数据与自校验 | 部分可替代；整表重写换不回等价收益 | 已删 155（全文件口径，`src/ShaderGen` 内 106；仅「零实例」部分可安全删） |

**B 层在那 8071 行里只有 95 行。** 这一条就决定了天花板：即便把 B 层删到 0，也只到 1.2%。

### 3.2 真正的体积在「组装逻辑」，不在「校验」

| 组件 | 代码行 | 性质 |
|---|---|---|
| `GenericMaterialBuilder.cpp` | 929 | 组装 |
| `MaterialShaderEmitter.cpp` | 675 | 组装 |
| `ShaderCodeModuleFile.cpp` | 573 | 解析（A 层） |
| `FragmentTemplateComposer.cpp` | 499 | 组装 |
| `ShaderCodeModuleRegistry.cpp` | 418 | 注册/解析（A 层） |
| **小计** | **3094 ≈ 38%** | 基本全是 A 层与组装 |

这 3094 行解决的是「**这一帧要拼哪一个变体**」。反射在编译之后才存在，
在它出现的那一刻这些决定已经做完了——所以它替不掉任何一行。

### 3.3 「整体换掉注记表」也不成立

已单独立项评估（见 `doc/annotation-vs-include-tree-assessment.md`）：

- **换语法 ≠ 删代码**：`slot`、`provides_capability`/`requires_capability`、`texture_reference`、`sampler_reference`、`require`
  都是**节点属性**，include 树只能表达「谁包含谁」，表达不了节点属性。搬进新格式 = 同一张表换皮。
- **成本 1650 行重写换回 56 行**（`uses` 一簇）。判定：**换命题不等于减化**。

---

## 4. 已交付（不只是删代码）

虽然削减未达标，这一轮不是空转。按价值排序：

1. **SPV 反射接口落地**（`707178f17`）：`ShaderReflectionData` 暴露反射结果，并据此推导 compute push-constant 的末段长度，替掉原先的猜测。
2. **B 层判据换人**（`439a6a517`）：删掉 `ValidateDependencyVisit` / `FindModuleIndex` / `MissingDependency` 等 C++ 模拟推断，
   判据改由三条**更早生效**的权威机制承担——registry 的 incomplete 剔除环、`BuildShaderCodeResourceManifest` 的 DFS（`UnknownCodeModule` / `CodeModuleCycle`，且阻断编译）、
   `SPV.generated-interface` 对 16 个变体做真实编译。
3. **`conflicts` 全链删除**（`61935e865`）：零生产实例的注记连同 5 处校验、1 个错误枚举值、1 个夹具变体一并删除，**−155 代码行**。
4. **超薄六阶段**：静态骨架外置到 `.tmpl`、资产迁移到 `.mi.toml` 并废除 `.material.toml`、ShadowCaster 骨架外置、LineQuad 快照补齐。
   （这些让代码更可读、资产更可维护，但**没有**减少行数。）
5. **安全网**：emission golden 覆盖 16 变体 + 59 条门禁用例。后续任何触碰装配逻辑的改动都有零位移判据。

**净收益的诚实表述**：正确性 + 可维护性明显改善；**代码量没有减少**。

---

## 5. 未做项与不做的理由

| 项 | 估算 | 状态 | 不做的理由 |
|---|---|---|---|
| `uses` 改由 `#include` 推边 | ~56 代码行 | **不做** | 收益 0.7%；且需给 2 个 Surface 模块补 `#include`、要复核 glslang include 深度上限（当前硬编码为 2）。风险与收益不成比例 |
| `provide`/`priority`/`kind` | ~45 代码行 | **不做** | 是「意图文档」：provider 实际由 `slot_role` + 模板 root 名选定，本簇保护的是意图而非机制。删它要同步清 ~180 行资产并引起定义哈希位移 |
| 注记表整体换 include 树 | 净 ≈ −56 | **不立项** | 1650 行重写；节点属性无处安放；换语法不是减代码 |
| `CollectShaderCodeModuleIncludePaths` → glslang `#include` | ~45 代码行 | **不做** | `uses` 推边后会复用而非删除；单独做属重复投入 |
| `MaterialShaderEmitter` 静态文本 → `.tmpl` | ~100 代码行 | **不做** | 纯搬运，不改变「需要装配的变体数」这一根本矛盾 |
| STL 整体迁移 | 436 处 `std::` | **不立项** | 需先补 HGL 侧 `ValueArray` 迭代能力与 `hgl::sort`（当前无等价物）、改 `GetShaderLibraryPath()` 签名（11 文件 / 26 调用点）。跨 CMCore 公共库，风险高于收益 |
| manifest 运行时消费者审计 | 0 | **不做** | 唯一可能的发现是「还有编译后事实可被反射替代」，但 B 层已删到 0，且 A/B/C 分层已证明结构上没有第三层 |

---

## 6. 遗留状态与接手入口

**验证现状（截至 `61935e865`，全绿）**

```
ShaderResourceSchemaRegressionGate     59 PASS / 0 FAIL
ShaderDocumentRegression               exit=0
ShaderDocumentProductionRegression     exit=0   (Loaded 82 code modules, 0 errors)
SceneRenderTemplateResolverRegression  exit=0
TestCSMIncrementalPass                 exit=0
```

**注意**：exe 必须在 `E:\ULRE` 下运行（`GLSLCompiler.dll` 在仓库根）。
`ULRE_DEBUG.toml` 受版本控制，`channels.shadergen` 必须保持 `false`。

**已知无关失败**（与本工程无关，勿误判）：`TexConvCore` 的 `LNK1104 TexImage.lib`；
`MaterialErrorFallback` / `DualWorldShadows` 退出期崩溃；`BidirectionalMapStressTest` 超时。

**文档入口**
- `doc/ultra-thin-implementation-status.md` — 过程台账（§10.4 P1–P7 清单、§10.5 天花板对账、§10.6 立项评估）
- `doc/annotation-vs-include-tree-assessment.md` — 注记表改写立项评估（含 `conflicts` 实施结果 §3.1a）

---

## 7. 建议

1. **本命题结项**：SPV 反射取代 ShaderGen 推断，能替代的部分已经替代完（95 行），剩下的定义上不可替代。
2. **不要把 3.1% 包装成进展**。相对引入反射前的基线，ShaderGen 代码量是 **+274 行**。承认这一点，下一个命题才有正确的起点。
3. **下一个命题应当是「减少需要装配的变体数量」**——合并变体、把分支下沉到运行时 uniform、或减少模板组合维度。
   这才是那 3094 行组装逻辑的唯一出口；它不需要反射，也不需要改注记格式。
4. **保留现有安全网**。emission golden 与 `SPV.generated-interface` 门禁是上述命题的前置条件：动装配逻辑必须有零位移判据。
