# B8 设计稿：`EntityGPU`（128 B）与 `.ulrescene` 直载 —— 拆分建议 + 拍板点

> 版次：v1.1（2026-10-02）。**本稿只做设计与拍板，不含代码改动**（用户指定"先出设计与拍板点"）。
> **范围决议（用户 2026-10-02）**：`.ulrescene` 直载（本稿 T4）**本轮不做** ⇒ 本轮范围 = **T1 + T2**；
> T1 落成时进一步收窄为**纯 C++ 结构 + 断言**（GLSL 镜像与派生表本体等第一个真消费者，随 T3 一起做）。
> 关联真源：`doc/future/ULRE_FINAL_TARGET_v2_设计约束.md` §0–§3（总纲/行 arena/句柄语义/EntityGPU）
> + `doc/future/stage_B_execution_plan.md` §0.1（已定输入）§B8（占位）+ `doc/future/stage_B_debt_list.md` §1.3/§1.4/§1.6/§2.3。
> 决断口径：凡与上列真源冲突处，本稿只**提出并标注**，不擅自改真源（改真源需用户拍板）。

---

## 0. 设计输入

### 0.1 已定（不再讨论；逐条给出处）

| # | 已定内容 | 出处 |
|---|---|---|
| 1 | `EntityGPU` = `flags(4) + type[16](16) + row[16](64) + work_flags(4)` = **88 B 载荷** ⇒ `alignas(64)` ⇒ **128 B**；**16 槽** | v2 §3；`stage_B_execution_plan.md` §0.1-5 |
| 2 | 槽位语义 = **`ComponentType` 值**，每类型最多一行 ⇒ 与 `Entity::component_mask`（位掩码）**1:1**；不扩容枚举 | P-3（用户拍板 2026-10-02）；`stage_B_debt_list.md` §2.3 |
| 3 | 五类 GPU 可见组件 = `Transform` / `MaterialData` / `MaterialRuntime` / `Geometry` / `Texture`；`persistent_id` 不引入 | v2 §3；§0.1-4 |
| 4 | **相机 / 阴影 / 可见性等世界级表不进 Entity**（相机 = 世界级 16 槽 × 帧槽，`CameraComponent` 只描述参数） | v2 §3；P-6（用户拍板 2026-10-02） |
| 5 | Entity 槽里存**该 arena 内的行号**；`row == 0` = 无（全仓行 0 哨兵惯例） | v2 §1；P-3 |
| 6 | 句柄语义（非租约）：长生命周期、析构不归还行；失效检测走**行内 revision** | v2 §2；B4-ii 已落地形态 |
| 7 | **T10（容量与行号冻结）必须早于 T11** | §0.1-6 |
| 8 | 中间态纪律五条 + 固定验证集 | §0.1-7 / §0.1-8 |

### 0.2 本稿**订正**（一处，之前我方结论过宽）

**旧说法（本会话前一条消息）**："B8 的触发条件已满足（全类型号齐 + 各类型行/句柄就位）"。
**实际（按 §0.1-6 原文）**：触发条件有**两半** ——

- **(a) 全部组件 ID 化 + 访问器完成** ⇒ **✅ 已满足**（B1–B7 + A2 收口：五类皆有类型号与行/句柄；A7c/A7d 访问器双格式在位）。
- **(b) 「子场景树快速插入 / 展开」设计完成** ⇒ **❌ 未设计、未实现**（`doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md` §9.2/§9.3 口径）。

而 §0.1-6 明写"**T11 + `.ulrescene` 直载已延后**（触发条件 = 子场景树快速插入/展开设计完成）" ⇒
**按字面，B8 整批仍卡在 (b)**。v2 §7 的 T11 行另写"`EntityGPU` = 128 B（非 64 B）+ 取消逐 new；
**场景直载仍等子场景树之后**" ⇒ 两者合读，**可拆**：`EntityGPU` 与"取消逐 new"不依赖 (b)，只有**直载**依赖。
⇒ 本稿据此提出拆分建议（见 §5、拍板点 P-B8-1）。

---

## 1. 目标与非目标

**目标**

