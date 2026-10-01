# stage B 欠账清单（存储形：行 arena / 访问器 / EntityGPU / 预算冻结）

> 采集基线：工作区 A7c 完成态（HEAD = `19f2b1c5e`「A7b」+ A7c 未提交改动），采集日期 2026-10-01。
> **凡本文给出的事实一律为代码实测**（附 `path:line`）；**没有实测**的（行字节数、GPU 实机行为、未跑过的路径）一律标
> 「未实测」或「待定」，不写成结论。
> 本文只记录**已知现状与差距**，不是愿望清单；「要不要做 / 做成什么形状」由用户拍板。

---

## 0. 一句话

stage A（A0–A7）已把**组成、职责与判据**定形（组件类型表 / 位掩码 / 三张材质表 / `GeometryData` / `ShadowProxy` /
`PrimitiveState` / 世界级相机访问器 / 策略表唯一判据）；**存储表示**仍是 OOP 组件 + 每类自建容器。

stage B 的六件事里：**Transform 一类已行存储化（T8 完成）**，其余**全部未开工**。

---

## 1. 六件事逐条：现状 / 差距

### 1.1 按 (scope, 类型) 行 arena —— 只有"表"，没有"arena"

**已落地**
- 类型表：`inc/hgl/ecs/support/ComponentTypeTable.h:26-37`（枚举 `ComponentType{None,Transform,Geometry,MaterialData,MaterialRuntime,Texture,End}`，
  `COMPONENT_TYPE_COUNT = 6`）；`:66-75` 表本体（type / scope / arena_name / gpu_row_bytes / cpu_type_name）；
  `:80-91` constexpr **同序自检**（漏登记是编译错误）；`:93-118` 查询函数；`:121-135` 位掩码；`:152-166` implies 表 + 闭包自检。
- 唯一有真实行存储的类型：**Transform** —— `:70` `{ComponentType::Transform, World, "ECS:Transform", 64, "TransformDataStorage"}`。

**差距**
1. **四类没有 GPU 行**：`:71-74` 里 Geometry / MaterialData / MaterialRuntime / Texture 的 `gpu_row_bytes = 0`，
   且 `cpu_type_name` 直接写着 `"(待 ID 化)"`。
2. **arena 只是字符串，没有分配器**：`arena_name`（`:54`）在全仓的**唯一消费者是一个契约测试**
   （`src/ecs/support/TestTransformFlatStorage.cpp:30` 只校验它非空）；**没有**任何"(scope,type) → arena 分配器/注册表"的运行时实体
   （`grep -rn 'GetComponentTypeInfo|GetComponentGPURowBytes|GetComponentScope' inc/ src/` 只命中 `TestTransformFlatStorage.cpp:42-43` 两条 static_assert）。
3. **scope 列有 3/6 是保守默认**：Geometry / MaterialData / Texture 记 `Global`，MaterialRuntime / Transform 记 `World`；
   表头自己注明"后四类的 scope 待各自 ID 化时定稿，现记 World 作保守默认"（`:62-64`，注意当前实际值已是混合态）。

### 1.2 访问器（值类型薄句柄）

**已落地**：`TransformAccessor`（`inc/hgl/ecs/support/TransformAccessor.h`）与 `BoundingBoxAccessor`
（`inc/hgl/ecs/support/BoundingBoxAccessor.h`）。

**差距**：其余类型仍走**裸指针**世界入口，且注释自己写明"stage B 会换成值类型句柄"：
- `MaterialData* GetMaterialData` / `GetOrCreateMaterialData`（`inc/hgl/ecs/core/Context.h:553-556`、`:560-561`）
- `GeometryData* GetGeometryData` / `GetOrCreateGeometryData`（`Context.h:566-569`、`:573-574`）
- `ShadowProxy* GetShadowProxy`（`Context.h:580-583`）
- 相机（A7c 新增）：`CameraComponent* GetCameraByEntity` / `GetOrCreateCamera`（`Context.h:461-464`、`:471-472`）
- 材质运行期 slot：`MaterialRuntimeSlot& GetOrCreateMaterialRuntimeSlot` / `MaterialRuntimeSlot* GetMaterialRuntimeSlot`（`Context.h:662-682`）

