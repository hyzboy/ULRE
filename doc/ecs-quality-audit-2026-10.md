# ECS 体系质量审计（2026-10）

> 范围：`inc/hgl/ecs`（92 文件）+ `src/ecs`（79 文件）。
> 方法：四路并行（我方 + 三个子代理）。
> 本档为**只读审计**结论汇总；除本档自身外未修改任何代码。
>
> 标注约定：
> - `✅复核` = 我方亲自读源码复核过（非仅采信子代理 grep）
> - `S1` = systems/components 分组、`S2` = core 分组、`S3` = support 分组、`M` = 我方
> - 数字行号以审计当时的工作树为准（工作树含 `69df4fdde` 之后的框架简化）

---

## 0. 两个真正的缺陷（优先级高于一切"代码质量"项）

这两条不是风格问题，是**运行期可出错的状态机缺陷**。

### 0.1 `LineBuffer::EnsureCapacity` 失败后 `gpu_capacity` 不回落 ⇒ 后续调用带空 geometry 返回成功 ✅复核
`src/ecs/support/line/LineRenderPipeline.cpp:109-209`
- `:113` 入口守卫 `if (needed <= gpu_capacity) return true;`
- `gpu_capacity = new_cap` **只在 :207 全成功路径赋值**
- 五条失败路径（`:137-138`、`:146-153`、`:156-161`、`:170-176`、`:189-200`）都 `SAFE_CLEAR(geometry)` 后 `return false`，**都不重置 `gpu_capacity`**
- 后果：**先成功扩容到 N，再失败一次扩容**（`geometry` 已置空）→ 之后任何 `needed <= N` 的调用在 `:113` 直接 `return true`，而 `geometry == nullptr`、四个 `va_*.Bind(nullptr)` 也已失效 ⇒ 调用方继续用空句柄
- 佐证：`:601-614` 的 Build-mismatch 告警路径与 `.cpp:224` 的 `AddSegment` 失败，正是这个状态的症状面

### 0.2 L2W 缓冲被按设备上限截断，而行号算术仍用预算值 ✅复核（支撑组指出）
`src/ecs/support/TransformAssignmentBuffer.cpp`
- `:460-470` `AllocateBudgetBuffers` 把 `row_count` **按设备 SSBO 上限截断**（注释自认"按设备上限截断（请调小预算）"）
- 但 `:195-199` `GetDynamicBaseIndex()` 及两个 writer 仍用**预算**（`max_static_rows + kFirstObjectL2WSlot` / `max_dynamic_rows`）
- 后果：设备 SSBO 上限低于声明预算时，动态段基址指向已建缓冲之外，而代码仍在写 ⇒ 越界写

---

## 1. 死代码

### 1.1 可免费删除的零调用 API（高置信）
| 符号 | 位置 | 来源 |
|---|---|---|
| `Object::GetID()` / `objectId` / `nextObjectId` / `SetName()` | `core/Object.h:19,23,28,46,52` | M ✅复核 |
| `RenderItem::Compare()`（24 行排序策略） | `core/RenderItem.h:84`、`RenderItem.cpp:8-31` | S2 ✅复核（另见 §6） |
| `RenderItem::index`（只写不读） | `core/RenderItem.h:35`；唯一写 `MaterialBatch.cpp:91` | S2 ✅复核 |
| `src/ecs/core/RenderItem.cpp:33-38` 空残留（2 个 legacy include + 空 namespace） | — | S2 |
| `RenderGraph::Pass` 的 `enabled/runUpdate/submitTransforms/runRender` + `onBeforePass/onAfterPass` | `core/RenderGraph.h:39-56` | S2 ✅复核 |
| `MaterialBatch::static_count`（恒 0） | `core/MaterialBatch.h:48` | S2 |
| `EntityGPU::work_flags`（自注"留 0"） | `core/EntityGPU.h:51` | S2 |
| `ECSContext::GetActiveCameraID()` | `core/Context.h:517` | S2/M ✅复核（另见 §6） |
| `ECSContext::GetMaterialRuntimeSlot(EntityID/Entity*)` | `core/Context.h:880-888` | S2 |
| `ECSContext::GetShadowProxy(...)` ×4 全载 | `core/Context.h:696-699`、`Context.cpp:2984-3002` | S1/S2 ✅复核 |
| `ECSContext::GetShadowInfo()` / `SetShadowInfo()` | `core/Context.h:747,749` | S1/S2 |
| `ECSContext::GetResourceNamePrefix()` + 成员（只写不读 ⇒ 泄漏追踪功能**未实现**） | `core/Context.h:336,639,640` | S2 ✅复核 |
| `EntityManager::GetAllEntityPointers(Entity*)`（非 const） | `core/EntityManager.h:120` | S2 |
| `PrimitiveRenderItem::GetRuntimeSlot()` | `core/PrimitiveRenderItem.h:46` | S2 |
| `MainLightShadowSystem::GetShadowInfo()` / `SetShadowInfo()` | `systems/render/MainLightShadowSystem.h:75,77` | S1 |
| `LightSystem::SetMainLight(...)`（仅内部调用） | `systems/render/LightSystem.h:36` | S1 |
| `ColorPaletteSystem::GetPaletteUBO/SetColor/ResetToWhite`（整个编辑 API） | `systems/render/ColorPaletteSystem.h:43,46,49` | S1 |
| `EnvironmentSystem::SetAmbientEnvInfo/GetAmbientEnvInfo` | `systems/render/EnvironmentSystem.h:51,53` | S1 |
| `CameraSystem::GetOverrideCamera()` | `systems/tick/CameraSystem.h:136` | S1 |
| `CameraInputMapping::EditContext()/GetContext()`（⇒ 作者自定义键位路径不可达） | `systems/tick/CameraInputMapping.h:46-47` | S1 |
| `LineStatsSystem::SetLogInterval()`（该系统**无条件默认注册**） | `systems/render/LineStatsSystem.h:21` | M/S1 ✅复核 |
| `TransformSystem::SetDevice()`（**空函数但仍在被调用**） | `systems/tick/TransformSystem.h:52`；调用 `PrimitiveBatchPipeline.cpp:215` | S1 |
| `TransformSystem::SetUpdateMovable()/IsUpdateMovableEnabled()` | `systems/tick/TransformSystem.h:54-55` | S1 |
| `TransformSystem::last_dynamic_count`（只写不读） | `systems/tick/TransformSystem.h:31` | S1 |
| `RenderBufferUploadSystem::GetDevice()` | `systems/render/RenderBufferUploadSystem.h:42` | S1 |
| `MaterialData::GetAuthoredGeneration()` / `MaterialDataAuthoringResource::authored` | `components/MaterialData.h:133,50` | S1 |
| `RenderSceneUBOSystem::ResolveViewportUBO()` | `systems/render/RenderSceneUBOSystem.cpp:305` | S1 ✅复核 |
| `SyncBindingsForCurrentCommand`→`ApplyResourceLayoutBindings`（**空体**，Update/Render 净收益为零） | `systems/render/RenderSceneUBOSystem.cpp:295-325` | S1 ✅复核 |
| `RenderItemDataStorage::SetTransformID/SetGeometryID/SetMaterialID/SetTextureID/IsValidHandle/GetGPUCapacity/GetDeviceBuffer` | `support/RenderItemDataStorage.h:87-90,93,126,128` | S3 |
| `DrawItemIDStorage::GetDeviceBuffer/GetLastFrameUploadedBytes/GetLastFrameItemCount` | `support/DrawItemIDStorage.h:75,87-88` | S3 |
| `DrawItemIDStorage::SetExternalGPUBuffer`（整个 external-GPU 模式仅由 `_Obsolete` 示例与测试维持） | `support/DrawItemIDStorage.h:51` | S3 |
| `MaterialRuntimeTable::DetachOwned`（**50 行 CoW 机制，零生产调用者**）+ `AddRef`/`GetRefCount`/`GetFreeRowCount`/`GetGeneration` | `support/MaterialRuntimeTable.h:281,271,288,303,291` | S3 |
| `MaterialAuthoringSideTable::GetLiveEntryCount` / `MaterialRuntimePipelineTable::GetLiveEntryCount`（连测试都不用） | `.h:81` / `.h:63` | S3 |
| `EntityGPUTable::IsAllocated` / `EntityRenderIDTable::IsAllocated` | `.h:71` / `.h:80` | S3 |
| `TransformDataStorage::GetLocalMatrices`（**证伪其上方 2 行的注释**"0 引用的读取口已删"） | `support/TransformDataStorage.h:880`（注释 `:878`） | S3 |
| `TransformDataStorage::SetWorldMatrix`（公开写 `world_matrices`，而 `:296-297` 声明它是只读派生缓存，且它会清 `matrixDirty` 抑制重算） | `support/TransformDataStorage.h:319` | S3 |
| `GPUCullSystem::IsOverrideActiveThisFrame`（自注"collect 的 CPU 剔除跳不跳全看它"，**实际闸门是另一个**） | `support/GPUCullSystem.h:205` | S3 |
| `CameraInfoStorage::GetSlotOwnerShared` | `support/CameraInfoStorage.h:99` | S3 |
| `CPUCullReferenceSystem::GetVisibleCount/GetParityStats` + `ParityStats` 只写 | `support/CPUCullReferenceSystem.h:40,53,46-52` | S3 |
| `ComponentMaskAdd` / `ComponentMaskRemove` / `ComponentTypeOf_v` | `support/ComponentTypeTable.h:176,181,282` | S3 |
| `RenderNeed::EarlyZ`（自注"预留，暂不驱动任何路径"）/ `StrategyFact::Count`（**全仓 0 命中**） | `support/RenderStrategyTable.h:52,88` | S3 |
| `LineCollectStats::culled_by_hzb`（从不自增，却参与 `GetCullRatio()`） | `support/line/LineRenderPipeline.h:41` | S3 |
| `CPUCullReferenceSystem::gpu_visible_order_`（**16 KB**，只写不读） | `support/CPUCullReferenceSystem.h:85` | S3 |
| `PrimitiveBatchPipeline::prepared_frame_index`（写了但函数**没有 early return** ⇒ 不是守卫） | `support/PrimitiveBatchPipeline.h:38` | S3 |
| `ShouldEmitPeriodicLog`（**恒 false**，body 已注释，参数未用）+ `LogDeviceBufferSnapshot` | `support/TransformAssignmentBuffer.cpp:25-32,34` | S3 |
| `PrimitiveRenderPipeline::Render(cmd)` **空实现**（"intentionally left for future use"） | `support/primitive/PrimitiveRenderPipeline.cpp:51-56` | S3 |
| `PrimitiveBatchPipeline::RunSorting()` **空体**，而 `PrimitiveSortSystem` **已注册在真实帧图里** | `PrimitiveBatchPipeline.cpp:203-205`；`DefaultSystems.cpp:97` | S3 |
| `DrawItemCompaction` 的 indexed 标志（`IsIndexedDraw/GetDrawIndexOffset` 无生产调用者） | `support/DrawItemCompaction.h:18,24` | S3 |