1. 明确 `EntityGPU` 的**字段语义**（88 B 逐项）、**落位与归属**、**写者/读者**、**容量与失败模式**；
2. 明确"**取消逐 new**"（T11 的另一半）的目标形态与判据；
3. 给出 `.ulrescene` 直载的**格式轮廓**与它对 (b) 的**最小需求**（把依赖写成接口，未来做 (b) 时不用回头猜）；
4. 把上述拆成**可独立验收的分期**，每期带判据与回滚点。

**非目标**

- 不设计 `EntityGPU` 的**具体 GLSL 消费者**（GPU-Driven 剔除读实体行、GPGPU 场景查询）——那是消费者的批次；
- 不做 HLOD / prefab / `persistent_id`（§0.1-9 明确不做）；
- 不碰 §2.9 的**材质定义级行空间**（纹理引用 / 材质私有数据）——它**装不进** `row[16]`（那是"类型槽 → 行号"，装不下"定义级行号"，见 §2.9 表格），末期同批处理；
- 不改 `.ulrescene` 的**资产加载**侧（glTF 导入链按你的指示不动，未来重写）。

---

## 2. 数据模型：`EntityGPU`（88 B 载荷 + 40 B 填充）

```
struct EntityGPU                       // 128 B 行（alignas(64)）
{
    uint32_t flags;                    //  4 B  实体级标志（见下）
    uint8_t  type[16];                 // 16 B  槽 i = 该槽**是否存在**该类型组件（P-3 A：槽 = ComponentType 值）
    uint32_t row[16];                  // 64 B  槽 i 对应的 arena 行号；0 = 无
    uint32_t work_flags;               //  4 B  工作期派生位（由 type/row 推出）
};                                     // 载荷 88 B + 填充 40 B = 128 B（64 B 对齐的必然结果）
```

### 2.1 逐字段语义

| 字段 | 语义 | 备注 |
|---|---|---|
| `flags` | 实体级状态位。**最小集**：`Active`（槽在用）/ `Visible`（与 CPU 侧 `SetEntityVisible` 同源） | 具体位表在本期 T1 定稿并冻结（`static_assert` + 探针打印） |
| `type[16]` | `type[i] != 0` ⇒ 本实体有 `ComponentType(i)` 的组件 | **冗余观察**：槽号已蕴含类型 ⇒ 该数组恒为「`i` 或 `0`」。保留与否是拍板点 P-B8-3 |
| `row[16]` | `row[ComponentType 值]` = 该类型在 `(scope,type)` arena 里的行号；`0` = 无 | 与既有行池口径一致（`ActiveRowPool` 行 0 哨兵） |
| `work_flags` | 工作期派生（用户定调"只是为了运行时快速确定一些事"）。**本期留 0 并在注释写明"留给消费者定义"** | 不造没人读的位（"0 消费者 = 投机 API"纪律） |

### 2.2 归属：**世界私有**（推荐，理由与相机同源）

`Entity` 属**世界**（每个 `ECSContext` 自带 `EntityManager`，槽表 + 空闲栈都在世界内，`inc/hgl/ecs/core/EntityManager.h`：
`std::vector<EntitySlot> slots` + `std::vector<uint32_t> free_indices`）。若把实体表放进设备级 `GlobalSSBOType`，
多世界同帧会互相覆盖同一批行——**与相机行当年踩过的坑完全同型**（`ulre-ecs-layer` 记的"全局池 + 每世界从 0 开始的语义 id = 跨世界同行冲突"）。

⇒ 推荐落位：**世界地址表加一个字段** `addr_entity_gpu`（GLSL `buffer WorldAddressesRef` 镜像 + 门的两条 parity 用例 +
`kAddressOwnershipTable` 登记 + 夹具，即 `ulre-ecs-layer` 记的五处固定连带面），行空间由世界持有（与 `CameraInfoStorage` 同级）。

---

## 3. 通路：谁写、何时写、谁读

### 3.1 写者（CPU 权威 → 派生视图）

- **CPU 权威**：`Entity`（`id` + `component_mask`）+ 各类型的**行号**（`TransformAccessor` / 行号字段 / `MaterialRuntimeSlot` 等，均已在位）。
- **写入时机**（三处，均为变更驱动，**不每帧全量**）：
  1. 组件挂载/卸载（掩码变更路径 `ReplaceComponent` / `RemoveComponent` / `DetachAllComponents`）；
  2. 行号变更（`Mobility` 换边 = 换段、行迁移等已行一次性告警的路径）；
  3. 实体级标志变更（可见性 / 销毁）。
