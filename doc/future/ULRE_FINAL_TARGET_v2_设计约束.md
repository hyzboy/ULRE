# ULRE 终极形态 v2 —— 设计约束（2026-09-30 定稿）

> **来源**：本文以用户草案 `doc/future/ULRE_FINAL_TARGET.md` 为原文，逐条给出**已定调的设计约束**、
> 与**现有代码**的对应关系、以及需新增/待细化项。原文的畅想意图全部保留，只在必要处收紧或修正。
>
> **标注**：✅ 已由现有机制覆盖（有 path:line 依据）｜🆕 需新增｜⏳ 待细化（不阻塞）｜❌ 明确不做

---

## 0. 总纲（三条不可动摇的规则）

1. **单一真源**：每份数据只有一个权威位置，其余都是**派生/缓存**；派生侧不得独立修改（T8 血泪：组件退役时丢掉的正是"对世界的副作用"）。
2. **预算制**：正式形态**声明式容量 + 预分配**；超限走**一次性明确告警**（"预算没调够 ⇒ 行号已移动 ⇒ 固化 ID/存档失效"），不是静默扩容。Editor 例外**永远开着**（用户定调：不关心性能，发行版里谁要这么用卡死不管）⇒ **不引入 Editor/Release 分支**。
3. **CPU 权威 + GPU 派生视图**：CPU 侧为权威数据，GPU 侧是按行同号的派生镜像，经**版本号增量**同步；不引入回读、不双写。

---

## 1. Component Data（草案 §1）

> 草案原文：「所有的 Component 都会有自己的数据，这个数据必须定长，并且符合 GPU 访问最小对齐单元」「可以对应一个 C++ 结构和一个 GLSL 结构」「整体机制为预算制，除 Editor 模式外正式版不允许扩容」「每当增加一个 Component，是直接从大 SSBO 的数据队列申请一个 ID 用读写」「可参考现有 `GlobalSSBOBufferRegistry`」

**✅ 与现有机制对齐**
- 预算制 + 不扩容 + 超限报错**已有先例**：`inc/hgl/ecs/support/CameraInfoStorage.h:27`（容量 16 槽 × 帧槽 = 128 行，超限返回 `INVALID_SLOT`，**不扩容**）、`:44` 还带 `static_assert(kSlotCapacity == 16u, ...)`。
- "大 SSBO 数据队列"已有雏形：`inc/hgl/graph/module/GlobalSSBOBufferRegistry.h`（`GlobalSSBODataAccessor : ActiveRowLease`，per-type SSBO + `ActiveRowPool` 行池）。
- "C++ 结构 ↔ GLSL 结构对应"已有**校验门**：`S.global-addresses-struct-parity`（C++↔GLSL 结构漂移会静默回退默认材质 ⇒ 已被门拦住）。

**🆕 收紧后的定调**

| 项 | 定调 |
|---|---|
| 行 arena 形态 | **每个 (scope, 组件类型) 一个定长行 arena = 一个 SSBO**；Entity 槽里存的是**该 arena 内的行号**（不是全局行号）。批量读写 = "从 row 起 N 行"，天然连续（这正是草案 §2 `data_count` 的用法前提） |
| scope | **静态**：组件类型 → scope 是编译期/启动期常量映射（用户定调："不可能出现一会世界一会全局的情况"）。落成**一张静态表**：类型 → (scope, arena, 行宽) |
| 数据分工 | **GPU 要处理的都进 Entity**；CPU 数据不进 Entity。**运行时查找的钥匙是行号**（O(1)），不用哈希、不用各种 ID |
| 派生行的定位 | 草案 §2 的 `TransformComponentData{Matrix4f local_to_world}` 是**派生视图（GPU 行）**；CPU 侧 TRS 才是真源（与 T7/T8 已拍的"真源 vs 求值中间量"一致，不得当第二真源） |
| 组件结构 | 草案 §2 的 `ComponentDataAccessor` 泛化自现有 `GlobalSSBODataAccessor`，但**租约语义废弃**（见 §2） |

**❌ 明确不做**：GPU 侧 48B TRS SSBO（D1）、L2W 改 `Matrix4x3f`（D2）、全局 `g_CurrentTransformStorage`（D3）。

