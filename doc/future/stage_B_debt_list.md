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

**差距（T10 第 1–4 条，逐条对照 `doc/future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md:1158-1164`）—— ✅ 四条全部收口（2026-10-02）**

1. ~~行号仍会漂移~~ ⇒ **已消除**：`TransformAssignmentBuffer` 的 `EnsureCapacity(...)` / `"[TransformAssignmentBuffer] L2W recreated"`
   / `StatTransform` 现在**全仓 0 命中**（本批实测 grep；该文件的"按需扩容 + 重建"整块已删，L2W 行数改由预算算式**一次性**建）。
   行号 = **稳定槽位的函数**（静态 `1+slot`、动态 `1+max_static+frame×max_dynamic+slot`）；Mobility 换边 = 换段
   （旧行号作废 + 一次性告警），**不静默平移全体**。判据不是"看起来没重建"，而是**运行日志里重建类日志 0 次**。
2. ~~没有声明式预算/预分配 API~~ ⇒ **已存在**：`inc/hgl/ecs/support/TransformDataStorage.h:39-41`
   （`kDefaultMaxStaticRows` / `kDefaultMaxDynamicRows = 4096`，P-2 拍板）、`:105-121`（构造即按默认预算预分配）、
   `:150-152`（`GetStaticRowBudget()` / `GetRowBudget()`）；超限 **fail-fast 不扩容**。
3. ~~行字节瘦身未做~~ ⇒ **已做并实测（B2）**：**279 → 223 B/行**（19 条平行数组；`Test 9` 与探针两处独立测量一致，
   其中 `local_mat4 + world_mat4` = 128 B）。
4. ~~探针未运行~~ ⇒ **已运行**：B2 出每行字节、B5-b 出 `sizeof(MaterialRuntimeSlot) = 88 B`。

**统一预算口径（stage B 后；全部"声明式容量 + 超限 fail-fast + 不扩容"）**

| 行空间 | 容量 | 依据 | 行号冻结 |
|---|---|---|---|
| Transform（静态 / 动态） | 4096 / 4096 | `TransformDataStorage.h:39-41`（P-2） | ✅ 稳定槽位（下标即槽位、空洞复用） |
| MeshDrawParams | 16384（reserve 1） | `GlobalSSBOBufferRegistry.cpp:15` | ✅ 行号唯一来源 |
| GeometryAABB | 16384（reserve 0；与主表同 ID 索引，从不 Acquire） | 同上 `:24`（一号双表，有断言） | ✅ 跟主表 |
| 材质 surface 三池（PBR / Emissive / Transmission） | 1024（reserve 1） | 同上 `:16-18` | ✅ 池行号即内容身份 |
| MaterialData（材质**组件行**） | 1024（reserve 1） | 同上 `:29` | ✅ 句柄语义（组件分离不归还行） |
| MaterialRuntime（每实例 slot） | 4096 fail-fast | `MaterialRuntimeTable.h:41/223` | ✅ 行号即身份（B5） |
| MaterialVariant | 4096 fail-fast | `MaterialVariantTable.h:59/141` | ✅ |
| 相机（槽 × 帧槽） | 16 × `HGL_FRAME_SLOT_TOTAL` | `CameraInfoStorage.h:48-54`（`static_assert`） | ✅ 槽 0 专属默认相机 |
| 纹理引用配置（**按定义变长**） | 1024 / 定义 | `MaterialRecipe.h:68` | ⏳ 见 §2.9（末期同批） |

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

**差距 / 待定 —— ✅ 已定稿（B7，2026-10-02；用户拍板 P-6）**
- ~~相机不在 `ComponentType` 枚举里~~ ⇒ **B7 已补**：`ComponentType::Camera` + 类型表行
  `{World, "ECS:Camera", 0, "CameraComponent"}` + `kComponentTypeImplies` 对齐 + `ComponentTypeOf<CameraComponent>`
  槽位映射（`gpu_row_bytes` 记 0 = 该类型**没有自己的 GPU 行**）。