- **批量上传**：与世界其它 per-frame 数据同批（`SetFrameIndex` 前后的世界同步段），沿用"脏集合 + 一次性写"的既有形态。

### 3.2 读者：**今天为零**

`grep -rn EntityGPU inc/ src/` 只有 `inc/hgl/ecs/core/Entity.h`（掩码注释）与
`inc/hgl/ecs/support/ComponentTypeTable.h`（类型表注释）两处**注释**，无任何代码/类型/字段；
shader 侧当前走 `pc_root` + 批内表，不读实体表。

⇒ **这是本轮最重要的取舍依据**：按你自己反复确立的纪律「**0 消费者 = 投机 API，不做**（等真消费者出现时随需求一起加）」，
`EntityGPU` 表**不应现在建**；它的真消费者是「GPU-Driven 剔除读实体行 / GPGPU 场景查询 / `.ulrescene` 直载」，
三者都还没到。**但**"取消逐 new"（§4）有**独立且可实测**的价值 ⇒ 建议按 §5 拆分。

---

## 4. `EntityGPU` 的另一半：**取消逐 new**（有独立价值，建议先做）

**现状（2026-10-02 重新实测；设计稿 v1 引用的是旧技能记录，已推翻）**：

```
ProbeTransformDiagnostics 的 [T0] 堆分配段（[Entity + Transform(Static)]）：
  n=1000   336.2 B/实体,  6.155 块/实体
  n=10000  313.5 B/实体,  5.818 块/实体
  边际     310.9 B/实体,  5.781 块/实体
sizeof(Entity) = 168 B / sizeof(Component) = 104 B
```

- v1 写的「935.3 B / 9.000 次分配（`[Entity + TransformComponent]`）」是**旧值**：`TransformComponent`
  **已随 T6/T7 整删**（`grep -rn TransformComponent inc/ src/ example/` 全 0 命中）⇒ 基线必须按上表。
- 剩余的逐 new 来源（实测定位）：① `EntityManager::EntitySlot{ std::unique_ptr<Entity> }`（逐实体堆分配）
  ② `Entity` 每实体一份 `UnorderedMap` ③ 仍然存在的组件对象 + 其 `shared_ptr` 控制块 —— **只剩
  `GeometryData / MaterialData / LinesComponent / TextComponent / ShadowProxy`**（`inc/hgl/ecs/components/` 实测清单）。
- **`CameraComponent` 不在取消之列**：相机是**世界级**且相机槽的拥有者是 `weak_ptr<CameraComponent>`
  （`CameraInfoStorage::slot_owners`）⇒ 必须保持 `shared_ptr` 托管；相机数量极少（每世界 1 fallback +
  每个投影阴影灯 1），不构成逐实体开销。

**目标形态**：把逐实体/逐组件的堆分配换成**块式/池式存储**（地址稳定 ⇒ 全仓持有的 `Entity*` 不作废），
组件对象尽量内联为「行号/句柄」薄记录。有利条件：组件的数据真源**已经搬到行/存储**
（Transform = `TransformDataStorage` 句柄（组件对象已删）、`GeometryData` = 行号、`MaterialData` 行内只放 POD /
重类型在旁表、`MaterialRuntime` = slot）⇒ 剩余组件对象多半只是**薄壳**，可内联。

**判据（可自动判定）**

1. 探针：`[Entity + TransformComponent]` 的**分配次数 9.000 → ≤ 2**，每实体字节 **935.3 B → 目标值本期 T1 定稿**；
2. `static_assert(sizeof(EntityRecord) == N)` 冻结 + 探针同源打印（既有纪律）；
3. **同输入同结果对拍**：挂载/卸载/查询/遍历的语义与迁移前逐项等价（掩码位 + 访问器返回值）。

**风险**：`GetComponent<T>()` 链的调用点遍布全仓 ⇒ **按类型切批**（Transform 先行，它已有存储句柄；
再 Geometry / MaterialData / MaterialRuntime），每批自身可编译可验证，**不留 `if(新旧并存)`**。

---

## 5. `.ulrescene` 直载（依赖 (b)，本稿只留接口契约）

### 5.1 对 (b)「子场景树快速插入/展开」的**最小需求**（把依赖写成接口，避免将来返工）

直载 = 对 (b) 的**一次批量调用**，故 (b) 至少需要提供：

