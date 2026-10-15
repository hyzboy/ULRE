# ULRE 引擎 Transform 体系彻底转换：设计方案与实施计划（v2 · 实测订正版）

> **⚠ 历史记录档**：本文写于对应日期，文中提到的已删符号（`PrimitiveComponent` / `MaterialComponent` / `RenderableComponent` / `TransformComponent` / `VisibilityComponent`·`VisibilitySystem` / `component_mask` / `RenderItemDataStorage` / `AssignTransformIndices` / `PositionSourceSpec`·`TransformPolicySpec` 等）按**历史口径**读。
> 现行权威：`doc/ecs-layer-architecture-and-frame-flow.md`（ECS 层主档；§2.1 对象模型 / §5 组件面）＋ `doc/visibility-authority-chain.md`（可见性）＋ `doc/backlog.md` §2（逐条落地记录）。

> **文档定位**：针对 `ULRE_ECS_Component_To_ID_Architecture_Roadmap.md` 中阶段一（Transform 体系）在当前代码库中“仅完成一半”的现状，制定彻底消灭局部矩阵、轻量化 Accessor 句柄、简化标脏链路的深化重构技术规范与落地计划。
> **关联基线**：
> - 现状审查文档：`doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md`
> - 契约测试集：`TestTransformFlatStorage.exe`（**15 项**用例，全用例保持 100% 通过）
> - 核心文件：`inc/hgl/ecs/support/TransformAccessor.h`、`inc/hgl/ecs/support/TransformDataStorage.h`、`src/ecs/support/TransformAccessor.cpp`

---

## 〇、 v2 修订记录（2026-10-09，全部以仓库实测/构建产物为准）

| 修订项 | v1 的说法 | v2 的实测结论 | 证据 |
|---|---|---|---|
| 每行内存基线 | 189 B | **223 B** | Release 运行 `TestTransformFlatStorage.exe`：`Test 9 Passed: 每行数组同步（7 行 × 19 条平行数组 = 每行 223 B，其中派生/缓存 128 B）` |
| 每行内存目标 | 124 B | **158 B** | 223 − `local_mat4`(64) − `local_dirty`(1) = 158 |
| 缩减幅度 | −65 B（34.4%） | **−65 B（29.1%）** | 65/223 |
| 4096 节点占用 | 774.1 KB → 507.9 KB | **913.4 KB → 647.2 KB**（省 266.24 KB = 260.0 KiB） | 223/158 × 4096 B |
| Accessor 尺寸 | 24~32 B | **24 B**（无 32 B 情形） | 实测 `sizeof(TransformAccessor)==24`（8+4+4+8，对齐 8） |
| Accessor 瘦身动机 | “16 B 可在 RAX:RDX 两个寄存器按值传参” | **该 ABI 断言不成立**（MSVC x64 对 12/16/24 B 聚合**一律按引用**传递，只有 ≤8 B 走寄存器）；瘦身的真实动机是**消除冗余指针 / 与 BoundingBoxAccessor 范式一致** | `cl /O2 /FA` 生成汇编实测：`call16` 里 `movups xmm0,[rcx]` → 拷栈临时 → `lea rcx,$T1[rsp]` → `call`；`call8` 才是 `jmp sink8`（值在 rcx） |
| `GetLocalMatrix` | “保留” | **该 API 当前不存在**（T6 已随组件删除，`ULRE_Phase0-1_...:301`）；本轮要“新增”还是“不做”需单独拍板 | 全仓 `GetLocalMatrix\b` 仅命中文档 |
| 求值循环改法 | `if (!matrixDirty[id]) continue;` | **必须保留 `parent_dirty` 项**，且清零仍在循环后统一做；按 v1 伪码会精确打挂 Test 5 | `TestTransformFlatStorage.cpp:134-146`（`storage.SetPosition` 直写根节点 + 靠父脏传播得到 (20,5,2)） |
| 子树标脏归属 | “`SetPosition` 时连带递归标子（维持既有机制）” | **存储层没有这个机制**：`SetPosition/SetRotation/SetScale` 只写自己一行；子树标脏只在 `TransformAccessor::MarkLocalChanged`（走子表） | `TransformDataStorage.h:251/265/279/286` vs `TransformAccessor.cpp:79-106` |
| 契约测试数量 | 12 项 | **15 项**（Test 12 另含 21 条源码 needle 契约） | `TestTransformFlatStorage.cpp` 的 `// Test 1..15` |
| 兼容构造 | “保留三参重载、内部忽略 context” | **必须删除三参重载**：忽略 context 会让 `SetMobility` 不调 `MigrateTransform` ⇒ 实例拿默认行、全画在原点 | `TransformAccessor.cpp:526-527`；Test 8 |
| 构建/运行命令 | 直接跑 `Windows_64_Release/ClockUse.exe`、`RecursiveCube.exe` | **这两个 exe 只在 `Windows_64_Debug`**，且不在该 build 目标列表内 ⇒ 命令会失败 | `ls build/out/Windows_64_Debug/` |
| 变更布局后的构建纪律 | 未提 | **改存储成员布局前必须清 obj**，否则陈旧 TU 按旧偏移访问、首调用即崩 | 本仓 MSBuild 头依赖跟踪不可靠 |