**句柄形态（A7c-2 起）**：上列每个访问器都有**两种句柄重载**——`EntityID` 版（只持有 ID 的引擎内部代码）与 `Entity*` / `const Entity*` 版（手里已有实体的作者/系统代码）。两者是**不同的句柄、不是兼容层**：
- 实现体抽成接收 `Entity*` 的**核心**：`ComponentOfEntity<T>(Entity*)` / `GetOrAddComponentOfEntity<T>(Entity*, Args&&...)`（`src/ecs/core/Context.cpp:1625`、`:1631`、`:1637`）；
- `EntityID` 版只做一次 `GetEntity(owner)` 后转调（**每个重载恰好一次**，全文件 `GetEntity(owner)` 计 11 处）；
- 指针版**直通核心、零查询**（`GetEntity` 在指针路径上为 0 处）。

⇒ **stage B 换存储时，一个访问器的两个重载必须一起改**（同一个存储的两个入口，留一个就是半改状态）。契约钉在 `src/ecs/support/TestCSMIncrementalPass.cpp` 的 **Test 28**（行为 + 计数式源码契约）。

### 1.3 `EntityGPU`（128 B）

**已落地**：CPU 侧位掩码 `Entity::component_mask`（`inc/hgl/ecs/core/Entity.h:30`），
**单一写者**=组件挂载/卸载路径（`ReplaceComponent` 置位、`RemoveComponent` 清位，`Entity.h:107`），
类型表里注明"它就是 stage B `EntityGPU::type[16]` 的前身"（`Entity.h:29`、`ComponentTypeTable.h:121`）。

**差距**
1. `EntityGPU` **没有一行实现**：`grep -rn EntityGPU inc/ src/` 只命中 `Entity.h:29`、`ComponentTypeTable.h:121`
   两处**注释**，无任何代码/类型/字段。
2. 定稿形状在文档侧：`doc/future/ULRE_FINAL_TARGET_v2_设计约束.md:67`
   （`flags(4) + type[16](16) + row[16](64) + work_flags(4) = 88 B` 载荷 ⇒ `alignas(64)` ⇒ 128 B）
   与 `doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md:1172`。
3. 当前 `Entity` 每实体一份 `hgl::UnorderedMap<std::size_t, std::shared_ptr<Component>> components`
   （`Entity.h:33`）+ 逐组件 `make_shared`（`Entity.h:71-77`）⇒ 与"取消逐 new"（`:141`）相冲。

### 1.4 `.ulrescene` 直载

**现状：0 命中。** `grep -rn ulrescene inc/ src/` 无命中（只在 doc 出现）；
`SceneHeader` / `LoadSceneBlit` 同样 0 命中（审计口径见 `doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md:198`）。

**状态：已延后（用户拍板 2026-09-30）**：前提是"全部 Component ID 化 + 访问器完成"，
且要先解决**子场景树的快速插入/展开**（`doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md:1167-1172`）。

### 1.5 预算冻结 T10（容量与行号冻结）

**局部已有（实测）**
- `MaterialRuntimeTable::kMaterialRuntimeCapacityLimit = 4096`，超限 fail-fast
  （`inc/hgl/ecs/support/MaterialRuntimeTable.h:223`，语义见 `:39`）。
- `MaterialVariantTable::kMaterialVariantCapacityLimit = 4096`（`inc/hgl/ecs/support/MaterialVariantTable.h:141`，语义见 `:59`）。
- 相机槽：`kSlotCapacity = 16` + `static_assert`（`inc/hgl/ecs/support/CameraInfoStorage.h:48`、`:54`），
  行空间 `kRowCount = 16 × HGL_FRAME_SLOT_TOTAL`（`:50`）。