1. **批量建实体**：`N` 个实体一次性取得 ID/槽（预分配 + 无逐 new ⇒ 与 §4 同源）；
2. **层级一次性展开**：给定父子关系表 + 每节点 TRS，一次性写入 Transform 行与父子链（不逐节点 `SetParent`）；
3. **失败可回滚**：池耗尽（预算制、fail-fast）时不得留下半截场景；
4. **行号确定性**：同一份输入两次执行 ⇒ **逐行号相同**（这是直载可复现的前提，T10 的行号冻结已就位）。

### 5.2 格式轮廓（供后期细化，不在本批实现）

```
header：magic / version(uint32) / endianness / flags / 段表（offset+size+type）
实体段：EntityRecord[]（掩码 + flags + work_flags）—— 与 EntityGPU 的字段同源但**不含 row[]**（行号由加载分配决定）
arena 段：每个 (scope, type) 一个段：行数组（含行 0 哨兵）+ 该段的容量声明
资源段：资产**键**（路径/资产 ID）—— 实际加载仍走既有资源链（不碰 glTF 导入）
```

**判据（将来落地时）**：① 同场景两次直载 ⇒ 行号逐一相同（dump 对拍）；② 与"作者 API 建同样场景"的结果
**同输入同结果对拍**（逐行字节 + 渲染输出）；③ 版本不认识 ⇒ **显式拒绝并留痕**（fail-fast，不猜测）。

**与 §2.9 的边界**：材质定义级数据（纹理引用配置行 / 材质私有数据池）**不进 128 B**，也不能按 `row[16]` 表达
（`stage_B_debt_list.md` §2.9 的形状对照表已列），直载格式里需为它们留**独立的段**或明确"由材质定义 ID 重建"。

---

## 6. 分期实施（每期：内容 / 判据 / 回滚 tag）

| 期 | 内容 | 判据（可自动判定） | 回滚 |
|---|---|---|---|
| **T1** | `EntityGPU` **结构定义**（**纯 C++ struct** + `static_assert(sizeof==128)` / `alignof==64` + 逐字段 `offsetof` + `flags` 位表）—— GLSL 镜像**不随本期**（无读者 ⇒ 与派生表一起等 T3） | 编译期断言过；探针打印 `sizeof`/`alignof`/四个 `offsetof` 与断言一致；**不接线、无字段读写者**（结构本体 + 断言是"零成本定稿"，不算投机 API） | tag `b8-t1` |
| **T2-a** ✅ | 实体分配改块式（`EntityManager` 的 `unique_ptr<Entity>` ⇒ **块池**，块 = 64 实体，块内地址稳定；构造改就地 placement-new、析构改显式调 `~Entity()`） | ✅ **已完成（2026-10-02）**：`块/实体` 5.818 → **4.834**（边际 5.781 → **4.797**）、`B/实体` 持平（实体对象仍在堆上，只压块数）、`sizeof(Entity)` 168 B 不变；4 个实体生命周期测试 rc=0；PBRSpheres 0 真 error + **像素正常**；探针内置**地址稳定性守卫**（每 64 个边界复核第 0 个 `Entity*`，mismatch 期望 0） | tag `b8-t2a` |
| **T2-b** ✅ | `Entity` 的每实体 `UnorderedMap` ⇒ **小数组 + 线性扫描**（键仍是 `typeid(T).hash_code()`） | ✅ **已完成（2026-10-02）**：`块/实体` 4.834 → **2.834**、`B/实体` 313.4 → **209.5**（−33%）、`sizeof(Entity)` 168 → **112 B**；5 个测试 rc=0；PBRSpheres 像素正常 | tag `b8-t2b` |
| **T2-c** ⚠ | 剩余 5 个堆组件（`GeometryData`/`MaterialData`/`LinesComponent`/`TextComponent`/`ShadowProxy`）⇒ ~~内联薄记录~~ **改为「拆 `Component` 基类 + 类型化行空间/对象池，实体槽只存行号/指针」** | 见「T2-c 前置核实」：内联写法**已被实测否决**（`GeometryData` 120 B 中 104 B 是 `Component` 基类）；判据 = **[T0b] 与 [T0] 的差值**（每个组件对象的开销） | tag `b8-t2c` |
| **T3** | `EntityGPU` **派生表**（世界私有 + `addr_entity_gpu` + 写入路径 + 脏集合上传） | **前置 = 出现第一个真消费者**（届时随消费者同批做）；本期仅登记设计与"五处连带面"清单 | tag `b8-t3` |
| **T4** | `.ulrescene` 直载 | **前置 = (b) 完成**；判据见 §5.2 | tag `b8-t4` |