---

## 一、 背景与现状差距诊断 (Gap Analysis)

在过去的重构迭代（T8 系列任务）中，Transform 体系成功废除了 OOP 模式的 `TransformComponent.h`，将核心数据下沉至 `TransformDataStorage`，并通过 `TransformAccessor` 提供调用接口。然而，对照“全 ID + DOD 紧凑存储”的终极目标，目前仍存在三个**半拉子过渡态**缺陷：

### 1.1 现状缺陷 1：局部矩阵未根除，每行多付 64 B 常驻缓存

* **目标愿景**：局部变换全面统一为 48 字节 TRS（平移、旋转四元数、缩放），彻底废除局部 `Matrix4f` 内存存储；世界矩阵仅在输出或拓扑求值时产生。
* **实际现状**：
  * `TransformDataStorage.h:46/:57-59` 同时保留了 `positions` (vec3)、`rotations` (quat)、`scales` (vec3) **以及** `local_matrices` (64 B mat4)；`:62` 另有一级 `local_dirty`。
  * 修改局部属性时先置 `local_dirty=1`（`:251/:265/:279/:286`），平铺求值时合成一次 `local_matrices`（`UpdateLocalMatrix` `:299-310`），再与父世界矩阵相乘得出 `world_matrices`。
  * **每行账目（实测口径，由 `GetPerRowFields()` 自列、Test 9 打印）**：

    | 数组 | 每行字节 | 说明 |
    |---|---|---|
    | `local_mat4` | 64 | **本轮待删：纯派生缓存，引擎侧零消费者** |
    | `world_mat4` | 64 | 最终 L2W，保留 |
    | `positions`/`rotations`/`scales` | 16+16+16 | 局部唯一真源（`GLM_FORCE_DEFAULT_ALIGNED_GENTYPES` ⇒ `vec3`=16） |
    | `parent` / `depth` / `eval_order` | 4+2+4 | 层级拓扑 |
    | `local_dirty` | 1 | **本轮待删：严格被 `matrixDirty` 包含** |
    | `matrix_dirty` / `mobility` | 1+1 | 脏标记 / 移动性 |
    | `owners` / `change_masks` / `versions` / `generation` | 8+4+8+4 | T8 元数据（owner、变更掩码、上传判据版本、句柄世代） |
    | `write_armed` / `write_warned` | 1+1 | D4 运行期写静态告警 |
    | `child_offset` / `child_count` | 4+4 | 子表 CSR |
    | **合计** | **223 B** | 19 条平行数组；其中“派生/缓存” = `local_mat4 + world_mat4` = 128 B（`DerivedRowBytes()`） |

  * **⚠️ 基线数字的坑**：v1 与 `ULRE_Phase0-1_..._Plan.md:258` 的 189 B 是同一张**过时字段表**（漏登记 T8 后来加上的 `owners(8) / change_masks(4) / versions(8) / generation(4) / write_armed(1) / write_warned(1) / child_offset(4) / child_count(4)` 共 34 B）。189 + 34 = 223，与实测吻合。**引用 189 B 会连带把“改造后”也算错**（124 B → 实际 158 B）。

### 1.2 现状缺陷 2：`TransformAccessor` 冗余持有一个可反查的指针

* **目标愿景**：Accessor 仅为轻量抽象句柄，零成本拷贝，所有方法内联。
* **实际现状**（`inc/hgl/ecs/support/TransformAccessor.h:27-30`）：
  ```cpp
  class TransformAccessor {
      TransformDataStorage *storage = nullptr;   // 8 字节
      TransformID           id      = INVALID_TRANSFORM_ID;  // 4 字节
      uint32_t              generation = 0;      // 4 字节（防 ABA 僵尸句柄）
      ECSContext           *context = nullptr;   // 8 字节（用于反查 Entity 与换边通知）
  }; // 实测 sizeof == 24（对齐 8；不存在 32 字节的“部分对齐场景”）
  ```
* **膨胀根因**：
  1. `g_CurrentTransformStorage` 全局单例已被 **D3** 正确否决（多世界/离屏世界隔离），Accessor 因而必须持有所属存储的指针 —— 全仓已 0 命中该全局变量，方向正确。
  2. Accessor 中**冗余持有 `ECSContext*`**：`TransformDataStorage` 由 `ECSContext` 私有独占（`Context.h:175` + `Context.cpp:61` 的 `unique_ptr`），可以 `storage->GetContext()` 反查，无需在每个句柄实例中重复 8 字节。