- ~~**待定**：① 进 `(World, Camera)` 行 arena + 值类型句柄，还是 ② 参数并入现有世界相机行空间~~
  ⇒ **定稿 ②（P-6）**：**不新开 arena，参数留组件**。即**现状就是终态**——组件持 **CPU 权威参数**
  （`graph::Camera camera_data` / `graph::CameraInfo camera_info`，文件头 `CameraComponent.h:14-38` 已定稿），
  **世界相机表是 `CameraSystem::Publish` 出来的派生视图**（槽账目 + 宿主弱引用）。19 个参数字段
  **保持 OOP 组件字段形态**，不再要求行化（要求行化的是被否决的 ① 路线的代价）。
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
- ④ **A5b 从已删渲染组件迁来的每实例绑定**（指针已出清，形态见下）：`render_item_handle`、
  多实例 `instance_count` / `max_instances` / `allocated_instance_capacity`、`is_gpu_driven` / `is_indirect`。

**✅ 已闭合（B5-a + B5-b，2026-10-02）—— ④ 里的指针/句柄字段按「行 arena + 值类型句柄」全部出清，无"待定"残留**：

- **6 个缓冲字段 → 世界级句柄表行号**（`inc/hgl/ecs/support/MaterialRuntimeBufferTable.h`）：`l2w` / `l2w_index` /
  `mesh_draw_params` / `material_data_rows` / `indirect_cmds` / `indirect_count` 一律以 `*_handle`（`uint32_t`）表示；
  表按公共基类 `graph::BufferOwner*` 收/取，**同一指针 interned（同一句柄）+ 引用计数**，**句柄 0 = 无缓冲**；
  setter 统一「先 `Release` 本处旧句柄、再 `Register` 新值」（反复调用不堆计数、不误回收共享缓冲）；
  `DestroySlot` 按引用释放全部 6 个（`GetBufferHandles()` 交出清单）。示例调用点零改动（签名不变）。
- **`runtime_pipeline_pass` / `runtime_pipeline`（每 pass 解析产物）→ 世界侧旁表**
  （`inc/hgl/ecs/support/MaterialRuntimePipelineTable.h`：`Set(id, pass)` / `Get(id, pass)`，
  **pass 不匹配即未命中**——与旧 slot 字段语义逐字一致；失效与销毁两条路径都摘除）。
- **出清后 slot 全 POD、行宽冻结 88 B**：`static_assert(sizeof(MaterialRuntimeSlot) == 88)` +
  `ProbeTransformDiagnostics` 同源打印（编译期与运行期两处值一致）。
- **契约**：Test 37（24 条，含 6 条"指针禁回 slot"）/ Test 38（12 条，含 2 条字段禁回 + pass 比对语义 + 行宽断言）。
  反证分别 rc=37 / rc=38，逐字节还原后必绿。
- 剩下的②每实例选择器（`pass_selector` / `lod_selector` / `dither_enabled`）与③状态字段
  （重试/降频、授权代跟踪、生命周期/脏标志）**本来就该留在行内**（全 POD）⇒ 不属本项，无需搬。

### 2.3 `Entity::component_mask` → `EntityGPU::type[16]` 的对应关系

- 现状：`component_mask` 是 `uint32_t` **位掩码**（`Entity.h:30`），位 = `1u << (uint32_t)ComponentType`
  （`ComponentTypeTable.h:124-131`），`None` 返回 0 位 ⇒ 永不占位。
- 定稿形状：`EntityGPU::type[16]` = **16 个槽位 × (类型 1 B + 行号 4 B)**（`doc/future/ULRE_FINAL_TARGET_v2_设计约束.md:67`）。
- **✅ 已定稿（P-3 拍板，2026-10-02，用户：选 A）**：**槽位 = `ComponentType` 值 ⇒ 每类型最多一行**，
  与 `Entity::component_mask`（位掩码）**1:1 对应**；"每实体最多 16 个组件实例（任意类型）"的 B 方案**已被否决**
  ⇒ `ComponentType` **不需要扩容**、`component_mask` 的表示也不换。
- **只剩实现（B8）**：按此语义落地 `EntityGPU::type[16]`（类型 1 B/槽）+ `row[16]`（行号 4 B/槽）；
  类型值**只能追加**（B7 已按此把 `Camera` 追加在 `Texture` 之后）——类型值被位掩码与 `type[16]` 槽消费，
  插队会让既有类型全体平移。类型表 `kComponentTypeTable` 已同步到 stage B 真值（B6/B7）。

### 2.4 `system_group_component_counts`（按组**字符串**计数，键域 3 个）—— ✅ **已闭合（A2，2026-10-02）**

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
  stage B 把组件换成"类型 → 行"后，组开关应从**类型 ID / 位掩码**推导（O(1)，无字符串）。
- **注意（A0 教训）**：这是**活逻辑**，不是冗余计数；不能在"零行为变化"的批次里顺手删。