> **决议（用户 2026-10-02）：T4 本轮不做**（`.ulrescene` 整体推迟）。本轮只做 **T1（已落地：结构 + 断言 + 探针）** 与 **T2**。

> 说明：T1（结构 + 断言）与 T2（取消逐 new）**不依赖 (b)**，可按 §0.1 的纪律立即开工；T3/T4 不满足前置则不进。

---

## 7. 验证矩阵（沿用固定验证集）

| 项 | 命令 / 判据 |
|---|---|
| 构建 | `cmake --build build --config Debug --target <集>` ⇒ `error C` / `error LNK` / `error MSB` 全 0 |
| 门 | `ShaderResourceSchemaRegressionGate` ⇒ **42 PASS / 0 FAIL**（若动世界地址表：新增字段要过 `W.world-addresses-struct-parity` 与 `*-field-ownership`） |
| 测试 | 11 目标 rc=0（含 `TestCSMIncrementalPass` 的源码契约） |
| 探针 | `ProbeTransformDiagnostics` ⇒ `sizeof(EntityGPU)==128`、分配次数/每实体字节与目标值一致 |
| 示例 | 0 真 error + `策略判定对拍` 不一致 0；**涉及绘制/存储布局的批次必须看像素** |
| 行尾/BOM | 逐文件保持（写盘前按 `git show HEAD:<file>` 算目标行尾） |

---

## 8. 风险与默认选择

| # | 风险 | 默认 |
|---|---|---|
| R1 | 实体表**无消费者**却先建 ⇒ 违反"零投机"纪律 | **默认不建**（T3 等消费者）；只有 §4 的"取消逐 new"按独立价值做 |
| R2 | `alignas(64)` 的 40 B 填充（1M 实体 = 40 MB） | **保留**（v2 已定；64 B 对齐 = 单实体命中缓存行）；若要省，见 P-B8-4 |
| R3 | 实体表误放设备级 ⇒ 多世界同行冲突（相机同型坑） | **世界私有**（§2.2） |
| R4 | "取消逐 new" 动全仓 ⇒ 大面积不可编译 | **按类型切批** + 每批对拍 + 不留 `if(新旧并存)` |
| R5 | 直载消耗行池 ⇒ 半截场景 | 预算制 + **fail-fast + 可回滚**（§5.1-3） |
| R6 | `work_flags` / `flags` 先定义后无读者 | 位表定稿但**留 0**，注释写明"留给消费者定义"，不造读取路径 |

---

## 9. 待拍板项（每项：选项 / 推荐 / 影响）

| # | 议题 | 选项 | **推荐** | 影响 |
|---|---|---|---|---|
| **P-B8-1** | B8 是否拆分 | ① 整批等 (b) ② **拆分：T1+T2 先做，T3/T4 挂前置** | **②** | 拆分才能让"取消逐 new"这块独立价值落地；不拆则整批被 (b) 卡死 |
| **P-B8-2** | `EntityGPU` 表落位 | ① 设备级 `GlobalSSBOType` ② **世界私有（`WorldAddresses.addr_entity_gpu`）** | **②** | 实体属世界；①会重演相机"跨世界同行冲突" |
| **P-B8-3** | `type[16]` 是否保留（槽号已蕴含类型 ⇒ 冗余） | ① **保留**（与 v2 §3 一致；shader `if(type[i])` 直观） ② 去掉（省 16 B，改判 `row[i]!=0`） | **①**（并注明冗余） | ②更省，但改 v2 已定形状且 shader 判空要改用 `row` |
| **P-B8-4** | 对齐 | ① **保持 `alignas(64)` ⇒ 128 B** ② 收紧到 16 B ⇒ 96 B（省 25%，跨缓存行） | **①**（v2 已定） | ②在 1M 实体上省 32 MB，但随机访问可能跨行 |
| **P-B8-5** | "取消逐 new" 的粒度 | ① **按类型切批（Transform 先行）** ② 一次性全换 | **①** | ②一次性动全仓调用点，回归面过大 |
| **P-B8-6** | 实体表容量预算 | ① 4096（与 Transform static 同口径） ② 16384（与 MeshDrawParams 同口径） | **①** | 预算制 + 超限 fail-fast；数值可后续按实测调，但**一旦冻结即存档语义** |
| **P-B8-7** | `.ulrescene` 版本策略 | ① **显式 version + 未知即拒绝** ② 兼容旧版读取 | **①** | 零兼容纪律；②会引入兼容分支 |