* **但瘦身的理由不是 ABI**：实测 MSVC x64 对 12/16/24 B 的聚合体**一律按引用**传递（见 §〇 证据），所以“降到 16 B 就能两寄存器传参”不成立。真实收益是：
  * **消除第二真源**：`context` 与 `storage` 的归属必须永远一致，重复携带等于让“两个字段互相不一致”成为可能（一旦有人用 `TransformAccessor(storage, id)` 建柄，`SetMobility` 就静默不换边）；
  * **范式一致**：`BoundingBoxAccessor` 是同一问题的另一处实例（`inc/hgl/ecs/support/BoundingBoxAccessor.h:52`），本轮要么同步做，要么在文档里写明只做 Transform 的理由；〔2026-10-11：`BoundingBoxAccessor` 现值 **16 B** —— 冗余 `context` 与 `GetContext()` 已删（批 1.2 A4）；其句柄范式仍成立，只是不再是 24 B〕
  * 尺寸 24 → 16 B（按值返回的调用点栈临时少 8 B 拷贝，属附带小收益）。
* 现状里 `TransformAccessor::GetContext()` 这个 getter **零消费者**，`context` 只在内部被 `GetOwner()`（`:25-36`）与 `SetMobility → MigrateTransform`（`:526-527`）使用 —— 因此改成反查对调用面是纯内部变更。

### 1.3 现状缺陷 3：两级脏标记中有一级是冗余的（且“子树标脏”不在存储层）

* `TransformDataStorage.h` 同时维护 `local_dirty`（局部矩阵是否需重合成）与 `matrixDirty`（世界矩阵是否失效）。
* **实测结论：`local_dirty ⊆ matrixDirty`** —— 所有写者（`Allocate:185-186`、`SetPosition:254-255`、`SetRotation:268-269`、`SetScale:282-283`、`SetLocalTRS:291-292`）两者同置；唯一分歧是 `SetParent:630` 只置 `matrixDirty`（该行局部未变，只需重算世界）。既没有“只置 local_dirty”的路径，两级化就只剩维护成本。
* **必须同时纠正的认知**：存储层**不做**子树标脏（`TransformDataStorage.h:251/265/279/286` 只写自己一行）。子树标脏/版本 bump 在 `TransformAccessor::MarkLocalChanged`（`TransformAccessor.cpp:79-106`，走子表）。因此 `TestTransformFlatStorage.cpp:134-146` 这种“直接写存储”的级联，完全依赖求值循环自身的 `parent_dirty` 传播（`TransformDataStorage.h:849-852`）。**删 `local_dirty` 时这一点不能动。**

---

## 二、 核心架构重构方案 (Architectural Design)

```
[旧架构: 223 字节/行, 24B 句柄]
TransformAccessor (24B) ──> [storage* (8B) + id (4B) + gen (4B) + context* (8B)]
                              │
TransformDataStorage ───────> positions/rotations/scales (48B)   ← 唯一真源
                              local_matrices (64B)  <── 派生缓存，引擎侧零消费者
                              local_dirty (1B)      <── 冗余二级标脏
                              world_matrices (64B)
                              + 其他 14 条平行数组 (46B)

                    ▼ ▼ ▼  彻底重构  ▼ ▼ ▼

[新架构: 158 字节/行, 16B 句柄]
TransformAccessor (16B) ──> [storage* (8B) + id (4B) + gen (4B)]
                              │ (context 由 storage->GetContext() 反查)
                              ▼
TransformDataStorage ───────> positions/rotations/scales (48B)
                              world_matrices (64B)  <── 仅保留最终 L2W
                              matrixDirty (1B)      <── 单级直接标脏
                              + 其他 14 条平行数组 (46B)
                              (local 矩阵在世界求值循环中由寄存器即时合成，不落内存)
```

### 2.1 方案 A：砍掉 `local_matrices` 平行数组与 `local_dirty`

#### 设计原则
* **内存中只保留原生 TRS 与最终 World Matrix**：每行 223 → **158 B（−29.1%）**，4096 节点省 **266.24 KB**。这条收益是无条件的（纯常驻内存）。
* **级联路径是“拿算术换带宽”，必须量化后再拍板**：
  被父级带动重算的行，现状是“读缓存 `local_matrices` + 一次矩阵乘”；删掉缓存后要**每行重做 quat→mat 与 TRS 合成**。所以 v1 的“减少 1 次 64B 写 + 1 次 64B 读”**只对 TRS 真变了的行成立**，对“父动子跟”的级联行是净增算术。
  ⇒ **动工前先做计时对拍**：同一深树场景（如 `RecursiveCube` 量级）改前/改后各测 `UpdateDirtyWorldMatricesFlat()` 的耗时与行数，把数值写进本节的验收记录；若级联显著变贵，可选择“只对 static 行保留缓存”或直接接受（内存优先），但必须是**测出来的决定**，不能靠“现代 CPU 合成只要 15~20 周期”的估算。

