# 可执行任务：旧 `SSBOBufferRegistry` 死码清理 + 双注册表收敛

> **依据**：grep 实测存活度清单（附录 A，**T0 已核对完成**）。
> **基线**：分支 `GPUDriven4ID_1_MaterialDataBuffer`，HEAD=`0393187da`（工作树仅子模块/tool 目录脏）。
> **纪律**：每任务独立编译闭包 → build + run 验证才进下一任务；**改名放最后**；结构头手术先 clean-first 全量重编；提交信息中文。
> **不动**：`MaterialSSBOBufferRegistry`（per-type 池 + `ActiveIDManager` + `MaterialSSBODataAccessor`）与纹理引用池逻辑，仅做「迁址/改名/删死码」。
> **已知未决**：回归门 `V1.material-output-contract` 现 FAIL（与本计划无关）——每任务验收只**记录**该项是否仍为唯一 FAIL，不借清理掩盖。

---

## 目标

1. 旧 `SSBOBufferRegistry` 的死码全删（附录 A），只剩 3 个活职责：**纹理引用池 / L2W 域 / null 行**。
2. 职责与命名重新划分：
   - `MaterialSSBOBufferRegistry` = 材质数据行池 + null 行
   - 旧注册表改名 **`MaterialTextureReferenceRegistry`** = 纹理引用池
   - L2W 域**下沉** `TransformAssignmentBuffer`

---

## T0 前置核对 —— ✅ 已完成（2026-09-12，只读）

**结论（逐项证据见附录 A）：**

1. **`GetBuffer` / `GetGPUBuffer`（注册表的 `SSBOAddress` 重载）无外部调用者** ✅ —— 全仓仅 `SSBOBufferRegistry.cpp:344`（`EnsureBuffer` 内部）与 `:464`（`GetGPUBuffer` 内部）互调；其余同名命中都是 buffer 族自身的方法（`ArrayView.h:230`/`BufferView.h:78`/`DeviceBuffer::GetBufferDeviceAddress(buf->GetBuffer())` 等）。→ 与 #2 同轮删。
2. **`SSBOBinding` 是活代码（不是注释）**：`ArrayView.h:208 GetSSBOBinding()` —— 但**该方法零调用者**；`ArrayView.h:4` 的 include 仅为此服务。→ 删 `GetSSBOBinding`/`ssbo_id`/`ssbo_type`/`GetSSBOId`/`GetSSBOType` 后 `SSBOBinding` 自然无用户。
3. **`SSBOIdNamespaceBit` / `IsECSSSBOId` / `GetSSBOIdLocalPart` 零消费者**（仅 `SSBOTypes.h` 自身定义）→ 可删；**`ECSReservedSSBOId` 例外**（TAB 用，随 T4）。
4. **`SSBOType` 枚举值**：`MeshDrawParams` **零消费者**（仅自身定义）→ 可删；`UserDefined` / `LocalToWorld` / `LocalToWorldIndex` **仍活**（注册表/目录/schema/gate/TAB）→ 枚举整体**保留**。
5. **`ArrayView<T>` 是活的**（⚠️ 推翻原判断）：`TextRenderPipeline.h:60-62`（三表视图成员）+ `TextRenderPipeline.cpp:793/809/825`（`ArrayView<T>::Create`）——**类不能删**；但它的 `ssbo_id`/`ssbo_type`/`GetSSBOId`/`GetSSBOType`/`GetSSBOBinding` + `friend class SSBOBufferRegistry`(`:62`) + `#include <hgl/mtl/MaterialRecipe.h> ///< for mtl::SSBOType / mtl::SSBOBinding`(`:4`) **只服务旧注册表** → 清理。（`TypedArrayView<T>` 是另一类型，不受影响。）
6. **`GetMaterialTextureReferencePoolCount` 零消费者**，非诊断用途 → 可删。
7. **stride/version 依赖链确认**：`RegisterBuffer`（TAB `:512`/`:613` 两处）→ `ValidateStructStrideForDomain` → `GetSSBOTypeStructStride`/`GetSSBOTypeStructVersion`；三者消费者**仅** `SSBOBufferRegistry.cpp`（+ 自身定义）→ **只能在 T4 之后删**（且校验对 TAB 是**生效**的：L2W stride=64 / L2WIndex stride=4）。