---

## 9.5 T2 完成记录

### T2-c 前置核实（2026-10-02）—— 「内联」写法被数据否决 ⚠

动手前实测三件事，结论推翻设计稿 v1 的 T2-c 写法：

1. **`GeometryData` 实测 = 120 B，其中 104 B 是 `Component` 基类**（名字/vtable/槽位），真正载荷只有
   `const PrimitiveAsset*`(8 B) + `uint32_t`(4 B) = **12 B**。⇒ 把组件「内联进 `Entity`」会给**每个实体**
   （含无几何的）**+120 B**，探针 `B/实体` 直接劣化 ⇒ **不采用**。
2. **成本在基类，不在分配**：`make_shared` 是 1 块（对象 + 控制块合并），而 `Component` 基类带着
   `std::string name` + 多个虚函数 ⇒ 收益只能来自**拆掉基类**（类型化数据行 + 实体存行号/指针），而非内联。
**实测（2026-10-02，探针 [T0b] − [T0]，同 n=10000 同流程，只多挂一个 `GeometryData`）**：

| | B/实体 | 块/实体 |
|---|---|---|
| `[T0]` `[Entity + Transform]` | 209.5 | 2.834 |
| **差 = 每个 `GeometryData` 组件的真实开销** | **+195.4 B** | **+3.000 块** |

⇒ **载荷 12 B、壳 183 B（95%）**，且**每个组件 3 次分配**（不是 1 次 `make_shared`）——多出来的应是
实体侧 `vector<ComponentEntry>` 增长 + 组件向 `Context` 的注册。**这就是 T2-c 的靶子，也是验收判据**
（目标：差值压到「1 块/64 个」的摊薄量级）。

3. **探针原本量不到 T2-c**：`[T0]` 只建 `Transform`（无任何其他组件）⇒ 新增 **[T0b]
   `[Entity + Transform + GeometryData]`**（同 n、同流程，只多挂一个 `GeometryData`），
   打印「与 [T0] 同 n 的差值 = 每个组件对象的真实开销」作为 T2-c 的量化判据。

⇒ **T2-c 定形（待拍板）**：组件本体从「`Component` 派生类 + `make_shared`」改为**类型化行空间/对象池**
（块式，1 块/64 个，与 T2-a 同手法），实体槽只存行号或裸指针；随之处理 `Component` 基类的既有使用者
（`GetName()` 调试日志、`OnDetach()`、`GetComponentSlot()`、`GetSystemGroup()`）。
**相机例外不变**：`CameraComponent` 需保持 `shared_ptr`（相机槽拥有者 `weak_ptr<CameraComponent>`）；
用户 2026-10-02 追加定案：**相机只在 CPU 端出现、不进 `EntityGPU`**（一个场景不会同时用一堆相机，CPU 算得过来）。

**T2-c 定界（用户 2026-10-02）—— 「存储统一 ≠ 渲染/收集统一」**：

- `LinesComponent`：**全场景收集、最后一次绘制** —— `LineCollectSystem` → `LineRenderPipeline::RunCollect()`
  用 `context_->GetComponents<LinesComponent>(all)` 遍历**整个场景**收进 `collected_`，再由 `LineRenderSystem`
  一次 draw；**不共享** Mesh 的批/实例化逻辑。
- `TextComponent`：**2D 专用**，未来 2D 单开一套管理 ⇒ **现在不得为它做任何特化/深度绑定**，保持最小耦合
  （将来整条搬走时越少牵连越好）。
- ⇒ T2-c **只统一存储**（实体槽句柄 + 类型化对象池 + 池即活跃集），**不碰**这两条路径的收集与绘制语义；
  它们的收集调用点只从「`shared_ptr` 列表」改成「按类型的活跃集视图（裸指针）」。