#### 实现细节
1. **彻底移除物理数组 —— 删除清单（共 8 处引用 + 账目，漏一处即假绿）**：
   ```cpp
   // TransformDataStorage.h
   //  :46  hgl::ValueArray<glm::mat4> local_matrices;      ← 删
   //  :62  hgl::ValueArray<uint8_t>   local_dirty;         ← 删
   //  :131 Initialize() 的 local_matrices.Reserve(budget);
   //  :136 Initialize() 的 local_dirty.Reserve(budget);
   //  :180 Allocate()   的 local_matrices.Add(glm::mat4(1.0f));
   //  :185 Allocate()   的 local_dirty.Add(1);
   //  :217 Deallocate() 的 local_dirty[id] = 0;
   //  :254/:268/:282/:291 SetPosition/SetRotation/SetScale/SetLocalTRS 里的 local_dirty[id] = 1;
   //  :299-310 UpdateLocalMatrix() 整个方法（连带 :309 的 local_dirty[id]=0）
   //  :679 GetLocalMatricesData()  / :686-694 UpdateAllLocalMatrices()
   //  :880 GetLocalMatrices()（当前零消费者）
   //  :892 Clear() 里的 local_matrices.Clear() / :898 local_dirty.Clear()
   //  :821-822 与 :852-855 两处求值循环里的 local_dirty 判定与 UpdateLocalMatrix 调用
   //  —— 账目必须同步（见第 5 条）
   ```
2. **重构世界矩阵拓扑求值核心**（`UpdateDirtyWorldMatricesFlat()` / `UpdateAllWorldMatricesFlat()`）：
   ```cpp
   // 寄存器内合成局部矩阵（M = T * R * S）
   inline glm::mat4 ComposeLocalMatrix(const glm::vec3 &pos,const glm::quat &rot,const glm::vec3 &scale)
   {
       const glm::mat4 rot_mat = glm::mat4_cast(rot);
       glm::mat4 local = rot_mat;
       local[0] *= scale.x;          // 直接缩放列向量，省一次矩阵乘
       local[1] *= scale.y;
       local[2] *= scale.z;
       local[3] = glm::vec4(pos,1.0f);
       return local;
   }
   ```
   增量求值主循环（**保留 `parent_dirty`，清零仍在循环后统一做**）：
   ```cpp
   for (int i = 0; i < eval_count; ++i)
   {
       const HandleID idx = eval_order[i];
       const HandleID p   = parent_indices[idx];
       const bool parent_dirty = (p != INVALID_HANDLE && p < world_matrices.GetCount())
                               ? (matrixDirty[p] != 0) : false;

       // ⚠️ 不得简化成 `if (!matrixDirty[idx]) continue;`
       //    —— 存储层不做子树标脏，父动子跟只能靠 parent_dirty 传播；
       //       TestTransformFlatStorage.cpp:134-146 直接写存储并断言级联结果。
       if (matrixDirty[idx] || parent_dirty)
       {
           const glm::mat4 local = ComposeLocalMatrix(positions[idx],rotations[idx],scales[idx]);

           world_matrices[idx] = (p != INVALID_HANDLE && p < world_matrices.GetCount())
                               ? world_matrices[p] * local
                               : local;

           matrixDirty[idx] = 1;                  // 传导给子节点（循环后统一清）
           if (on_updated) on_updated(idx);
       }
   }
   for (int i = 0; i < eval_count; ++i)
       matrixDirty[eval_order[i]] = 0;
   ```
   （`UpdateAllWorldMatricesFlat()` 为全量口径，逐行直接合成 + 相乘即可，不需要脏判据。）
3. **单级标脏精简**：
   ```cpp
   matrixDirty[id] = 1;      // SetPosition/SetRotation/SetScale/SetLocalTRS 只需这一句
   ```
   彻底废除 `local_dirty` 字段与相关分支。**注意**：存储层现状**不**连带标记子孙（见 §1.3），本轮也不改这一点 —— 级联由求值循环的 `parent_dirty` 承担；若要改成“存储层标子树”，那是另一件事（需在存储层获得子表遍历能力），不要让两步粘在一起。
4. **只读局部矩阵接口的去留（本轮的空白点，需拍板）**：
   * 现状：**不存在** `TransformDataStorage::GetLocalMatrix(HandleID)`，也**不存在** `TransformAccessor::GetLocalMatrix()`（T6 已删，`ULRE_Phase0-1_...:301`）。引擎侧对局部矩阵**零消费者**，唯一消费者是示例（§2.3）。
   * 结论：不要为了“保住一个没人用的接口”新增 API。若示例最终选择 CPU 侧自合成，则新增一个 `TransformAccessor::GetLocalMatrix()`（按需瞬时合成、按值返回）是合理的；否则**不新增**。