**✅ 已闭合（A2，2026-10-02；P-4 拍板执行）**：热路径（组件挂卸 ⇒ 装/启组）已收敛为
**`enum class SystemGroupID` + `kSystemGroupNames[]`（全仓唯一一份组名）+ O(1) 数组**
（计数 `system_group_component_counts[kSystemGroupCount]`、已安装 `bool[]`、每组系统快照 `group_systems[]`）；
组件侧字符串虚函数**删除**，改 `virtual SystemGroupID GetSystemGroup()`；渲染图的组启用路径经
`SystemGroupFromName()` 严格转换，未知组名留痕告警。**未建 `ComponentType → 组` 的表**——线条 / 文本组件
还没有类型号（用户 2026-09-30 拍板"文本/线条属于动态 GEOMETRY，以后再说"）⇒ 等它们拿到类型号再并入类型表；
系统声明侧（14 个 `SetRenderElementType("…")`）仍字符串（一次性注册路径），两侧同名由 Test 25 契约钉住。
**验证**：门 42/0、10 测试 rc=0、8 示例 0 真 error / 0 对拍不一致 / 0 未知组名、**三组各自像素确认**
（TextDraw / PBRSpheres / LineRender）、反证 rc≠0 ↔ 还原必绿。详见 `stage_B_execution_plan.md` 的 A2 完成记录。

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

### 2.6 ✅ 已消除（B3-iii，2026-10-02）：批处理键的旧形态 = 几何运行时三指针 + 每实体 CPU draw range

**已消除（2026-10-02，B3-iii 落地并验证）**：下面这段是**变更前**的形态，保留作"为什么必须重构"的记录。
现状 = 排序/聚簇/ICB/命令四处一律读 `RenderItem::MeshDrawKey`（`MeshDrawParams` 行的 CPU 镜像 POD），
采集期从行填入；`GeometryData` 的三指针与逐帧武装函数整组删除。详见
`stage_B_execution_plan.md` 的「B3-iii 完成记录」（含等价性论证、验证数字与过程教训）。

**变更前形态（记录用）**：`GeometryData` 曾持每实体 `new` 出来的
`GeometryDataBuffer*` / `GeometryDrawRange*` / `Geometry*`，而这三者正是**批处理的主键**：

- `RenderItem::Compare`（`src/ecs/core/RenderItem.cpp:8-52`）= 批排序主键，按 `*GeometryDataBuffer <=>`
  （内容比较 geometry_id/vdm/vab/ibo）与 `*GeometryDrawRange <=>`（vertex_offset/first_index/counts）排序；
  `DrawBatch::Set(...)`（`inc/hgl/ecs/support/PipelineMaterialRenderer.h:62-79`）再由 `data_buffer->geometry_id` 定 `geometry_id`；
  建批点 4 处：`src/ecs/support/PrimitiveBatchPipeline.cpp:491/552/570/588`。
- 采集侧每帧经 `EnsureRuntimeGeometryFromAsset()`（`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:216-254`，
  调用点 `:2187` / `:2228` / `:2280`）按材质程序"武装"几何缓冲；4ID 的 `geometry_id` 走三级回退
  （`geom_buf->geometry_id` → asset→Geometry→`EnsureMeshDrawParams`）。

**终态方向**（与"把 GPU 可用部分抽成固定 ID 结构"一致）：排序/聚簇/ICB 一律读**行派生 POD 键**
（`geometry_id` + `vertex_base`/`index_base`/`vertex_count`/`index_count`/`is_indexed`，即 `MeshDrawParams` 行的 CPU 镜像），
三指针与其每实体 CPU 副本整组删除。ICB 组数由 `is_indexed ? index_count : vertex_count` 得到（与现状逐位等价）。

**已拍板（2026-10-02，用户）：按建议改** —— 排序/聚簇键改行派生 POD 键；**同 asset 的每实体 `GeometryDrawRange`
差异不再参与判定**（同几何 ⇒ 同一行 ⇒ 必然同键）。对纯几何路径等价（旧实现下两份副本内容也相同）；
文本/线段的批不参与该排序（单批）⇒ 实际影响为零。

**关联欠账（独立，仍开放；用户定性 = "动态 GEOMETRY"，明确"以后再说"）**：**文本 / 线段路径"每批改写同一个几何的行"** ——
`src/ecs/support/text/TextRenderPipeline.cpp:907`（`vertex_count = 字符数`）与
`src/ecs/support/line/LineRenderPipeline.cpp:655`（`vertex_count = 线段数 × 2`）都是**共享行被每批覆盖写**。
若同一几何被两个批引用且绘制量不同，行会被后写者覆盖（谁最后写谁赢）——现有示例未触发。
收敛方向：行 = 每几何一份不可变（绘制量差异进每 draw item 的数据，而不是行）。