---

## 2. Component 访问（草案 §2）

> 草案原文：`ComponentDataAccessor{ ActiveRowPool* data_pool; int32 data_id; int32 data_count; }` + `TypedComponentDataAccessor<T>`（`IsValid/Get/Write/Read`）+ `TransformComponentAccessor : TypedComponentDataAccessor<TransformComponentData>`；并定调「租约语义可以考虑废弃，全部转为句柄语义」

**定调（已拍板）**

| 项 | 定调 |
|---|---|
| 语义 | **句柄**（长生命周期，析构**不**归还行）；`ActiveRowLease` 那套 RAII 租约废弃 ⇒ 行生命周期由世界/Manager 显式管理 |
| 失效检测 | **行内 revision 校验**（用户定调选此方案）：复用现有 `TransformDataStorage.h:81` 的 `versions`（uint64 变更计数），**释放/重分配时 bump**；句柄存 `{arena/池, row, revision}`，读时比对，失配 ⇒ 空句柄（Debug 下断言 + 一次性日志）。好处：ID 仍 4 B、槽仍 4 B、**句柄仍是 24 B**（现 `TransformAccessor` 的 4 B 填充正好装 revision） |
| scope 解析 | 访问器不再持裸池指针，而是经**静态表**解析 scope（避免将来加第三种 scope 时改所有调用点）＋ 携带世界上下文（多世界安全，D3 的代价） |
| `data_count` | **仅表示一次批量访问多行**；**一个组件只占一行**（用户定调）⇒ 槽宽 4 B 保持 |
| 失效句柄的可见行为 | `IsValid()` 内含 revision 校验；失配返回空句柄；**禁止**复用后误读 |

---

## 3. Entity（草案 §3）

> 草案原文：`ComponentType{None, Geometry, Transform, Material, HLODProxy, End}`、`URLE_MAX_COMPONENT_COUNT = 8`、`struct Entity{ uint64 persistent_id; uint32 flag_bitmasks; uint8 component_type[8]; uint32 component_ids[8]; uint32 work_flags; }`、「Entity 最终也是平凡类型的纯数据，放入 SSBO 供 Compute Shader 访问」「整个存入文件…一次性载入」

**定调（按讨论修正）**

| 项 | 定调 | 依据 |
|---|---|---|
| `persistent_id` | **删除**（用户定调：原先只是保留设计）。运行时钥匙 = 行号；将来若需存档，另设"存档 ID → 行号"映射表 | — |
| 槽数 | **16 槽**（8 不够时扩；用户定调） | 用户 |
| 布局与尺寸 | `flags(4) + type[16](16) + row[16](64) + work_flags(4) = 88 B` 载荷 ⇒ `alignas(64)` ⇒ **128 B/实体（余 ~40 B）**；**旧文档的"64 B Entity"作废** | 16 槽 × (1+4) B |
| 余量用途 | `component_count` / 行号版本（缓存友好）/ LOD 层级提示 | ⏳ 待细化 |
| `flag_bitmasks` / `work_flags` | **工作期派生数据**（由 type/ID 推出，用户定调"只是为了运行时快速确定一些事"）⇒ 不是第二真源。**约定：改槽者同步刷位图**（或用时重建），Debug 下加"重建后比对"校验 | 用户 + 单一真源规则 |
| GPU 可见组件 | `Transform / MaterialData / MaterialRuntime / Geometry / Texture`（用户定调：为 GPU Scene / GPUDriven 准备，目前就这些）⇒ **5 类** ⇒ 16 槽有大量玩法余量 | 用户 |
| 不进 Entity 的 | 相机/阴影/可见性等**世界级表**（`CameraInfoStorage` 已是世界级 16 槽 × 帧槽）；CPU-only 组件（音频/剧情等）**在世界级 CPU arena 按行号索引**，不进 Entity | 用户 + §0-3 |
| 两份东西的命名 | CPU 侧 `EntityRecord`（权威）；GPU 侧 `EntityGPU`（派生视图，即上表 128 B 结构）。**避免两者都叫 Entity** | §0-3 |
| Material 拆分 | `MaterialComponent` 重定制为 `MaterialDataComponent`（数据）+ `MaterialRuntimeComponent`（运行期数据，渲染侧创建/销毁） | 用户 |
| 摄像机 | `CameraComponent` 只描述**参数**；槽位/资源仍归世界级（沿用 16 槽 × 帧槽 + 三级解析） | 现有模型 |

