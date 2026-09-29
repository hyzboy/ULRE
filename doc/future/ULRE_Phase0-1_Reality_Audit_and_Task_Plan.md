# ULRE 阶段零/一 前提核对与任务重排（Reality Audit → Task Plan）

> 核对对象：
> - `doc/future/ULRE_Architecture_Phase0_GLTF_TRS_Normalization.md`
> - `doc/future/ULRE_ECS_Component_To_ID_Architecture_Roadmap.md`（含 Phase1/2/3 三篇）
>
> 方法：把文档里每一条主张拿去和仓库实测对齐，判定分四类 —— **已成立** / **伪命题（代码已经是对的或该结构不存在）** / **方向错（照做会坏事）** / **真缺口**。
> 所有证据给 `path:line`；所有数字来自本次实测运行（见 §2）。
> 基线（本次实测，rc 为真值）：门 `ShaderResourceSchemaRegressionGate` = **42 PASS / 0 FAIL，rc=0**；`TestCSMIncrementalPass` 全过（末条 Test 23，rc=0）；`TestTransformFlatStorage`、`TestRenderItemDataStorage` rc=0。

---

## 0. 结论摘要

两篇文档共列 13 个 Task（0.1–0.6、1.1–1.5、Task 2–4）。逐条核对结果：

| 分类 | 条数 | 条目 |
|---|---|---|
| 伪命题（不需要做） | 5 | 0.1 quat 顺序、0.2 自研极分解、0.3 双重方向变换、0.4 多根/拆分重定向、1.2 dirty_bits |
| 方向错（做了会坏事） | 3 | 1.1 废弃 `ActiveRowLease`、1.3 全局 `g_CurrentTransformStorage`、48B TRS SSBO（`Task 1` 引子） |
| 已部分成立（文档以为从零开始） | 3 | 动静分区、4-ID 描述符、`EntityID` 代数 |
| 真缺口 | 2 | `0.4-3/0.6` 消费侧吃 TRS、`Task 2–4` 组件 ID 化 + 64B Entity/二进制场景 |

一句话：**这份路线图的"阶段零"基本不用做（其中 3 条是伪命题，2 条要先验证而不是实现）；真正有价值的主线在"引擎侧局部真源唯一化 → 组件 ID 化 → 容量/行号冻结 → 二进制场景"这条链上，而且链条顺序被原文档写反了（原文档把纯 GPU 结构当阶段零，把真正的前置项'容量与行号冻结'漏掉了）。**

### 进度（本次会话）

| 任务 | 状态 | 证据 |
|---|---|---|
| T0 量化探针 + 实测基线 | **✅ 完成** | 探针 `src/ecs/support/ProbeTransformDiagnostics.cpp`；数字见 §2.1 |
| T4 消费侧旋转提取修复 | **✅ 代码完成**（资产级视觉验证阻塞） | `SceneTest.cpp` 改 `math::DecomposeTransform`；顺带修 `CMMath` 镜像分支（§1.12） |
| 回归 | **✅ 全绿** | 门 42 PASS / 0 FAIL；`TestTransformFlatStorage`、`TestRenderItemDataStorage`、`TestCSMIncrementalPass` rc=0 |
| T1/T2/T3/T5…T11 | 待排期 | 见 §4 |

---

## 1. 逐条事实核对

### 1.1 Task 0.1「`glm::quat` 构造顺序倒错」→ **伪命题**

三处构造全部已是 `(w,x,y,z)`，且互相一致：

| 位置 | 代码 | 说明 |
|---|---|---|
| 导入侧 | `src/Tools/GLTFConvert/gltf/ToNodeTransform.cpp:17` | `glm::quat(src.rotation.w(), src.rotation.x(), src.rotation.y(), src.rotation.z())` — 正确 |
| 导出侧 | `src/Tools/GLTFConvert/export/SceneExportPack.cpp:157-160` | 注释明写 `glm::quat memory order: x,y,z,w`，按 `[x,y,z,w]` 落盘 |
| 引擎读取侧 | `example/Geometry/LoadScene/LoadStaticMesh.cpp:483` / `:619` | `math::Quatf(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2])` = `(w,x,y,z)`，与落盘一致 |

文档点名要改的 `src/Tools/GLTFConvert/math/TRS.cpp` **全文只有 3 行**（`// fastgltf specific functionality removed; TRS now purely math/GLM based.`），里面没有任何构造逻辑可"纠正"。

**结论**：不需要修。需要的是一条"四元数顺序契约"回归（防止未来有人再写成 `(x,y,z,w)`）。

### 1.2 Task 0.2「实现 `DecomposeMatrixToTRS`」→ **伪命题（库已代劳）+ 与既有偏好冲突**

- `src/Tools/GLTFConvert/gltf/import/GLTFImporter.cpp:87` 明确启用 `fastgltf::Options::DecomposeNodeMatrices` —— **解析期**就把 `matrix` 节点分解成 TRS。因此 `ToNodeTransform.cpp:24-28` 的 `matrix` 分支在正常路径上取不到，`NodeTransform::Type::Matrix` 是死路。
- 自研极分解 = 与库重复实现（与"用 glm 现成函数替代自研打包"的既有口径相反），且手写版本在镜像/剪切/退化矩阵上的行为弱于库。