5. **每行账目必须同步（否则 Test 9 会假绿）**：
   * `PER_ROW_FIELD_COUNT`：19 → **17**（`TransformDataStorage.h:446`）；
   * `GetPerRowFields()`：删 `local_mat4`（`:455`）与 `local_dirty`（`:463`）两条；
   * `DerivedRowBytes()`：128 → **64**（`:489`，当前硬编码 `sizeof(mat4)*2`）；
   * ⚠️ **只删 initializer 而忘了改 `PER_ROW_FIELD_COUNT` 时**：多余元素被值初始化为 `{nullptr,0,0}`，`FindPerRowCountMismatch()` 会返回 `nullptr`（`name` 为空指针）而被判定为“无不一致” ⇒ **Test 9 静默通过**。必须两处一起改。
6. **构建纪律（本仓铁律）**：`TransformDataStorage` 是内联头，改成员布局后必须**先清目标中间目录再编**（`build/**/*.dir/*` 或 clean-first），否则陈旧 TU 按旧偏移访问，表现为“测试读到垃圾值 / 首调用即崩”，看起来像真回归。

---

### 2.2 方案 B：`TransformAccessor` 降至 16 字节（消歧 + 范式一致）

#### 设计原则
* 尺寸严格 16 字节（`static_assert` 钉死），**理由写“消除与 storage 冗余的 context 指针、避免两个字段不一致、与 BoundingBoxAccessor 同范式”**，不要写“寄存器传参”（§〇 已实测否证）。
* 不做任何兼容层：**三参构造直接删除**，改所有调用点。

#### 结构重定义
```cpp
class TransformAccessor
{
private:
    TransformDataStorage *storage = nullptr;            // 8 字节
    TransformID           id      = INVALID_TRANSFORM_ID;// 4 字节
    uint32_t              generation = 0;               // 4 字节（世代防僵尸）
    // 已删除：ECSContext *context;（8 字节）—— 由 storage->GetContext() 反查

public:
    TransformAccessor() = default;

    /// 建柄只收 storage + id；世代在此捕获（句柄失效机制不变）
    TransformAccessor(TransformDataStorage *s,TransformID i)
        : storage(s),id(i),generation(s ? s->GetGeneration(i) : 0) {}

    ECSContext *GetContext() const { return storage ? storage->GetContext() : nullptr; }
};
static_assert(sizeof(TransformAccessor) == 16,
              "TransformAccessor must be exactly 16 bytes (storage*+id+generation)");
```
* **调用点（全仓仅 5 处，一次改完）**：
  * `src/ecs/core/Context.cpp:967` —— `GetTransform()` 里的 `TransformAccessor(GetTransformStorage(),id,this)` → 去 `this`；
  * `src/ecs/support/TransformAccessor.cpp:48` —— `FromOwner(storage,owner,context)`：**该方法当前零调用者**（全仓只有声明与定义）⇒ 建议直接删除，或改成双参；
  * `src/ecs/support/TransformAccessor.cpp:286/:322/:358` —— `SetWorldPosition/Rotation/Scale` 内部建父柄，只需 `storage + parent`；
  * `TransformAccessor.h:39` 的默认参数 `ECSContext *c=nullptr` 一并删掉。
  * （v1 提到的 `Entity.cpp` / 示例代码**没有**构造点，全仓 grep 零命中。）

#### 关键前置改造
* `TransformDataStorage` 补反向非拥有指针：
  ```cpp
  ECSContext *context = nullptr;                 // 非拥有；由所属 world 在构造期回填
  void        SetContext(ECSContext *ctx) { context = ctx; }
  ECSContext *GetContext() const          { return context; }
  ```
* `ECSContext::ECSContext()`（`src/ecs/core/Context.cpp:58-61`，`transform_storage(std::make_unique<TransformDataStorage>())`）补一句 `transform_storage->SetContext(this);`。
* 同步更新 `inc/hgl/ecs/core/Context.h:177-179` 那条“两者互不引用”的注释 —— 新增反向指针后语义已变。
* **不得依赖 context 非空**：`TransformDataStorage` 存在独立实例（`TestTransformFlatStorage.cpp:59`、`ProbeTransformDiagnostics.cpp:122`、Test 11 的 `budget_storage`），其 `context == nullptr` 是合法状态；所有 `GetContext()` 消费点必须容忍空。

---

### 2.3 方案 C：`ComputeTransformHierarchy` 示例的适配（二选一，禁止留双真源）