**由 T0 产生的任务微调**：T1 增「`ArrayView` 的 SSBO-id 成员/方法/include/friend 清理」；T2 增「`SSBOIdNamespaceBit`/`IsECSSSBOId`/`GetSSBOIdLocalPart` 删除」；T6 的 `ArrayView` 复核项改为「已确认活，仅成员清理」。

---

## T1 删「材质数据行段/arena 分配器」整面（附录 A #1+#2+#3+#5）—— ✅ 已完成（2026-09-12）

> **执行记录**：全量构建 EXIT=0（重编 739 TU，无新警告）；回归门 **38 PASS / 1 FAIL**（仍是既有的 `V1.material-output-contract`，与 T1 前基线逐项一致）；示例冒烟 **9/9 errors=0**（PBRSpheres/TextureQuad/SimpleCube/AutoMergeMaterialInstance/BasicLitMeshes/TextDrawTest/RenderToTexture/TextureRect/TextureRectArray，帧数 4000+）。
> **净变更**：8 文件 **+14 / −549**（`SSBOBufferRegistry.h` −159、`.cpp` −105、`ArrayView.h` −63/+14、`SSBOTypes.h` −11、删 `ActiveArrayView.h` −220）。
> **T0 追加发现**：`ActiveArrayView<T>`（220 行）全仓零消费者（仅自身头 + VS 构建缓存）→ 一并删除。
> **顺带修正**：`ArrayView` 的移动构造/移动赋值原先**丢失 `stride_bytes`**（`MoveFrom` 后未搬运该字段），已补上。

**改动点**（`SSBOBufferRegistry.h/.cpp`）：
1. 删 `AllocateArrayAccessor<T>`（`.h:189-235`）+ `AllocateSSBOId`（`.h:166` / `.cpp:40-43`）+ `next_ssbo_id`（`.h:56`）。
2. 删 `EnsureArrayAccessor<T>`（`.h:247-272`）、`EnsureBuffer`（`.h:84-88` / `.cpp:337-416`）、`GetBuffer`/`GetGPUBuffer`（`.h:95-97` / `.cpp:456-466`，T0 已证无外部调用者）。
3. 删 `row_segments`（`.h:50`）、`RowSegmentInfo`（`.h:39-46`）、`TryGetRowSegment`（`.h:141-152`）、`Release()` 的 `row_segments` 段（`.cpp:157-162`）。
4. **`ArrayView` 清理**（`inc/hgl/vk/buffer/ArrayView.h`）：删 `ssbo_id`/`ssbo_type` 成员、`GetSSBOId`/`GetSSBOType`/`GetSSBOBinding`、`friend class SSBOBufferRegistry`（`:62`）、`#include <hgl/mtl/MaterialRecipe.h>`（`:4`）；同步删移动构造/赋值的对应字段搬运（`:154-155/160/172-173/177`）；`SSBOTypes.h` 的 `SSBOBinding` 随之删。
5. doc 注释同步：`.h:168-188`、`ArrayView.h:17/26/33-38/41/53`、`BufferView.cpp:14`。

**验收**：编译过；`SimpleCube`/`PBRSpheres` 冒烟 errors=0 帧数连续；gate 记录（应仍只 V1 FAIL）。

**风险**：`EnsureBuffer` 依赖 `ValidateStructStrideForDomain`（T4 才删）→ 本任务不动该函数。

---

## T1.5 `active_id_manager` 下沉进 `MaterialSSBOBufferStorage` —— ✅ 已完成（2026-09-12）

> **动机（用户口径）**：把「单数据类型的 Buffer + BufferView + 独立访问器」做成**一个整体**（包含原则：Buffer 创建与 View 创建分离，未来可整体抽离为通用行池，供 `SSBOBufferRegistry`/纹理池复用）。
> **实际改动**：`MaterialSSBOBufferStorage` 增 `ActiveIDManager ids` 成员；删掉并列的 `ActiveIDManager active_id_managers[]` 与两个 `GetMaterialDataIDManager()` 帮助函数；`Acquire/Release/IsMaterialDataIDActive/CommitMaterialData` 改走 `storage->ids`；`Release()` 里 `storage = {}` 改逐字段重置（因 `ActiveIDManager` 拷贝已禁用、只可移动）。**语义逐字等价**（同一套 `HasIdleID/GetIdle/GetHistoryMaxId/CreateActive/IsActive/Release` 调用）。
> **验证**：purge 依赖（48 文件 / 67 obj / 模块缓存）+ 全量构建 EXIT=0（39 TU）；gate **38 PASS / 1 FAIL**（仍为既有 `V1`）；示例冒烟 **9/9 errors=0**（~4200 帧）。

