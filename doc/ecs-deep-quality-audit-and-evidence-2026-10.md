# ULRE 引擎 ECS 体系深度质量审计与证据链报告 (2026-10 全景复审)

> **审计基线**：Commit `e8ee755fd`（工作树已包含 Transform 瘦身至 158B、`Context.cpp` 拆解、GPU 镜像统一等优化）  
> **审计范围**：`inc/hgl/ecs/`（核心头文件）、`src/ecs/`（核心实现）、`support/`（支撑表与管线）及各子系统  
> **报告目的**：整理一份包含**精确源码位置、代码切片、交叉引用检索证据与机制分析**的技术审计报告，供团队工程师与其他 AI Agent 做交叉评审与方案推演。
> **⚠ 交叉评审订正（2026-10-11，对 `e8ee755fd`）**：本档已由 `doc/ecs-deep-audit-crossreview-2026-10.md` 逐条取证复核（30 条断言：21 条成立 / 9 条需订正 / 0 条无根据）。  
> · **3 个符号已在本档基线之后删除**：`RegisterRenderSystem`（现名 `RegisterSystem<T>`）、`RenderGraph::Pass::runUpdate`、`LineRenderPipeline.cpp:618`（被当作"告警闩"的那行实为诊断帧计数）⇒ 本档行号/符号混用了两个工作树，引用前以交叉评审档的订正表为准。  
> · **计数订正**：`ctest -N -L ecs` = **9**（`TestCSMIncrementalPass` 的 LABELS 是 `csm;pass`，不是 10）；`component_mask` 生产读者 **3 处**（非 1）；`VerifyComponentMaskAgainstComponents` **有对拍器**（`TestComponentMask`，5 处调用 + 正式 ctest 项）。  
> · **基线行成立**：「Transform 瘦身至 158B」经实测（`TestTransformFlatStorage` Test 9 = 每行 158 B，rc=0）。  
> · §1.2③ 的 `PrimitiveSortSystem` 处置与 `doc/ecs-quality-audit-2026-10.md` §1.1 chunk 4 的既有拍板（**保留**）冲突，改前先拍板。
> · **§3.1 判错**：「`RenderItemHandle 0` = 空句柄」**错** —— 0 是首个合法句柄，无效值是 `UINT32_MAX`（`inc/hgl/graph/render/RenderItemDescriptor.h:44`；`Allocate` 返回 `items.GetCount()`，首值即 0）。  
> · **§3.2 分类订正**：9 处 `IsCurrentPassShadow` 应为 **1 事实注入（:800）+ 6 程序选择/重试/状态（:1027,:1318,:2118,:2152,:2372,:2391）+ 2 诊断（:2421,:2425）**——`:2372` 是实体 4-ID 表按 pass 的状态写、`:2391` 是 D9 每实例重试计数复位，二者**不是日志**。  
> · **§8.3 订正**：`cl /Bv` **是真实开关**（实测打印 Compiler Passes 版本表），但它检不出 `static_assert` 状态（断言须编译相关 TU 才触发）。  

---