现状（唯一消费者，全仓仅此一处）：
* `example/ApiValidation/ComputeTransformHierarchy.cpp:180/:355` 调 `storage.UpdateAllLocalMatrices()`；
* `:314` 把 `GetLocalMatricesData()` 当 SSBO 上传；`:360` 每帧 `Write` 前 4 行；
* shader 侧 `:75/:83` 读 `local_matrices.values[node_idx]` 后做 `parent_world * local`。

**不要采用 v1 的“示例内部临时维护一份 local”作为长期形态** —— 那是引擎删真源、示例留第二份，属明确的死码/双真源。

* **选项 (a)（推荐，终态）**：升级为 **TRS SSBO + compute shader 内合成**。把 `positions`/`rotations`/`scales` 作为三条 SSBO（或一条打包）喂给 compute，shader 内先合成 local 再乘父世界矩阵。示例就此成为“GPU 直算层级”的纯正范式，且不再需要任何局部矩阵缓存。
* **选项 (b)（最小改动）**：示例自己从 TRS 现算 local 到一个**示例私有**缓冲再上传，并在代码注释里明确“这是示例派生缓冲，不是引擎真源”。仅作为临时选项。

---

## 三、 收益量化分析 (Quantitative Benefits)

以典型场景 **4,096 个活跃 Transform 节点** 为基准（数字全部由 `PerRowBytes()`/`DerivedRowBytes()` 实测或直接推算）：

| 指标 | 改造前 (Current) | 改造后 (Target) | 收益幅度 |
|---|---|---|---|
| **每行常驻内存** | **223 B**（19 条平行数组） | **158 B**（17 条） | **−65 B / −29.1%** |
| **4096 节点内存占用** | **913.4 KB**（892.0 KiB） | **647.2 KB**（632.0 KiB） | **净省 266.24 KB（260.0 KiB）** |
| 其中“派生/缓存”部分 | 128 B（`local_mat4 + world_mat4`） | **64 B**（仅 `world_mat4`） | 派生占比 57% → 41% |
| **Accessor 句柄尺寸** | **24 B**（实测） | **16 B**（断言钉死） | 少 8 B；**不含寄存器传参收益**（MSVC x64 对 ≥12 B 聚合一律按引用） |
| **拓扑求值访存** | 读 TRS + 写 Local + 读 Local + 写 World | 读 TRS + 写 World | 仅对 TRS 真变了的行成立；级联行是**净增算术**（§2.1 设计原则，需计时对拍） |
| **标脏链路复杂度** | 两级 (`local_dirty` + `matrixDirty`) | **单级 (`matrixDirty`)** | 消除一个严格冗余的判据（`local_dirty ⊆ matrixDirty`，已实测） |

---

## 四、 详细任务实施计划 (Implementation Roadmap)

本重构计划分为四个相互依赖、可独立编译验证的步骤：

```
[Step 1: Storage 补齐 Context 反向指针]
                     │
                     ▼
[Step 2: Accessor 瘦身至 16B 并删除冗余 context*（无兼容层）]
                     │
                     ▼
[Step 3: 剔除 local_matrices 数组与 local_dirty（含账目 + 清 obj）]
                     │
                     ▼
[Step 4: 示例适配（选项 a/b）+ 全量回归 + 反复活护栏]
```

每步的硬验收（用户口径）：① 该步独立可运行（build 0 error → 固定验证集 → 示例抽跑）；② **同一批里删掉被取代的旧代码**，全仓 grep 零残留；③ 禁止“新旧并存”的兼容分支；④ 做不到一步内新旧都跑得通 ⇒ 继续切分而不是加兼容层。

---

### Step 1：在 `TransformDataStorage` 补齐 Context 反向引用
* **目标**：使任意持有 `TransformDataStorage*` 的代码均可反查所属世界，为 Accessor 瘦身铺路。
* **修改文件**：
  * `inc/hgl/ecs/support/TransformDataStorage.h`：增加 `ECSContext* context = nullptr;` 与 `SetContext`/`GetContext`（非拥有语义写注释）。
  * `src/ecs/core/Context.cpp`：`ECSContext::ECSContext` 构造函数里 `transform_storage->SetContext(this);`。
  * `inc/hgl/ecs/core/Context.h:177-179`：更新“两者互不引用”的注释。
* **反向兼容**：本步不改布局、不删字段 ⇒ 不触发 ABI 错位，但仍建议清 obj 保证一次干净编译。
* **验证标准**：全仓编译 0 error；`ctest --test-dir build -C Release -L "ecs" --output-on-failure` 全绿（15 项）。

### Step 2：`TransformAccessor` 瘦身至 16 字节
* **目标**：删除 Accessor 内的 `context` 成员与三参构造；`static_assert(sizeof == 16)`。
* **修改文件**：
  * `inc/hgl/ecs/support/TransformAccessor.h`：删成员 `context`；删三参构造与默认参数（**保留兼容构造 = 禁用项**，见 §〇）；`GetContext()` 改反查；加 `static_assert`。
  * `src/ecs/support/TransformAccessor.cpp`：`:48` `FromOwner`（零调用者 ⇒ 直接删或改双参）、`:286/:322/:358` 内部临时柄去掉 `context` 实参。
  * `src/ecs/core/Context.cpp:967`：`GetTransform()` 去掉 `this`。