> ⚠️ **过程中一个假回归（值得记）**：首次构建后 5 个用材质字段数据的示例（PBRSpheres/SimpleCube/AutoMerge/BasicLitMeshes/RenderToTexture）在 `MaterialSSBOBufferRegistry` 初始化时误报 `Duplicate default material buffer: type=EmissiveSurface`。用「同函数内 `sizeof` vs helper 步进」对打印定位到：`sizeof(storage)=136` 而 `GetMaterialBufferStorage` 的步进是 **40**（旧布局）——是 MSVC `/scanDependencies` 模块缓存 + 旧 obj 导致**链接器绑定了旧布局的内联 COMDAT**，**T1.5 代码本身无罪**。清 obj + `*.module.json` 后全绿。已写入技能 `crlf-safe-editing`。

---

## T1.6 抽出通用「行池」三件套（ActiveRowPool / ActiveArrayView / ActiveRowLease）—— ✅ 已完成（2026-09-12）

> **用户口径（包含原则）**：把「一个数据类型的 Buffer + BufferView + 独立访问器」抽成可复用整体，**Buffer 创建与 View 创建分离** —— 遍历期只按 (行距, 容量) 建 Buffer（类型擦除、可 for 遍历），使用期再把 BufferView 关联上去；先自己用（材质字段池），日后给 `SSBOBufferRegistry`（L2W 域 / 纹理引用池）复用。
> **落地**：
> - **新增** `inc/hgl/vk/buffer/ActiveRowPool.h` + `src/Vulkan/buffer/ActiveRowPool.cpp`：类型擦除行池 = Buffer + 元数据（CPU/GPU 基址、行距、容量、ssbo_id）+ 行号空间（`ActiveIDManager`）。`Create(device,name,row_bytes,capacity,ssbo_id)` 只建 Buffer（`CreateArenaBuffer`：HOST_VISIBLE 直写 + BDA，建后清零整块标脏）；`Acquire/Release/IsActive`、`RowCPU/RowGPU`、**按行** `CommitRow`。
> - **重写** `inc/hgl/vk/buffer/ActiveArrayView.h`：行池上的**类型化视图**（使用期关联）——借用池、不拥有 Buffer/行号空间；`Attach` 校验 `sizeof(T)==行距` 后把 `ArrayView<T>` 挂到池宿主窗口（外部窗口：直写、不经 Map）；`GetByID/WriteByID/ReadByID/CommitByID`。
> - **新增** `inc/hgl/vk/buffer/ActiveRowLease.h`：**通用 RAII 租约**（构造 = 关联池 + 申请行号；析构 = 自动归还），不依赖任何 registry —— 任何持有 `ActiveRowPool` 的地方都能造。
> - `MaterialSSBOBufferRegistry` 改持 `ActiveRowPool material_row_pools[RANGE_SIZE]`（创建期 `ENUM_CLASS_FOR` 遍历建池，不再手写 storage/ID 管理器）；`MaterialSSBODataAccessor<T>` 退化为「通用租约 + 材质身份（`GetMaterialSSBOBinding`）」，**示例侧 API 与语义不变**（`Write`/`GetSSBOBinding`/`GetSSBOId`/`operator bool` 全保留）。
> - `ArrayView<T>` 增 `template<typename> friend class ActiveArrayView;`（外窗口关联路径）；`src/Vulkan/CMakeLists.txt` 登记 3 个新文件。
> - 顺带删掉 registry 的**零消费者**查询面：`MaterialSSBOBufferBinding`、`TryGetMaterialBinding`、`GetMaterialBuffer`、`GetMaterialGPUBuffer`、`GetMaterialElementCapacity`、`GetMaterialSSBOId`（保留 RPC 用的 `TryGetRowBuffer`/`IsMaterialDataIDActive`、GraphicsContext 用的 `IsInitialized`）。
> **验证**：purge 依赖 + 全量构建 **53 TU**（C++/LNK 错误 **0**；`BUILD_EXIT=1` 仅来自 CMCore 无关测试的 vcpkg 后置步骤，属既有环境问题）；gate **38 PASS / 1 FAIL**（既有 `V1.material-output-contract`）；示例冒烟 **9/9 errors=0**（~4200 帧）。
> **净变化**：tracked diff +145 / −598；新增 3 文件（~530 行）。
> **下一步（复用面）**：把 `SSBOBufferRegistry` 的 L2W 域、`MaterialTextureReferencePool` 换装同一行池 —— 需要把两种「非固定容量」语义做成**池外策略**：L2W 的「增长 → 重建 buffer（基址变）」、纹理池的「`retire_epoch` 延迟回收」。