**❌ 暂不做**：`HLODProxy`（无实现，留名字与钩子）；prefab（多模型合并 ⇒ HLOD 代理）——属远期，且变种多。

---

## 4. 两级 Transform 展开（草案 §4）——由「离线压平」落地

> 草案原文：「最终进行渲染分发时，永远保持只有 2 级的 Transform L2W 展开」+ 四档距离（积木/房子/小区 + HLOD）

**定调（按讨论修正）**

- 🆕 **压平的归属：Editor 离线产物，不是加载期**（用户定调）。做法：离线把模型内每个 node 的 TRS **预组合成"相对模型根"**，**连续排列**；运行时只做 `World(模型实例) × node_相对` = **永远 2 级**。
- ✅ **不破 T5**：产出仍是 TRS（不是矩阵表）——只是把"父链"换成"模型根相对"，TRS-only 不变。
- 🆕 **连带收益**：模型内不再需要父子链 ⇒ 运行时 `parent_indices/children/eval_order` 只在**场景组装层**（模型实例之间）保留，且很浅 ⇒ 顺带缓解行宽膨胀（T8 新增的 `children` 24–32 B/行可只留给组装层）。
- 🆕 **验收口径**：压平前后世界矩阵**对拍同值**（同 T5 口径）；产物仍是 TRS-only 格式。
- ❌ **近期不做 HLOD**（用户定调"先空着"）：草案 §4 的四档距离（房子 HLOD/小区）**只保留语义目标**，实现留到有 HLOD 生成器时；prefab 压平（全部 node 展开到 prefab 根）留钩子。
- ⏳ 待细化：`WorldAddresses`/`GlobalAddresses` 两级地址模型如何承接"每个模型实例一行"的 L2W 寻址。

---

## 5. 动画与海量 NPC（新增，来自讨论）

**动画定调**：**当静态数据一样压平**——所有 node 的 TRS 连续排列；要动时由**另外的 CPU 或 ComputeShader 直接改这段区**，渲染侧与静态 node **无区别**。

- 🆕 **配套规则（必须）**：GLTF 动画给的是**父空间** TRS；压平后行里是**模型根相对** ⇒ **动画剪辑也必须预组合到模型根相对**（离线烘），否则写进去就是错的。
- ✅ **骨骼动画走独立通道**（skin palette），不参与 L2W 展开 —— 这是行业通行做法（Unity/Unreal/Godot 皆如此；骨骼层级只服务 CPU 侧采样/混合，渲染只吃 palette）。
- 行业参考：① 骨骼 = 独立层 + palette SSBO；② GPU 动画 = compute 写 palette / VAT / 骨骼纹理；③ 整物体动画 = 写该物体的一行。我们采用 ③ 的统一形式（"写行"）+ ① 的骨骼通道。

**海量 NPC 烘焙 bank（用户提出的既有做法）**：预烘焙所有 NPC 基础动画的 TRS 到一个大 SSBO，每个 NPC 的"某动作某帧"直接改该 SSBO 的**数据偏移**。

整合结论：**不需要新机制**——"离线压平"已让**烘焙段**与**运行时行**同构（都是"模型根相对 TRS 的连续数组"），段与行只是同一块内存的两个偏移。

| 方案 | 做法 | 代价 | 适用 |
|---|---|---|---|
| (a) 每帧拷贝 | bank 段 memcpy 进 NPC 行区 | 10k NPC × 20 node × 48 B × 60 Hz ≈ **576 MB/s** | 不推荐 |
| (b) **零拷贝引用** | 行指向 bank 段（`段基址 + node 索引`） | 带宽 ≈ 0 | ✅ 海量背景 NPC |
| (c) 自有行 | 行可写，动画播放器写它 | 只更新真正变的部分 | ✅ 主角 / 被逻辑改的对象 |