### 1.2 死枚举 / 死分支
- **`ScenePipelineMode` 7 值中 5 个是空壳** — `core/ScenePipelineMode.h:19-23`；`Context.cpp:572-578` 全落同一空 `break`。仅 `StandardLitCSM` 有实现。且 `TestCSMIncrementalPass.cpp:1211-1224` 只断言 set/get 往返 ⇒ **测试全绿但功能为空**。项目已留置（`doc/backlog.md:674`）。`M ✅复核`
- **`ComponentType::Texture` 零消费者槽位** — `support/ComponentTypeTable.h:35-36,106`。`M ✅复核`（注释自认"无组件载体…零消费者，仅登记"）
- **`CameraControlMode::Free` 是组件默认值，而其处理器全空** — `CameraComponent.cpp:19` + `CameraSystem.cpp:202-208`（`ProcessInput`/`UpdateTransform` 皆空）⇒ **未配置的相机静默冻结**。`M ✅复核`
- **`LinesComponent::LineStyle`（`Dashed`/`Dotted` 永不选中，`style` 永不读）** + `LineChange::Style/Geometry` 与 `TextChange::*` 全部**只写不读** — `support/.../LinesComponent.h:29-34,46,118,122-132`；唯一活消费者 `TextRenderPipeline.cpp:664` 只判 `mask != 0`。`M ✅复核`（LineStyle）
- **`CameraInputMapping` 的 `kActionResetView`(R) / `kActionFocusView`(F) 已绑定但无处置** — `CameraInputMapping.cpp:31-32`。`M ✅复核`

### 1.3"接受即丢弃"的参数
| 参数 | 位置 |
|---|---|
| `RenderMainLightShadowPass(..., float deltaTime)`（连 `(void)` 都没有） | `MainLightShadowSystem.cpp:211` |
| `AddOrUpdateSystem(bool is_render, ...)`（只用于拼告警串） | `Context.cpp:1502-1514` ✅复核 |
| `UploadEntityGPUDirtyRows(uint32_t frame_slot)`（`(void)frame_slot`，实写全部 8 份） | `Context.cpp:1702-1740` ✅复核 |
| `TransformSystem::Update(deltaTime)`（`(void)deltaTime`） | `TransformSystem.cpp:68-70` |
| `MaterialData::BuildResolvedRecipe(..., const ShaderProgram*)`（一路传到底再丢） | `MaterialData.cpp:364` |
| `RenderPipelineSystem` 的 5 个 `Update(float dt)` override + `Render(cmd,float dt)`（dt 从不转发给任何 `On*`） | `RenderPipelineSystem.cpp:24,36,48,60,72,84` |
| `PrimitiveRenderSystem::OnRender(RenderPipelineBase* /*pipeline*/, ...)`（丢掉 lookup+校验结果，改读 `render_frame_cache`） | `PrimitiveRenderSystem.cpp:39` |
| `TestFrustumWithWorldAABB(RenderItem* item, ...)`（不用 `item`） | `PrimitiveBatchPipeline.cpp:259-263` |
| `TextRenderPipeline::BuildInputs` 的 `Entity* owner`（未用 ⇒ 缺 visibility 检查） | `TextRenderPipeline.cpp:634` |
| `RenderPipelineDrawSystem::Render(cmd, float dt)`（dt 丢弃） | `RenderPipelineSystem.cpp:84` |