---

## T1.7 行池扩展（字节行 / 预留行 / 延迟回收）+ 纹理引用池换装 —— ✅ 已完成（2026-09-12）

> **需求（用户）**：还有「**一行只有字节数、没有 C++ 结构体**」的用法（例：`MaterialTextureReferencePool` 里 per-material 的纹理 ID 列表 = `uvec2[reference_count]`，行距运行时才知道、元数随材质变化），行池必须兼容。
> **通用层扩展**（`ActiveRowPool`）：
> - `Create(..., reserve_rows, bda_align16)`：**预留行**（如行 0 恒为零行——占住行号、永不分配）+ 可选 16B 对齐设备地址（纹理引用行用，材质字段行不需要）。
> - `ReleaseDeferred(id, ready_epoch)` + `CollectRecyclable(completed_epoch, out_recycled)` + `IsPendingRelease(id)`：**延迟回收**（retire 语义），到期回收时按行清零。
> - **新增字节视图/租约**：`ActiveByteView.h`（`WriteRow` 整行写先清零 / `WriteAt` 偏移写 / `ReadByID` / `CommitByID`）、`ActiveByteLease.h`（RAII 租约，与 `ActiveRowLease<T>` 对称）。
> **首个消费者**：`MaterialTextureReferencePool` 换装（**公开 API 逐字不变**）——删掉自管的 `buffer/cpu_base/gpu_base/free_rows/retirements/ReleaseRow/IsRetired` 与 `MaterialTextureConfigurationRetirement` 结构，保留池外语义（pool_key/layout 匹配、每行分配代、活跃计数）；`Acquire/Write/Retire/CollectRetired/IsValidAllocation/GetZeroRowAddress` 全部由行池承接。`Write` 改走 `ActiveByteView::WriteRow`（整行写 + **按行 MarkDirty**，与材质字段池一致；`IGPUBuffer::MarkDirty` 只记状态、上传仍由 ECS 统一做）。
> **验证**：purge 44 文件 / 34 obj + 全量构建 **79 TU、C++/LNK 错误 0、BUILD_EXIT=0**；gate **38 PASS / 1 FAIL**（既有 `V1`）；示例冒烟 **9/9 errors=0**（~4200 帧，含纹理/文本例——每帧都走 acquire/retire/collect 路径）。
> **净变化**：`MaterialTextureReferencePool` −218/+…（自管行管理整段消失）；通用层 +214。

---

## T1.8 统一为「一套字节机制」（删掉类型化视图/租约）—— ✅ 已完成（2026-09-12）

