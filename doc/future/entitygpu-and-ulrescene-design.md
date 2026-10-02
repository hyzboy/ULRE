# B8 设计稿：`EntityGPU`（128 B）与 `.ulrescene` 直载 —— 拆分建议 + 拍板点

> 版次：v1（2026-10-02）。**本稿只做设计与拍板，不含代码改动**（用户指定"先出设计与拍板点"）。
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

**现状（实测）**：`Entity` 每实体一份 `hgl::UnorderedMap<std::size_t, std::shared_ptr<Component>>`（`inc/hgl/ecs/core/Entity.h`），
`AddComponent` 逐组件 `make_shared`；`EntityManager::CreateEntity(std::unique_ptr<Entity>)` 逐实体堆分配。
探针基线（`ProbeTransformDiagnostics`，**实测**）：单 `[Entity + TransformComponent]` = **935.3 B / 9.000 次分配**。

**目标形态**：`EntityRecord` 平坦数组（按 `EntityID` 索引）+ 组件**不再各持一个 CPU 对象**。
有利条件：组件的数据真源**已经搬到行/存储**（Transform = `TransformDataStorage` 句柄、`GeometryData` = 行号、
`MaterialData` 的行内只放 POD、重类型在旁表、`MaterialRuntime` = slot）⇒ 今天组件对象多半只是**薄壳**。

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
| **T1** | `EntityGPU` **结构定义**（C++ struct + GLSL 镜像 + `static_assert(sizeof==128)` + 逐字段 `offsetof`）+ `flags` 位表定稿 | 编译期断言过；探针打印 `sizeof`/`offsetof` 与断言一致；**不接线、无消费者**（结构本体 + 断言是"零成本定稿"，不算投机 API） | tag `b8-t1` |
| **T2** | **取消逐 new**（§4，按类型切批：Transform → Geometry → MaterialData → MaterialRuntime） | 探针分配次数与每实体字节达标；掩码/访问器语义对拍逐项等价；固定验证集全绿 | tag `b8-t2-<type>` |
| **T3** | `EntityGPU` **派生表**（世界私有 + `addr_entity_gpu` + 写入路径 + 脏集合上传） | **前置 = 出现第一个真消费者**（届时随消费者同批做）；本期仅登记设计与"五处连带面"清单 | tag `b8-t3` |
| **T4** | `.ulrescene` 直载 | **前置 = (b) 完成**；判据见 §5.2 | tag `b8-t4` |

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
