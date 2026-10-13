# §2.2 问题 7 详述：材质四条键空间，且 `MaterialVariantKey` 是 `MaterialRuntimeKey` 的严格前缀

> **审计出处**：`doc/ecs-quality-audit-2026-10.md` §2.2「并行表家族」第 7 条（原文："材质四条键空间：variant(2 元组) / runtime(3 元组) / authoring(row id) / pipeline(EntityID) 描述同一实体的材质态；且 `MaterialVariantKey` 是 `MaterialRuntimeKey` 的严格前缀"）
> **结论一句话**：**前缀关系成立**（我核了定义，前两维逐字相同）；四条键空间也各自服务不同消费者，**不该合并**。真正的问题只有两条：**前缀关系没有共用实现**（加一维要改两处、哈希前缀重复计算），以及**四条键的"谁在哪个解析步产生、谁是权威"没有写在一处**。

---

## 一、四条键空间的定义（含证据行）

### ① 变体键：`MaterialVariantKey` —— 2 元组，"program 身份"

`inc/hgl/ecs/support/MaterialVariantTable.h:93-103`：

```cpp
struct MaterialVariantKey
{
    uint64_t program_build_context = 0;   // ① primitive_type + 顶点格式 + 设备 profile + purpose
    uint64_t recipe_hash           = 0;   // ② 本次解析所用 effective recipe 的 HashMaterialRecipe

    bool operator==(const MaterialVariantKey &rhs) const noexcept
    { return program_build_context == rhs.program_build_context && recipe_hash == rhs.recipe_hash; }
};
```

注释把"两维都决定 program 身份"的理由写清了：缺 ① 分不清 purpose/图元/顶点格式/设备 profile；缺 ② 同 build context 下不同材质会互相覆盖。

### ② 运行期键：`MaterialRuntimeKey` —— 3 元组，"共享行身份"

`inc/hgl/ecs/support/MaterialRuntimeTable.h:86-105`：

```cpp
struct MaterialRuntimeKey
{
    uint64_t program_build_context = 0;   // program 身份（build context 哈希）
    uint64_t recipe_hash           = 0;   // 材质配方身份
    uint32_t data_index_row        = 0;   // 材质 SSBO 行号（授权态的"自有 SSBO 行"维度；无绑定记 0）

    bool operator==(const MaterialRuntimeKey &rhs) const noexcept
    { return program_build_context == rhs.program_build_context
          && recipe_hash == rhs.recipe_hash
          && data_index_row == rhs.data_index_row; }
};
```

⇒ **前两维与 ① 逐字相同、类型也相同** ⇒ `MaterialVariantKey` 是 `MaterialRuntimeKey` 的**严格前缀**（差第三维 `data_index_row`）✓ 审计说法成立。

### ③ 授权态：`MaterialAuthoringSideTable` —— 按 **row id**

`inc/hgl/ecs/support/MaterialAuthoringSideTable.h:55`（类）+ 审计 §2.2 提到"材质行 0 作'无材质'哨兵"（`:91-95`）。这一条是**授权侧**（CPU 作者流程写的 SSBO 行），与 ①② 的"program/配方身份"不是同一维度。

### ④ 管线缓存：`MaterialRuntimePipelineTable` —— 按 **EntityID（+pass）**

`inc/hgl/ecs/support/MaterialRuntimePipelineTable.h:32-64`：

```cpp
void Set(EntityID id, graph::RenderPass *pass, graph::Pipeline *pipeline);
graph::Pipeline *Get(EntityID id, graph::RenderPass *pass) const;
std::unordered_map<EntityID, Entry> entries;
```

这一条是**每实例**（不是每键）的解析产物缓存。

### 键的解析点（谁产生）

- ② 的构造：`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:1247`（`const MaterialRuntimeKey runtime_key{…}`）、`:935`（`MaterialRuntimeKey{forward_build_context, recipe_hash, …}`）；`MaterialRuntimeTable::Intern(key)`（`MaterialRuntimeTable.cpp:85`）。
- ② 的校验：`MaterialRuntimeTable.cpp:103-108` 拒绝"含每帧/每实例维度"的键（注释："请检查是否有每帧/每实例维度被误加入 `MaterialRuntimeKey`"）—— 说明**键的纪律有明确边界**，这是好设计。
- 每张键各有自己的 hash functor（`MaterialVariantKeyHash`、行键 hash；都走 FNV1a 折字段）。

---

## 二、问题（三条，都可证）

### 2.1 前缀关系**没有共用实现**（真正的机械问题）

两处各自：声明结构体、各自写 `operator==`、各自写 hash functor（`h << key.program_build_context << key.recipe_hash`）。前缀这件事**只存在于人的脑子里**：