---

### 2.7 示例侧仍有自建几何 AABB 表（2026-10-02 记录）—— ✅ **已闭合（A3，2026-10-02）**

`example/ApiValidation/ComputeAsteroidBelt.cpp`：`cpu_geometry_aabbs[GEOMETRY_VARIANT_COUNT]`（10 个变体，
extents 来自 `default_extents[]`）+ 自建 `geometry_aabbs_buffer` + 地址表 `geometry_aabbs` 字段 +
GLSL `GeometryAABB/GeometryAABBsRef` + 候选用**变体下标** `geom_id` 索引该表。

**同类欠账已在 `ComputeFrustumCull` 消除**（见 `stage_B_execution_plan.md` 的「B3 尾巴」；该示例改为读引擎
`GeometryAABB` 池、候选 `geometry_id` = 引擎行号、CPU 真值同源）。

**改造障碍（先验证再动手）**：该示例的 `planet_geometry` 来自**模型加载**，引擎池里这一行是否有真值未验证
（可能是"未设"哨兵 `center.w < 0`）；若模型路径不设 `SetBoundingVolumes`，需先让加载路径写入包围体，
否则剔除会按退化 AABB 走。内置的 10 个几何走 inline geometry（`CreateWithAABB`）应已就位。

### 2.8 先存在的红：ATS `D3 receive_shadow` 契约的"变红"方向恒为 0 px（2026-10-02 记录）
**✅ 已闭合（A3，2026-10-02）—— 障碍实测排除，示例已改读引擎池**：
- **不会退化**：`Geometry::RefreshGeometryAABBRow()`（`src/SceneGraph/VKGeometry.cpp:112-142`）对本地 AABB
  无效的几何写的是**「始终可见」哨兵**（`kAlwaysVisibleRadius`，原文「缺数据不得导致被剔除」）⇒
  模型加载来的几何要么有真值、要么保守可见，改读引擎池**没有退化剔除风险**。
- **改法**：`SimulationAddressTable` 尾部加 `uint32_t geometry_ids[GEOMETRY_VARIANT_COUNT]`（尺寸断言 40→80），
  GLSL `SimAddressTableRef` 同步加 `uint geometry_ids[10]`（scalar layout 无填充）；GLSL 剔除改
  `geometry_aabbs.boxes[table.geometry_ids[geom_id]]`；地址表 `geometry_aabbs` 填**引擎池基址**
  （`GetGPUBase(GlobalSSBOType::GeometryAABB)`，与 `ComputeFrustumCull` 同范式）。**删除**：
  `cpu_geometry_aabbs[10]` + 填充 + `default_extents[10]` + 自建 buffer（320 B）的创建/释放/声明 ⇒ `grep` 零残留。
- **验证**：BUILD_RC=0 / error C=0 / warning C=0；运行 55s 真 error 0，引擎池写入真值可见
  （`GeometryAABB 行写入 id=1 radius=1.7321 extents=(1,1,1)`）；**像素**：模型加载的主星未被误剔 +
  1,000,000 陨星环完整致密（若 AABB 读成零/垃圾值会出现整环消失或该剔不剔）。

**现象**：`ATS_SELFCHECK=1 AlphaTestShadow` 退出码 1，唯一失败项是

```
[D3-CONTRACT] receive_shadow FAIL: 关掉后 受影→未受影(变红) 0 px / 未受影→受影 0 px
              （同帧外观变化共 10473 px；期望变红 >= 3000 且反向 <= 变红/4）
```

同帧 `bias_multiplier PASS`（外观变化 593213 px）、`[D1-CONTRACT] c0 PASS`（填充率 57.6%）、
`[D4-CONTRACT] 行池写入 PASS`（CommitRow 拒绝 0）⇒ **不是读回失败，是"变红"这个方向没被触发**。

