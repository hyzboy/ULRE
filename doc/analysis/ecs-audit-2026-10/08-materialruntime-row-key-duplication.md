# §2.2 问题 8 详述：`MaterialRuntimeRow` 把 key 存了"约 3.5 份"——**要订正为"看着像 3 份、实际只有 0.5 份是重复"**

> **审计出处**：`doc/ecs-quality-audit-2026-10.md` §2.2「并行表家族」第 8 条（原文："`MaterialRuntimeRow` 把 key 存了约 3.5 份（`key` + `recipe_hash` + `data_index_row` + `cached_effective_recipe_hash`）"）
> **结论一句话**：**这条要订正**。`row.recipe_hash` / `row.data_index_row` **不是** `key` 的副本 —— 它们是**绑定值/缓存戳**，而 `key` 是**身份维度**：两者只在"**共享行**"上取值相同，在 **CoW 自有行**上 `key` 被显式清空（`DetachOwned`：`owned_row.key = {}`）⇒ 自有行只有 `row.*` 可用。`cached_effective_recipe_hash` 则是**另一个哈希**（program 解析后的 effective recipe），跟 key 无关。⇒ 真正可省的：**0 处字段**；真正该补的：**不变量文档 + 一条 debug 对拍**。

---

## 一、行字段全貌（`inc/hgl/ecs/support/MaterialRuntimeTable.h`）

```cpp
struct MaterialRuntimeRow
{
    MaterialRuntimeKey key{};                       // 身份三元组（interned；自有行 = 空）

    MaterialVariantID forward_variant;              // 变体 ID（前向 / 阴影两个程序槽）
    MaterialVariantID shadow_variant;

    uint32_t data_index_row = uint32_t(-1);         // 材质 SSBO 行（授权态；-1 = 尚未物化）
    void    *material_row_cpu;  uint64_t material_row_gpu;

    graph::MaterialTextureConfigurationAllocation material_texture_configuration;   // 纹理引用配置
    void    *material_texture_row_cpu;  uint64_t material_texture_row_gpu;
    uint64_t material_texture_zero_row_gpu;
    uint64_t material_texture_configuration_hash;

    uint64_t recipe_hash = 0;                       // ← 审计点名的"第 2 份"
    graph::mtl::MaterialRecipe cached_normalized_recipe{};
    graph::mtl::MaterialRecipe cached_effective_recipe{};        // 已解析配方缓存（program 解析产物）
    uint64_t cached_effective_recipe_hash = 0;      // ← 审计点名的"第 0.5 份"（**其实是不同物**）
    graph::mtl::MaterialRecipe shadow_cached_normalized_recipe{};

    uint32_t refcount;  uint32_t generation;  bool owned;  bool alive;   // 行管理（非绑定内容）
};
```

字段准入判据写在结构上方（`:134-139`）：

> 字段准入判据：**只放随授权态变化的绑定**。· 变体 ID（program 身份所在；program 本体归 `ShaderProgramManager` 缓存持有）；· 材质 SSBO 行号与 CPU/GPU 地址（由授权态 + 世界缓冲布局决定）；· 纹理引用配置行（池行所有权随行）；· **已解析配方缓存（normalized / effective）与各自哈希**。行管理字段（refcount / generation / owned / alive）不属于绑定内容。

⇒ "配方缓存 + 哈希"是**准入判据里明写的成员**（有意保留），不是漏删。

---

## 二、逐字段核对：哪些是"重复"，哪些是"不同物"

| 字段 | 与 `key` 的关系 | 判据（证据） |
|---|---|---|
| `key`（三元组） | 身份本身 | interned 索引键；`Intern` 用 `==`/hash（`MaterialRuntimeTable.cpp:85-108`） |
| `data_index_row` | **共享行上等于 `key.data_index_row`；自有行上不等**（`key` 为空） | `DetachOwned` 内容拷贝后 `owned_row.key = {}`（`MaterialRuntimeTable.cpp:198-201`，"自有行**不入键索引**"）⇒ 自有行只剩 `row.data_index_row` 可用。结构注释亦自认"`data_index_row` **同时是行键的一维**"（`:152-154`） |
| `recipe_hash` | **共享行上等于 `key.recipe_hash`**；自有行上 `key` 空 ⇒ 只剩它 | 写点 `RenderPrimitiveCollectSystem.cpp:1287`（`row->recipe_hash = recipe_hash;`，在 program 解析成功后、与配方缓存同批写）；读点 `:1344-1346` 作**缓存戳**：`if (!shadow_pass && row->recipe_hash == row->cached_effective_recipe_hash) effective_recipe = row->cached_normalized_recipe;` |
| `cached_effective_recipe_hash` | **与 key 无关** | 它 = `HashMaterialRecipe(material_binding_recipe)`（program **解析后**的 effective recipe，`:1291-1292`）⇒ 是"缓存对应的另一侧哈希"，与"身份配方哈希"是**两个不同的哈希** |
| `cached_normalized_recipe` / `cached_effective_recipe` / `shadow_cached_normalized_recipe` | 解析产物缓存（准入判据明列） | 同上 |