**结论**：不实现分解器。改成两件事：① **收敛死路**（见 T1）；② **对拍验证**库的分解在 `det<0`/镜像情形下是否正确（见 T2）。

**补充（引擎侧）：不要自研，也不缺函数** —— 引擎已有 `math::DecomposeTransform`
（`CMMath/inc/hgl/math/Matrix.h:343`、实现 `CMMath/src/Math/Matrix4f.cpp:398`），
它就是消费侧该用的那个。**但它的镜像分支原本是错的**（`if (outScale.x > epsilon)`
在 `outScale.x` 为负时跳过归一化）——已在本次 T4 一并修正，见 §1.12。
另外，文档 Task 0.2 给出的 `DecomposeMatrixToTRS` 示例代码在镜像情形下**自身也是错的**：
它同时把 `outS.x` 与 `c0` 取负（双重翻转），净效果是旋转列取反；正确约定是
「保留 `c0`、把负号只放进 `S.x`」（探针里作为变体 (d)，重建误差 5.96e-08）。

### 1.3 Task 0.3「双重坐标系方向变换」→ **伪命题（这是刻意的相似变换配对）**

两个调用点是**紧挨着的**一对，不是"两端各转一次"：

- `src/Tools/GLTFConvert/gltf/import/GLTFImporter.cpp:114-115`：先节点局部变换、再图元顶点。
- 节点侧 `src/Tools/GLTFConvert/math/NodeTransform.cpp:157-181`：`M' = R M R⁻¹`（TRS 分支：`t' = R·t`、`r' = q·r·q⁻¹`、`s` 不变；Matrix 分支 `m' = qMat·m·qInv`）。
- 图元侧 `src/Tools/GLTFConvert/gltf/import/GLTFOrientationPrimitives.cpp:75-122`：顶点 `v' = R v`（`POSITION/NORMAL/TANGENT/BITANGENT`）。

于是 `M' v' = R M R⁻¹ R v = R (M v)`：世界坐标整体绕 X 轴 +90°，**自洽**。`NodeTransform.cpp:167-169` 的注释写明这正是"因为顶点也被旋转"。

文档建议的"图元保持原生、只根节点乘转换矩阵"会**打破这个配对**（只保留一侧 ⇒ 世界坐标被转两次或零次），并把一次性离线转换推到运行期逐帧计算。

**结论**：不做。改为补一条世界坐标对拍验证（T3），把结论钉在文档里，避免以后有人再来"修"这个不存在的 bug。

### 1.4 Task 0.4「多根节点 + Multi-Primitive 拆分重定向」→ **多根已支持；"拆分"根本不存在**

**多根已经是对的**：

- `src/Tools/GLTFConvert/gltf/convert/CopyScenes.cpp:16-17` 全量复制 `scene.nodes`；
- `src/Tools/GLTFConvert/export/SceneExportTransforms.cpp:44` `for (int32_t root : scene.nodes)` 遍历所有根；
- `src/Tools/GLTFConvert/export/SceneExportBuild.cpp:47-48` 把全部根 remap 成 scene-local 索引并写入 `rootNodes`（`SceneExportJson.cpp:68`）；
- 全仓无 `scene.nodes[0]` / `scenes[0]` 式"唯一根"假设（grep 0 命中）。

**"节点拆分"不存在**：`grep -rn "split\|Split"` 在 `gltf/ pure/ export/` 下 **0 命中**；`export/SceneExportNodes.cpp:35-45` 是"一个 node 直接带 primitive 索引列表"，不拆分节点。所以文档里"被拆分节点的子节点 `parent_index` 必须重定向"这条规则**当前不适用**。

**结论**：目前不需要写任何重定向逻辑。若将来确实要按 primitive 拆节点，那是一次新设计，不要预先加代码。

### 1.5 Task 0.4-3 / 0.6「只导出局部变换 + 加载验证」→ **部分成立，真问题在消费侧**

导出侧现状（`src/Tools/GLTFConvert/export/SceneExportNodes.cpp:27-33`）**同时**导出三份：

```cpp
glm::mat4 localM     = src.transform.rawMat4();          // :27  局部 64B
ne.localMatrixIndex  = GetOrAddMatrix(outData.matrixTable, localM);   // :28
glm::mat4 worldM     = worldMatrices[originalNode];      // :29  世界 64B
ne.worldMatrixIndex  = GetOrAddMatrix(outData.matrixTable, worldM);   // :30
if (src.transform.isTRS()) ne.trsIndex = GetOrAddTRS(outData.trsTable, src.transform.trs); // :32-33
```

TRS 表**已经导出**（48B 语义），但引擎消费侧没用它：

- `example/Geometry/LoadScene/LoadStaticMesh.cpp:478-485` 与 `:614-621` 已经把 `hasTRS/translation/rotation/scale` 解析出来了；
- `example/Geometry/LoadScene/SceneTest.cpp:197-202` 却忽略 `hasTRS`，改用世界矩阵反推：

```cpp
se.transform->SetLocalPosition(glm::vec3(node.worldMatrix[3]));
se.transform->SetLocalRotation(glm::quat_cast(glm::mat3(node.worldMatrix)));   // ← 真 bug
se.transform->SetLocalScale(glm::vec3(glm::length(worldMatrix[0]), ...));
```