**差距（T10 第 1–4 条，逐条对照 `doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md:1158-1164`）**
1. **行号仍会漂移（最硬的一条未做）**：`TransformAssignmentBuffer::EnsureCapacity(...)`
   （`src/ecs/support/TransformAssignmentBuffer.cpp:163`）与随后的 `"[TransformAssignmentBuffer] L2W recreated"`
   （`:511`）**都还在**；`doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md:1161` 明写要删掉该路径。
   同页 `:179` 也把"当前 `TransformDataStorage` 用 `hgl::ValueArray` 自增长、无分区无上限"记为**固定容量不成立**。
2. **没有声明式预算/预分配 API**：`grep -n 'Initialize' inc/hgl/ecs/support/TransformDataStorage.h` **零命中**
   ⇒ 文档里的 `Initialize(max_statics, max_dynamics)` 形态尚未存在。
3. **行字节瘦身（`fixed_pixel` / `children` 改稀疏侧表）未做**；文档写的「每行 ≈267 B → ≈211 B」
   （`doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md:1159` 附近）是**文档口径**，
   本轮**未实测**当前每行字节 —— **未实测**。
4. 探针复核：`src/ecs/support/ProbeTransformDiagnostics.cpp`（CMake 目标 `ProbeTransformDiagnostics`）在，本轮**未运行**。

### 1.6 T11（`EntityGPU` 128 B）与 `.ulrescene`

同 §1.3 / §1.4，两件**同批延后**（`doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md:1167`）。

---

## 2. A0–A7 遗留的特殊项

### 2.1 `CameraComponent` 组件的终态去向

**现状（A6 拨正 + A7c 收口示例后）**
- 相机是**世界级资源**：真源 = 世界相机表的**槽账目**（槽 → 宿主弱引用）
  `inc/hgl/ecs/support/CameraInfoStorage.h:129`（`std::weak_ptr<CameraComponent> slot_owners[kSlotCapacity]`）。
- 组件只描述**参数**（文件头定稿 `inc/hgl/ecs/components/CameraComponent.h:14-38`）；
  解算输入/结果留在组件内：`graph::Camera camera_data`（`:77`）、`graph::CameraInfo camera_info`（`:80`）、
  `const graph::ViewportInfo* viewport_info`（`:84`）。
- 世界级访问器（A6 落地 + A7c/A7c-2 补实体入口）：`Context.h:442`（`GetCamera(slot)`）、`:445`（`GetDefaultCamera`）、
  `:448`（`SetDefaultCamera`）、`:451`（`GetCameraSlot`）、`:461-464`（`GetCameraByEntity`：ID 版 + `Entity*`/`const Entity*` 版）、
  `:471-472`（`GetOrCreateCamera`：ID 版 + `Entity*` 版）、`:476`（`EnsureCameraSlot`）、`:480`（`ReleaseCameraSlot`）、
  `:485`（`EnsureFallbackCamera`）、`:492`（`GetFallbackCamera`）。

**差距 / 待定**
- **相机不在 `ComponentType` 枚举里**（`ComponentTypeTable.h:26-37` 六项无 Camera）⇒ 相机没有 (scope, type) arena，
  也没有 GPU 行；它的 19 个参数字段仍是 OOP 组件字段。
- **待定**：stage B 相机是 ① 进 `(World, Camera)` 行 arena + 值类型句柄，还是 ② 参数并入现有世界相机行空间。
  当前实现**两头都在**（世界表管槽/行，组件管参数）——这是 stage B 必须一次收口的分叉，**尚无定稿**。
- 两条**非实体**相机路径要一起改：`EnvironmentSystem` 的 `AutoCSMLightCamera`（系统内建，`make_shared` 创建，
  不进 `component_registry`）与常驻 fallback（`Context.h:478`/`:485`），槽宿主走 `CameraSlotGuard`
  （`inc/hgl/ecs/support/CameraSlotGuard.h`）。