- **跨切面警告**：`GetComponents<T>(std::vector<std::shared_ptr<T>>&)` 这个**带 `shared_ptr` 的收集 API 形态**
  是当年 `shared_ptr` 留在组件上的原因之一；T2-c 改组件存储 ⇒ 必须**同批**更换它的全部调用点
  （`LineRenderPipeline.cpp`、`LineBoundsUpdateSystem.cpp`、`TextRenderPipeline` 对应处），否则就是新旧两套并存。

**① 已落地（2026-10-02）—— `Lines` / `Text` / `ShadowProxy` 登记 CPU-only 类型号**：

- 枚举追加 3 项（只能追加：类型值被位掩码与 `EntityGPU.type[16]` 消费）；
- 类型表 3 行：`ComponentScope::World`、**`gpu_row_bytes = 0`**、承载类型 = `LinesComponent` / `TextComponent` / `ShadowProxy`；
- `kComponentTypeImplies` 3 条（对齐枚举，漏填会静默零初始化）；
- 3 个组件头的 `ComponentTypeOf<T>` 特化（与 `GeometryData` / `MaterialData` / `Camera` 同范式）；
- **Test 41**（13 条契约）：四件套 + 决策钉子；**行宽列（第 4 列）被编码进 needle** ⇒ 任何"把这三种升级成
  GPU 行"的改动都会让用例变红；并钉住「文本 / 线段 = 动态 GEOMETRY，以后再说」的定界**未翻案**。
- 实测：门 **42 PASS / 0 FAIL**、`TestCSMIncrementalPass` + `TestComponentMask` rc=0。
- 为什么做这步：这三种类型此前**没有类型号**（`ComponentTypeOf` 无特化 ⇒ 都落 `None`），而类型号是
  (scope,type) 台账、实体侧掩码与将来「按类型句柄」的共同前提 ⇒ **先登记号（CPU-only）**；
  **是否进实体侧池另说** —— 按上面的定界，Mesh 侧（`GeometryData`/`MaterialData`/`ShadowProxy`）进池，
  `Lines`/`Text` 只提供"按类型的活跃集视图"给各自的收集路径。

### T2-b（2026-10-02）—— 每实体哈希表 ⇒ 小数组 + 线性扫描 ✅

- **⚠ 设计稿 v1 的写法不可行（记录在此，避免后人照做）**：原文写「按 `ComponentType` 的固定 7 槽数组」——
  实测**不成立**：`LinesComponent` / `TextComponent` / `ShadowProxy` **没有类型号**（`ComponentTypeOf<T>` 无特化
  ⇒ 都落到 `ComponentType::None` ⇒ 全挤槽 0 冲突），而给它们补类型号会与「文本/线条属动态 GEOMETRY，以后再说」
  的定案冲突、并污染 (scope,type) 台账；同时 `CameraComponent` 必须保持 `shared_ptr`（相机槽拥有者是
  `weak_ptr<CameraComponent>`）。⇒ 索引键**仍用 `typeid(T).hash_code()`**（语义与旧版逐字一致）。
- **落成形态**：`std::vector<ComponentEntry>`（`{ std::size_t type_hash; std::shared_ptr<Component> component; }`）
  + 三个私有助手（`FindComponentEntry` ×2 / `EraseComponentEntry`，删条目用**尾替 + pop_back**，顺序无关 ⇒ O(1) 不留空洞）。
  组件数实测 ≤4 ⇒ 线性扫描比哈希更快，且省掉哈希表**每次插入的桶数组 + 节点**两次分配。
- **实测（探针 [T0]，`[Entity + Transform(Static)]`）**：
  | 指标 | T2-a 后 | T2-b 后 |
  |---|---|---|
  | 块/实体（n=10000） | 4.834 | **2.834** |
  | 边际 块/实体 | 4.797 | **2.797** |
  | B/实体 | 313.4 | **209.5**（−33%） |
  | `sizeof(Entity)` | 168 B | **112 B** |
- **验证**：5 个测试 rc=0（`TestCSMIncrementalPass` / `TestComponentMask` / `TestTransformFlatStorage` /
  `TestRenderItemDataStorage` / `TestVisibilityStorage`）；`PBRSpheres` 0 真 error + **像素与 T2-a 一致**；
  地址稳定性守卫仍 `mismatch=0`。