**归因（实测 A/B，2026-10-02）**：把 B7 的两处改动（`ComponentTypeTable.h` / `CameraComponent.h`）按
`git show HEAD:<file>` 整体覆盖回 `0f4d54a32`（B6）内容后（`grep -c "ComponentType::Camera"` = 0 确认回退到位），
**逐字节相同**地复现同一条失败（数字完全一致：0 px / 10473 px）⇒ **与本会话 B3–B7 各批无关，先存在**。
`ulre-ecs-layer` 技能记过这类症状的根因类别：「shadow pass 用**退化相机**渲染 ⇒ `receive_shadow` 拨动
0 px 变化，但 `bias_multiplier` 仍改外观」（历史修法 = `RenderTo` 按 `req.camera` 发布相机行）
⇒ 该方向仍有漏，或另有来源，需要专门定位。

**附带观察（未归因）**：同一条比较的"外观变化 px"从技能/计划里记的基线 `18187~18189 px` 漂到
`10473 px`（`bias` 侧 `600662 → 593213` 基本未动）⇒ 中间某个提交改动了 `receive_shadow` 的生效幅度，
**未定位到具体提交**（需按批回退转储对比或 `git bisect`）。

**处置**：**不在存储形批次里顺手修**（同技能里"预存在的 CSM 生命周期缺口"那条纪律）。
待 B 线收口后单开一批：先按技能配方打印「pass 相机 / 相机行内容 / 受影像素方向」，再谈改法；
修完补进验收集（`ATS_SELFCHECK=1` 必须 rc=0 才算绿）。

---

### 2.9 【新发现·结构性】行空间有两类——组件行是"按类型定宽"，材质纹理引用是"按定义变长"

**用户 2026-10-02 点出**：设想的组件数据段是定长（这点确实成立：Transform 64 B / MaterialData 16 B /
MaterialRuntime 88 B 都是编译期冻结的），但**纹理引用每个材质要的数量不一样** ⇒ 它的数据形态与其它
组件不同，**既不属于 Global 也不属于 World**，而是**每种材质独有**。决议：**先不动它，放到计划末期处理**。

**用户 2026-10-02 补充定性（本条的范围由此定死）**：**纹理引用应当算 `MaterialPrivateData` 的一部分**——
今天「材质私有数据（surface 池）」与「纹理引用（配置池）」是**同一个职责被分开放了两处**。
决议：以下三项**统一标记为「与 §2.9 同批处理」**，不再各自开条、也不在任何一批里顺手做：

| 项 | 今天在哪 | 末期归口 |
|---|---|---|
| **材质私有数据**（surface） | `GlobalSSBOType` 里三条**手工登记**的池（`PBRSurface` / `EmissiveSurface` / `TransmissionSurface`，`GlobalSSBOBufferRegistry.cpp:16-18`）+ `GlobalAddresses` 三条独立地址字段 + `MaterialShaderEmitter.cpp:278-280` 每定义一个 `case`（⇒「新增一种材质」要动四处） | 统一收成「**材质定义级行空间**」一族 |
| **纹理引用** | `MaterialTextureReferencePool`（每 `(definition, layout)` 一份、**按定义变长的字节行**、无 C++ 结构体）+ 每实例 slot 的 `pool_key + row_index` | **同上**（它本来就是 `MaterialPrivateData` 的一部分，只是被分开了） |
| **`ComponentType::Texture`** | 类型号已登记、**无组件载体**、零消费者（B6） | **不建实体级组件**——它的载体形态只可能出现在上述族里 ⇒ 与 §2.9 同批决定（连同「要不要保留这个类型号」一起拍） |

**实测（本条不是推测，仓库里已经有这一类）**：

- `inc/hgl/graph/module/MaterialTextureReferencePool.h` 的文件头自述就写着：
  「`MaterialTextureReferencePool` —— **每个 `(definition, layout)` 一份的字节行池**。行 = 「该材质的纹理引用列表」
  （`uvec2[reference_count]`，**元数随材质变化、行距运行时才知道**）——**没有 C++ 结构体**，故内部换装通用件：
  `ActiveRowPool`（Buffer + 行号空间，物理行 = max+1，**行 0 预留为零行**）+ `ActiveRowView`（按字节访问）
  + 延迟回收（`ReleaseDeferred` / `CollectRecyclable`，retire epoch）」。