⇒ 审计的"3.5 份"**拆开是**：
- `key.recipe_hash` + `row.recipe_hash` ⇒ **共享行上是同一值的两份**（自有行上 `key` 空、只剩 `row`）；
- `key.data_index_row` + `row.data_index_row` ⇒ 同上；
- `cached_effective_recipe_hash` ⇒ **不同物**（0.5 那份从这里来）。

**真正"两份同一个值"的确实存在（共享行），但删任何一份都会破坏自有行** ⇒ 这不是可免费删除的重复。

---

## 三、那"重复"的代价是什么（诚实评估）

- **内存**：`row.recipe_hash`（8 B）+ `row.data_index_row`（4 B + padding）≈ 16 B/行；行本身含 3 个 `MaterialRecipe` 结构体 + 纹理配置 + 若干地址 ⇒ 这 16 B 不是主要成本。**按内存算，删它们收益很小。**
- **一致性风险**：同一值的两份，谁改一份忘另一份就漂移。具体后果：
  - `row.data_index_row` 漂移 ⇒ "尚未物化（-1）"判定错 ⇒ 批处理侧误判"行未就绪"或"已就绪"（`RenderPrimitiveCollectSystem` 按它判 `data_index_row != -1`）；
  - `row.recipe_hash` 漂移 ⇒ 缓存戳比较（`:1344-1346`）失效 ⇒ 该复用 `cached_normalized_recipe` 的地方**退化**去 `CreatePipeline`（性能退化，静默）；或反之**错误复用**旧配方（视觉错误）。
- **真正的"多余"其实是 `key` 在共享行里的自持**：`key` 同时存在于"索引表（map 的 key）"与"行体"两处 —— 这是**有意的**（自有行要从行体里取键去做 `index_by_key.DeleteByKey(rows[id].key)`，`MaterialRuntimeTable.cpp:172-174`）⇒ 拆链/回收不必回查索引。**这才是 3.5 份里最有"解释价值"的那一份**，而它同样删不得。

---

## 四、处置选项

### 选项 (a)：补"不变量文档"（零风险，建议先做）

在 `MaterialRuntimeRow` 头注里明写两条不变量与一条例外：

```
· 共享行（interned，owned == false）：row.data_index_row == key.data_index_row
                                     row.recipe_hash     == key.recipe_hash
· 自有行（CoW 分裂，owned == true） ：key 为**空**（不入键索引）⇒ 以 row.* 字段为准
· cached_effective_recipe_hash 与 key.recipe_hash 是**两个不同的哈希**
  （前者 = program 解析后 effective recipe；后者 = 身份配方），不可互相替代
```

成本：~8 行注释；收益 = 下次审计不会再把 `row.recipe_hash`/`row.data_index_row` 当"可删副本"（这正是本条要防的翻案），也告诉改这块的人"哪里必须成对更新"。

### 选项 (b)：加一条 debug 对拍（低成本，把不变量钉住）

在 `Intern`/`GetMutable` 的返回点或 `DetachOwned` 前后，`ULRE_ECS_DEBUG_API` 下断言：

```cpp
assert(row->owned || (row->data_index_row == row->key.data_index_row
                   && row->recipe_hash     == row->key.recipe_hash));
```

成本：~5 行 + 开关；收益 = "两份同一个值"的漂移会在 debug 期点名而不是变成静默性能/视觉问题。

### 选项 (c)：真删字段（**不建议**）

要让 `row.data_index_row` / `row.recipe_hash` 消失，必须解决"自有行 key 为空"这一档 —— 例如给自有行也保留键（破坏"自有行不入键索引"的语义）、或把绑定值搬到索引侧（`detach` 时索引已摘链 ⇒ 无处可放）。两者都比现状更复杂。**收益（~16 B/行）远小于风险。**

### 选项 (d)：减内存的真方向（若不缺这一块可不动）

真正能减行宽的是**配方缓存三件套**（`cached_normalized_recipe` / `cached_effective_recipe` / `shadow_cached_normalized_recipe`，每个 `MaterialRecipe` 结构体不定但明显大于 16 B）：它们是 program 解析产物、按共享行存一份。若要减，方向是"缓存搬到独立的解析结果表、行里只留哈希/句柄"—— 这是一次**结构性改动**（准入判据也会跟着改），建议单独评估，不要与本次拍板混在一起。

---

## 五、建议

1. **(a) + (b)**：文档 + debug 门，~15 行，零风险，直接把本条"结案"（它本质上是"命名/文档误导"类问题，而不是"可删冗余"）；
2. **(c) 不做**（明确写进审计档，防下次翻案）；
3. **(d) 单独立项**（若你有减行宽/减内存的需求，我可以单独做一份"配方缓存外置"的设计与量化报告）。