- **剩余 2.8 块/实体**：实体对象 1 块（每 64 个共享）+ 组件对象各 1 块（`make_shared` 自身）+ 注册/侧表 ⇒ 下一刀是 **T2-c**
  （剩余堆组件 `GeometryData`/`MaterialData`/`LinesComponent`/`TextComponent`/`ShadowProxy` ⇒ 内联薄记录）。

### T2-a（2026-10-02）—— 实体分配改块池 ✅

- **改法**：`EntityManager::EntitySlot` 的 `std::unique_ptr<Entity>` ⇒ **裸指针（不拥有）** +
  新增块池 `std::vector<std::unique_ptr<std::byte[]>> blocks`（块 = `64` 个实体，`kEntityBlockSize`）；
  `CreateEntity<T>` 由「`Context` 里 `make_unique` + `std::move`」改为**块内就地 `placement new`**
  （上移为头内模板，**签名与 ~140 个调用点零改动**；`static_assert(T == Entity)` —— 实测全仓无 Entity 子类）；
  `DestroyEntity` / `Clear` 改为**显式调 `~Entity()`**（块池不拥有对象），`Clear` 顺带 `blocks.clear()`。
  槽复用 + `generation` + `free_indices` + `ExpandSlots` 的扩容策略**逐字保留**（本批只换分配形态）。
- **实测（探针 [T0]，`[Entity + Transform(Static)]`）**：
  | 指标 | T2-a 前 | T2-a 后 |
  |---|---|---|
  | 块/实体（n=10000） | 5.818 | **4.834** |
  | 边际 块/实体 | 5.781 | **4.797** |
  | B/实体 | 313.5 | 313.4（持平） |
  | `sizeof(Entity)` | 168 B | 168 B |
- **剩余块数说明**：4.8 块/实体 ⇒ 大头不在实体分配（每实体 `UnorderedMap` 的桶 + 注册/侧表）⇒ 正是 **T2-b** 的靶子。
- **验证**：4 个实体生命周期测试（`TestCSMIncrementalPass` / `TestComponentMask` / `TestTransformFlatStorage` /
  `TestRenderItemDataStorage`）全 rc=0；`PBRSpheres` 0 真 error + **像素正常**（10×10 内建几何完整）。
- **守卫**：探针新增「地址稳定性」检查 —— 建实体的循环里**每 64 个边界**复核第 0 个 `Entity*` 是否仍由 ID 解析回同一地址
  （块池扩容若移动对象，全仓 `Entity*` 立即悬垂）。这是本批唯一的结构性不变量。

## 10. 变更记录

| 旧说法 | 新结论 | 原因 |
|---|---|---|
| 本会话前一条消息："B8 触发条件已满足（全类型号齐 + 行/句柄就位）" | **只满足一半**：(a) 已满足，(b)「子场景树快速插入/展开」未做 ⇒ §0.1-6 字面上 T11 与直载同为延后 | 只读了欠账 §1.4 的"前提是全部组件 ID 化 + 访问器完成"，漏了 v2 §7 的 T11 行与 §0.1-6 的第二个前置 |
| （无）T11 是否与直载同批 | **建议拆**：`EntityGPU`(T1) 与"取消逐 new"(T2) 不依赖 (b)；只有直载(T4)依赖 | v2 §7 的 T11 行与 §0.1-6 合读得出，属需用户确认的拆分（P-B8-1） |

---

## 11. 附录：关键落点索引（符号级，不用行号）

- `EntityGPU` 现状：`inc/hgl/ecs/core/Entity.h`（`component_mask` 注释）、`inc/hgl/ecs/support/ComponentTypeTable.h`（类型表注释）
- 实体存储：`inc/hgl/ecs/core/EntityManager.h`（`slots` + `free_indices`）、`Entity.h`（`components` map + `make_shared`）
- 行池与预算：`inc/hgl/graph/ssbo/SSBOTypes.h`、`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp`（`kGlobalSSBOConfigs`）
- 世界地址表：`inc/hgl/graph/ubo/WorldAddresses.h`、`ShaderLibrary/ubo/scene_ubo.glsl`（`WorldAddressesRef`）、
  门用例 `W.world-addresses-struct-parity` / `W.world-addresses-field-ownership`、`kAddressOwnershipTable`
- 探针：`src/ecs/support/ProbeTransformDiagnostics.cpp`（每实体字节 / 分配次数）
- 五处连带面（改世界/全局地址表的固定动作）：见技能 `ulre-ecs-layer`