**推荐：(b) 为主 + (c) 兜底**，配套规则：
1. **bank 只读**：同一 (动作, 帧) 段被成百上千实例共享 ⇒ 禁止写实例私有状态（受伤扭曲/挂点等）走 (c)。
2. **寻址即两级展开**：`World(NPC实例) × bank[段基址 + node索引]`，纯加法。
3. **可不落 TRS**：海量 NPC 可在 compute 里**直接从 bank 算 L2W** 写实例表，省一次写+读。
4. **更新率分档**：按距离/屏幕占比分档（如 60/30/15 Hz），远景更低。⏳ 具体档位实现时定。
5. **bank 格式**（用 §1 的预算制）：段 = `node_count × TRS(48 B)` + 段头（node 数、包围盒/锚点用于剔除），GPU 最小对齐；逐帧全烘最简单（例：30 fps × 2 s × 20 node × 48 B ≈ **57 KB/动作**，100 个动作 ≈ **5.7 MB**）。
6. 行业对照：VAT / bone palette / 烘焙动画库的 **node TRS 版**；顶点版 VAT 将来可并存（骨骼通道是其入口）。

---

## 6. 视口列表（阴影 / 多视口）——替代"ShadowWorld = 另一个世界"

**定调（用户）**：`ShadowWorld` 不是新世界，而是**同一世界数据的另一条渲染列表**；且"顺手把多视口做了吧！阴影不也是视口的一种吗"。

- 🆕 **`RenderList`（= 视口）**：每条 = **相机行 + 视口矩形/裁剪 + 收集过滤器**（layer / caster 标记）+ **输出目标**。主视图、阴影视图、分屏多视口、离屏 RT 共用同一套。
- ✅ 雏形已存在：`RenderPassRequest{target, clear, use_target_clear, delta_time, camera}`（backlog A2 已落地）+ `ViewportInfo` + 世界级相机行（`CameraInfoStorage`）。
- 🆕 阴影侧：`ShadowProxyComponent` 只描述"**它产生什么阴影**"（以及代理之下是多个子 entity 还是一个），逐灯光的阴影视图按它过滤 caster。
- ⏳ 待细化：多列表的存放位置（`WorldAddresses` 增"当前渲染列表"地址 vs `pc_root` 多一个指针）。
- ✅ 顺带收益：backlog A3（RenderGraph 跨 RT pass 链）/ A7（离屏 RT in-flight 槽）的多视口需求被同一套机制覆盖。

---

## 7. 与路线图的接口

| 任务 | 本约束带来的变化 |
|---|---|
| **T9**（下一步） | 除"连续化 + Material 分层"外，**新增**：① 类型→scope/arena/行宽**静态表**；② 句柄 revision 校验（泛化现有 `versions`）；③ CPU 权威/GPU 派生的存储布局。此刻只有 Transform 一个消费者 ⇒ 成本最低 |
| **T10** | 改成**声明式预算（per-type arena 容量）+ 预分配 + 超限一次性告警**；**无 Editor 分支**（总纲 2） |
| **T11**（已延后） | `EntityGPU` = **128 B**（非 64 B）+ 取消逐 new；场景直载仍等"子场景树快速插入/展开"之后 |
| **T12** | ① 离线压平产物（模型内 node → 模型根相对 TRS；动画剪辑同构烘制）；② 动画 bank + 行"引用段/自有行"两模式 + 分档更新率；HLOD/prefab 留钩子 |
| **新轨** | **视口列表（RenderList=Viewport）**，一并覆盖阴影与多视口 |

**已完成的邻接项**（不再重复）：T8（TransformComponent 退役）✅、T10 前置的探针行账目（存储自列 18 条平行数组 + Test 9 不变量）✅、backlog C.1（TexConvCore 链接）✅。

---

## 8. 待细化清单（不阻塞）+ 明确不做

**待细化**
1. Entity 余量字段的具体用途（`component_count` / 行号版本 / LOD 提示）。
2. 动画 bank 的分档更新率数值。
3. 视口列表的存放位置（WorldAddresses vs pc_root）。
4. 骨骼 palette 的容量/格式（本轮未涉及）。
5. `flag_bitmasks` 刷新时机（写槽同批 vs 用时重建）——两种都能满足单一真源，实现时择一并加 Debug 校验。