> **用户口径**：类型化（`ActiveArrayView<T>`/`ActiveRowLease<T>`）与字节（`ActiveByteView`/`ActiveByteLease`）两套并行是多余的 —— **全部按字节来**，类型由使用方在访问点强转。
> **落地**：
> - 通用层收敛为**一套**：`ActiveRowPool`（池）+ `ActiveRowView`（行视图，按字节）+ `ActiveRowLease`（行租约，非模板）。删除 `ActiveArrayView.h` / `ActiveByteView.h` / `ActiveByteLease.h`。
> - 视图/租约同时提供**字节接口**（`GetByID` / `WriteRow`（整行写，先清零）/ `WriteAt`（偏移写）/ `ReadByID`）与**类型化便利**（`GetAs<T>()`、`WriteAs<T>(id,v)`、`ReadAs<T>(id,out)`；租约上为 `GetAs<T>()` / `template<typename T> Write(const T&)` / `Read(T&)`）—— 类型只在访问点出现一次。
> - 材质侧 `MaterialSSBODataAccessor` **去模板**（= 字节租约 + 材质身份），`GetMaterialDataAccessor(mtl::MaterialSSBOType)` 为主入口，另留 `GetMaterialDataAccessor<T>()` 便捷重载（`MaterialRowTypeTraits` 查表 + 行距校验）。
> - 示例/gizmo：**23 处** `MaterialSSBODataAccessor<X>` 声明去掉类型参数（每文件净改 1 行，行尾未动）；`acc.Write(row)` 调用点**零改动**（模板成员按实参推导）。
> - 清理：`ArrayView.h` 的 `friend class ActiveArrayView;`（指向已删类）删除；`BufferView::AttachWindow` 现仅被 `ArrayView` 那个已无人用的外部窗口构造调用 → 记入 T2 候选（连同 `stride_bytes` 死分支）。
> **验证**：purge 42 文件 / 34 obj + 全量构建 **78 TU、CPP_ERRORS=0、BUILD_EXIT=0**；gate **38 PASS / 1 FAIL**（既有 `V1`）；示例冒烟 **9/9 errors=0**；旧名（`ActiveArrayView`/`ActiveByteView`/`ActiveByteLease`）全仓零残留。
> **用法对照**：
> ```cpp
> auto acc = registry->GetMaterialDataAccessor(mtl::MaterialSSBOType::PBRSurface);  // 主入口（字节）
> // 或 auto acc = registry->GetMaterialDataAccessor<ssbo::PBRSurfaceRow>();        // 便捷（查表 + 行距校验）
> acc.Write(pbr_row);                              // 类型由实参推导：写 sizeof(T) 字节 + 按行提交
> auto *row = acc.GetAs<ssbo::PBRSurfaceRow>();    // 需要指针时显式指定类型
> // 纯字节行（纹理引用）：acc.WriteRow(refs, n*sizeof(ref)) / acc.WriteAt(off, bytes, n)
> ```

---

## T2 删「绑定时代残留 + 伪 id」（附录 A #4+#6+#7+#9）

1. 删 `Touch`（`.h:80` / `.cpp:276-288`）、`TryGetBinding`（`.h:93` / `.cpp:446-454`）、`HasBinding`（`.h:92` / `.cpp:441-444`）、`GetElementCapacity`（`.h:99` / `.cpp:468-472`）、`GetCount`（`.h:101`）、`GetMaterialTextureReferencePoolCount`（`.h:135-139`）。
2. 删 `MakeECSSSBOId` / `SSBOIdNamespaceBit` / `IsECSSSBOId` / `GetSSBOIdLocalPart`（`SSBOTypes.h:104-125`，T0 已证零消费者）；`ECSReservedSSBOId` **保留**（TAB 用，T4 迁）。`MakeRecipeSSBOId` **保留**（仍被 `SerializedDescriptorEntry.h`/`ShaderResourceSchema.h`/`MaterialSSBOBufferRegistry.cpp`/gate 用）。
3. `SSBOBinding` / `MakeSSBOAddress`：若 T1 已随 ArrayView 删 `SSBOBinding`，此处只清 `MakeSSBOAddress`（T0 未见消费者；`SSBOAddress` 结构保留至 T4）。

**验收**：编译过；gate 全跑（仍只 V1 FAIL）；删除符号全仓 grep 零引用。

---

## T3 null 行迁入 `MaterialSSBOBufferRegistry`

1. 新注册表增 `GetNullRowAddress()` + `null_row_buffer`/`null_row_address`（沿用惰性创建：`CreateArenaBuffer("NullMaterialRow", 64)` + `memset 0` + `GetBufferDeviceAddress`）。
2. 旧注册表删 `GetNullRowAddress`（`.h:154-158` / `.cpp:98-116`）+ 两成员 + `Release()` 段（`.cpp:130-135`）。
3. 3 个调用点改走新注册表：`LineRenderPipeline.cpp`、`PrimitiveBatchPipeline.cpp`、`TextRenderPipeline.cpp`（改名后 getter 由 T5 统一处理，本任务先按现名取对象）。