---

---

## 七、落地记录（2026-10-13；选项 (a) + (b) 完成，(c) 不做、(d) 记入「单独立项」）

**(a) 不变量文档**：`MaterialRuntimeRow` 字段旁新增「字段不变量」块 —— 共享行同值 / 自有行 key 为空（以 `row.*` 为准）/
`row.data_index_row` 是**物化标记·发布值**而不是 `key.data_index_row` 的副本（未物化 = `uint32_t(-1)`）/
`cached_effective_recipe_hash` 与 `key.recipe_hash` 是**两个不同的哈希**。

**(b) debug 对拍**：`MaterialRuntimeTable` 两个**静态**助手，**定义在 .cpp**（不是头内 inline ⇒ 破坏/还原 A/B
不受 MSBuild「头内 inline 体扩散到间接调用者」陈旧 obj 陷阱影响），函数体内 `#if ULRE_ECS_DEBUG_API`
（台帐开关，默认 AUTO：Debug 开 / Release 关 ⇒ 生产零开销）：

| 助手 | 调用点 | 判据 |
|---|---|---|
| `VerifyWrittenRecipeHash(row, written, what)` | 解析后写 `row->recipe_hash` 之后 | 共享行：`row.key.recipe_hash == 写入值`；自有行（key 为空）跳过 |
| `VerifyWrittenDataIndexRow(row, what)` | 物化发布 `row->data_index_row` 之后 | 已物化的共享行：`row.data_index_row == row.key.data_index_row`；`-1` 哨兵与自有行跳过 |

**对本文 §四(b) 拟的断言的订正**：原拟的**裸等值** `row.data_index_row == row.key.data_index_row` 会在「未物化」行
**误报**（此时 `row.data_index_row` 是哨兵 `uint32_t(-1)`，与身份行号无关）⇒ 实现里加了哨兵跳过分支后才成立。
这正是 §六 要求「先查清再断言」的那一条 —— 查清结论：两个值确实取自同一个
`material_ssbo_binding.data_index`（`ResolveRowDataIndex(recipe)` vs `entity_data_index`），**只在已物化时**才有可比性。

**「先查清再断言」的实测证据**：接线后跑 4 个材质重示例（`PBRSpheres` / `AlphaModes` / `LoadScene` / `CascadeShadowMap`，
Debug 宏开）+ 18 秒 PBR 稳态窗口（108071 行日志）⇒ 两类不一致告警 **0 条**。

**红的样子（实测）**：把身份键构造里的 `recipe_hash` 改成 `recipe_hash ^ 0x1ull`（键与写入值不同源）⇒ Debug 跑
`PBRSpheres` 打出 **100 条**：

```
[MaterialRuntimeTable] 行 recipe_hash 与身份键不一致（写入点=RenderPrimitiveCollectSystem: 解析后写 recipe_hash）：
key.recipe_hash=9756265579378922461 写入值=9756265578922460（共享行的两者必须同值；自有行不参与）
```

逐字节还原 ⇒ **0 条**。

**契约**：源码契约 **Test 51**（6 条存在性 + 1 条计数「两个助手各自门在 `#if ULRE_ECS_DEBUG_API` 下」+ 1 条禁复活
「对拍不许用 `assert`」—— Release 下即使开着宏，NDEBUG 仍会把它整条抹掉）。

**验收**：Debug/Release `rc=0 / 0 error`；门 45 PASS / 0 FAIL；`ctest -L ecs` 12/12；`TestCSMIncrementalPass` 1/1（含 49/50/51）；
示例帧摘要 `PBRSpheres 0x3DD88A0999CDC486` / `AlphaModes 0x2FE87B15EE8B8A16` / `LoadScene 0xC9E38ED2F49A069B` /
`CascadeShadowMap 0x1CDAADEBF18B7CE4` **全部逐位不变**。

**(c) 不做**（维持原判）：删 `row.recipe_hash` / `row.data_index_row` 必须先解决「自有行 key 为空」这一档，
收益（~16 B/行）远小于风险。
**(d) 单独立项（未开工）**：配方缓存三件套（`cached_normalized_recipe` / `cached_effective_recipe` /
`shadow_cached_normalized_recipe`）外置才是减行宽的真方向，属结构性改动，需单独评估与量化。

---

## 六、未证实 / 需要确认

- `row.data_index_row` 与 `key.data_index_row` 在**共享行**上是否**永远**相同：按 `Intern`（键含 `data_index_row`）+ 物化写回（`RenderPrimitiveCollectSystem.cpp:1287` 一带）的语义应当相同，但我未逐路径证明（例如"先 Intern 后物化"是否会用新行号重建键）——若要 (b) 的断言，需先把这条查清（否则断言可能误报）；
- `recipe_hash` 的"身份配方"与 `cached_effective_recipe_hash` 是否在某些路径上应当相等（若本应相等，说明其中一个写入点漏了归一化 —— 那属于另一条问题）。