- 若明天要给"配方身份"再加一维（例如 material purpose 细分的第二维），必须**同时**改 `MaterialVariantKey` 与 `MaterialRuntimeKey` 的结构、`==`、hash —— **漏一处就静默分叉**（变体表与运行期表对同一实体算出不同身份）；
- 哈希前缀被**重复计算**（同一 `program_build_context`/`recipe_hash` 在两次哈希里各折一遍）—— 量级微不足道，但它是"两份实现"的证据。

### 2.2 四条键的"权威与产生步"没有写在一处

现在要知道"这个实体的材质态由谁决定"，需要分别在四个文件的头注里拼：

| 键 | 权威/产生 | 消费 |
|---|---|---|
| ① variant | collect 解析 program 时（build context + effective recipe hash） | program 缓存（`ShaderProgramManager` 持有 program 本体） |
| ② runtime | collect 物化材质行之后（`Intern`） | 共享行（绑定状态：变体 ID / SSBO 行 / 纹理配置 / 配方缓存） |
| ③ authoring | 作者流程写材质 SSBO 行 | 授权行（`data_index_row` 的来源） |
| ④ pipeline | 每实例每 pass 的管线解析结果 | 绘制时取已解析管线（复用） |

审计点这条的理由正是"同一实体的材质态被四种键描述"——**语义上确实是同一件事的四个投影**，但**没有一张图把四者的先后顺序画出来**（哪个先产生、哪个从哪个派生）。新人（或下次审计）很容易把它们当"四套重复的键"而想去合并。

### 2.3 "无对拍"（与 #1/#2/#3 同族）

没有任何断言/门在检查"① 的前两维 == ② 的前两维"。前缀关系一旦被改坏，症状是**材质串味**（不同材质共用 program 或反过来互相覆盖），非常难定位。

---

## 三、为什么不该合并四条键空间

| 若合并成一条会怎样 | 后果 |
|---|---|
| variant + runtime 合一条 | variant 是**静态且有界**的（文件头"键的规矩"），runtime 带 `data_index_row`（授权态、每实例可变维度）⇒ 合并会让"变体表"随授权行数增长、缓存命中率崩（同一 program 被拆成多份） |
| runtime + authoring 合一条 | authoring 是**行**空间（row id），runtime 是**身份**空间（interned 键）⇒ 合并会把"共享"降级成"每行一份" |
| runtime + pipeline 合一条 | pipeline 是**按 (EntityID, pass)** 的解析产物缓存（每实例每 pass），runtime 是按身份共享的 ⇒ 合并会让共享行爆炸成每实例一份 |

⇒ 结论：**四张表都该保留**；要收敛的是"前缀的共用实现"和"权威链的文档"。

---

## 四、处置选项

### 选项 (a)：前缀共用实现（推荐，成本低、收益明确）

在 `MaterialRuntimeTable.h`（或一个共用小头）里给出：

```cpp
/// 变体键 = 运行期键去掉"授权 SSBO 行"维 —— 前缀关系**唯一实现**
inline MaterialVariantKey VariantKeyOf(const MaterialRuntimeKey &k)
{ return MaterialVariantKey{ k.program_build_context, k.recipe_hash }; }
```

并让 `MaterialVariantKey` 的构造/比较/哈希都从这一份派生（例如给两键共用一个 `HashProgramAndRecipe(context, recipe_hash)` 折字段函数）。收益：加一维只改一处；前缀关系**从口头变成代码**。

成本：低（~20 行）；风险：低（纯机械等价改写，可用一个 `static_assert`/契约测试钉"两键前两维逐字相同"）。

### 选项 (b)：把四条键空间的权威链写进头注（零风险，建议同批做）

在 `MaterialRuntimeTable.h` 文件头画一张 4 行小表（本报告 §2.2 那三列表），并明确："①③ 是输入、② 是共享行身份、④ 是每实例解析产物"。

### 选项 (c)：加一条对拍契约（低成本，把前缀关系钉住）

源码级契约（`TestCSMIncrementalPass` 风格）或运行时断言：`VariantKeyOf(runtime_key) == variant_key` 在 collect 的两个构造点成立。成本 ~20 行；收益 = 前缀被改坏时立刻红。

---

## 五、建议

1. **(a) + (b) + (c) 一起做**，一批 ~50 行，全是"收敛 + 文档 + 门"，不碰语义；
2. 明确**不做**的事：不合并四张表（理由见 §三）；
3. 如果将来要给"配方身份"加维度，规则应写成："**先改 `MaterialRuntimeKey`，再由 `VariantKeyOf` 自动跟随**；若新维度不影响 program 身份，则不得进前缀"——这条规则值得写进头注（它正是审计 §2.2 那条"前缀"的用途）。

---

---

## 七、落地记录（2026-10-13；选项 (a) + (b) + (c) 一批做完）

**(a) 前缀共用实现（唯一一份）**