**`quat_cast(mat3(worldMatrix))` 在存在非均匀缩放时是错误的**（`mat3` 不是纯旋转矩阵，`quat_cast` 的输入前提被破坏）。这是本次核对里**唯一一条确凿的运行时正确性缺陷**，而且它被"要不要导出世界矩阵"的争论完全掩盖了。

**结论**：真任务 = 消费侧改吃已导出的 TRS（并做 scale-aware 回退），见 T4；世界矩阵表是否退役见 T5。

### 1.6 Task 1.1「废弃 `ActiveRowLease`」→ **方向错（路径搞错了）**

- `ActiveRowLease` 在 `inc/hgl/vk/buffer/ActiveRowLease.h`，是**通用行租约**；唯一派生使用者是 `GlobalSSBODataAccessor`（`inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:32-41`）= **材质行访问**。
- transform 路径完全不经过它：`inc/hgl/ecs/support/TransformDataStorage.h` 里没有任何租约对象，行号由 `TransformSystem::RefreshHandleOrder`（`src/ecs/systems/tick/TransformSystem.cpp:438-479`）+ `TransformAssignmentBuffer` 管。

**结论**：删它等于砍掉材质行访问，与本文档目标无关。**禁止列入本阶段**。

### 1.7 引子「48 字节 `TransformTRS`（GPU std430 对齐）」→ **CPU 侧真源已经是 TRS；GPU 侧没有消费者**

- 局部真源**已经**是 TRS：`inc/hgl/ecs/support/TransformDataStorage.h:42-44` 的 `positions`/`rotations`/`scales` 三组 SOA 数组；`glm::mat4 local_matrices`（`:31`）只是**懒更新派生缓存**（`UpdateLocalMatrix` `:139-150`，`local_dirty` 门控）。
- GPU 侧全是 mat4：`ShaderLibrary/common/l2w_ssbo.glsl:18` 注释「mat4 数组（scalar 布局 stride 64B，与 CPU `Matrix4f` 逐字节一致）」、取数点 `ShaderLibrary/vertex/helpers/orient_world.glsl:17/25` 的 `l2w.mats[TransformID]`；CPU 写者 `src/ecs/support/TransformAssignmentBuffer.cpp:254` / `:352` 直接读 `storage.GetWorldMatrix(handle)`。
- 所以新建 48B TRS SSBO **没有任何消费者**（不是"为将来准备"，是当场死结构），且新增一张 C++↔GLSL 并行表就多一处结构漂移风险。

**结论**：GPU 侧不引入 TRS（见 §6 不做清单）。CPU 侧"消灭局部 `Matrix4f`"才是真话题，但要区分**真源 vs 缓存**（T6/T7）。

### 1.8 Task 1.1/1.2「动静物理分区 + 固定容量 + `uint8_t dirty_bits`」→ **行空间已分区，存储层未分区；脏标记已成立**

| 子项 | 实测 |
|---|---|
| 动静分区 | **已存在（行空间）**：`inc/hgl/ecs/core/Context.h:129-130` 两组弱引用列表；`TransformSystem.cpp:438-479` 生成 `static_handles/dynamic_handles` + 两张索引表；`TransformAssignmentBuffer` 静态段 + 动态 ring（identity 槽 0 见 `TransformAssignmentBuffer.cpp:22-23`） |
| 固定容量 | **不成立**：`TransformDataStorage` 用 `hgl::ValueArray` 自增长（`Allocate()` `:54-71`），无分区、无上限；`EnsureCapacity` 触发时 L2W buffer 会重建（`TransformAssignmentBuffer.cpp:488-511`，有 `L2W recreated` 日志） |
| `uint8_t dirty_bits` | **已成立**：`TransformDataStorage.h:47-49` 已经是三个 `uint8_t` 数组（`local_dirty`/`matrixDirty`/`mobility`），不是 bitset |

**真价值点**：`TransformID` **就是 L2W 行号**。只要容量会增长、行号会漂移，"预烘焙 ID / 二进制场景直载"就不可行 ⇒ 这是 Phase 3 的**前置项**，原文档把它漏在了任务清单之外（见 T11）。

### 1.9 Task 1.3「`TransformAccessor` 只含 4 字节 id + 全局 `g_CurrentTransformStorage`」→ **方向错（破坏多世界）**

- 现状：组件持 `storageHandle` + `bound_storage`（`inc/hgl/ecs/components/TransformComponent.h:43-44`），storage 由**世界**（`ECSContext`）持有，`GetStorage()`（`:805`）带共享 storage 回退。
- 文档的全局单例与现状冲突：L2W/行表是**每世界私有**（`inc/hgl/ecs/support/TransformAssignmentBuffer.h:36-40` 注释：世界私有设施、不进 `SSBOBufferRegistry`），地址分 GlobalAddresses（跨世界资源池）/ WorldAddresses（世界私有）两档。全局 `g_CurrentTransformStorage` 会直接退回"两个世界同帧互相踩"的老坑（C1/C2/C3 三轮才修完）。

**结论**：Accessor 若做，句柄**必须携带世界上下文**（`{world*, id}` 或"世界号高位 + 行号"）；全局裸指针禁止。