**验收**：编译过；文本/Line/批三类冒烟（`TextDrawTest` / 线段示例 / `SimpleCube`+`PBRSpheres`）errors=0；null 行地址值语义不变。

---

## T4 L2W 域下沉 `TransformAssignmentBuffer`（+ 连带删 stride/version 表与 `SSBOAddress`）

1. TAB 自管理 L2W/L2WIndex 域：把 `RegisterBuffer`/`ClearDomain` 的登记语义内联为 TAB 私有（T0 确认 TAB 仅用 `SSBOAddress{LocalToWorld, ECSReservedSSBOId::LocalToWorldData, 0}` 与 `{LocalToWorldIndex, …::LocalToWorldIndex, 0}` 两个常量地址 + 缓冲容量）。
2. 旧注册表删 `domain_map`（`.h:30`）、`SSBOBufferBinding`（`.h:17-24`）、`RegisterBuffer`（`.h:82` / `.cpp:290-335`）、`ClearDomain`（`.h:90` / `.cpp:418-439`）、`MakeKey`/`Find`/`FindMutable`（`.cpp:45-60`）、`Release()` 的 `domain_map` 段（`.cpp:137-155`）。
3. 连带删（此时零消费者）：`ValidateStructStrideForDomain`（`.cpp:12-33`）、`GetSSBOTypeStructStride`/`GetSSBOTypeStructVersion`（`SSBOTypes.h:63-102`）、`SSBOAddress`/`MakeSSBOAddress`、`ECSReservedSSBOId`（TAB 内联后）。
4. `SSBOType` 枚举删零消费者的 `MeshDrawParams`（T0 已证）；`UserDefined`/`LocalToWorld`/`LocalToWorldIndex` 保留。

**验收**：编译过；L2W 多实例示例冒烟（`PBRSpheres`/`RenderBoundBox`/`AutoMerge`/`RayPicking`/`PlaneGrid3D`）；gate 记录；删除符号零残留。

**风险**：TAB 的域校验语义——原 `ValidateStructStrideForDomain` 对 L2W/L2WIndex **生效**（stride 64/4 校验），下沉时要么**逐字保留**该断言（在 TAB 内），要么明确记录「TAB 自建缓冲无需校验」。**二选一并写进提交信息**。

---

## T5 改名收敛：`SSBOBufferRegistry` → `MaterialTextureReferenceRegistry`

全 token 同步：类名/文件/`GRAPH_MODULE_CLASS`/`GRAPH_MODULE_CONSTRUCT`、`GraphicsContext::GetSSBOBufferRegistry()`（→ `GetMaterialTextureReferenceRegistry()`）、`src/SceneGraph/CMakeLists.txt`（文件列表 + `source_group`）、全部调用点（`MaterialComponent.cpp`/`RenderPrimitiveCollectSystem.cpp`/`RenderSceneUBOSystem.cpp`/`TransformSystem.cpp`/`LineRenderPipeline.cpp`/`PrimitiveBatchPipeline.cpp`/`TextRenderPipeline.cpp`/`GizmoResource.cpp` 等）、注释与日志 tag。

**验收**：`grep -rn "SSBOBufferRegistry" inc/ src/ example/` **零命中**；编译过；gate 记录；全示例冒烟。

---

## T6 收尾评估与文档同步

- [ ] `ArrayView.h` 复核（T0 已确认活、仅成员清理）→ 确认 T1 清理后无残留 `mtl::SSBOType`/`SSBOBinding` 依赖。
- [ ] `SSBOType` 终态复核：`UserDefined`/`LocalToWorld`/`LocalToWorldIndex` 各自消费者记录（T4 后）。
- [ ] `doc/gpu-driven-4id-draw-item-plan-2026-09.md`：更新 §2.6/§10 —— 注册表命名（`MaterialTextureReferenceRegistry` + `MaterialSSBOBufferRegistry` 职责划分）。
- [ ] 技能 `ulre-ssbo-vertex-input`：记录「两个注册表职责边界 + `MaterialSSBOType` vs `SSBOType`」。
- [ ] 附录 A 全条目终检 + 提交。

