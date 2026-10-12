# §2.2 问题 1 详述：`Entity::component_mask` 是 `component_rows` 的"缓存视图"，但对拍器生产零调用

> **审计出处**：`doc/ecs-quality-audit-2026-10.md` §2.2「并行表家族」第 1 条
> **结论一句话**：掩码（"本实体有哪些组件"的第二份）在全仓的**生产读者只有 1 处**，而它唯一的对拍器 `VerifyComponentMaskAgainstComponents()` **生产零调用**（只在 `TestComponentMask.cpp`）⇒ "缓存视图"这条不变量目前是**口头约束**，没有任何门在守。

---

## 一、现状（含证据行）

### 1.1 两份数据 + 一条不变量

| 位置 | 内容 | 角色 |
|---|---|---|
| `inc/hgl/ecs/core/Entity.h:37` | `uint32_t component_mask = 0;` | **派生视图**（缓存） |
| `inc/hgl/ecs/core/Entity.h:42` | `uint32_t component_rows[COMPONENT_TYPE_COUNT] = {};` | **唯一真源**（1 基行号；0 = 无） |

字段自己的注释（`Entity.h:33-40`）写明：

> 组件槽位位掩码……**单一写者 = 组件挂载/卸载路径**。T2-c 起它是 `component_rows` 的**缓存视图**（`row != 0` ⇔ 该位为 1），供 O(1) 策略判定；**两者一致由 `VerifyComponentMaskAgainstComponents` 对拍**。

### 1.2 唯一写者：`Entity::SetComponentRow`

`src/ecs/core/Entity.cpp:33-47` —— 写行号表**并且**同步掩码位（两件事在同一函数里，所以当前实现不会漂移）：

```cpp
const uint32_t old = component_rows[index];
component_rows[index] = row;
if (row) component_mask |=  ComponentTypeBit(type);
else     component_mask &= ~ComponentTypeBit(type);
return old;
```

### 1.3 对拍器：写了，但没人调

`src/ecs/core/Entity.cpp:49-59`：

```cpp
bool Entity::VerifyComponentMaskAgainstComponents() const
{
    uint32_t expected = 0;
    for (uint32_t i = 1; i < COMPONENT_TYPE_COUNT; ++i)
        if (component_rows[i]) expected |= (1u << i);
    return expected == component_mask;
}
```

调用点统计（`grep -rn VerifyComponentMaskAgainstComponents inc/ src/`）：

| 位置 | 性质 |
|---|---|
| `Entity.h:35,65` | 注释 + 声明 |
| `Entity.cpp:50` | 定义 |
| `TestComponentMask.cpp:56,90,105,119,162,170` | **测试**（同一个用例里对拍 4 次） |

⇒ **生产 0 调用**。

### 1.4 掩码的生产读者：全仓只有 1 处

| 读者 | 位置 | 说明 |
|---|---|---|
| `GetComponentMask()` | `Entity.h:59` | 定义 |
| ↳ 生产 | `src/ecs/support/PrimitiveBatchPipeline.cpp:616` | `EvaluateRenderNeed(prim_entity->GetComponentMask(), …)` —— 策略判定的输入 |
| ↳ 测试 | `TestComponentMask.cpp`（多处）、`TestRenderStrategyTable` 构造掩码 | |
| `HasComponentType(type)` | `Entity.h:62` → `ComponentMaskHas(mask,type)` | **生产零调用者**（`grep HasComponentType(` 在 `inc/`+`src/`+`example/` 的非测试文件里只剩定义与两处注释） |

消费侧的语义在 `inc/hgl/ecs/support/RenderStrategyTable.h:202-210`：`EvaluateRenderNeed(component_mask, fact_mask)` 内部第一件事是 `ExpandComponentImplies(component_mask)`（implies 闭包，§2.3 已抽成公共件）⇒ **掩码是策略判定的"输入形状"**，而不是随便一个缓存。

### 1.5 代价与收益的量比

- 收益：把 `component_rows` 的"有没有"压成一次 `u32` 读 ⇒ `EvaluateRenderNeed` 的输入只要一次加载（每帧每个进入批的 item 一次，`PrimitiveBatchPipeline.cpp:616`）。
- 代价：每个实体多 4 B（在 `Entity` 里，`ComponentType` 数 ≤ 16 ⇒ 掩码还可能上 u16）；外加**一份必须与真源同步的状态**（漂移后果 = 策略判定错 ⇒ 该画的没画 / 不该画的画了，且**无任何日志**）。
- 关键量比：**1 个生产读者** vs **1 份必须同步的副本 + 1 个没人调的对拍器**。