### 1.10 Task 2/3「组件 ID 化 / 64B Entity / `.ulrescene`」→ **真缺口，但顺序与前提要重排**

| 项 | 实测 |
|---|---|
| 4-ID 描述符 | **已存在**：`inc/hgl/graph/render/RenderItemDescriptor.h:19-31`（`transform_id/geometry_id/material_id/texture_id`，16B + `static_assert`）⇒ GPU 侧 ID 闭环已成立（原文档说的 `PipelineID` 实际是 `texture_id`） |
| `Entity` | **OOP**：`inc/hgl/ecs/core/Entity.h:21-33` `class Entity : public Object` + `UnorderedMap<std::size_t, std::shared_ptr<Component>>` ⇒ 每实体一次堆分配 + 每组件一次堆分配 + vptr |
| `EntityID` 代数 | **已成立**：`inc/hgl/ecs/core/EntityHandle.h:11-19` 已含 `index` + `generation`，"防僵尸句柄"这一半已有基础 |
| `.ulrescene` / `SceneHeader` / `LoadSceneBlit` | **0 命中**（唯一 `SceneHeader` 命中是 GLTFConvert 的 MiniPack 条目名，`export/SceneExportPack.cpp:435`） |
| 组件侧 | `PrimitiveComponent`/`RenderableComponent`/`MaterialComponent` 仍在，且 `MaterialComponent` 不是纯数据（承载 recipe、`shadow_retry_frames` 影子重试状态机、程序解析失败降级——见 `doc/ecs-layer-architecture-and-frame-flow.md` 与技能 `ulre-ecs-layer` 的 D9 条）⇒ 不能按"纯数据表迁移"处理，必须最后动 |

### 1.11 文档 §1.1 的量化「48B 膨胀到 120~160B」→ **方向对，数字明显低估**

按头文件字段估算（T0 用探针钉死）：

- `TransformComponent` 自身 ≈ 190B（`storageHandle` 4 + `bound_storage` 8 + 三份局部副本 40 + `parent_id` 8 + `child_ids` 24 + `cachedWorldMatrix` **64** + 标志/像素定尺字段 ≈ 40）+ 基类 `Component`（`inc/hgl/ecs/core/Component.h:23-28`：vptr + `std::string` 32 + owner 三件套 + `version` + `change_mask`）≈ 80 + `enable_shared_from_this` 16 ⇒ **≈ 290B**；
- 另有 `UnorderedMap` 节点 ≈ 40B、`make_shared` 控制块 ≈ 24B ⇒ 单个 transform 实体 **≈ 350B + 至少 2 次堆分配**；
- 再加 `TransformDataStorage` 每行 ≈ 180B（含 local 64B + world 64B 两份矩阵）⇒ **单对象 ≈ 530B 量级**（T0 实测更大：**939.9 B / 9.000 次分配**，见 §2.1）。

结论：文档的"120~160B"低估约 3 倍（按实测约 **6 倍**），但"内存税 + 数据二次搬运"的论断方向正确。

---

### 1.12 引擎侧 `math::DecomposeTransform` 的镜像缺陷（本次发现并修正）

- **位置**：`CMMath/src/Math/Matrix4f.cpp:398`（声明 `CMMath/inc/hgl/math/Matrix.h:343`）。
- **缺陷**：缩放提取后归一化用的是 `if (outScale.x > epsilon) Row[0] /= outScale.x;`。
  镜像（`det<0`）时 `outScale.x` 为负 ⇒ 该列**不被归一化** ⇒ 旋转矩阵非正交 ⇒
  `quat_cast` 结果错。实测（探针 [T4] 镜像行）：`|q|=0.881518`（应为 1）、重建误差 **2.42296**。
- **修正**：`std::fabs(outScale.x/y/z) > epsilon`（一行条件）。修后 `|q|=1.000000`、
  重建误差 **5.96046e-08**。
- **影响面**：只有镜像/负缩放输入受影响（此前是垃圾值），非镜像路径逐位不变；
  仓库内使用者：`CMMath/src/Transform/Transform.cpp:134`、`example/Geometry/LoadScene/SceneTest.cpp`（本次 T4）。
- **提交口径**：`CMMath` 是 **submodule（独立仓库）**，本改动需在 `CMMath` 仓单独提交，
  主仓只更新 submodule 指针。

---

## 2. 现状量化基线（供每步回归对比）

| 项 | 实测值 | 来源 |
|---|---|---|
| 门 `ShaderResourceSchemaRegressionGate` | **42 PASS / 0 FAIL，rc=0** | `build/out/Windows_64_Debug/ShaderResourceSchemaRegressionGate.exe`（本次运行） |
| `TestCSMIncrementalPass` | 全过（末条 Test 23），rc=0 | 同上目录 |
| `TestTransformFlatStorage` | rc=0 | 同上 |
| `TestRenderItemDataStorage` | Stage 1–5 全过，rc=0 | 同上 |
| `TransformComponent` 引用面 | **75 文件 / 347 处**；`AddComponent<...TransformComponent>` **74 处**；`GetComponent<...TransformComponent>` **24 处** | 全仓 grep（排除 doc/） |
| 受影响的示例 | 30+ 文件（`example/Basic` 22、`example/Environment` 6、`example/Geometry` 3、`example/Gizmo` 4、`example/Texture` 5…） | 同上 |
| GLTFConvert | **独立 submodule 仓库**（`.gitmodules` → `git.hyzgame.com/GLTFConvert`），改动必须在子仓单独提交 | `.gitmodules` |
| L2W 格式 | `mat4[64B]` 数组 + `uint32` 行表，经 `pc_root` 下发（BufferDeviceAddress，无描述符） | `ShaderLibrary/common/l2w_ssbo.glsl`、`inc/hgl/graph/RootAddressPush.h` |
| 单 transform 对象内存 | **实测 935.3 B / 9.000 次分配**（边际 939.9 B，见 §2.1） | T0 探针实测 |