---

## 附录 A：死码终表（**T0 已核对**）

| # | 符号 | 位置 | 外部消费者（T0 实测） | 归属 | T0 结论 |
|---|---|---|---|---|---|
| 1 | `AllocateArrayAccessor<T>` + `AllocateSSBOId` + `next_ssbo_id` | `.h:166/189-235` / `.cpp:40-43` | 无（仅 doc 注释） | T1 | ✅ 可删 |
| 2 | `EnsureArrayAccessor<T>` / `EnsureBuffer` | `.h:84-88/247-272` / `.cpp:337-416` | 无 | T1 | ✅ 可删 |
| 3 | `row_segments` / `RowSegmentInfo` / `TryGetRowSegment` | `.h:39-50/141-152` / `.cpp:157-162` | 无 | T1 | ✅ 可删 |
| 4 | `Touch` / `TryGetBinding` / `HasBinding` / `GetElementCapacity` / `GetCount` | `.h:80/92-101` / `.cpp:276-288/441-472` | 无 | T2 | ✅ 可删 |
| 5 | `GetBuffer` / `GetGPUBuffer`（`SSBOAddress` 重载） | `.h:95-97` / `.cpp:456-466` | 无外部调用；仅内部互调（`.cpp:344/464`） | T1 | ✅ 可删 |
| 6 | `GetMaterialTextureReferencePoolCount` | `.h:135-139` | 无（非诊断） | T2 | ✅ 可删 |
| 7 | `MakeECSSSBOId` / `SSBOIdNamespaceBit` / `IsECSSSBOId` / `GetSSBOIdLocalPart` | `SSBOTypes.h:104-125` | 无 | T2 | ✅ 可删 |
| 8 | `GetSSBOTypeStructStride`/`GetSSBOTypeStructVersion`/`ValidateStructStrideForDomain` | `SSBOTypes.h:63-102` / `.cpp:12-33` | 仅 `SSBOBufferRegistry.cpp`（经 TAB 的 `RegisterBuffer`） | T4 | ⏳ 依赖 TAB |
| 9 | `SSBOBinding` | `SSBOTypes.h` | 仅 `ArrayView.h:208 GetSSBOBinding()`（**该方法零调用者**） | T1/T2 | ✅ 可删（随 ArrayView 成员） |
| 10 | `SSBOAddress` / `MakeSSBOAddress` / `ECSReservedSSBOId` | `SSBOTypes.h` | 仅 TAB（`SSBOAddress` 常量 + `ECSReservedSSBOId`） | T4 | ⏳ 依赖 TAB |
| — | `MakeRecipeSSBOId` | `SSBOTypes.h:107-110` | **仍活**（`SerializedDescriptorEntry.h`/`ShaderResourceSchema.h`/`MaterialSSBOBufferRegistry.cpp`/gate） | 保留 | — |

**T0 新增发现（超出原清单）**

| 项 | 结论 | 证据 |
|---|---|---|
| `ArrayView<T>` | **活**（不可删类）；仅 `ssbo_id`/`ssbo_type`/`GetSSBOId`/`GetSSBOType`/`GetSSBOBinding`/friend/include 需清 | `TextRenderPipeline.h:60-62`、`TextRenderPipeline.cpp:793/809/825` |
| `SSBOType::MeshDrawParams` | **零消费者**，枚举值可删（枚举整体保留） | 仅 `SSBOTypes.h` 自身 |
| `TypedArrayView<T>` | 另一类型（buffer 子系统），不受影响 | `LineRenderPipeline.h:79-80`、`GeometryCreater.h:233` |

## 附录 B：任务依赖

```
T0（✅ 已完成）
 ├─→ T1（删分配器面 + ArrayView 成员清理）
 ├─→ T2（删绑定残留/伪 id）
 ├─→ T3（null 行迁新注册表）
 └─→ T4（L2W 下沉 + 删 stride/version 表 + SSBOAddress）──→ T5（改名收敛）──→ T6（收尾/文档）
```
（T1–T4 相互独立；**T5 必须最后**；T4 内 TAB 校验语义二选一需在提交信息中写明。）