### 1.4 死计算（每帧付费但结果无人读）
- `RenderBufferUploadSystem` 每帧为每个 buffer 做 `GetBufferName()` + 3 次 `find()`，只喂 4 个**仅在注释掉的日志里被消费**的计数器 — `:71-75,81-88,112-122`。`S1`
- `TransformAssignmentBuffer::WriteDynamicDirtyIndices` 无条件算采样行（含 `storage.GetWorldMatrix()`），只在 `#if HGL_TRANSFORM_DEBUG_LOGGING` 下使用 — `:388-403` vs `:405-427`。`S3`
- `DefaultSystems.cpp:196-214` 创建 7 个 `shared_ptr` 后逐条 `(void)` 丢弃。`S1/S2`
- `SwapchainSubmitSystem::last_submit_ok` + `WasSuccessful()` 无读者；`.cpp:3-5` 还 include 了三个从不引用的渲染系统头。`S1`

---

## 2. 冗余结构

### 2.1 系统登记：9 套并行容器 ✅复核
`core/Context.h:126-153,267`
```
tick_systems / render_systems        UnorderedMap<size_t, shared_ptr<System>>   :126-127
systems_by_element_type              map<string, vector<shared_ptr<System>>>   :130
system_group_component_counts[]                                                :133
installed_system_groups[]                                                      :134
group_systems[]                                                                :137
tick_system_order / render_system_order                                        :149-150
tick_dependencies / render_dependencies                                        :155-156
render_pipelines                     unordered_map<string, unique_ptr<...>>    :267
```
后果：`GetSystem<T>()`（:1022-1033）**先查 tick 再查 render**（两次哈希）；依赖边存两份（`System.h:62` + `:155-156`）；枚举数组与字符串 map 双索引（`Context.cpp:3059` 在枚举快路径里回落字符串查找）。