---

## 2.1 T0 探针实测明细（2026-09-29，`ProbeTransformDiagnostics.exe`）

复现：`cmake --build build --config Debug --target ProbeTransformDiagnostics` →
`build/out/Windows_64_Debug/ProbeTransformDiagnostics.exe`（cwd = 仓库根；无需窗口/GPU）。

**结构尺寸**

| 结构 | 实测 |
|---|---|
| `EntityID` | 8 B |
| `Component`（基类，含 vptr + `std::string` + version/change_mask） | **104 B** |
| `TransformComponent` | **336 B** |
| `Entity`（`Object` + `UnorderedMap<size_t, shared_ptr<Component>>`） | 160 B |
| `TransformDataStorage`（含 11 组 `ValueArray` + 拓扑状态） | 488 B |
| `TransformDataStorage` 每行（各平行数组元素之和） | **189 B**（positions 16 + rotations 16 + scales 16 + local_mat4 **64** + parent 4 + world_mat4 **64** + depth 2 + eval_order 4 + 3×dirty/mobility 3） |
| `sizeof(glm::vec3)` | 16（本编译配置按 16B 对齐，故 positions/scales 各占 16 而非 12） |

**堆净增（`ECSContext` + N 个 `[Entity + TransformComponent(Static)]`，CRT 堆快照有符号差）**

| N | 净增块 | 净增字节 | 每实体 |
|---|---|---|---|
| 1000 | 9000 | 893,745 | 893.7 B / 9.000 块 |
| 10000 | 90000 | 9,352,557 | 935.3 B / 9.000 块 |
| 边际（10000 相对 1000） | — | — | **939.9 B / 9.000 块** |

**结论**：文档 §1.1 的「48B 的 TRS 膨胀至 120~160 字节」低估约 **6 倍**——含存储行与容器，
单对象实际 **≈ 940 B 且 9 次堆分配**（每实体 = Entity 对象 + 组件对象 + map 节点 +
I2W 记账 + 每行存储等）。这是阶段一/三收益论证的基准数字。

---

## 3. 需要拍板的决策点（含建议）

| 编号 | 决策 | 建议 | 理由 |
|---|---|---|---|
| D1 | GPU 侧是否引入 48B `TransformTRS` SSBO | **不上** | 无消费者；多一张并行表 = 多一处结构漂移（门 `S./W.*-struct-parity` 兜的就是这族问题） |
| D2 | L2W 是否 `mat4` → `Matrix4x3f`（每行省 16B） | **不改** | 触及 `l2w_ssbo.glsl` + 门的两条 parity 用例 + `TestRenderItemDataStorage.cpp` 三份内嵌 GLSL 夹具 + 6 处 CPU 写者；每行 16B 的收益不抵风险 |
| D3 | `TransformAccessor` 是否用全局 `g_CurrentTransformStorage` | **禁止** | 世界私有不变量；句柄必须带世界上下文 |
| D4 | `matrixTable` 是否退役（TRS-only 导出） | **做，但排在 T2 对拍之后** | 每节点现导 2×64B 与 48B TRS 重复；但世界矩阵有真实使用者（`SceneTest.cpp:197-202`），退役前必须先解决消费侧 |
| D5 | 引擎示例是否改吃导出 TRS | **做（优先）** | 顺带消灭 `quat_cast(mat3(scaled))` 的真 bug |
| D6 | `TransformComponent` 的"成员 + storage"三份局部副本是否收敛 | **做** | 双写 = 双真源，是"数据二次搬运"论断的真实落点 |

---

## 4. 重排后的任务清单

> 约定：每个任务独立可编译、可运行、可回归；改结构体/头文件后**必须 purge 陈旧 TU 再构建**（见 §5），不接受"构建 rc=0"作为验证。

### T0 量化探针（先钉死数字）— ✅ 已完成（2026-09-29）

- **落地**：新增 `src/ecs/support/ProbeTransformDiagnostics.cpp` + CMake 目标
  （`src/ecs/CMakeLists.txt`，diagnostic，不注册为 ctest）。实测数字见 §2.1。
- **验收**：`cmake --build build --config Debug --target ProbeTransformDiagnostics` →
  `ProbeTransformDiagnostics.exe` rc=0，数值可复现（连跑两次一致：9.000 块/实体）。
- **踩坑记录**：① `_CrtMemDifference` 在类别计数下降时按无符号下溢（打印出
  "1518 次分配/实体" 这种不可能的数字），必须逐类别按 signed 求差；
  ② `const glm::vec3 t(glm::vec3(M[3]));` 会被 MSVC 当成函数声明（most vexing parse，
  error C2664），写成 `= glm::vec3(...)`。