### 2.2 `MaterialRuntimeSlot` 里的每实例字段

**现状**：每实例小记录 `MaterialRuntimeSlot`（`inc/hgl/ecs/support/MaterialRuntimeTable.h:162` 起），
按实体稀疏存放（`std::unordered_map<EntityID, MaterialRuntimeSlot> slots`，`:237`）。字段分四组：
- ① 共享行引用 `row`（`:166`）；
- ② 每实例选择器 `pass_selector` / `lod_selector` / `dither_enabled`（`:169-171`）；
- ③ D9 重试/降频 `shadow_retry_frames`（A4 从共享变体记录迁来，`:176`）+ 授权代跟踪副本（`:180-181`）
  + 生命周期/脏标志（`:184-187`）；
- ④ **A5b 从已删渲染组件迁来的每实例绑定**：`render_item_handle`、`runtime_pipeline_pass` / `runtime_pipeline`、
  多实例 `instance_count` / `max_instances` / `allocated_instance_capacity`、
  4 个 `graph::DeviceBuffer*`（L2W / L2W 索引 / MeshDrawCommand / MaterialInstanceAddresses）、
  间接绘制 `IndirectMeshTaskBuffer*` + `indirect_count_buffer` + offset、`is_gpu_driven` / `is_indirect`（`:190-215` 一带）。

**待定**：④ 里的**指针/句柄字段**在「行 arena + 值类型句柄」模型下**不能照搬**（行内不得放重型/指针类型，
参见 T8 的 `math::AABB` 352 B 教训）。要么改成**资源池行号**，要么留在世界侧旁表。**当前没有定稿**。

### 2.3 `Entity::component_mask` → `EntityGPU::type[16]` 的对应关系

- 现状：`component_mask` 是 `uint32_t` **位掩码**（`Entity.h:30`），位 = `1u << (uint32_t)ComponentType`
  （`ComponentTypeTable.h:124-131`），`None` 返回 0 位 ⇒ 永不占位。
- 定稿形状：`EntityGPU::type[16]` = **16 个槽位 × (类型 1 B + 行号 4 B)**（`doc/future/ULRE_FINAL_TARGET_v2_设计约束.md:67`）。
- **映射关系未实现、也未定稿（待定）**：位掩码表达"有哪些类型"（≤ 5 位），`type[16]` 表达
  "第 n 号槽是什么类型 + 落在哪一行"。**16 个槽位 ≠ 6 个类型种类**（`COMPONENT_TYPE_COUNT = 6`，`ComponentTypeTable.h:39`）；
  槽位到底是"每实体最多 16 个组件实例"还是"类型固定位次"，实仓**没有任何代码可依据** ⇒ **待定**。
  若 `type[16]` 意为"类型种类可达 16"，则 `ComponentType` 需要扩容。

### 2.4 `system_group_component_counts`（按组**字符串**计数，键域 3 个）

- 声明：`std::unordered_map<std::string, uint32_t> system_group_component_counts`
  （`inc/hgl/ecs/core/Context.h:105`）。
- 写者（唯一两条）：`RegisterComponentInstance` 取 `comp->GetSystemGroupName()` 字符串 → 计数 +1 →
  `EnsureSystemGroupSystems` + 组启用（`src/ecs/core/Context.cpp:1750-1759`，计数行 `:1755`）；
  `UnregisterComponentInstance` → 计数 -1 → 归零则关组（`:1781-1793`，查找行 `:1783`）。
  `clear()` 在 `:264`、`:307`。
- **键域实测就是 3 个字符串**：`"Primitive"`（`inc/hgl/ecs/components/GeometryData.h:76`）、
  `"Line"`（`inc/hgl/ecs/components/LinesComponent.h:59`）、
  `"Text"`（`inc/hgl/ecs/components/TextComponent.h:56`）；契约钉在
  `src/ecs/support/TestCSMIncrementalPass.cpp:4071-4082`（A7a Test 25）。