### 2.2 并行表家族（14 张 per-world/per-entity 表）
`EntityGPUTable`、`EntityRenderIDTable`、`TransformDataStorage`、`BoundingBoxDataStorage`、`VisibilityDataStorage`、`RenderItemDataStorage`、`DrawItemIDStorage`、`MaterialVariantTable`、`MaterialRuntimeTable`、`MaterialAuthoringSideTable`、`MaterialRuntimePipelineTable`、`CameraInfoStorage`、`LightRowTable`、各类型 `ComponentPool`，**外加** `RenderFrameCache::renderItems` / `materialBatches`。其中已证实的"派生副本叠加"：
- **`component_mask` 是 `component_rows` 的缓存视图**，而唯一一致性校验 `VerifyComponentMaskAgainstComponents()` **生产零调用**（仅 `TestComponentMask.cpp`） — `core/Entity.h:37,42,63`。`S2`
- **同一份"实体有哪些组件"存 4–5 份**：`component_rows` → `EntityGPU.type[16]/row[16]` → `EntityGPUTable` CPU 行 → GPU SSBO；另有 `EntityRenderIDTable`。`EntityGPU.h:47-49` **自注"显式冗余"**。`S2`
- **同一份 16 B `RenderItemDescriptor` 存两张世界表**：`RenderItemDataStorage::items`（按 handle + free list）与 `EntityRenderIDTable::rows`（按 `EntityID.index × pass`），各自脏跟踪/缓冲/分配策略，**静默可失步** — `RenderItemDataStorage.h:35` vs `EntityRenderIDTable.h:53`。`S3`
- **`DrawItemIDStorage` 与 `RenderItemDataStorage` 是两套同构的 GPU 镜像索引存储**：字段相同（device_buffer/gpu_capacity/gpu_address/dirty），`EnsureGPUBuffer` 算法与魔数（1024 默认 / 64 下限 / ×2 增长 / `delete` 重建）逐字相同，仅 sizeof 与"是否迁移行"不同 ⇒ ~90 行重复 + **两套分叉的增长策略** — `RenderItemDataStorage.cpp:244-302` vs `DrawItemIDStorage.cpp:60-104`。`S3`
- **`EntitySlot::alive` 与 `entity != nullptr` 完全等价**，且每个读取点同时判两者 — `EntityManager.h:29-34,115`、`.cpp:91,104`。`S2`
- **可见性三种表示、两种极性**：`VisibilityDataStorage` 的**缺席集**（自注"唯一真值"）vs `EntityGPUFlags::Visible` 的**正位** vs 每帧重算的 `StrategyFact::EntityVisible` — `VisibilityDataStorage.h:22` / `EntityGPUTable.h:143-156` / `RenderStrategyTable.h:77`。`S3`
- **材质四条键空间**：variant(2 元组) / runtime(3 元组) / authoring(row id) / pipeline(EntityID) 描述同一实体的材质态；且 **`MaterialVariantKey` 是 `MaterialRuntimeKey` 的严格前缀** — `MaterialVariantTable.h:91-106` vs `MaterialRuntimeTable.h:78-100`。`S3`
- **`MaterialRuntimeRow` 把 key 存了约 3.5 份**（`key` + `recipe_hash` + `data_index_row` + `cached_effective_recipe_hash`） — `MaterialRuntimeTable.h:125-150`。`S3`
- **`frame_index` 存三份** — `Context.h:262` / `RingLayout` / RT 自身。`"非法相机槽⇒0 号槽"写三处** — `Context.cpp:810-812,817,2392-2394`。`S2`

### 2.3 逐字复制的机制
- **脏行上传机制在 `EntityGPUTable` / `EntityRenderIDTable` / `LightRowTable` 里逐字重复**：`CollectDirtyRows`、`ClearDirty`、`GetDirtyCount` 三处**字节相同**；`MarkDirty`/`ReportOverflow`（同一闩、同一 `GLogError`，仅文案不同）/`IsAllocated`/`Clear` 近乎相同；成员 `row_dirty` + `overflow_reported` 声明 3 次。命名漂移：`EntityGPUTable` 叫 `Clear()` 而 `EntityRenderIDTable` 同名概念叫 `ClearAll()`。世界侧上传路径重复同一套 quartet 3 次 — `Context.cpp:1701-1712,1750-1755,1791-1796` ⇒ **~120 行**。`S3`
- **`WriteStaticDirtyIndices` 与 `WriteDynamicDirtyIndices` ~95% 相同** — `TransformAssignmentBuffer.cpp:206-301` vs `:303-429`（差异只有 base 算术、日志串、日志采样块），含两处**逐字相同的跳洞注释**（`:268-269` / `:370-371`）⇒ **~200 行**，同一修复要改两遍。`S3`
- **4 个 `Initialize*Table()` + 3 个 `Upload*DirtyRows()` 近乎逐字重复**（"CreateSSBO → SetUpdateClass → StructView → BDA 校验 → zero-fill → Commit → LogInfo" ×7）— `Context.cpp:1616,1823,1900,1979,2071` 与 `:1702,1745,1786` ⇒ **~450 行**，`Context.cpp` 内最大的机械简化点。`S2`
- **`SharedComponentPool<T>` 重实现 `ComponentPool<T>`**：`RemoveActive`、`GetActiveCount`、`GetActiveRow`、`DestroyRow` 守卫链、`Clear` 四阶段体**逐字重复** — `support/ComponentPool.h:98-111 vs 260-273` 等 ⇒ ~120 行。且 `ComponentPoolBase` 只暴露 3 个虚函数（**没有 `GetActiveCount`**），而 `Context.h:1146` 在具体类型上调它 ⇒ **基类对活跃集遍历毫无价值**。`S3`
- **implies 不动点闭包实现两次** — `ComponentTypeTable.h:207-230` 与 `RenderStrategyTable.h:197-209` 内联副本。`S3`
- **`GetMaxWorldScale` 公式在 CPU 剔除参考里被重推一遍** — helper 在 `BoundingVolumeCull.h:18-32`（注释解释了为何必须缩放），而 `CPUCullReferenceSystem.cpp:361-365` 自己重算 scale + `std::max`。**这一对系统本是互相校验用的**，却各自实现同一最易错的公式。`S3`
- **两套同构 profiler** — `CullPerfTimer`（`GPUCullSystem.cpp:33-60`）与 `PerfTimer`（`PrimitiveBatchPipeline.cpp:121-146`），都匿名 namespace、都读 `getenv("PBR_PERF")`、都按 240 帧窗口平均。`S3`
- **重复计数常量** — `RENDER_NEED_COUNT=6` + `kRenderNeedRules_Count` + enum；`STRATEGY_FACT_COUNT=8` + 无人用的 `StrategyFact::Count`；`kSystemGroupCount` + `SystemGroupID::Count` + 字面 `static_assert == 4`。`S3`

### 2.4 `RenderTo` 手工遮蔽三个权威值（风险最高的一条冗余）✅复核
`Context.cpp:739,777-779,786-788,941-942,944-945`
- 覆写 RT 的**权威清屏色**、覆写 `RenderTargetSystem` 的缓存 RT，再还原
- **每个提前 return / 异常路径都是一次留下错误状态的机会**

---

## 3. 多路径

### 3.1 注册/身份两条路
- **`RegisterTickSystem` / `RegisterRenderSystem` / `RegisterSystem(is_render,…)` 实为一条路**：`is_render` 只用于告警串，容器由**相位**决定（`Context.cpp:1502-1514`）。⇒ 误配相位的系统**静默**改类。`S2 ✅复核`
- **`CreateEntity<T>` 两个互相矛盾的 `static_assert`**：`Context.h:914` 放宽 `is_base_of`，`EntityManager.h:51` 要求 `is_same_v<T,Entity>`；~150 个调用点全传 `Entity`/无参 ⇒ 外层放宽不可用 + 编译错陷阱。`S2`
- **`Object::objectId`（死）与 `Entity::id`（活）两套身份**；系统键用 `type_info::hash_code()` 而依赖用 `std::type_index`。`S2`

### 3.2 组件读取/销毁多条路
- **4 种读组件行**：`Entity::GetComponent<T>` / `GetComponentByRow<T>` / `GetComponents<T>` / `GetTypedPool<T>`，全汇入同一 pool slot。`S2`
- **4 种实体遍历**，含被头文件自己标注"新增消费者一律不要用"的 `GetAllEntityPointers`（每条实体指针一个 vector 元素 = 每帧一次堆分配）—— 而 `RenderGraph.cpp:180-181` 的 `GatherSceneStats` **仍在热路径用它**。`S2`
- **5 个重叠销毁入口 + 双重 detach**：`EntityManager.cpp:50` 显式 `DetachAllComponents()`，紧接着 `~Entity()`（`Entity.cpp:16`）**再调一次** ⇒ 每销毁一个实体付一次**必然全空**的 O(COMPONENT_TYPE_COUNT) 扫描。`S2`
- **组件读取双域**：`Entity::GetComponent<T>`（实体级）vs `GetMaterialData/GetGeometryData/GetShadowProxy/GetCameraByEntity`（世界级）。`Context.h:560-571` 自己警告实体级读法会成"第二个相机读取入口 ⇒ 双真值"。`M ✅复核`

### 3.3 变换遍历两条路（同一函数内混用）✅复核
`Context.h:778-783` 用重笔警告裸遍历 `GetStaticTransforms()/GetMovableTransforms()` 会把空洞当有效 id，"**越界崩溃（2026-10-02 实测）**"，要求一律用 `ForEach*`。而 `TransformSystem.cpp` **两种风格混用**：安全路径 `:98,162,195,214`；危险路径 `:181,265,266,271,303,334,345,358,365,489,490`（靠手写 `if (!IsValidTransformID(handle)) continue;` 兜住 ⇒ 复制了 `ForEach*` 已封装的跳洞逻辑，漏一处即复现崩溃）。附带 `:265/271`、`:266/303` 是同数组重复绑定。

### 3.4 帧生命周期 / 阴影 / viewport 多路
- **两条帧生命周期**：`RenderSystemCore::BeginFrame/EndFrame` 与已注册的 `SwapchainNextImageSystem`/`SwapchainSubmitSystem` **做同两件事**，且都在跑（`DefaultSystems.cpp:203-204` + `Context.cpp:429,630-659,904`）；`RenderSystemCore.h:32-47` 注释还描述着已不存在的 `core->BeginFrame() → Tick → EndFrame()` 循环。`S1`
- **两条互斥的阴影写法**：`MainLightShadowSystem::RenderMainLightShadowPass`（自动 CSM）vs `example/Shadow/ShadowMap.cpp:1276-1289` **手工**写 `EditShadowInfo()` 且从不调 `EnableMainLightShadow` ⇒ 绕过 `cascade_mask`/`consumed_static_scene_revision`/光行 `shadow_kind`。`S1`
- **viewport 同步 5 路扇入** — `RenderTargetSystem.cpp:49-54`、`Context.cpp:1043-1052`、`DefaultSystems.cpp:225`、`CameraSystem.cpp:297-302`、`OffscreenWorld.cpp:124,171`；而 `CameraSystem.cpp:720-726` 自带"**不要在发布路径里绑**"的警告。`S1`
- **`GetGraphicsContext()` 声称已吸收 render-ctx fallback，实际仍有 ≥6 处手写** — `Context.h:614-615` vs `RenderSceneUBOSystem.cpp:44-67`、`RenderPrimitiveCollectSystem.cpp:352-357,462-467`、`EnvironmentSystem.cpp:32-44`、`ColorPaletteSystem.cpp:42-47`、`LineRenderPipeline.cpp:315-319,767-770`、`TextRenderPipeline.cpp:121-123,296-297`；`ResolveEnvManager`（`Context.cpp:2217-2226`）用 `const_cast` 再手写一遍。`S2`
- **三层 `Ensure*` 且核心那层最多被重跑 4 次** — `DefaultSystems.cpp:187,235,255-264`。`S2`
- **`SetSystemGroupEnabled` 快/慢两路实现同一状态变更** — `Context.cpp:3048-3055` vs `:3057-3074`；慢路径在组名缺失于 `systems_by_element_type` 时**静默 no-op**。`S2`

### 3.5 渲染层多路（支撑组量化结论）
- **`PrimitiveRenderSystem` 与 `PrimitiveOverlayRenderSystem` 只差一个取反 + 一个相位** ✅复核：匿名 helper 相同、ctor 相同、gather 相同、`std::sort` 相同、`renderer->Render(...)` 参数逐字相同；唯一差异是 `:64` `if (!IsOverlayLikeBatch(batch)) continue;` vs `PrimitiveRenderSystem` 的正向判断，加 `SetExecutionPhase(RenderDebug)` vs `RenderDrawSubmit` ⇒ **~90 行**。**本子树最清晰的合并候选。**
- **6 个阶段型 `RenderPipelineSystem` 子类都是"GetPipeline + 一行转发"的复制品**；Line 三件套唯一的不同是为了打日志而 `dynamic_cast<LineRenderPipeline*>` — `LineCollectSystem.cpp:30`、`LineBuildSystem.cpp:29`、`LineRenderSystem.cpp:36`。`S3`
- **三条 pipeline 的"取资源/绑全局集/推根地址/画"样板各写一遍** — `LineRenderPipeline.cpp:738-793`、`TextRenderPipeline.cpp:280-332`、`PipelineMaterialRenderer.cpp:120-198`（~60 行/pipeline）。`S3`
- **`PrepareFrame` 无 once-per-frame 守卫（帧守卫写法 3 处只有 2 处正确）** — `PrimitiveBatchPipeline.cpp:102-104`（忘 early return）vs `LineRenderPipeline.cpp:383-386`、`TextRenderPipeline.cpp:213-215` 正确；且 `PrimitiveRenderPipeline::RunCull/RunSort/RunBuild` 各自再调一次 `impl_->PrepareFrame` ⇒ **最多 4 次/帧**，每次都 `GetDrawItemIDStorage()->Reset()`。`S3`
- **`ValidatePipeline` 内部已调 `GetPipeline`，每个调用点又调第二次** ⇒ 6 个系统每帧 12 次字符串键 map 查找。`S3`

**量化**：`~700 行`（占约 6,500 行非测试体的 10%+）是同目录另一文件的逐字/近乎逐字复制。

---

## 4. 特例 / 魔数

### 4.1 测试台架挂在生产默认路径上（最重的特例）
- **`CPUCullReferenceSystem` 是台架，却由 `DefaultSystems.cpp:112` 无条件注册**（`GPUCullSystem` 在 `:103`）。头文件自述"CPU 参考剔除系统（T3d-2；T3g **台架**化）"、"**每 60 帧一个采样窗口**…同步读回 GPU 结果 + 位图对拍"。常驻代价（`CPUCullReferenceSystem.h:78-96`，**每世界约 184 KB**）：`cpu_visible_map_[4096]` + `gpu_visible_map_[4096]` + `gpu_visible_order_[4096]`(16 KB) + **`l2w_row_to_entity_[32768]`(128 KB)** + `alive_owner_[4096]`(32 KB)；外加**每个应用每 60 帧一次 GPU 读回 stall**。而 GPU 剔除侧已"**接管已强制**（opt-in 面整体删除）"（`DefaultSystems.cpp:107`、`GPUCullSystem.h:44`）⇒ 被对拍的一方已无开关，参考实现仍在每个示例里空跑。`M ✅复核`
- **`LineStatsSystem` 无条件注册**（`DefaultSystems.cpp:164`），每 120 帧一条日志，而唯一调参口 `SetLogInterval` **零调用**（`doc/backlog.md:176` 已登记"降为按需注册"）。`M ✅复核`
- **测试代码住在生产目录**：`src/ecs/support/` 下 **11 个** `main()` 可执行体（含 **6449 行**的 `TestCSMIncrementalPass.cpp`，其中 228 处 `GLogError`），与 `ULRE.ECS` 库同列在一个 `CMakeLists.txt`（`:334` vs `:343-412`），合计 **~10,800 行**。`S3/M ✅复核`
- **10 个测试目标中 4 个从未注册进 CTest** —— `TestBoundingBoxStorage`、`TestRenderStrategyTable`、`TestComponentMask`、`TestVisibilityStorage`（`add_test` 只出现在 `CMakeLists.txt:415,422,429,436,443,450`）⇒ **构建了但永不运行，静默腐烂**。`M ✅复核`

### 4.2 硬编码"0 号/1 号"约定（无单一权威）
`kMainLightRowIndex`（光源行 0 恒为主光；`Context.cpp:2043` + `LightSystem.cpp:21,29,37` + `MainLightShadowSystem.cpp:135,143,173,181,222`，**8 处**，且有测试 `static_assert == 0` 钉死）；0 号相机槽恒为默认相机；`kIdentityL2WSlot=0` / `kFirstObjectL2WSlot=1`（`TransformAssignmentBuffer.cpp:22-23`）；材质行 0 作"无材质"哨兵（`MaterialAuthoringSideTable.h:91-95`）；`RenderItemHandle 0` 作空句柄（`ComponentPool.h:141,188`）。`S2/S3`

### 4.3 其它特例
- **`is_current_pass_shadow` 由指针比较推断** — `Context.cpp:835-838` `foreign_target = req.target && (req.target != saved_target)`；代码注释自己记录了上一版读 `render_target` 导致"判据永假、兜底失效（**实测踩过**）"。该位驱动 ≥12 处决策 ⇒ 一次重构即可复现。`S2 ✅复核`
- **`RenderPassOptions` 只在 4 个请求字段非默认时才转发** — `Context.cpp:867-869` ⇒ 手工维护字段清单，加第 5 个选项（如已硬编码的 `clear_depth_value`）静默回落默认。`S2`
- **`Pass::Update` 相位下界被 `std::max(startPhase, RenderDrawSubmit)` 改写** — `RenderGraph.cpp:121-123`，而所有 pass 都声明从 `RenderCollect` 起 ⇒ 文档语义对所有 pass 都是错的。`S2`
- **tick/render 分类按枚举序数值** — `Context.cpp:1503` ⇒ 重排 `ExecutionPhase` 会静默改类。`S2 ✅复核`
- **`CullPushConstants` 仅由 4 个手维护字面 `offsetof` 断言守卫** — `GPUCullSystem.cpp:68-93`，作者自注"布局漂移 = shader 读错字段 = 静默剔错，**门抓不到**"。`S3`
- **所有带 `pass_index` 的 CPU 侧访问器硬编码 pass 0** — `GPUCullSystem.h:73`、`.cpp:518`、`CPUCullReferenceSystem.cpp:109,155,335` ⇒ **每个 batch/counter 缓冲的 shadow 半边无人校验**。`S3`
- **`ArenaSectorBase/BatchBlockBase` 对越界 pass 静默钳到 0**，而 `SetBatchLayout` 对坏 pass **fail-fast** — `GPUCullSystem.h:116-127` vs `.cpp:431-436` ⇒ 同一类内两套相反约定。`S3`
- **`LineBuffer::AddSegment` 在一次有效性检查后做 8 次无门 accessor 写**（中途失败即留下撕裂的顶点对，调用方只 `write_fail_count++`）— `LineRenderPipeline.cpp:234-251`、`:589`。`S3`
- **恒真守卫（假信心）** — `LineRenderPipeline.cpp:570-573`：`kMaxTransformID = uint32 max; transform_id = resolved > kMaxTransformID ? 0 : …`，而 `resolved` 本就是 `uint32_t` ⇒ 分支静态不可达。`S3`
- **~100 行 `getenv` 探针内联在渲染路径**（`PREPASS_TIMING`）— `Context.cpp:449-546`，且**只覆盖 `RenderTo` 路径** ⇒ 其数字与主帧**不可比**。`S2`
- **`ULRE_ARENA_DEBUG` 在 `Render()` 内每次读取（未缓存）** — `TextRenderPipeline.cpp:262,584`，而同类的 `PBR_PERF`/`ECS_TAKEOVER_TRACE`/`CULL_SSBO_DUMP` 有缓存 ⇒ 5 个开关 3 种缓存策略。`S3`
- **函数内 `static` 可变状态**（进程级、非 per-world、不可重置的告警闩）— `GPUCullSystem.cpp:452,531`、`LineRenderPipeline.cpp:618`、`LineRenderSystem.cpp:39,51`、`PipelineMaterialRenderer.cpp:58`、`TransformDataStorage.h:646`、`PrimitiveBatchPipeline.cpp:173,194`。`S3`
- **120/60 帧重试魔数**（`RenderPrimitiveCollectSystem.cpp:214-215`）、**16:9 兜底**（`MainLightShadowSystem.cpp:248-251`）、**16 位 `entity_id` 打包校验和**（`:2406`，`index` 超 16 位即碰撞 ⇒ 废掉该校验和的自述目的）、**按 buffer 名字字符串匹配分类**（`RenderBufferUploadSystem.cpp:85-88`）。`S1`
- **`RenderPrimitiveCollectSystem` 自称"策略表是唯一判据"，却手写 8 处 `IsCurrentPassShadow()` 分支** — `:799,1026,1317,2117,2151,2371,2390,2420`（`:799` 还把该字段当"事实"注入策略表）⇒ 注释与代码直接矛盾。`S1`
- **`c == 0` 级联特判两处** — `MainLightShadowSystem.cpp:307`（无条件整级重绘）与 `:324`（mobility_filter 分流）；同一政策在 `CascadedShadowController.cpp:532` 以 `config_.c0_dynamic_overlay` 再表达一次。`S1`
- **`StrategyFacts` 除 `shadow_pass` 外全默认 true** — `RenderStrategyTable.h:96-103` ⇒ 漏填字段的失败模式是**过度收集**而非漏收集。`S3`
- **`TextRenderPipeline` 硬编码文本材质只在 index 0 有名为 `base_color` 的纹理引用** — `TextRenderPipeline.cpp:490-503`。`S3`

---

## 5. 名不符实

| 项 | 位置 | 实际行为 |
|---|---|---|
| `RegisterTickSystem` / `RegisterRenderSystem` | `Context.h:1009,1016` | 不决定任何列表；相位决定（`Context.cpp:1503`）✅复核 |
| `Object::GetID()` | `core/Object.h:46` | 标识不了任何东西（零读者）✅复核 |
| `ECSContext::GetActiveCameraID()` | `Context.h:517` | 零调用者；真正的调用点用 `GetActiveCameraRow()` ✅复核（另见 §6） |
| `CameraSystem::GetActiveCameraComponent()` | `CameraSystem.cpp:275-284` | **读操作写世界状态**（经 `GetMainCameraComponent` → "幂等**认领**" ⇒ `SetDefaultCamera`）✅复核 |
| `SyncRenderTargetViewport()` | `Context.cpp:1143-1157` | 不同步 viewport，而是比较分辨率并触发 `OnResize` |
| `UploadEntityGPUDirtyRows(uint32_t frame_slot)` | `Context.h:446` | 写全部 8 份并丢弃参数；兄弟上传函数无参数 |
| `RenderGraph::Pass::runUpdate` | `RenderGraph.h:41-42` | 相位下界被替换为 `RenderDrawSubmit`，文档语义对所有 pass 均错 |
| `MaterialBatch::Clear()` | `MaterialBatch.cpp:54-84` | 清内容**并重挂所有权位**（`own_* = true`）⇒ "clear" 改变析构语义 |
| `Entity::SetComponentRow` 返回值 | `Entity.h:86` | 0 同时表示"无旧行"与"类型非法、未写入"（`Entity.cpp:35-36`） |
| `EntityGPU::type[16]` | `EntityGPU.h:47-49` | 自注"**显式冗余**" |
| `ECSContext::GetCamera(uint32_t slot)` / `GetDefaultCamera()` | `Context.h:549-552` | 返回槽账本的宿主；后者字面是 `GetCamera(0)` |
| `RenderFrameCache::BeginFrame()` | `Context.cpp:58-69` | 不重置 `cameraInfo`（只 `Shutdown` 置空）⇒ 帧边界后可能读到上一帧相机 |
| `ECSContext::IsEntityVisible` | `Context.h:773-776` | storage 为空时返回 `true` ⇒ **失效开放**，与 GPU 表的 fail-fast 纪律相反 |
| `EntityManager::Clear()` | `EntityManager.h:124` | 重置 `next_index` 但不重置 `max_entities` |
| `EntityManager::AddFreeIndex` | `EntityManager.cpp:149-156` | 维护有序（`lower_bound`）却按 **LIFO 栈**消费（`.h:57-62`）⇒ O(n) 插入白付 |
| `TransformSystem::UpdateStaticDirty` | `TransformSystem.cpp:142-173` | 只标脏不提交；真正提交的是 `SubmitTransformUpdates` |
| `TransformSystem` 放置 | `systems/tick/` | 不在 `DefaultSystems.cpp` 注册（由 `Context.cpp:140-146` 自动注册）⇒ 读安装器会误判其可选/死 |
| `RenderSystemCore` 放置 | `systems/render/`、列进 `ECS_CORE_FILES` | **不是 `System` 子类**（无 `Update`/相位）⇒ `GetSystem<>()` 永找不到；且同一对文件在 CMake 登记**两次**（`:22,39` 与 `:42-45`）✅复核 |
| `RenderSceneUBOSystem` | `systems/render/` | 名字指向已不负责的 UBO 绑定；**实际职责是纹理 bindless 句柄注册表**（`RenderPrimitiveCollectSystem.cpp:1209,1432` 伸进来取句柄） |
| `RenderBufferUploadSystem` | `.cpp:47-56` vs `.h:20-22` | 类注释描述的是**通常是 no-op** 的那半（设备注册表扫描），真正干活的是两个 `SyncToGPU` |
| `RenderBufferUploadSystem` 双 context | `.h:30` + 继承的 `System::context` | 同一对象两个名字，`.cpp:22` 用 `world ? world : context` 消歧 |
| `MaterialBatch::static_count` | `MaterialBatch.h:48` | 恒 0 |
| `RenderItem::index` | `RenderItem.h:35` | 只写不读 |
| `TransformDataStorage::SetWorldMatrix` | `.h:319` | 写公开声明的"派生只读缓存"并清 `matrixDirty` 抑制重算 |
| `TransformDataStorage::GetLocalMatrices` | `.h:880` | **证伪其上方 2 行注释**"0 引用的读取口已删" |
| `GPUCullSystem::IsOverrideActiveThisFrame` | `.h:205` | 自称 CPU 剔除闸门，零调用；真闸门是 `PrimitiveBatchPipeline::IsGPUCullFullTakeoverActive()`（读**另一对**帧闩） |
| `GPUCullSystem.h:299-303` | 注释 | **为一个不存在的成员写的文档块**（下一行是 `visible_indices_addr_`）⇒ 读者会以为有守卫 |
| `LineCollectStats::culled_by_visibility` | `LineRenderPipeline.cpp:432-436` | 对**无 segment** 的组件自增（真实可见性剔除在 `:442-445` 用同一计数器）⇒ 把"空"与"被祖先隐藏"混为一谈 |
| 两处日志打印同一值两个标签 | `LineRenderPipeline.cpp:795-797`；`CPUCullReferenceSystem.cpp:262-265` | `submitted=X expected=X`；`值链=X/X`（恒 1:1 比值）—— 恰是运维读来判帧健康的那几行 |
| `TextSyncSystem` | `text/TextSyncSystem.cpp:10` | 名为 Sync 相位，实际 `SetExecutionPhase(RenderBatch)`；基类 `SyncSystem` 文档写的是 Upload/FrameSync |
| `PrimitiveSortSystem` / `PrimitiveCullSystem` | `.h:8` vs `.cpp:10` | 头文件声明 "RenderSort/RenderCull phase"，实际都设 `RenderCollect`，且 **`RunSorting()` 空体 ⇒ 排序系统不排序**（但已注册进真实帧图） |
| `TransformAccessor::MarkDirty()` | `.h:130` / `.cpp:535-539` | 只标脏不 bump 版本；其私有兄弟 `MarkLocalChanged` 的注释说明"只标脏不 bump，子节点 L2W 行不会重传" ⇒ 该公开 API 标记的是**永不上传的活** |
| `TransformAccessor::SetFixedPixelSizingContext` | `.cpp:405-426` | 名字像设置上下文，实际**直接改实体 SCALE**（`SetLocalScale` → 递归 `MarkLocalChanged` 全子树） |
| `MaterialRuntimeSlot::pass_selector/lod_selector/dither_enabled` | `MaterialRuntimeTable.h:167-168` | 注释称"当前 pass / LOD 档"，实际 `pass_selector` 只被测试写；真实 per-pass 机制是另一张表 |
| `MaterialRuntimeRow::generation` | `.h:18-20,155` | 描述了"与 Transform/BoundingBox 同约定"的 ABA 免疫规则，但该机制在此**无消费者**（`GetGeneration` 只被测试读） |
| `GPUCullSystem::GetGPUVisibleCount` | `.h:160-165` | 头部要求先 `SyncForParityReadback()`，而其唯一生产消费者 `DumpCullSSBOs` 直接当"全局幸存数"用 |
| **`ComponentTypeTable.h:105` 说 `MaterialRuntimeSlot` "行宽冻结 88 B"** | vs `MaterialRuntimeTable.h:211-212` `static_assert(sizeof(...) == 40)` | 两份"冻结契约"互相矛盾，88 B 那份在**每个组件类型消费者都读的表**里 ✅复核（88 B 侧） |
| `RenderPipelineBase::RunCollect()` | `.h:33` `= 0` vs `PrimitiveRenderPipeline.h:36` `override {}` | 声明为必需，对 primitive 是刻意的 no-op ⇒ "Collect" 不是该层的阶段 |
| `PrimitiveBatchPipeline::PrepareFrame` | `.cpp:85-113` | 名字与两个兄弟相同，但**没有 once-per-frame 守卫** ⇒ 每帧多跑 3 次全量并反复 `Reset()` |
| `RenderItemDataStorage::GetActiveCount` | `.h:101` | 由"总数 − 空闲数"推算，而 `Release` 无"已释放"检查（`.cpp:59-68`）⇒ 重复释放即虚高 |
| `SceneStats` | `RenderGraph.h:98-119` | 名字承诺场景统计，实为一个组名 `set<string>` + 两个成员查询 + 缓存键 |
| `Object` 生命周期注释 | `Object.h:54-59` | 说"**三套**体系"却只列两套（第三套 `Object::OnUpdate` 已删）✅复核 |
| `ECS_SYS_07_RenderSwapchainNextImage_SwapchainAcquireSystem_FILES` | `src/ecs/CMakeLists.txt:71` | CMake 变量名指向不存在的 "SwapchainAcquireSystem"（实际文件是 `SwapchainNextImageSystem`）✅复核 |

---

## 6. 跨层问题：文档与代码已脱钩（横切）

这类问题比单个死符号更值得处理，因为它**正在误导规划**。

| 文档声称 | 代码实际 |
|---|---|
| `SortBatchItems`（**5 份文档**描述为批排序主函数，含 `PrimitiveBatchPipeline.cpp:786-831` 行号） | **全仓不存在**（`grep SortBatchItems` 只命中 `.md`）；排序已移到 `PrimitiveRenderSystem.cpp:60` / `PrimitiveOverlayRenderSystem.cpp:55` 的 `std::sort(ordered_batches…)`（排的是**批**，不是 item）✅复核 |
| `RenderItem::Compare()` 是"**批排序主键**"（`doc/render-flow-cull-sort-drawcall.md:114`） | **零调用者** ✅复核 |
| `doc/future/stage_B_execution_plan.md:370`、`doc/future/stage_B_debt_list.md:249` 把 `RenderItem::Compare` 当作**待改造的活约束**（"删它们等于改实例化合并判定"，并记录了一轮"已试改已还原"的侦察） | 该函数**已死** ⇒ 规划正围绕一个不存在的依赖做设计 ✅复核 |
| `PrimitiveRenderSystem.cpp:86-92` 调用 `context->GetActiveCameraID()`（**3 份文档**） | 实际调用 `GetActiveCameraRow()`（`PrimitiveRenderSystem.cpp:87`）⇒ `GetActiveCameraID()` 零调用者 ✅复核 |
| `SetElementTypeSystemsEnabled(...)`（**4 份文档**） | **代码里没有该符号**；实际 API 是 `SetSystemGroupEnabled(SystemGroupID,bool)`（`Context.h:1042`、`Context.cpp:3040`）✅复核 |
| `doc/ecs-layer-architecture-and-frame-flow.md` 引用 `Context.h:422-429/433-436/440-443/410/48-63/128/130` | 实际对应 `:999-1019 / :986 / :77-88 / :261 / :263` ⇒ 按行号跳转会落到无关代码 |
| `doc/future/entity-rows-gpu-cull-plan.md:1253` 把 "CPU 排序 `PrimitiveBatchPipeline`（两处 `std::sort`）" 列为待删活代码 | 该文件已无 `std::sort` |
| `ComponentTypeTable.h` 注释"0 引用的读取口已删"（`TransformDataStorage.h:878`） | 下一行 `:880` 就是零引用的 `GetLocalMatrices` |

---

## 7. 已核查、判定"不是问题"（防止重复翻案）

- `ViewportInfo::GetViewport()` 返回的是**分辨率**（`Vector2u`），故 `Context.cpp:1155` 的比较是正确的 resize 检查，**不是** x 偏移 bug。`S2`
- `ExecutionPhase` 16 个值**全部有生产者**，相位分发器均可达 ⇒ `System.h` **无死枚举值**。`S2`
- `EntityGPU` 本体及其 flags、`type[]`/`row[]`、shadow-fact 字段**全活**（只有 `work_flags` 死）。`S2`
- `RenderPassRequest` 各字段（除上述注释项）全被设置并消费；`MaterialBatch::device`/`own_*`/`gpu_slice[]` 全被读。`S2`
- `uses_render_item_resolve` 恒假，但同行的 **`gpu_driven_override` 是活的**（`GPUCullSystem.cpp:832` 置 true）✅复核
- 系统侧**历史性双实现确已清干净**：无第二套相机系统、无重复光照更新路径；`LineBoundsUpdateSystem`（写包围盒）与 `LineStatsSystem`（读统计）状态不相交。`S1`
- `EntityGPUTable::Deactivate`、`GPUCullSystem::UnmapCurrentBatchCounters`、`LinePeriodicLog`(8 处)、`ParityCheckShadowCaster`、`shadow_tracked_material_data_generation`、`CollectDirtyRows/GetDirtyCount/ClearDirty`、`GetMaxWorldScale`、`Set4ID`、`GetSharedRowCount/GetOwnedRowCount`、`EvaluateRenderNeed`、`SystemGroupFromName` 等**均活**。`S3`
- **工具警告**：用 `Select-String -Pattern <数组>` 做批量符号计数会**静默少计**（曾把 `UnmapCurrentBatchCounters`、`LinePeriodicLog` 报成 0 引用）。本档所有"零调用"结论均已用原始 `grep` 复核。`S3`

---

## 8. 建议处置顺序

| 优先级 | 项 | 类别 | 备注 |
|---|---|---|---|
| **P0** | §0.1 `LineBuffer::EnsureCapacity` 失败后 `gpu_capacity` 不回落 | **缺陷** | 后续调用带空 geometry 返回 true |
| **P0** | §0.2 L2W 缓冲按设备上限截断而行号算术用预算 | **缺陷** | 越界写 |
| **P0** | §2.4 `RenderTo` 三处手工遮蔽/还原权威值 | 冗余 | 建议改 RAII 守卫（提前 return 安全） |
| **P0** | §3.1 `Register*System` 的 `is_render` 被丢弃 | 名不符实 | 相位误配**静默**改类 |
| **P0** | §1.2 `CameraControlMode::Free` 默认值 + 处理器全空 | 特例 | 建议先加显式告警，把静默冻结变成报错 |
| ~~P0~~ | §1.2 `uses_render_item_resolve` 恒假 | 死代码 | ✅ **已处置（2026-10-09）**：恒假开关整体删除（`MaterialBatch.h` 字段 + `.cpp` 复位 + `PrimitiveBatchPipeline.cpp` 两处判定）；**4-ID 表 / `addr_global_render_items` / shader 解析链保留** —— 那是 GPU-Driven 路径的活路径 |
| **P1** | §6 文档-代码脱钩（尤其把死函数当活约束的规划档） | 文档完整性 | 正在误导设计 |
| **P1** | §2.3 4×`Initialize*Table` + 3×`Upload*DirtyRows`（~450 行） | 冗余 | `Context.cpp` 最大机械简化 |
| **P1** | §3.5 `PrimitiveRenderSystem` / `PrimitiveOverlayRenderSystem` 合并（差一个取反） | 多路径 | ~90 行，最清晰候选 |
| **P1** | §2.3 三张表的脏行机制抽公共件（~120 行）+ 两个 `Write*DirtyIndices`（~200 行） | 冗余 | |
| **P1** | §3.2 双重 `DetachAllComponents`；§2.2 `component_mask` 一致性仅测试保证 | 多路径/冗余 | |
| **P1** | §4.1 `CPUCullReferenceSystem` 退出默认注册；4 个未注册测试补 `add_test` | 特例/测试 | 低风险 |
| **P2** | §1.1 零调用 API 批量删除（约 45 项，含 `Object` 身份半边） | 死代码 | 免费 |
| **P2** | §3.3 变换遍历统一走 `ForEach*` | 多路径 | 须逐处核对跳洞 |
| **P2** | §4.2 硬编码 0/1 号约定收敛为具名权威 | 特例 | |
| **P3** | CMake 空壳变量（`ECS_SYS_17..26` 全空仍列在 `ECS_SOURCE` + `source_group`）、重复登记、变量名不符；测试移出生产目录 | 结构 | |

---

## 附：审计未覆盖 / 已知边界

- `TestCSMIncrementalPass.cpp`（6449 行）仅结构层面分析，未逐行审其断言内容。
- `src/ecs` 之外的消费者（`SceneGraph/`、`example/`）只在与 ECS 交界处检查。
- 本档写于工作树含 `69df4fdde 简化WorkObject/WorkManager/AppFramework` 的状态；该提交已顺带消掉上一轮框架层的若干项（`WorkManager` 序列空架子、`TickObject` 双名、`AppFramework` 空虚函数等）。