* **验证标准**：
  * `sizeof(TransformAccessor) == 16` 断言成立；
  * **Test 8 必须仍然过**（Mobility 换边走到 `storage->GetContext()->MigrateTransform`）—— 这是本步唯一的真回归门；反证：把 `SetContext` 那行注释掉，Test 8 必须精确失败（“SetMobility(Movable) 后该行未进可动列表”），确认不是假绿。

### Step 3：彻底剔除 `local_matrices` 与 `local_dirty`
* **目标**：消灭常驻局部矩阵数组，改用循环内即时合成；行内存 **223 B → 158 B**。
* **修改文件**：`inc/hgl/ecs/support/TransformDataStorage.h`（删除清单见 §2.1 第 1 条；账目 19→17、Derived 128→64）。
* **执行顺序**：① 先按 §2.1 第 6 条**清 obj**；② 改代码；③ 再清一次 obj 后编。
* **验证标准**：
  * `TestTransformFlatStorage.exe`（Release）输出必须变为 `每行 223 − 65 = 158 B`（Test 9 打印项），且 Test 4 / 5 / 6 / 9 全绿；
  * 条件护栏：**不要把 `parent_dirty` 一起删掉**（Test 5 会红，这是本步的正确反证）；
  * 量化前提（动工前/动工后各一次）：深树场景 `UpdateDirtyWorldMatricesFlat()` 耗时对拍，把数字写进本节作为决策依据。

### Step 4：示例适配、反复活护栏与全量回归
* **目标**：示例按 §2.3 选项 (a)（推荐）或 (b) 适配，加上机器化“防复活”护栏，完成全量回归。
* **修改文件**：
  * `example/ApiValidation/ComputeTransformHierarchy.cpp`：走 (a) 则改 shader + SSBO 布局为 TRS 直算；走 (b) 则示例自持派生缓冲并注明。
  * `src/ecs/support/TestTransformFlatStorage.cpp:477-509`（Test 12 的 `SourceNeedle` 表，当前 21 条）：**新增两条**把“删干净”变成机器验收：
    `{ "inc/hgl/ecs/support/TransformDataStorage.h", "local_matrices", 0, ... }`、
    `{ "inc/hgl/ecs/support/TransformDataStorage.h", "local_dirty",    0, ... }`。
* **验证集执行（已按仓内实情订正）**：
  ```bash
  # 1. 编译：示例须显式进目标列表（否则后面没得跑）
  cmake --build build --config Release --target ULRE.ECS TestTransformFlatStorage TestCSMIncrementalPass ComputeTransformHierarchy

  # 2. CTest（标签 ecs / transform 均存在）
  ctest --test-dir build -C Release -L "ecs" --output-on-failure
  ctest --test-dir build -C Release -R TestCSMIncrementalPass --output-on-failure

  # 3. 示例：Release 里当前只有 46 个 exe，ClockUse/RecursiveCube 仅存在于 Debug
  #    ⇒ 要么把它们加进上面的 --target 列表，要么改跑 Debug 版；
  #    注意本仓限制：窗口不可见时 WorkManager 会整帧跳过，shell 里跑看不出“有没有动”
  #    ⇒ 判定依据用退出码 + 日志里的 error 行，不要拿截图当回归证据。
  ./build/out/Windows_64_Debug/ClockUse.exe
  ./build/out/Windows_64_Debug/RecursiveCube.exe
  ./build/out/Windows_64_Release/ComputeTransformHierarchy.exe
  ```

---

## 五、 风险控制与防复活契约 (Risk Control & Safeguards)

1. **多世界隔离性不能破**：
   * 严禁为了简化参数复活 `g_CurrentTransformStorage` 全局指针。必须坚持 `Accessor -> storage -> context` 的单向严格归属（全仓现为 0 命中，保持下去）。
2. **Mobility 换边通知不能丢**：
   * `TransformAccessor::SetMobility` 必须经 `storage->GetContext()->MigrateTransform(...)` 触发静态/动态槽位迁移，否则物体退化为“在原点静止”（实例拿默认行）。
   * **禁令**：不得保留“忽略 context 参数的兼容构造”——它把这条通知变成静默丢失。