| 件 | 落点 |
|---|---|
| program 身份前缀的**折字段** | `HashMaterialProgramIdentity(program_build_context, recipe_hash)`，落在 `MaterialVariantTable.h`（两键都能看到它）；两处 hasher 都经它 |
| 前缀的**唯一实现** | `MaterialRuntimeTable.h` 的 `inline MaterialVariantKey VariantKeyOf(const MaterialRuntimeKey &k) noexcept` |
| **生产接线** | `RenderPrimitiveCollectSystem` forward 路径：先构造 `runtime_key`，再由 `variant_table->Intern(VariantKeyOf(runtime_key))` **派生**变体键（原来两处各手抄两维）|

⚠ **哈希取值未变**：运行期键哈希 = 以 `HashMaterialProgramIdentity(...)` 的结果为**种子**继续折第 3 维
（`FNV1aHasher64(seed)` 的续算语义）⇒ 抽取前后**逐位相同**，不造成任何缓存失效（实测 PBR 帧摘要逐位不变）。

**(b) 权威链头注**：`MaterialRuntimeTable.h` 文件头新增四行表（① variant 2 维 / ② runtime = ① + 自有 SSBO 行 /
③ authoring 行 id 空间 / ④ pipeline 每实例每 pass）+ 先后顺序（③⇒①⇒②⇒④）+ 两条规矩
（"加配方身份维度先改 `MaterialRuntimeKey`，`VariantKeyOf` 自动跟随；不决定 program 身份的维度不得进前缀"）。

**(c) 前置的编译期钉死 + 源码契约**

- **5 条 `static_assert`**（`MaterialRuntimeTable.h`）：前两维**类型**逐字相同、**偏移**逐字相同、第 3 维排在**前缀之后**
  ⇒ 「只给一个键加维」这种漂移**立刻编译失败**（这正是报告 §2.1 担心的"漏一处静默分叉"）。
- **源码契约 Test 50**（`TestCSMIncrementalPass`）：4 条存在性（共用折字段 / `VariantKeyOf` / static_assert / 权威链头注）
  + 1 条生产接线（`VariantKeyOf(runtime_key)`）+ **2 条禁复活**（两处 hasher 都不许再手写前缀折字段）。

### 红的样子（两条，实测）

1. **只给 `MaterialVariantKey` 加一维** ⇒ 编译期即红：
   `error C2338: static assertion failed: '第 3 维必须排在前缀之后（它是唯一的后缀维；插到前面就不成前缀了）'`；逐字节还原 ⇒ rc=0 / 0 错。
2. **变体 hasher 改回手写折字段** ⇒ `Test 50 Failed: 禁复活：变体键哈希不许再手写前缀折字段 …（禁复活）`，退出码 **50**；逐字节还原 ⇒ 退出码 0。

### 验收

两配置 `rc=0 / 0 error`；门 45 PASS/0 FAIL；`ctest -L ecs` 12/12；`TestCSMIncrementalPass` 1/1（含 Test 49/50）；
Debug 帧摘要 `PBRSpheres 0x3DD88A0999CDC486`（材质最敏感用例）/ `LoadScene 0xC9E38ED2F49A069B` /
`GPUDrivenCull 0x90E0745C2C7B8599` **三项逐位不变**；示例日志无 `variant unavailable`/覆盖类告警。

### 对 §六 待证点的就地订正

- ~~① 与 ② 的实际取值是否在运行时永远满足前缀~~ ⇒ **只有 forward 变体键是运行期键的前缀**（同一
  `build_context_hash` + `recipe_hash`，已改为**由构造派生**）。
  **阴影变体键不是前缀**：它用 `ShaderProgramPurpose::ShadowDepth` 的 build context，而同一实体的运行期行键用
  **forward** 的 build context ⇒ 阴影键是它的**兄弟键**（同 recipe、不同 purpose）。代码里已加显式注释钉住这一点
  （防日后有人"顺手"把阴影侧也改成 `VariantKeyOf`）。
- ~~③ 的 row id 与 ② 第三维是否同一空间~~ ⇒ 已在权威链表里写明：`data_index_row` 就是 ③ 那个行号。
- **不做**（维持 §三 原判）：不合并四张表。

---

## 六、未证实 / 需要确认

- ① 与 ② 的实际取值是否在运行时永远满足前缀（我核的是**定义**；要证"取值一致"需要一条对拍，即选项 (c)）；
- `MaterialAuthoringSideTable` 的 row id 与 `MaterialRuntimeKey::data_index_row` 是否**同一空间**（若是，则 ③ 与 ② 的第三维是同源的两个投影，这点值得在 (b) 的表里写明；按当前读到的注释，`data_index_row` 是"授权态的'自有 SSBO 行'" ⇒ 很可能是同一空间，但我未逐条验证）；
- 四张表各自的**生命周期**（谁先死、世界销毁顺序）——审计 §2.2 未列，属"内存布局/销毁顺序"族，若与本次拍板无关可留待后续。