- **为什么 stage B 关注它**：它在**运行期按字符串**做哈希查找，并驱动"随组件挂卸自动装/启系统"。
  stage B 把组件换成"类型 → 行"后，组开关应从**类型 ID / 位掩码**推导（O(1)，无字符串）。**待定**是否保留字符串形态
  （当前 3 个键都是编译期常量字符串，可改成枚举）。
- **注意（A0 教训）**：这是**活逻辑**，不是冗余计数；不能在"零行为变化"的批次里顺手删。

### 2.5 A5b 删掉的 `positionSourceSpec` / `transformPolicySpec` / `overridePipeline`

- `overridePipeline`：**全仓 0 命中**（`grep -rn overridePipeline inc/ src/ example/` 无输出）⇒ 已彻底删除，
  **没有**以新形式回归。
- `positionSourceSpec` / `transformPolicySpec`：**没有消失**，但形态不是"组件字段"而是 **`RenderItem` 的虚接口**：
  - `inc/hgl/ecs/core/RenderItem.h:4-5` 仍 include 两个头；`:61`
    `virtual TransformPolicySpec GetTransformPolicySpec() const { return TransformPolicySpec{}; }`；
    `:62` `virtual PositionSourceSpec GetPositionSourceSpec() const { return PositionSourceSpec::MeshVertex; }`。
  - 头文件本体仍在：`inc/hgl/ecs/support/PositionSourceSpec.h`（15 行）、
    `inc/hgl/ecs/support/TransformPolicySpec.h`（20 行）。
  - 唯一消费者：`src/ecs/support/PrimitiveBatchPipeline.cpp:292-300`
    （读两个 spec 并 switch `PositionSourceSpec::{MeshVertex, Quad2DGenerated, TerrainHeightmapGrid, ProceduralGenerated}`）。
- **待定（推测定标）**：A5b 删的是**组件上的字段**，`RenderItem` 虚接口是**另一层**。
  是否借 stage B 一并收敛（例如把"位置来源/变换策略"降为**每实例 slot 上的选择器**，去掉虚函数）—— **没有定稿**；
  现状是「虚接口 + 默认实现」，**不是**「组件字段」。

---

## 3. stage B 的验收口径（沿用 stage A 硬纪律）

`doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md:1058-1062` 与 `:1156`：

1. **每步独立可运行**：build 0 error → 固定验证集 → 示例抽跑；
2. **每步同批删掉被取代的旧代码**，`grep` 零残留算验收项（不许"新的有了旧的还在"）；
3. **禁止 `if(新旧并存)` / `if(特例)` 分支**，语义收敛一律走判定表；
4. 做不到"一步内新旧都跑得通" ⇒ 说明步太大，**继续切分**而不是加兼容层；
5. **行为等价重排用同输入同结果对拍**。
6. 本仓附加铁律：**清 obj 全量重编后才拿结论**（本仓实测：只改头文件时 `cmake --build` 不一定重编依赖 TU）；
   行尾 / BOM **逐文件保留**（判行尾只能用字节计数，不能用 `git show`）。

---

## 4. 明确不做（避免反向工作）

`doc/future/ULRE_FINAL_TARGET_v2_设计约束.md:159`：
GPU 侧 48 B TRS SSBO（D1）；L2W 改 `Matrix4x3f`（D2）；全局 `g_CurrentTransformStorage`（D3）。
近期不做 HLOD / LOD 生成、prefab（多模型合并）、场景直载（已延后）。

---

## 5. 本轮未实测项（白纸黑字）

- 当前 `TransformDataStorage` **每行字节数**（文档写 ≈267 B / 目标 ≈211 B）—— 未实测，未跑 `ProbeTransformDiagnostics`。
- `EntityGPU` 128 B 的 GPU 侧实机布局/对齐 —— 无实现，只有文档形状。
- `.ulrescene` 直载链路 —— 无实现，未实测。
- 相机参数进 arena 的具体方案 —— 仅列为"待定分叉"，未做设计。