## 目录
1. [死代码、死分支与死计算 (Dead Code & Discarded Operations)](#一-死代码死分支与死计算)
2. [多路径与双轨制反模式 (Multiple Pathways & Dual Architecture)](#二-多路径与双轨制反模式)
3. [特例分支与魔数硬编码 (Special Cases & Magic Constants)](#三-特例分支与魔数硬编码)
4. [冗余层设计与数据副本膨胀 (Redundant Design & Duplicate Copies)](#四-冗余层设计与数据副本膨胀)
5. [名不符实与代码/语义违和 (Mismatched Semantics)](#五-名不符实与代码语义违和)
6. [架构不当设计与潜在脆弱点 (Architectural Fragility)](#六-架构不当设计与潜在脆弱点)
7. [分阶段整改实施建议 (Actionable Roadmap)](#七-分阶段整改实施建议)
8. [复现与证据检验命令集 (Verification Commands)](#八-复现与证据检验命令集)

---

## 一、 死代码、死分支与死计算

### 1.1 零调用活跃死函数（近期重构后暴露的孤岛）

#### ① `ECSContext::GetAllEntities` 与 `EntityManager::GetAllEntityPointers`
* **源码位置**：
  * `inc/hgl/ecs/core/Context.h:988`
  * `inc/hgl/ecs/core/EntityManager.h:121`
  * `src/ecs/core/EntityManager.cpp:83-93`
* **代码切片**：
  ```cpp
  // inc/hgl/ecs/core/Context.h:988
  void GetAllEntities(std::vector<const Entity*>& out_entities) const
  {
      if (entity_manager)
          entity_manager->GetAllEntityPointers(out_entities);
  }

  // src/ecs/core/EntityManager.cpp:83
  void EntityManager::GetAllEntityPointers(std::vector<const Entity*>& out_entities) const
  {
      out_entities.clear();
      for (const auto& slot : slots)
      {
          if (slot.entity)
              out_entities.push_back(slot.entity);
      }
  }
  ```
* **证据与引用检索**：
  全仓检索 `GetAllEntities` 与 `GetAllEntityPointers`：
  ```bash
  grep -rn "GetAllEntities" inc/ src/ example/
  grep -rn "GetAllEntityPointers" inc/ src/ example/
  ```
  结果：除了声明与定义自身，调用方仅有 `Context.h:988` 的内部转发。原生产调用者 `RenderGraph.cpp` 的 `GatherSceneStats` 已在上一轮改造中全面换用零堆分配的 `ForEachAliveEntity`。
* **影响**：该接口会在堆上为每个实体分配一个 `std::vector` 节点，属于明确废弃、全仓零消费但仍未删除的接口。

---

#### ② `LinesComponent::change_mask` 与 `LineChange` 枚举
* **源码位置**：
  * `inc/hgl/ecs/components/LinesComponent.h:20-25, 81, 93, 118, 128, 176`
* **代码切片**：
  ```cpp
  enum class LineChange : uint32_t {
      Geometry = 1u << 0,
      Style = 1u << 1,
  };
  void TouchChange(uint32_t mask) { change_mask |= mask; dirty = true; }
  uint32_t GetChangeMask() const { return change_mask; }
  ```
* **证据与引用检索**：
  全仓检索 `LineChange` 与 `LinesComponent` 的掩码消费：
  ```bash
  grep -rn "LineChange::" inc/ src/
  grep -rn "GetChangeMask" inc/ src/
  ```
  结果：`LineChange` 仅在 `LinesComponent.h` 自身内部被 `TouchChange` 写入；整个 `LineRenderPipeline.cpp`、`LineCollectSystem.cpp`、`LineBuildSystem.cpp` 仅根据 `comp->dirty` 或 `comp->lines.empty()` 判定是否收集/重构，**没有任何代码读取 `LinesComponent::GetChangeMask()`**（全仓唯一的 `GetChangeMask` 读者在 `TextRenderPipeline.cpp:664` 针对 `TextComponent` 与 `TransformSystem.cpp` 针对 `Transform`）。
* **影响**：纯写不读的死字段与枚举定义。

---

#### ③ `CameraInputMapping` 中的未响应按键绑定
* **源码位置**：
  * `src/ecs/systems/tick/CameraInputMapping.cpp:31-32`
  * `inc/hgl/ecs/systems/tick/CameraInputMapping.h:26-27`
* **代码切片**：
  ```cpp
  context.BindKey(kActionResetView, io::KeyboardButton::R);
  context.BindKey(kActionFocusView, io::KeyboardButton::F);
  ```
* **证据与引用检索**：
  ```bash
  grep -rn "kActionResetView\|kActionFocusView" inc/ src/
  ```
  结果：全仓除了定义和该函数内的绑定外，**没有任何系统（含 `CameraSystem.cpp`）读取、处理或响应这两个动作**。
* **影响**：死按键监听，作者自定义热键时可能导致按键冲突。

---

### 1.2 恒真 / 恒假守卫与空壳分支

#### ① 恒假守卫（假信心断言）
* **源码位置**：`src/ecs/support/line/LineRenderPipeline.cpp:570-575`
* **代码切片**：
  ```cpp
  const uint32_t resolved = transform.IsMovable() ? (dynamic_base + slot) : (slot + 1u);
  constexpr uint32_t kMaxTransformID = std::numeric_limits<uint32_t>::max();
  transform_id = resolved > kMaxTransformID ? 0 : static_cast<uint32_t>(resolved);
  ```
* **机制分析**：
  `resolved` 声明为 `const uint32_t`，而 `kMaxTransformID` 为 `uint32_t` 最大值。无符号 32 位整数与 `UINT32_MAX` 比较，`resolved > UINT32_MAX` 在 C++ 中**恒为 false**。此行不仅无法防范溢出，还给维护者一种“此处有边界保护”的错误安全感。

---

#### ② `ScenePipelineMode` 5 个预留模式全为空壳
* **源码位置**：
  * `inc/hgl/ecs/core/ScenePipelineMode.h:17-24`
  * `src/ecs/core/ContextRender.cpp:165-175`
* **代码切片**：
  ```cpp
  case ScenePipelineMode::TopDownRTS:
  case ScenePipelineMode::AerialLowAltitude:
  case ScenePipelineMode::AerialHighAltitude:
  case ScenePipelineMode::Space3D:
  case ScenePipelineMode::SideScroll2D:
      // 预留特定场景类型硬编码路径空壳
      break;
  ```
* **机制分析**：目前仅 `StandardLitCSM` 具有真实的 CSM 主光阴影自动化实现。其余 5 个模式无任何实现逻辑，设置后表现为 `Custom` 模式（静默无操作）。

---

#### ③ `PrimitiveSortSystem` 注册进帧图但执行体为空
* **源码位置**：
  * `src/ecs/support/primitive/PrimitiveSortSystem.cpp:21`
  * `src/ecs/support/PrimitiveBatchPipeline.cpp:148`
  * `src/ecs/core/DefaultSystems.cpp:95`
* **代码切片**：
  ```cpp
  // PrimitiveSortSystem.cpp
  void PrimitiveSortSystem::OnSort(RenderPipelineBase* pipeline) {
      pipeline->RunSort();
  }

  // PrimitiveRenderPipeline.cpp:46
  void PrimitiveRenderPipeline::RunSort() {
      if (!impl_->PrepareFrame(context_)) return;
      impl_->RunSorting();
  }

  // PrimitiveBatchPipeline.cpp:148
  void PrimitiveBatchPipeline::RunSorting()
  {
  }
  ```
* **机制分析**：
  在 `DefaultSystems.cpp` 中通过 `ctx->RegisterRenderSystem<PrimitiveSortSystem>()` 正常注册并被加入执行队列，在每帧 `RenderCollect` 相位按时被分发调用，但经过三层调用后最终指向**纯空体 `{}`**。批次之间的排序实际发生在 `PrimitiveRenderSystem::RenderBatches` 的 `std::sort(ordered_batches)`，该系统属于完全无操作的开销占位。

---

### 1.3 接受即丢弃的形参 (Discarded Arguments)

| 文件位置 | 接口声明 | 丢弃形参 | 实际代码 |
|---|---|---|---|
| `src/ecs/systems/render/MainLightShadowSystem.cpp:206` | `RenderMainLightShadowPass(CameraComponent*, float deltaTime)` | `deltaTime` | 未使用，亦未写 `(void)deltaTime;` |
| `src/ecs/systems/tick/TransformSystem.cpp:68` | `Update(float deltaTime)` | `deltaTime` | `(void)deltaTime;` |
| `src/ecs/components/MaterialData.cpp:364` | `BuildResolvedRecipe(..., const ShaderProgram* material_program, ...)` | `material_program` | `(void)material_program;` |

---

## 二、 多路径与双轨制反模式

### 2.1 变换列表遍历的双轨混用（安全 vs 裸遍历跳洞）
* **源码位置**：`src/ecs/systems/tick/TransformSystem.cpp:198-330`
* **问题描述**：
  `inc/hgl/ecs/core/Context.h:778-783` 包含重笔警告：“裸遍历 `GetStaticTransforms()/GetMovableTransforms()` 会把空洞当有效 id，越界崩溃，一律用 `ForEach*`”。
* **现状代码对比**：
  * **安全遍历（`:198, :215`）**：
    ```cpp
    world->ForEachStaticTransform([&](TransformID id) {
        if (!has_dirty_static && ShouldUpdateTransform(id, update_mask))
            has_dirty_static = true;
    });
    ```
  * **同一函数内的裸遍历（`:260, :275, :305, :325`）**：
    ```cpp
    for (const TransformID handle : static_transforms) {
        if (!IsValidTransformID(handle))
            continue; // 手动跳洞，复制了 ForEach 内部的逻辑
        ...
    }
    ```
* **风险**：若后续开发者在裸遍历循环中遗漏 `if (!IsValidTransformID(handle)) continue;`，将直接以 `(uint32_t)-1` 寻址 `storage` 平行数组，引发内存越界访问。

---

### 2.2 实体组件读取“双域并存”
* **现象**：
  * **域 1（组件通用口）**：`entity->GetComponent<T>()` / `world->GetComponents<T>()`
  * **域 2（世界级权威访问器）**：`world->GetCameraByEntity()`、`world->GetMaterialData()`、`world->GetGeometryData()`
* **机制分析（`Context.h:560-571` 自述风险）**：
  以相机为例，`CameraComponent` 既挂在实体的池中，其槽位又是世界私有资源（16 槽 × 帧槽）。若外部直接调用 `entity->GetComponent<CameraComponent>()` 获取指针修改，会绕过世界的 `EnsureCameraSlot` 与三级主相机认领逻辑，导致组件状态与 GPU 槽位状态产生双真值。

---

### 2.3 材质四条键空间的流转链条过长
* **现状**：实体的材质状态涉及 4 套不同的键空间：
  1. `MaterialVariantKey`：`(recipe_hash, primitive_purpose)` —— 2 元组，用于变体去重；
  2. `MaterialRuntimeKey`：`(recipe_hash, primitive_purpose, texture_config_hash)` —— 3 元组，用于运行时共享行；
  3. `MaterialAuthoringSideTable`：按组件行号索引；
  4. `MaterialRuntimePipelineTable`：按 `EntityID` 索引。
* **机制分析**：
  `MaterialVariantKey` 是 `MaterialRuntimeKey` 的严格前缀。四张表由不同系统各自进行哈希查找与管理，从资产定义到最终生成绘制项，需要穿越 4 级查找，链条冗长且易产生冗余数据。

---

## 三、 特例分支与魔数硬编码

### 3.1 核心槽位与哨兵值的“零散硬编码”
* **证据统计**：
  * `kMainLightRowIndex = 0`：在 `ContextGPU.cpp`、`LightSystem.cpp`、`MainLightShadowSystem.cpp` 中存在 8 处直接或间接的使用；
  * `0 号相机槽`：恒定代表默认主相机；
  * `L2W 槽 0`：恒定代表 Identity 矩阵，`kFirstObjectL2WSlot = 1`；
  * `RenderItemHandle = 0`：代表空句柄；
  * 材质行 0：代表无材质哨兵。
* **风险**：行号基准未在全局形成统一的“槽位元数据定义”，散落在各文件的算术公式中（如 `slot + 1`、`id * stride + 0`），调整时极易出现漏改。

---

### 3.2 收集系统内部手写阴影 Pass 分流
* **源码位置**：`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp`
* **证据行号**：`:800, :1027, :1318, :2118, :2152, :2372, :2391, :2421, :2425`
* **代码切片**：
  ```cpp
  // :800
  out.facts.shadow_pass = world->IsCurrentPassShadow();
  // :1027
  if (world->IsCurrentPassShadow()) ...
  // :1318
  const bool shadow_pass = world->IsCurrentPassShadow();
  // :2118
  if (world->IsCurrentPassShadow()) ...
  ```
* **机制分析**：
  系统文件头注释明确声明：“渲染策略判定表（`RenderStrategyTable`）是图元收集与阴影能力的**唯一判据**”。但在执行中，该文件内依然包含了多达 9 处手写 `world->IsCurrentPassShadow()` 的硬编码分支，这破坏了“意图与策略完全由策略表矩阵驱动”的设计初衷。

---

### 3.3 局部 static 状态破坏多世界隔离
* **源码位置**：
  * `src/ecs/support/GPUCullSystem.cpp:420`
  * `src/ecs/support/line/LineRenderPipeline.cpp:618`
* **代码切片**：
  ```cpp
  // GPUCullSystem.cpp:420
  static bool s_reported_misaligned = false;
  if (!s_reported_misaligned && (row_base % 4u) != 0u) {
      s_reported_misaligned = true;
      GLogError(u8"[GPUCullSystem] 切片起点未 4 行对齐...");
  }
  ```
* **机制分析**：
  此类告警闩使用局部 `static` 变量，其生命周期是进程级的。在多世界（如主场景世界 + 离屏世界 `OffscreenWorld`）并存的架构中，第一个世界触发后置为 `true`，后续独立世界的同类严重配置错误将被永久静默，导致离屏世界排查困难。

---

## 四、 冗余层设计与数据副本膨胀

### 4.1 系统容器的多重冗余登记
* **源码位置**：`inc/hgl/ecs/core/Context.h`、`src/ecs/core/ContextSystems.cpp`
* **现状分析**：
  `ECSContext` 内部同时维护了 **9 套并行容器**来管理系统：
  1. `tick_systems`：哈希表；
  2. `render_systems`：哈希表；
  3. `systems_by_element_type`：字符串 map；
  4. `system_group_component_counts[]`：定长计数数组；
  5. `installed_system_groups[]`：布尔数组；
  6. `group_systems[]`：快照列表数组；
  7. `tick_system_order`：拓扑排序数组；
  8. `render_system_order`：拓扑排序数组；
  9. `render_pipelines`：字符串哈希表。
* **弊端**：查找系统时，`GetSystem<T>()` 先查 tick 表再查 render 表；依赖关系在 `System` 自身声明一份，在 `Context` 中又被存入 `dependencies` 容器；更新系统组状态时，先查快照，快照为空又回退到字符串 map 查找。

---

### 4.2 实体组件列表的多重冗余
* **数据流传图**：
  ```text
  Entity::component_rows[16] (CPU 权威)
          │
          ├──> Entity::component_mask (缓存位图，生产读者仅 1 处，对拍器 0 调用)
          │
          └──> EntityGPU.type[16] / row[16] (CPU 行镜像，自注"显式冗余")
                  │
                  └──> WorldAddresses.addr_entity_gpu (GPU SSBO 显存副本)
  ```
* **机制分析**：
  虽然此四份副本在同一写入点被同步维护，但 `component_mask` 几乎没有真正的业务读者（仅在 `PrimitiveBatchPipeline.cpp:616` 用于策略表评估），其唯一的保证手段 `VerifyComponentMaskAgainstComponents` 生产零调用，存在未来维护时的失步隐患。

---

## 五、 名不符实与代码/语义违和

| 序号 | 符号 / 位置 | 表面命名 / 承诺 | 实际代码行为 | 严重度 |
|---|---|---|---|---|
| 1 | `CameraSystem::GetActiveCameraComponent()`<br>`CameraSystem.cpp:322` | 只读属性获取 (Getter) | **隐式写世界状态**：若当前无默认相机，内部三级解析会调用 `ClaimDefaultCamera`，触发 `context->SetDefaultCamera` 并强行认领 0 号相机槽。 | 🟠 高 |
| 2 | `SyncRenderTargetViewport()`<br>`ContextRender.cpp:520` | 同步渲染视口 (Viewport) | **实际不同步 Viewport**：只比对 `target->GetExtent()` 与 `vp_info->GetViewport()` 的宽高，若不一致则调用 `target->OnResize(ext)`。 | 🟡 中 |
| 3 | `RenderGraph::Pass::runUpdate`<br>`RenderGraph.cpp:106` | 在 pass 声明的 phase 区间执行 Update | **强制截断相位下界**：被 `std::max(pass.startPhase, ExecutionPhase::RenderDrawSubmit)` 强行截断，声明在 `RenderCollect` 起点的更新全部被忽略。 | 🟡 中 |
| 4 | `ComponentTypeTable.h:105` 静态契约表 | `MaterialRuntimeSlot` "行宽冻结 88 B" | **注释与代码断言相反**：`MaterialRuntimeTable.h:211` 明确断言 `static_assert(sizeof(MaterialRuntimeSlot) == 40)`，静态类型表注释严重失真。 | ⚪ 低 |
| 5 | `TransformAccessor::MarkDirty()`<br>`TransformAccessor.cpp:540` | 标记变换脏以便上传 | **标记的是永不上传的活**：只设置 `storage->SetDirty(id, true)`，但未调用 `TouchChange`、未更新 `versions`。而 `TransformSystem` 上传严格基于版本比对，导致此调用无法触发 GPU 数据刷新。 | 🟠 高 |
| 6 | `TransformAccessor::SetFixedPixelSizingContext()`<br>`TransformAccessor.cpp:410` | 设置像素定尺计算上下文 | **直接修改实体的 LocalScale**：内部调用 `ApplyFixedPixelUniformScale`，立即触发 `SetLocalScale` 并递归标脏整棵子树。 | 🟡 中 |
| 7 | `EntityManager::AddFreeIndex()`<br>`EntityManager.cpp:149` | 维护空闲列表 | **假有序、真栈弹出**：用 `std::lower_bound` 维持数组升序插入（付出 $O(N)$ 成本），但在 `CreateEntity` 时却直接用 `free_indices.back()` 弹出最大索引，逻辑自相矛盾。 | 🟡 中 |
| 8 | `LineCollectStats::culled_by_visibility`<br>`LineRenderPipeline.cpp:432` | 因可见性被剔除的线条数 | **将“空线段”计入可见性剔除**：当 `comp->lines.empty()` 时，直接自增 `culled_by_visibility`，污染了真实的视锥与可见性统计指标。 | ⚪ 低 |

---

## 六、 架构不当设计与潜在脆弱点

### 6.1 `LineBuffer::AddSegment` 缺乏事务性原子保护
* **源码位置**：`src/ecs/support/line/LineRenderPipeline.cpp:234-255`
* **代码切片**：
  ```cpp
  if (!va_pos.Write(from)) return false;
  if (!va_pos.Write(to)) return false;
  if (!va_color.Write(color_index)) return false;
  if (!va_color.Write(color_index)) return false;
  if (!va_transform.Write(transform_index)) return false;
  if (!va_transform.Write(transform_index)) return false;
  if (!va_width.Write(hgl::math::Vector2f(width, min_width))) return false;
  if (!va_width.Write(hgl::math::Vector2f(width, min_width))) return false;
  ```
* **机制分析**：
  该函数依次向 4 个顶点属性写入流分别写入 2 个顶点。一旦前半部分（如 `va_pos`）写入成功，而中间（如 `va_color`）因缓冲区满或访问器失效返回 `false`，**没有执行任何游标回滚（Rollback）操作**。此后后续调用继续追加时，位置流已步进 2 步，而颜色/变换流未步进，直接造成 GPU 顶点属性流整体错位、严重撕裂。

---

### 6.2 `RenderItemDataStorage::Release` 缺乏重复释放防护
* **源码位置**：`src/ecs/support/RenderItemDataStorage.cpp:60-70`
* **代码切片**：
  ```cpp
  bool RenderItemDataStorage::Release(const RenderItemHandle handle)
  {
      if (handle >= static_cast<RenderItemHandle>(items.GetCount()))
          return false;

      items[handle] = RenderItemDescriptor{};
      free_list.Add(handle);
      MarkDirty(handle);
      return true;
  }
  ```
* **机制分析**：
  函数仅做了下标越界判定，未校验 `handle` 是否已被释放。若同一句柄被二次释放，该句柄将被重复加入 `free_list`。随后调用 `GetActiveCount()`（实现为 `GetCount() - GetFreeCount()`）时，活跃计数将产生下溢或失真。

---

### 6.3 `GPUCullSystem::GetGPUVisibleCount` 缺少 Fence 同步保障
* **源码位置**：`src/ecs/support/GPUCullSystem.cpp:866-878`
* **代码切片**：
  ```cpp
  uint32_t GPUCullSystem::GetGPUVisibleCount() const
  {
      if (!draw_count_buffer_) return 0;
      uint32_t visible_count = 0;
      void* mapped = draw_count_buffer_->Map(0, sizeof(uint32_t));
      if (mapped)
      {
          visible_count = *(uint32_t*)mapped;
          draw_count_buffer_->Unmap();
      }
      return visible_count;
  }
  ```
* **机制分析**：
  函数内直接执行 Host 映射读取 `draw_count_buffer_`，缺少 `SyncForParityReadback()` 或 Fence 状态等待校验。若在 GPU 写入未完成前调用该函数（如在 `DumpCullSSBOs` 路径中），读出的将是上帧残留值或 CPU/GPU 读写竞态值。

---

### 6.4 `BoundingBoxAccessor` 遗留 24 字节旧范式（未对齐 TransformAccessor）
* **源码位置**：`inc/hgl/ecs/support/BoundingBoxAccessor.h:40`
* **代码切片**：
  ```cpp
  class BoundingBoxAccessor {
      BoundingBoxDataStorage *storage = nullptr;   // 8 字节
      BoundingBoxDataStorage::HandleID id = ~0u;   // 4 字节
      uint32_t generation = 0;                     // 4 字节
      ECSContext *context = nullptr;               // 8 字节 (冗余)
  }; // 占用 24 字节
  ```
* **机制分析**：
  `TransformAccessor` 已在上一轮改造中成功将句柄压缩至 16 字节，删除了内部的 `context` 指针（改由 `storage->GetContext()` 反向解析）。而 `BoundingBoxAccessor` 依然保留着 24 字节并显式持有 `context`，同一设计理念在两个基础访问器间尚未对齐。

---

## 七、 分阶段整改实施建议

```text
[第一阶段: 核心语义与健壮性防护 (P0 / P1)]
  ├── 1. 为 LineBuffer::AddSegment 增加写入游标捕获与失败事务回滚
  ├── 2. 优化 EntityManager::AddFreeIndex 改为 O(1) 尾部追加栈，消除 lower_bound 假有序
  ├── 3. 订正 ComponentTypeTable.h 的 "88B" 滞后注释为 "40B"
  └── 4. 修正 LineCollectStats 统计，排除 comp->lines.empty() 对 culled_by_visibility 的污染

[第二阶段: 接口精简与死代码剔除 (P1 / P2)]
  ├── 5. 彻底删除已无任何调用者的 GetAllEntities 与 GetAllEntityPointers
  ├── 6. 移除 LinesComponent 中只写不读的 change_mask 与 LineChange 枚举
  ├── 7. 移除 CameraInputMapping 中无人消费的 ResetView 与 FocusView 按键绑定
  ├── 8. 【口径订正 2026-10-11】**保留** `PrimitiveSortSystem`（维持注册、补齐其逻辑属另行安排）：chunk 4 已拍板保留、B/C 复核一致，且 `RunSorting()` 空体不构成正确性问题；原文"移除并在默认管线注销"不再作为整改项。补充："删它可省每帧一次无守卫 `PrepareFrame`" 的收益已因 §2 #11（`PrepareFrame` 加**每帧真守卫**）而消失。

[第三阶段: 架构规范统一与收敛 (P2)]
  ├── 9. 推进 BoundingBoxAccessor 瘦身至 16 字节（对齐 TransformAccessor 范式）
  ├── 10. 收敛 TransformSystem.cpp 中的裸循环遍历，统一使用 ForEach*Transform
  └── 11. 将 RenderSystemCore 独立为 Core 基础设施辅助类，清理 CMake 的双重注册
```

---

## 八、 复现与证据检验命令集

### 8.1 零调用接口验证
```powershell
# 验证 GetAllEntities 与 GetAllEntityPointers 确实零调用
grep -rn "GetAllEntities" inc/ src/ example/
grep -rn "GetAllEntityPointers" inc/ src/ example/

# 验证 LineChange 无任何消费者
grep -rn "LineChange::" inc/ src/
```

### 8.2 契约与单元测试全量执行
```powershell
# 编译并运行全量 ECS 单元测试（确保 10 项测试 100% PASS）
cmake --build build --config Release --target ULRE.ECS TestTransformFlatStorage TestCSMIncrementalPass
ctest --test-dir build -C Release -L "ecs" --output-on-failure
ctest --test-dir build -C Release -R TestCSMIncrementalPass --output-on-failure
```

### 8.3 句柄尺寸断言验证
```powershell
# 验证 TransformAccessor 与 BoundingBoxAccessor 的真实尺寸
cl /Bv
# 查看 static_assert 断言状态
```