- 行宽与元数**由材质定义算出**：`inc/hgl/mtl/MaterialRecipe.h:466-468`（`reference_count` / `row_stride`，
  layout hash 含「Texture keys + declaration order + sampler/policy」`:209`）；容量 `DefaultMaterialTextureConfigurationCapacity = 1024u`
  （`:68`，注释「Live rows; pool row zero is reserved」`:`213`）。
- 每实例 slot **已经持有这条行的句柄**：`MaterialRuntimeTable.h:139-145`
  （`MaterialTextureConfigurationAllocation{ pool_key, row_index, reference_count, row_stride, allocation_generation, … }`
  + `material_texture_row_cpu/gpu` + `material_texture_zero_row_gpu` + `material_texture_configuration_hash`）。
- 内容 = `MaterialTextureReference{ descriptor_index /*= bindless 句柄*/, array_layer }`（8 B，`static_assert`）
  的数组，按定义里声明的槽位序索引，整块哈希进材质内容键（`RenderPrimitiveCollectSystem.cpp:1583-1589 / 1667-1669 / 1675-1678`）。

**两类行空间的形状差异（末期要统一的就是这张表）**

| | 组件行空间（stage A/B 模型） | 材质纹理引用行空间（本类） |
|---|---|---|
| 行空间的键 | `(scope, type)` | **`(definition, layout)`** —— 材质定义 |
| 行宽 | **编译期固定**（`static_assert(sizeof(Row) == N)`） | **运行时才知道**（`row_stride`，16 B 对齐） |
| CPU 侧类型 | 有 C++ 结构体、类型化读口 | **没有结构体**，按字节访问（`ActiveRowView`） |
| 归属 | Global = 设备级 / World = 世界级 | 设备级图侧池（`hgl::graph::module`），**但不是 Global 坐标** |
| 行 0 | 哨兵（无/零） | 预留**零行**（另有 `zero_row_gpu` 便于"无绑定"读） |
| 回收 | 行不归还（句柄语义） | **延迟回收**（retire epoch `= 3`） |
| 定位键落点 | `EntityGPU::row[type]`（B8 规划） | 每实例 runtime slot 的 `pool_key + row_index` |

**同类近亲（末期一起收）**：材质**私有数据**池 `PBRSurface` / `EmissiveSurface` / `TransmissionSurface`
也是"按定义定形"的行空间，但今天走的是**手工登记**范式——`GlobalSSBOBufferRegistry.cpp:16-18` 固定行宽 + 固定 1024 行、
`GlobalAddresses` 里三条独立地址字段、`MaterialShaderEmitter.cpp:278-280` 一个定义一个 `case` 映射
⇒ 「加一种材质」今天要动**枚举 + 注册表 + UBO 地址 + ShaderGen 发射器**四处。

**末期处理清单（现在不做，只留位置）**

1. 把「行空间的键」这一维显式写进模型：`(scope, type)` **或** `(definition, layout)`，而不是把后者当特例；
   与 B8 的 `EntityGPU::row[16]` 一并考虑（那个数组装的是"类型槽 → 行号"，装不下"定义级行号"）。
2. 统一访问器形态：编译期定宽（结构体 + 类型化读口）与运行时变长（字节行 + `ActiveRowView`）**两种行都要能取**。
3. 把材质私有数据池从"手工登记四处"收进同一机制（否则"新增一种材质"永远是四处改动）。
4. 与 P-5 的纪律对齐：行/表内不放指针（`MaterialTextureConfigurationAllocation` 里 `cpu_row` 是池内视图指针，
   属池自身管理，可接受；但**跨模块**引用的指针仍应句柄化）。

**当前动作：无**（用户 2026-10-02 决议：**不动它**）。B 线各批不因此改范围。

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

- ~~当前 `TransformDataStorage` **每行字节数**（文档写 ≈267 B / 目标 ≈211 B）~~ —— **已实测（2026-10-02，B2）**：
  **279 → 223 B/行**（19 条平行数组；`Test 9` 与 `ProbeTransformDiagnostics` 两处一致；其中 `local_mat4 + world_mat4` = 128 B）。
  行空间冻结与稳定槽位见 `stage_B_execution_plan.md` 的 B1 / B1b（static 4096 + dynamic 4096、下标即槽位、空洞复用）。
- `EntityGPU` 128 B 的 GPU 侧实机布局/对齐 —— 无实现，只有文档形状。
- `.ulrescene` 直载链路 —— 无实现，未实测。
- ~~相机参数进 arena 的具体方案 —— 仅列为"待定分叉"，未做设计。~~ ⇒ **✅ 已定稿（B7 / P-6 ②）**：
  **不新开 arena，参数留组件**（组件 = CPU 权威，世界相机表 = 派生视图）。
- **ATS `D3 receive_shadow`「变红」契约仍红（先存在，见 §2.8）** —— B 线各批验收时该项**记为已知红**，
  不得当作本批引入；B 线收口后单开一批处理。