- **目标**：把 §1.11 的估算换成实测值，作为后续收益论证的基准。
- **做法**：新增探针程序（建议 `src/ecs/support/ProbeTransformFootprint.cpp` + CMake 目标，参照 `src/ecs/CMakeLists.txt:332-341` 的既有测试注册方式），打印 `sizeof(TransformComponent)`、`sizeof(Component)`、`sizeof(TransformDataStorage)`、每行字节、`Allocate()` 后的堆分配次数（用 `_CrtMemCheckpoint`/自增计数器任一种），以及 1 万个 transform 的实际内存占用。
- **验收**：输出数字回写本文档 §2；探针可重复运行且数值稳定。
- **为什么先做**：阶段一/三的全部收益都靠这组数字，而不是文档里低估 3 倍的那组。

### T1 GLTFConvert 死路收敛（**子模块内**提交）

- **目标**：删掉 `DecomposeNodeMatrices` 开启后不可达的矩阵路径，让"局部变换只可能是 TRS"成为编译期/运行期事实。
- **文件**：`src/Tools/GLTFConvert/math/NodeTransform.h:36-48`、`math/NodeTransform.cpp:36-48, 110-122, 138-155, 165-172`、`gltf/ToNodeTransform.cpp:3-11, 22-28`。
- **前置**：`toZUpMat4()` 与 `Type::Matrix` 的全仓引用清点（本次已 grep：`toZUpMat4` 仅有声明+定义、**0 调用者**，可删；`Type::Matrix` 只被自身分支使用）。
- **验收**：① 子仓构建通过；② 用同一批模型跑转换，导出产物（json + pack）与改前**逐字节一致**（TRS 表、节点表、primitive 表计数与内容不变）；③ GUI/CLI 两条入口都能转。
- **风险**：与主仓双仓同步（产物格式被 `example/Geometry/LoadScene/LoadStaticMesh.cpp` 消费），必须两仓同批验证。

### T2 负缩放/镜像对拍（把 Task 0.2 从"实现"改成"验证"）

- **目标**：证明 fastgltf 的 `DecomposeNodeMatrices` 在 `det<0`（镜像）/剪切矩阵上给出的 TRS 与原始矩阵等价。
- **做法**：构造最小 glTF（手写 json，或用现有模型改造）含 `matrix` 且 `det<0`；跑 GLTFConvert；把导出的 `matrixTable` 局部矩阵与用 `TRS::toMat4()`（`math/TRS.h:40-47`）展开的矩阵**逐元素 diff**（阈值 1e-5），并渲染确认左右手性一致。
- **验收**：diff 全在阈值内 → 文档 Task 0.2 正式判定为"库已覆盖"；若超阈值 → 才引入最小修正（并记录 fastgltf 的具体行为）。

### T3 方向转换配对验证（钉死 Task 0.3 的结论）

- **目标**：用一条独立证据链证明"顶点旋转 + 节点相似变换"这一对是等价的 Z-up 转换，而不是双重变换。
- **做法**：取一个多级旋转/有深度的模型，跑转换后由引擎侧按 T4 的路径加载，打印每节点世界 TRS，与外部工具（Blender 导出或原始 glTF 数据手工 Z-up 换算）对拍。
- **验收**：位置误差 <1e-3、朝向误差 <0.1°；结论写回文档（防止未来重开）。

### T4 引擎消费侧改吃 TRS — ✅ 已完成（2026-09-29，代码修复；资产级视觉验证待资产）

- **落地**：`example/Geometry/LoadScene/SceneTest.cpp:195-208` 的三行改为
  `math::DecomposeTransform(node.worldMatrix, world_pos, world_rot, world_scale)`
  （用既有引擎函数，不自研分解）；顺带修正 `CMMath` 的镜像分支（见 §1.12）。
- **验收（数值，无需资产）**：`ProbeTransformDiagnostics` 的 [T4] 段—
  非均匀缩放 S=(2,0.5,1)：旧写法 `|q|=1.076772`、旋转误差 **10.560°**、重建误差 **0.447018**；
  `DecomposeTransform`：`|q|=1.000000`、误差 0.000°、重建误差 **5.96046e-08**。
  镜像 S=(-2,0.5,1)：`DecomposeTransform` 修前 `|q|=0.881518`/重建误差 2.42296，
  修后 `|q|=1.000000`/重建误差 **5.96046e-08**。
- **回归**：门 42 PASS / 0 FAIL；`TestTransformFlatStorage`、`TestRenderItemDataStorage`、
  `TestCSMIncrementalPass` 三者 rc=0；`LoadScene`（含 SceneTest.cpp）编译链接通过。
- **未完成（阻塞）**：**资产级视觉验证**——`res/ABeautifulGame.StaticMesh/` 在本检出为空，
  示例加载路径 `res/ABeautifulGame.StaticMesh/ABeautifulGame.Scene.scene` 不存在
  （探针的「资产备注」段会打印这一事实）。两条补法：① 用 GLTFConvert 由 `res/model/*.glb`
  生成该 `.scene`（会写入 `res` 子模块）；② 给 SceneTest 加命令行/环境变量指定场景路径。