**明确不做（避免反向工作）**
- GPU 侧 48B TRS SSBO（D1）；L2W 改 `Matrix4x3f`（D2）；全局 `g_CurrentTransformStorage`（D3）。
- 近期：HLOD/LOD 生成、prefab（多模型合并）、场景直载（已延后）。
- 不再引入 `persistent_id` 之类"预留设计"（需要时按需设计）。

---

## 9. 演进路径：stage A（组成形）→ stage B（存储形）

> **总则：语义先行、存储后搬。** T8 的代价证明——把"责任迁移"与"存储搬迁"混在同一批里，出错表现会伪装成渲染 bug
> （三条"对世界的副作用"丢失 ⇒ 92 个实例全画在原点）。拆成两阶段后，stage B 退化为"机械替换 + 对拍同值"。
> 本条由用户 2026-10-01 拍板。

### 9.1 两个阶段

- **stage A（组成形）**：把胖组件按职责拆成可挂载的子组件；建立"组件集合 → 渲染策略"的**声明式判定表**；
  材质按 Data / Variant / Runtime 三层落位。**不换存储表示**（仍是组件对象），但**公开 API 按终态形状设计**
  （直取访问器 `GetGeometry(entity)` / `GetMaterialRuntime(entity)` / `SetEntityVisible(...)`，不用 `GetComponent<>()` 链）
  ⇒ stage B 只换内部实现，**调用点只迁一次**。
- **stage B（存储形）**：把 stage A 定下的职责一对一映射到 (scope, 类型) 行 arena + 访问器 + `EntityGPU`；
  纯机械替换 + 对拍同值。

### 9.2 五条护栏（缺一条中间态就会变质）

| # | 护栏 | 落法 |
|---|---|---|
| P1 | **API 按终态形状** | 直取访问器；终态只换内部 ⇒ 调用点只迁一次 |
| P2 | **策略集中** | `组件集合 → pass/收集器需求 + implies 展开` 一张表；**禁止散落 `if (hasA && hasB)`**；stage B 用位掩码 O(1) 求值同一张表 |
| P3 | **Entity 组件类型位掩码**（stage A 就加） | 单一写者 = 挂载/卸载 ⇒ 策略判定 O(1)；它就是 `EntityGPU::type[16]` 的前身；顺手把"组启停双写者"收敛为"仅 gather" |
| P4 | **渲染侧只读派生扁平表** | 收集系统自建数组，不做每帧 `GetComponent` 链（现状 `Entity::components` 是 `typeid` 哈希查找） |
| P5 | **中间态不留存** | 见 §9.4 纪律（每步同批清理 + 零残留 grep） |

### 9.3 材质三层：**1:N:N 扇出**，不是恒等链（2026-10-01 修订）

> 先纠正一个易读错的写法：三层**可以**重合（全共享），但**不必**重合。同一个 `MaterialData` 可能产生**多个** `MaterialVariant`；
> 同一个 `MaterialVariant` 也可能对应**多个** `MaterialRuntime`。

```
MaterialData ──1:N──▶ MaterialVariant ──1:N──▶ MaterialRuntime
(授权/参数覆盖)        (解析：program/PSO/行布局)     (每实例绑定)
```

**差异维度 → 放哪里（核心规则）**

| 维度 | 典型例子 | 放哪里 | 为什么 |
|---|---|---|---|
| **静态**（决定 program/PSO） | `PassType`（仓里已含 `ForwardDither`/`ForwardA2C`/`Shadow*`/`EarlyZ*`）、`MaterialRecipe::compile_defines`（**必须归一化+哈希成有界键**：去序、去重、trim）、quality、未来的**材质 LOD 档** | **变体键** | 只有"需要换管线"的差异才配进键；**取值必须能有界枚举**，否则是设计错误（一次性告警/fail-fast） |
| **每帧动态、基数小** | 某棵树要不要 Dither、当前 LOD 档、遮挡/半透裁决结果 | **每实例选择器**（runtime 行里的一个索引/标志位）+ 必要时动态状态 | **禁止进变体键**（每帧变化的键 ⇒ 变体爆炸）；**也禁止每帧 CoW**（行抖动） |
| **每实例持久** | 参数覆盖、纹理覆盖、自有 SSBO 行 | **runtime 自有行（CoW）** | 只有"改了就是改了"的差异才分裂行；释放走 refcount；行号复用按 §2 世代约定 `+2 保持奇数` |