---

## 二、为什么现在没出事

漂移的唯一入口是 `SetComponentRow`（挂载/卸载路径），而它在同一个函数里既写行号表又改掩码位 ⇒ 只要**所有人都经过它**，掩码就是对的。风险不在当前代码，而在**下一个改挂载/卸载路径的人**：

- 若新增一条"直接写 `component_rows`"的路径（或批量挂载的快捷路径），掩码不会自动跟随 ⇒ 静默漂移；
- 若有人写 `HasComponentType()`（目前零调用，但它是公开 API）⇒ 他会依赖这份缓存，而没有任何门保证它是对的。

同族的第二个实例（同类问题、同一个模式）：`EntityGPUTable::VerifyAgainst(entity, out_mismatch_type, out_mismatch_row)`（`inc/hgl/ecs/support/EntityGPUTable.h:222-255`）也是对拍器，**同样零调用者**（详见问题 2 的报告）。两个对拍器都写了、都没人调 ⇒ 这不是巧合，是"门写好了但没接进生产"的同一个习惯。

---

## 三、处置选项

### 选项 (a)：保留掩码，在**唯一写者**处做 debug 期对拍（推荐先做）

- 落点：`Entity::SetComponentRow` 末尾（或在挂载/卸载的调用点），`#if ULRE_ECS_DEBUG_API` 下 `VerifyComponentMaskAgainstComponents()` 不成立即 `GLogError` 点名（实体名 + `type` + 期望/实际掩码）。
- 成本：~5 行；**生产零开销**（挂载/卸载不是热路径，且开关默认关）。
- 收益：把"缓存视图"从口头约束变成 **debug 期 fail-fast**；漂移会在下一次跑 debug 示例时立刻点名，而不是在某个帧里静默少画一个物体。
- 风险：零（默认关；打开只增加打印）。
- 验证：破坏验证 —— 临时抽掉 `component_mask |= ComponentTypeBit(type)` ⇒ 期望 debug 下点名失败；还原后复绿。

### 选项 (b)：删掩码，策略入口改收实体/行号表

- 形态：`EvaluateRenderNeed` 改收 `const Entity &`（或 `const uint32_t *rows`）内部现场拼掩码（O(16)），或让 collect 在**边界**拼一次随 `RenderItem` 传下去。
- 收益：少 4 B/实体 + **彻底消除第二份**（问题 1 与问题 2 是同一张"哪些组件"的两份副本，合起来看更值）。
- 成本：改策略入口签名（`PrimitiveBatchPipeline.cpp:616` 一处生产调用）+ 每次判定多 ~16 次读（在 collect 热路径，item 数量级 = 每帧可绘制实体数）。
- 风险：中（动的是策略判定输入；implies 闭包/规则表口径不能变）。需在改后用 `TestRenderStrategyTable` + `TestComponentMask` + 示例对拍证明"判定结果逐项不变"。

### 选项 (c)：掩码作"边界派生量"

(b) 的落地形态：在 collect 拿到实体时算一次掩码放进 `RenderItem`（或策略输入结构），下游只读这份**一次性派生量**，不再存回 `Entity`。这样既没有常驻副本，也没有每帧 O(16) 重复。

---

## 四、建议

1. **先做 (a)**：它是"门"的正确位置，零风险，且能让 (b)/(c) 的改动有对拍兜底。
2. **(b)/(c) 与问题 2 一起拍板**：`component_mask` 与 `EntityGPU.type[16]/row[16]` 是同一张表的两个下游副本，单删一个收益有限；一起收敛才是"少一份状态"。
3. 顺手记账：`HasComponentType()` 当前**生产零调用**（§1.1 的"0 消费者 = 投机 API"纪律）——要么在 (b)/(c) 里给它找真读者，要么按 §1.1 口径删除，别让它继续挂着"看起来有人用"的 API。

---

## 五、未证实 / 需要确认

- 我按全仓 grep（`inc/`、`src/`、`example/`）确认 `GetComponentMask()` 只有 1 处生产读者、`HasComponentType()` 生产零调用；若存在生成式/宏展开的间接读者（我未发现任何迹象），结论需要修正。
- 掩码宽度是否已按 `COMPONENT_TYPE_COUNT ≤ 16` 收紧到 u16（当前是 `uint32_t`）—— 属 §4.2「魔数/宽度」一族，需与"组件槽位数是否扩容"一起定。