- **目标**：消灭 `quat_cast(mat3(scaledMatrix))` 这一真缺陷，并停止在消费侧反推。
- **文件**：`example/Geometry/LoadScene/SceneTest.cpp:197-202`（改用 `node.hasTRS/translation/rotation/scale`）；无 TRS 时走 scale-aware 回退（列模长归一后再取旋转，不要直接 `quat_cast`）。
- **验收**：① 构造一个**非均匀缩放节点**做对照，改前朝向错、改后正确（截图/数值双证据）；② 原场景渲染与改前逐像素一致（RenderDoc 截帧或帧内读回差分）；③ 5 个代表性示例 0 VUID。

### T5 `matrixTable` 退役（TRS-only 导出）——依赖 T2 + T4

- **目标**：导出侧每节点只留 TRS（identity 用 `trsIndex < 0` 表达），删 `matrixTable` 里的 local/world 双份。
- **文件**：`src/Tools/GLTFConvert/export/SceneExportNodes.cpp:27-33`、`export/SceneExportData.h:20-22, 53-54`、`export/SceneExportPack.cpp:189-191, 350-352, 457-459, 478-484`、`export/SceneExportJson.cpp:76-78`；主仓 `example/Geometry/LoadScene/LoadStaticMesh.cpp`（`PackedNode`/`NodeList` 解析、`StaticMeshNode::{localMatrix,worldMatrix}`）、`inc/hgl/graph/mesh/StaticMesh.h:22-23`。
- **注意**：世界变换有真实消费者（`SceneTest.cpp` 的扁平化烘焙）⇒ TRS-only 后世界变换要在加载期自行连乘得到（一次，不逐帧）。
- **验收**：3 个模型转换+加载正常；pack 体积下降（记录改前后数字）；门 42 PASS / CSM 契约不变。

### T6 局部真源唯一化（组件三份副本 → 一份）

- **目标**：`TransformComponent` 的 `local_pos/local_rot/local_scale`（`inc/hgl/ecs/components/TransformComponent.h:46-48`）删除，读写全部直落 `TransformDataStorage`（唯一真源）。
- **文件**：`src/ecs/components/TransformComponent.cpp`（`:104`、`:111/114`、`:146/149`、`:182/185`、`:210-213`、`:223-231`、`MigrateStorage :599`、`UpdateWorldMatrix :664-`）+ 头文件。
- **验收**：`TestTransformFlatStorage`、`TestCSMIncrementalPass`（含 D4 静态写入告警 Test 15 契约）、门；示例 `ClockUse`（static/movable 混合）、`RecursiveCube`（层级）、`ComputeTransformHierarchy` 冒烟。

### T7 `local_matrices` 缓存去留（与 T6 同批判定）

- **现状消费者只有三处**：`TransformComponent::GetLocalMatrix`（`TransformComponent.cpp:221-226`）、`TransformDataStorage::UpdateAllLocalMatrices/GetLocalMatricesData`（`TransformDataStorage.h:234-251`）、示例 `example/Basic/ComputeTransformHierarchy.cpp:314/360`（把 local 矩阵当 mat4 上传给 compute shader 做层级求值 demo）。
- **决策**：若 `ComputeTransformHierarchy` 的定位是"未来 GPU 层级求值的探路"，缓存保留（并在注释写明"仅该示例消费"）；若暂不做 GPU 层级求值，缓存与示例一起删（零兼容口径）。
- **验收**：删除后每行省 64B（T0 探针复测）；示例/测试全绿。

### T8 `TransformComponent` → `TransformID`（阶段一主体）

- **爆炸半径（已量化）**：75 文件 / 347 处、`AddComponent<...TransformComponent>` 74 处、示例 30+ 文件。
- **顺序**：① 引入 `TransformID`（= 现有 `HandleID` 语义）+ 带世界上下文的 `TransformAccessor`；② 组件瘦身为"只有 id"（去掉三份副本、`cachedWorldMatrix`、`child_ids` 容器）；③ 示例批量改调用点；④ 删 `inc/hgl/ecs/components/TransformComponent.h` + `src/ecs/components/TransformComponent.cpp`。
- **隐藏工作量（必须预先评估）**：**组开关 = 组件计数**（`Context::RegisterComponentInstance` 自动装组/开开关，见技能 `ulre-ecs-layer` 不变量）⇒ 删掉组件类会连带改系统组的激活条件；`RenderItem::GetTransform()` 返回 `shared_ptr<TransformComponent>`（`inc/hgl/ecs/core/RenderItem.h:51`）也要一起换成 ID。
- **验收**：每步 build + 四个测试 + 门 + 5 个代表性示例 0 VUID；`grep -rn TransformComponent src inc example` 最终**零命中**（用户会逐文件复查是否删干净）。

### T9 其余组件 ID 化（Geometry / Material / Visibility / Camera / Light）

- **顺序原则**：**纯数据先行，带行为/状态机的最后**。`MaterialComponent`（recipe、`shadow_retry_frames` 重试状态机、程序解析失败降级）最后动；`VisibilityComponent`/`BoundingBoxComponent` 最接近纯数据，可先做。
- **验收**：同 T8 口径；每个组件独立一批（API 变更与全部调用点必须同批，否则中间态不可编译）。

### T10 容量与行号冻结（Phase 3 的物理前提，原文档漏项）