**一句话决策规则**：这处差异**需不需要换 program/PSO**？
- 不需要 ⇒ 放**每实例数据/选择器**（零解析成本，每帧可切）；
- 需要 ⇒ 进**变体键**，但必须能把取值**限定为有界集合**。

**你举的三种情形对应下来是**
1. **10×10 球，只有纹理与 PBR 参数不同**：同一组（同纹理同参数）的球**三层完全可以全同** ⇒ 全共享；而**不同组之间 `MaterialData` 不同，却仍可能共用同一个 `MaterialVariant`**——因为纹理/参数差异活在每实例行与 SSBO 里，**不改变 program**。也就是说**变体的共享范围比 Data 更宽**。
2. **未来做材质 LOD**：同一个 `MaterialData` 会因为"便宜的 LOD 档"使用**不同的 program** ⇒ **1 Data : N Variant**、N 个 runtime；而"这个实例当前用哪档"是**每帧动态**的 ⇒ 放每实例选择器（LOD 档的**取值集合**进键，具体档位不进）。
3. **森林 + 角色走到树后要 Dither 半透**：材质完全相同（1 Data），但该实例需要走 `PassType::ForwardDither` ⇒ **同一 Data 出第二个 Variant**（跨 pass 的差异是既有维度）；**"谁需要 dither"是每帧上下文** ⇒ 放每实例选择器，**不能**去改共享行（会污染同一材质的其它树），**也不必**每帧 CoW。

**写者归属（单一写者）**：驱动上表第二行的必须有**专属系统**作为唯一写者（LOD 选择 / 遮挡 dither 裁决各一个），且其更新率可与主帧分档；最近期的 dither 写入者是既有的 pass 选择路径（`ForwardDither` 已在 `PassType` 中）。

**仍沿用 9.3 的其余结论**：`MaterialData` 按 (definition, 参数指纹) 去重（Global）；`MaterialVariant` 按键缓存去重（Global）；
`MaterialRuntime` 为共享行 + CoW + refcount（World）；变体键**现在就定死**；已有 `program_build_context_hash` / `cached_effective_recipe_hash`
升级为"键 → 解析结果"映射；**变体与行都按预算制**（上限 + 一次性明确告警，与"材质行 arena 1024 不扩容"同口径）；解析惰性、共享/独占行数可观测。

### 9.4 中间态纪律（强制，写进每一步的验收）

1. **每步必须可编译可测**：build 0 error → 固定验证集 → 示例抽跑。
   **任何一步做不到"一步内新旧都跑得通"就说明该步太大，继续切分**——而不是引入兼容分支。
2. **同批清理**：每一步把该步取代的旧代码**在同一批里删掉**，不允许"新的有了旧的还在"；
   `grep` 零残留作为该步验收项。
3. **禁止 if 特例**：不接受 `if (新旧并存)` / `if (特例)` 分支；语义收敛一律走判定表（P2）。
4. **对拍**：涉及行为等价的重排，验收 = 同输入同结果（T5 口径）。

### 9.5 stage A 步骤（每步独立可运行，详细任务书见计划文档 §9.1）

A0 地基（纯新增、零行为变化）→ A1 策略判定表（先只读不驱动，与现有判据对拍）→ A2 材质 Data 层 →
A3 材质 Variant 层（把 `shadow_*` 复制字段收敛成变体的一项）→ A4 材质 Runtime 层（共享行 + CoW）→
A5 Primitive 拆分（Geometry / MaterialBinding / ShadowProxy / LOD 钩子 + 可见性三真值收敛）→
A6 Camera（独立）→ A7 stage A 收口（零残留 + 判定表唯一判据 + 记录"stage B 仍欠什么"）。