3. **测试覆盖不得降级**：`TestTransformFlatStorage` 现有 **15 项**契约（跳洞安全、世代防僵尸、预算拒绝分配、稳定槽位、稀疏侧表、源码禁复活……），任何变动不得减少断言或屏蔽断言；“12 项”是错误计数。
4. **改存储成员布局必清 obj**：`TransformDataStorage` 为内联头，陈旧 TU 按旧偏移访问会表现为“垃圾值 / 首调用崩溃”，看起来像真回归。反证类构建尤其要先清中间目录再编，并核对 `.obj` mtime 晚于头文件 mtime。
5. **禁止示例侧双真源**：局部矩阵真源随 `local_matrices` 一起消失；示例若需要 mat4，只能从 TRS 现算（选项 a 放 GPU、选项 b 放示例私有缓冲并注明），不得回写引擎存储。
6. **同范式一致性**：`BoundingBoxAccessor`（`inc/hgl/ecs/support/BoundingBoxAccessor.h:52`）持有同样的冗余 `context`。**〔2026-10-11：该冗余已删 —— 批 1.2 A4 把 `BoundingBoxAccessor` 瘦到 16 B，与本文范式一致〕**本轮若只做 Transform，请在此处记录理由，避免留下两种范式。

---

## 六、 执行记录（2026-10-09：本批 Step 1/2/3 + Step 4 的示例适配已落地）

**落地范围**：Step 1 / Step 2 / Step 3 / Step 4 的示例适配——示例走 **选项 (a)**（TRS SSBO + Compute Shader 内合成）。
`TransformAccessor::GetLocalMatrix` **不新增**（引擎侧零消费者；示例已在 GPU 端从 TRS 现算）。

| 验收项 | 门槛 | 实测结果 |
|---|---|---|
| 全仓 Release 构建 | 0 error | `cmake --build build --config Release` → **rc=0**，388 个目标；仅有 Qt / TexConv 工具链**既有** LNK 警告（与本改动无关） |
| 契约测试 | 15 项全绿 | `TestTransformFlatStorage.exe` → **rc=0**；Test 9 打印 **「7 行 × 17 条平行数组 = 每行 158 B，其中派生/缓存 64 B」**（改造前 19 条 / 223 B / 128 B） |
| ctest ecs 套件 | 全绿 | `ctest -C Release -L ecs` → **9/9 passed**；`-R TestCSMIncrementalPass` → passed |
| Step 2 反证 | 抽掉 `transform_storage->SetContext(this)` ⇒ Test 8 必须精确失败 | **rc=10**，`Test 8 Failed: SetMobility(Movable) 后该行未进可动列表（渲染侧会取不到它的行 ⇒ 实例画在原点、且永不更新）`；恢复后复绿 |
| Step 3 反证 | 抽掉求值循环的 `parent_dirty` ⇒ Test 5 必须精确失败 | **rc=7**，`Test 5 Failed: dynamic cascade update failed, expected (20,5,2), got (10.00, 5.00, 2.00)`；恢复后复绿 |
| 示例（选项 a） | GPU 与 CPU 层级求值一致 | `ComputeTransformHierarchy.exe`（Release）：**`PASS: 212 nodes across 8 levels verified successfully! max_diff ≈ 3.8e-6 ~ 1.1e-5`**（GLSL 从 TRS 现算 vs CPU 平铺，阈值 1e-4） |
| 示例抽跑 | 启动/日志无错 | `RecursiveCube.exe` / `ClockUse.exe`（Release）启动正常、持续 Tick，日志无 error（仅 shader-cache `.meta` 缺失 = 缓存未命中，属正常路径） |
| 防复活护栏 | 机器验收 | Test 12 的 needle 表 **21 → 23 条**：新增 `TransformDataStorage.h` 的 `local_matrices` / `local_dirty` 必须 **0 次** |

**同批一起改掉的调用点**（API 变更与全部调用点同批，禁「新旧并存」）：
`src/ecs/core/Context.cpp`（构造期回填 + `GetTransform` 去 `this`）、`src/ecs/support/TransformAccessor.cpp`（`GetOwner` / `SetMobility` / 三处内部父柄 / 删除零调用者的 `FromOwner`）、
`inc/hgl/ecs/support/TransformAccessor.h`、`example/ApiValidation/ComputeTransformHierarchy.cpp`（TRS SSBO + GLSL 合成）、`src/ecs/support/TestTransformFlatStorage.cpp`（needle）。

**本批明确未做**：
1. `BoundingBoxAccessor` 的同类冗余 `context` 未动（本轮只做 Transform，理由见 §五.6）。
2. `RecursiveCube` / `ClockUse` 的**视觉动画**未验证：本仓 `WorkManager` 在窗口不可见时整帧跳过，shell 里跑看不出「有没有动」⇒ 只验证了启动与日志无错，需要目视时由本机开窗口跑。
3. **级联路径的 CPU 时间未做正式对拍**（§2.1「设计原则」里列的量化前提仍是待办）；本轮以 Test 4/5/6/9 的**正确性**为准，内存收益已实测（223 → 158 B）。