- **目标**：`TransformDataStorage` 固定容量 + 明确分区（`[0, static_count)` / 之后 dynamic），运行时**禁止**扩容/重建；超限 fail-fast 报错（与"材质行 arena 1024 上限不扩容"的既有口径一致）。
- **为什么必须**：`TransformID = L2W 行号`，容量增长会重建 buffer 并重排行（`TransformAssignmentBuffer.cpp:488-511`）⇒ 不冻结行号，`.ulrescene` 预烘焙 ID 无从谈起。
- **验收**：现有示例在固定容量下正常；构造超限场景验证 fail-fast 有明确错误信息；L2W 不再出现重建日志。

### T11 64B `Entity` + `.ulrescene` 直载（阶段三）

- **依赖**：T8/T9（组件 ID 化）+ T10（容量/行号冻结）。
- **范围**：`inc/hgl/ecs/core/Entity.h` 重写为 `alignas(64)` 键值表（16B 元数据 + 48B 属性区）、`SceneHeader` 对齐 + StringPool 外置、Blit 加载器（Windows 上 `mmap` 不可直接照抄：需 `CreateFileMapping`/一次性 `fread`，且**不能直接 alias 文件页进 GPU 可写缓冲**，静态段仍要拷一次）。
- **验收**：离线导出工具 + 直载器；场景还原时间与显存直推链路（度量并记录）。

---

## 5. 固定验证集（每个任务后照跑）

```bash
LOG=$LOCALAPPDATA/Temp/build.log
cmake --build build --config Debug --target <T> > "$LOG" 2>&1; echo "rc=$?"
grep -cE "error C[0-9]+|error MSB" "$LOG"        # 期望 0

build/out/Windows_64_Debug/ShaderResourceSchemaRegressionGate.exe   # 期望 42 PASS / 0 FAIL
build/out/Windows_64_Debug/TestTransformFlatStorage.exe              # 期望 rc=0
build/out/Windows_64_Debug/TestRenderItemDataStorage.exe             # 期望 rc=0
build/out/Windows_64_Debug/TestCSMIncrementalPass.exe                # 期望全过（末条 Test 23）
```

**硬性注意项（都是踩过的坑）**：

1. **改类成员/结构大小后必须 purge 陈旧 TU 再构建**，否则头/库布局错位，症状是测试读到垃圾值或段错误（看起来像真回归）：
   ```bash
   "$LOCALAPPDATA/hermes/skills/software-development/crlf-safe-editing/scripts/purge-stale-deps.sh" \
       E:/ULRE E:/ULRE/build <src dirs...> -- <headers...>
   # 判据：必须打印 purged: TUs=N objs=M 且 N>0；漏 -- <header> 会 rc=1 静默退群而构建仍 rc=0
   ```
   来不及 purge 时的退路：`find build -type d -path "*.dir/Debug" -print0 | xargs -0 rm -rf`。
2. **别用 `cmake --build … | grep error` 判构建结果**：管道吞错误、链尾跑的是上一次的 exe。先落盘日志 → 看 rc → 再 grep。
3. **判据是运行日志里的预期内容（新用例名/计数），不是链式退出码**；`grep -c` 零匹配自身返回 1。
4. 改动触及 shader/材质/schema 时，验证前清 `build/cache-hot/shader-cache/`，否则拿旧产物比对会误判。
5. GLTFConvert 与主仓是**两个仓库**：改产物格式必须两仓同批验证，不能只编一边。

---

## 6. 明确"不做"清单（避免无消费者/反向工作）

- 不自研 `DecomposeMatrixToTRS`（fastgltf 已覆盖，且与"用现成库"的既有口径冲突）
- 不引入 GPU 侧 48B `TransformTRS` SSBO（无消费者）
- 不把 L2W 改成 4x3 `Matrix4x3f`（收益 16B/行 < 触及 shader + 门 parity + 三份 GLSL 夹具 + 6 处写者的代价）
- 不删 `ActiveRowLease`（材质行访问在用）
- 不做全局 `g_CurrentTransformStorage`（破坏世界私有不变量）
- 不为"节点拆分重定向"预先写代码（拆分不存在）
- 不改 quat 构造（三处已一致），只加顺序契约回归

---

## 7. 风险与未决问题

1. **fastgltf 分解的镜像/剪切行为未实测** —— T2 就是为此；若实测不达标，才需要最小修正，而不是照抄文档的极分解实现。
2. **`ComputeTransformHierarchy` 示例的定位待拍板**（未来 GPU 层级求值的探路 vs 现状死代码）——直接决定 `local_matrices` 缓存与 SoA 平行数组的去留（T7）。
3. **T8 的隐藏工作量**：删 `TransformComponent` 会连带改"组开关 = 组件计数"机制与 `RenderItem::GetTransform()` 接口，工作量被原文档低估。
4. **T10/T11 的先后被原文档写反**：原文档把"48B 结构 / SoA 指针数组"当阶段零，把真正的物理前提（容量与行号冻结）漏掉；按现顺序执行会在阶段三发现行号漂移而返工。
5. Windows 上 `.ulrescene` 的 mmap 语义与 Linux 不同（`CreateFileMapping`，且不能把只读文件页直接当 GPU 可写源）——T11 的设计要按平台重写而非照抄文档里的 `mmap` 示例。
