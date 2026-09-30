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
| T1 GLTFConvert 死路收敛 | **✅ 完成** | `NodeTransform` 只剩 None/TRS；导入边界统一做配对转换 + 镜像感知分解；见 §4 T1 |
| T2 负缩放/镜像对拍 | **✅ 完成（发现 3 个真 bug）** | 修前/修后：TRS 非均匀 0.383→1.6e-07；matrix 非均匀 0.500→1.2e-07；**matrix 镜像 1.243→1.9e-07**；见 §1.2 订正与 §4 T2 |
| T3 方向转换配对验证 | **✅ 完成** | 判据 M' = R·M·R⁻¹；含顶点级世界 AABB 对拍（≤1.2e-07）；见 §4 T3 |
| 回归 | **✅ 全绿** | 门 42 PASS / 0 FAIL；`TestTransformFlatStorage`、`TestRenderItemDataStorage`、`TestCSMIncrementalPass` rc=0；GLTFConvert CLI/GUI 均构建通过 |
| T5…T11 | 待排期 | 见 §4 |

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

**结论（已按 T2 实测订正）**：不要照文档那样写 `DecomposeMatrixToTRS`，但**也**不能满足于"库已代劳"——
实测证明 fastgltf 的 `decomposeTransformMatrix`（`math.hpp:911-948`）**取无符号列模长、没有 det<0 分支**，
对镜像矩阵的分解是**有损**的（镜像被静默丢弃，且四元数变成非单位值：`|q|=1.1634`、`det(mat3)=+2.414`）。
所以正确的落法是：**关闭 `DecomposeNodeMatrices`**，在导入边界自己分解——但必须写成"负号只进缩放"的版本
（文档给的双重翻转写法是错的），并加保真自检。见 §4 T1/T2。

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

**结论**：配对模型本身正确（不能改成"只根节点乘转换矩阵"），但节点侧的**实现**在本次 T3 被查出是错的：
`convertInPlaceYUpToZUp` 的 TRS 分支用 `t'=R·t、r'=q·r·q⁻¹、s 原样保留`，而 `R·diag(s)·R⁻¹` 在 R=Rx90 时
是"y/z 互换"的对角阵而不是原 `s` ⇒ 非均匀缩放 + 旋转的节点偏差 0.383（实测）；
matrix 分支虽用了 `qMat·m·qInv` 正确共轭，但它拿到的已经是 **fastgltf 分解后的 TRS**（镜像已丢）。
已在 T1 统一改为"先 `M' = R·M·R⁻¹`、再镜像感知分解"。见 §4 T1/T3。

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

### 4.0 已确认的执行顺序（2026-09-29 用户拍板）

**先完成 A（导入链收尾），再进 B（引擎主线）**，理由：输入面（GLTFConvert 产物）先稳定下来，引擎侧改动才有可靠前提。

| 序 | 项 | 状态 |
|---|---|---|
| A1 | OBB run-to-run 非确定性 → 确定性 tie-break（让产物逐字节可复现） | ✅ 已完成（2026-09-29）：249 个产物两轮**逐字节一致** |
| A2 | 单位变换规约（导入边界 eps=1e-6 恒等收敛） | ✅ 已完成（2026-09-29）：检查脚本"残差占行" 120→**0** |
| A3 | 未启用扩展：`enableExtensions` + 不支持的干净 fail-fast | ✅ 已完成（2026-09-29）：15 个里 14 个转成功（0–3s）、1 个干净拒绝（0s，理由明确） |
| A4 | 多场景**全部**导出（命名 `sceneN`） | ✅ 已完成（2026-09-29）：`MultipleScenes` → `scene0`/`scene1` 两份产物；单场景命名零变动（`BasicModel.Scene.*`/`AnimatedCube.unnamed.*`） |
| B1 | T5 `matrixTable` 退役 | ✅ 已完成（2026-09-30）：JSON + pack 双格式 TRS-only；示例装载器改为从 TRS 组合世界矩阵；对拍 max\|Δlocal\|=\|Δworld\|=1.94e-07 |
| B2 | T6 局部真源唯一化（+ T7 `local_matrices` 去留） | ✅ 已完成（2026-09-30）：组件三份副本（TRS/世界矩阵/脏标记）+ `GetLocalMatrix()` 删除，读写直落 storage；`local_matrices` 判定为"求值中间量"保留；死接口清 14 个 |
| B3 | T8 `TransformComponent` → `TransformID` | **已完成**（见 §T8 状态块） |
| B4 | T9 其余组件 ID 化 | 待 B3 |
| C1 | T10 容量与行号冻结（**必须早于 C2**） | 待 B4 |
| C2 | T11 64B `Entity` + `.ulrescene` 直载 | 待 C1 |


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

### T1 GLTFConvert 死路收敛（**子模块内**）— ✅ 已完成（2026-09-29，含 fail-fast 收口）

- **落地（`src/Tools/GLTFConvert/`，子模块）**：
  - `math/NodeTransform.{h,cpp}`：**删** `Type::Matrix`、`m` 联合成员、`NodeTransform(const glm::mat4&)`、
    `setMatrix()`、`isMatrix()`、`toZUpMat4()`、`convertInPlaceYUpToZUp()`；结构只剩 `{ Type type; TRS trs; }`。
  - `gltf/ToNodeTransform.{h,cpp}`：导入边界统一做三件事 —— ① 原始矩阵（TRS 或 matrix 都先组矩阵）；
    ② `M' = R·M·R⁻¹` 配对转换；③ **镜像感知分解**（负号只进缩放；单轴 scale=0 用另两轴叉积补齐正交基）
    + **保真自检：残差 > 1e-4 即失败**（fail-fast，含错误信息里的节点名与残差 + DCC 侧的处置建议）。
  - `gltf/import/GLTFImportNodes.cpp` → `bool ImportNodes(...)`：任一节点分解失败即中止导入；
    `GLTFImporter.cpp` 透传失败 → `GLTFConvert_Process` 写 `err_buf` + 打日志 → CLI `return 1`。
  - `gltf/import/GLTFImporter.cpp`：删 `fastgltf::Options::DecomposeNodeMatrices`（库的分解对镜像有损，见 T2）、
    删 `RotateNodeLocalTransformsYUpToZUp` 声明与调用（该职责已并入 ToNodeTransform）。
  - `gltf/import/GLTFOrientationNodes.cpp`：**删除**，并从 `CMakeLists.txt` 的 `GLTF_IMPORT_FILES` 移除。
  - **进一步收敛（用户评审后）**：`NodeTransform` 的 `Type`（`enum{None,TRS}`）**删除**——它只是
    `!trs.empty()` 的复制品（同一事实两个真源），且 `isNone()` 零调用者、`isTRS()` 仅 1 处
    （`export/SceneExportNodes.cpp:32` → 改为 `if (!src.transform.trs.empty())`）。
    `math/NodeTransform.cpp` 随之**删除**（结构只剩 `{ TRS trs; }` + `rawMat4()`，头文件化），
    CMake `MATH_FILES` 同步移除。**行为中立性已实测**：删前/删后导出的 `trsTable`/`matrixTable`/
    `nodes`/`rootNodes`/`primitives`/`geometries` 表 **max|Δ| = 0.000e+00**（逐位一致），
    检查脚本 PASS 4/4。
  - **检查脚本收编**：`check/verify_transform_chain.py`（独立可用，只用 Python 标准库 + 一个 GLTFConvert.exe）
    + `check/README.md`。两种接入方式（均已实测）：
    ① **自定义目标**（推荐）`cmake --build build --config Debug --target GLTFConvertTransformCheck` → `CHECK RESULT: PASS (3/3)`；
    ② ctest（`-DGLTF_BUILD_TRANSFORM_CHECK=ON` + `ctest --test-dir build/src/Tools/GLTFConvert -C Debug -R GLTFConvertTransformChain`
    → `1/1 Passed`）。**注意：根级 `ctest` 在 ULRE 根看不到任何测试**（根未调用 `enable_testing()`，`src/ecs` 的 `add_test` 同样如此，属既有状态）。
- **残留检查**：`grep -rn "Type::Matrix|isMatrix|setMatrix|toZUpMat4|convertInPlaceYUpToZUp|RotateNodeLocalTransformsYUpToZUp|DecomposeNodeMatrices"`
  ⇒ **0 命中**（唯一假命中是 `SceneTableType::MatrixTable` 的字面包含）。
- **构建**：改 `NodeTransform.h`（结构布局）前先清了 `build/src/Tools/GLTFConvert/**/Debug` obj 树；
  `GLTFConvertCore.dll` + `GLTFConvert.exe`（CLI）+ `GLTFConvertQt.exe`（GUI）三目标 **rc=0 / 0 errors / 0 warning**。
- **产物一致性（实测，已订正为"数值等价"而非"逐字节一致"）**：见 T2 段末「字节一致性」小节 ——
  `.mesh`/`.material` 全 IDENTICAL，TRS/矩阵表差 ≤ 9.537e-07（float32 末位），AABB/sphere/obbCenter/obbHalf ≤ 9.5e-07；
  只有 `*.geometry`（3/5 文件、3~11 字节）与 OBB 轴向量（旋转对称形状的规范自由度）不同。
- **风险（仍在）**：产物格式被主仓 `example/Geometry/LoadScene/LoadStaticMesh.cpp` 消费 ⇒ 子模块与主仓必须同批验证。

- **原目标**：删掉 `DecomposeNodeMatrices` 开启后不可达的矩阵路径，让"局部变换只可能是 TRS"成为编译期/运行期事实。
- **文件**：`src/Tools/GLTFConvert/math/NodeTransform.h:36-48`、`math/NodeTransform.cpp:36-48, 110-122, 138-155, 165-172`、`gltf/ToNodeTransform.cpp:3-11, 22-28`。
- **前置**：`toZUpMat4()` 与 `Type::Matrix` 的全仓引用清点（本次已 grep：`toZUpMat4` 仅有声明+定义、**0 调用者**，可删；`Type::Matrix` 只被自身分支使用）。
- **验收**：① 子仓构建通过；② 用同一批模型跑转换，导出产物（json + pack）与改前**逐字节一致**（TRS 表、节点表、primitive 表计数与内容不变）；③ GUI/CLI 两条入口都能转。
- **风险**：与主仓双仓同步（产物格式被 `example/Geometry/LoadScene/LoadStaticMesh.cpp` 消费），必须两仓同批验证。

### T2 负缩放/镜像对拍 — ✅ 已完成（2026-09-29，**查出 3 个真 bug**）

- **判据**：`M' = R·M_raw·R⁻¹`（唯一正确基准），按元素最大差比对；用例 = 合成 glTF（手写，覆盖
  TRS 均匀/非均匀、matrix 非均匀、matrix 镜像 det<0、matrix 剪切、TRS 镜像）+ 真实模型 `res/model/BasicModel.glb`。
- **结果（修的 before/after，全部实测）**：

  | 用例 | 修前 err | 修后 err | 备注 |
  |---|---|---|---|
  | TRS 非均匀 + 旋转（S=(2,0.5,1)、绕 X 40°） | **3.830e-01** | 1.598e-07 | `err(TRS分支公式)=3.830e-01` ⇒ 现状输出正好等于那个手写公式 |
  | matrix 非均匀（det>0） | **5.000e-01** | 1.192e-07 | 同源（先被库分解成 TRS，再走错误代数） |
  | matrix 镜像（det<0） | **1.243e+00** | 1.904e-07 | 修前 `\|q\|=1.1634`（非单位）、`det(mat3)=+2.414` ⇒ **镜像被静默丢弃** |
  | TRS 镜像 | 2.384e-07 | 2.384e-07 | TRS 直通本来就是对的 |
  | matrix 剪切 | 3.492e-01（`\|q\|=1.0131`） | 2.466e-01 + **显式告警** | 剪切无法用 TRS 表示 ⇒ 改为可观测 |

- **三个真因**：
  1. **TRS 分支的手写代数错**：`t'=R·t、r'=q·r·q⁻¹、s 原样`。`R·diag(s)·R⁻¹` 在 R=Rx90 时是
     "y/z 互换"的对角阵（`diag(sx,sz,sy)`）而不是原 `s`，所以非均匀缩放 + 旋转的节点被歪掉。
  2. **fastgltf 的分解对镜像有损**：`math.hpp:911-948` 取无符号列模长、无 det<0 分支 ⇒ 镜像丢失，
     且对非正交输入产生**非单位四元数**（`|q|=1.1634`）。
  3. **剪切被静默吞掉**：TRS 结构无法表达剪切，旧路径既不报也不查 ⇒ 现在分解后做保真自检并告警
     （实测告警输出：`[Import] 警告：node 5 ("matrix_shear") 的局部变换无法用 TRS 精确表示（含剪切或畸形），残差 0.246565`）。
- **产物一致性**：`BasicModel.glb` 改前/改后（同参数 `--no-images`）导出比对 —— 见下方小节。

#### 字节一致性（`BasicModel.glb`，改前 vs 改后，同参数）

| 文件 | 结果 |
|---|---|
| `*.mesh`（5）、`*.material`（1） | **IDENTICAL** |
| `*.geometry`（5） | 2 个 IDENTICAL；3 个差 3/11/4 字节（0.25%/0.02%/0.06%，文件大小不变） |
| `*.scene`（MiniPack） | 大小不变（148504 B），582 字节不同（0.39%） |
| `*.Scene.json` | 15336 → 15918 B（浮点十进制文本长度变化导致，"键集合"与元素数完全一致） |

数值差（JSON 解析后按字段求 max|Δ|）：

| 表 | max\|Δ\| |
|---|---|
| `trsTable` / `matrixTable` | **9.537e-07**（float32 末位；现在统一走"矩阵→分解"，旧路径 TRS 直通） |
| `boundsTable` 的 `aabbMin/Max`、`sphere`、`obbCenter`、`obbHalf` | ≤ 9.5e-07 |
| `boundsTable` 的 **`obbAxisX/Y/Z`** | 最大 **1.586**（bounds[3] Cone / bounds[5] Cylinder） |

**OBB 轴向量的差异不是回归**：锥/圆柱这类**旋转对称**形状的 OBB 在垂直于对称轴的平面内存在
**规范自由度**（任取一组正交基都同样贴合），输入发生 float 末位变化就会换一组基 —— 证据是
对称轴分量逐位相同（如 bounds[3] 的三号分量 `-0.775036` 两版完全一致）、`obbHalf` 差仅 3.6e-07。
换句话说旧值也不是"唯一正确值"。**结论**：对不含镜像/剪切节点的模型，本次改动只带来
float 末位差 + OBB 轴的规范差 ⇒ 等价，而非零字节差。

#### 追加实测（2026-09-29）：转换器存在 **run-to-run 非确定性** —— 字节比对不能当回归判据

用**同一个二进制**连跑两次同一模型（`--no-images --no-meshlet BasicModel.glb`），20 个产物文件里
**7 个不一致**。字段级定位：

| 表 / 字段 | A/A（同二进制两次）max\|Δ\| |
|---|---|
| `trsTable` / `matrixTable` / `nodes` / `rootNodes` / `primitives` / `geometries` | **0.000e+00**（逐位稳定） |
| `boundsTable` 的 `aabbMin/Max`、`sphere`、`obbHalf` | 0（稳定） |
| `boundsTable` 的 **`obbAxisX/Y/Z`** | 最大 **2.0**（合成三角形）/ **0.382**（Cone、Cylinder） |
| `boundsTable` 的 `obbCenter` | ~1e-6（float 噪声） |

原因：`math/OBB.cpp` 的 `fromPointsMinVolumeImpl` 是**穷举朝向搜索**（coarse→fine→ultra 步长，
`#pragma omp critical` 内更新 best）。对**平面 / 旋转对称**形状，大量朝向的体积完全相等（tie），
赢家取决于哪个线程最后写入 ⇒ 朝向随运行变化；`obbCenter` 的 1e-6 差同样来自"选了另一个等价朝向"。
顶点数据本身稳定（`ok.geometry` 的 11 字节差异落在内嵌 `BoundingVolumes` 段，偏移 176..226）。

**影响**：① **不能用字节比对做转换器回归** —— 必须按字段比对，且只对变换/结构表要求 0 差；
② 依赖 OBB **朝向**的上层逻辑（剔除 / 排序 / 实例化）不能在多次运行或构建间假设其稳定，
但剔除实际依据的 **AABB 是稳定的**。

> ⚠ **本段结论已被 A1 修复推翻（2026-09-29）**：`math/OBB.cpp` 的并行归约改成**严格全序** tie-break 后，
> 同一二进制连跑两次 **249 个产物逐字节一致**（详见 §7 第 9 条）。上面保留的是修复**前**的历史证据，
> 用来说明当时为什么不能用字节比对。
**候选修法（未做，见 §7）**：① tie 时用确定性 tie-break（体积在 eps 内相等 → 取轴分量字典序最小者）；
② 并行结果先收集成向量、再串行做全序归约；③ 对结果做规范化（对称轴上的朝向按规则选）。

- **验收依据**：检查脚本已收编入库 —— `src/Tools/GLTFConvert/check/verify_transform_chain.py`（+ `check/README.md`）。

- **原目标**：证明 fastgltf 的 `DecomposeNodeMatrices` 在 `det<0`（镜像）/剪切矩阵上给出的 TRS 与原始矩阵等价。
- **做法**：构造最小 glTF（手写 json，或用现有模型改造）含 `matrix` 且 `det<0`；跑 GLTFConvert；把导出的 `matrixTable` 局部矩阵与用 `TRS::toMat4()`（`math/TRS.h:40-47`）展开的矩阵**逐元素 diff**（阈值 1e-5），并渲染确认左右手性一致。
- **验收**：diff 全在阈值内 → 文档 Task 0.2 正式判定为"库已覆盖"；若超阈值 → 才引入最小修正（并记录 fastgltf 的具体行为）。

### T3 方向转换配对验证 — ✅ 已完成（2026-09-29）

- **判据**：配对 `M'·v' = R·(M·v)`（顶点 `v' = R·v`、节点 `M' = R·M·R⁻¹`），据此做四层检查：
  - **[A] 节点局部**：导出 TRS 展开 vs `R·M_raw·R⁻¹`；
  - **[B] 导出内部自洽**：`matrixTable` 的 `localM` vs TRS 展开（同源，应完全一致）；
  - **[C] 顶点级**：叶子节点的 `boundsTable` AABB vs `worldM·(R·v)` 的 AABB —— 这一条同时证明
    **顶点恰好被旋转一次**且世界矩阵与顶点同坐标系（若被旋转两次或平方，AABB 会明显不符）；
  - **[D] 镜像/剪切**：见 T2 表。
- **结果**：合成 glTF 6 个叶子节点 **[C] 最大差 1.192e-07**；真实模型 `BasicModel.glb` 7 个节点 [A] 全 **≤ 9.537e-07**。
- **修复前的反证**（同一脚本、旧二进制）：节点 2（TRS 非均匀+旋转）世界 AABB 为
  `min=(4,-1.286,0) max=(6,0,1.532)`，正确值是 `min=(4,-0.643,0) max=(6,0,0.766)` —— 即**旧路径确实把这类节点放错位置**，
  而文档 Task 0.6 想验证的"各子构件位置与朝向 100% 准确"在旧路径上不成立。

- **原目标**：用一条独立证据链证明"顶点旋转 + 节点相似变换"这一对是等价的 Z-up 转换，而不是双重变换。
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

### T4.5 真实资产（Khronos `glTF-Sample-Assets`）验证 — ✅ 已完成（2026-09-29）

用官方样本集 `J:\Khronos\glTF-Sample-Assets\Models`（**144 个模型 / 13,191 个节点**）回答
"现实资产里是否大量存在没有 transform 的子节点"，并顺带做了真实资产级回归。

**① 变换分布（静态统计，脚本 `%TEMP%\t123\khronos_transform_stats.py`）**

| 节点类别 | 数量 | 占比 |
|---|---|---|
| 无任何变换键（`matrix`/`translation`/`rotation`/`scale` 都没有） | 320 | **2.4%** |
| 显式 TRS 且恒等 | 11 | 0.1% |
| 显式 TRS 且非恒等 | 12,353 | **93.6%** |
| `matrix` 定义 | 507 | 3.8% |
| 「单位/无变换」合计 | 331 | **2.5%** |

- 无变换节点中**含子节点的中间层节点 35 个**、叶子 285 个；85/144 个资产至少有一个。
- **分部集中**：`RecursiveSkeletons` 84/924 = 9.1%、`CarConcept` 17/101 = 16.8%、
  `MetalRoughSpheresNoTextures` 18/119 = 15.1%、`ABeautifulGame` 1/49；`BasicModel`/`VirtualCity` 为 0。
  ⇒ 不能按"平均 2.4%"把它当边缘情况，但也不是主流形态（93.6% 的节点带真实非恒等 TRS）。
- **`matrix` 定义里：镜像（det<0）13 个**（全部在 `VirtualCity` 一个资产内）、**剪切 0 个、退化 0 个**
  ⇒ ① 现有 fail-fast（剪切即拒）**不会拒掉任何官方样本**；② 镜像不是虚构场景，`VirtualCity` 就是真实实例。

**② 检查脚本对真实资产的三处假设不成立（已修，`src/Tools/GLTFConvert/check/verify_transform_chain.py`）**

| 症状 | 真实原因 | 修法 |
|---|---|---|
| `RecursiveSkeletons` [C] 差 **60.0** | 该节点是**蒙皮网格**（`JOINTS_0/WEIGHTS_0`）：顶点由 skin 矩阵驱动，与节点世界矩阵无关 | `node_positions()` 遇蒙皮属性返回 None（跳过并计入"跳过数"） |
| `VirtualCity` [A] 差 **6.104e-05** > 1e-5 | 节点平移量级 **751.4** ⇒ float32 分辨率就是 **9.0e-05**，固定绝对容差必假阳性 | 容差改为 `绝对 + 相对·max|量级|`（`--rel-tolerance`，默认 1e-5），错误信息带上 |M|max |
| `ABeautifulGame` [C] **3.194e-03**、`CarConcept` **2.459e-01** | 顶点被**截断到 4096 点**（该网格 28,901 点）⇒ AABB 缺极值 | 顶点**不采样**：全量读；超上限（4M）直接跳过 |
| （预防）bounds 被多节点共享 | 同一 `boundsIndex` 被 >1 个节点引用时，"本节点世界 AABB"前提不成立 | 这类节点跳过并单独计数 |

**③ 回归结果：`CHECK RESULT: PASS (9/9)`**（合成 3 + `BasicModel.glb` + 5 个真实资产）

```
model:BasicModel.glb      节点=7   无变换=0  AABB=5   max[A]=9.54e-07
model:RecursiveSkeletons  节点=924 无变换=84 AABB=0(跳过84/蒙皮) max[A]=1.91e-06
model:MetalRoughSpheres.. 节点=119 无变换=18 AABB=98  max[A]=1.79e-07
model:VirtualCity         节点=234 无变换=0  AABB=105(跳过19) max[A]=1.22e-04  ← |t|≈751 下的 1.3 ULP
model:ABeautifulGame      节点=49  无变换=1  AABB=33  max[A]=1.19e-07
model:CarConcept          节点=101 无变换=17 AABB=79  max[A]=2.38e-07
```
（原 [A]/[D] 数值缺陷回归 —— 非均匀 TRS 3.830e-01→1.598e-07、matrix 镜像 1.243e+00→1.904e-07 —— 在真实资产上
保持 <2e-06；`|q|-1` 全部 ≤1.1e-07。判据仍是 `M' = R·M_raw·R⁻¹`。）

**④ 真发现：无变换键的节点并没有"零成本"，它照样占 `trsTable` 一行 —— 被 1 ULP 顶掉**

- **实测**：`RecursiveSkeletons` 84/84、`MetalRoughSpheresNoTextures` 18/18、`CarConcept` 17/17、
  `ABeautifulGame` 1/1 —— **120/120 个无变换节点全部写了独立 trs 行**，其值为"单位变换 + 残差"：
  `max|t| = 0`（平移精确为 0）、`max|q−1| = 0`（四元数精确为单位）、**`max|s−1| = 1.19e-07`**
  （= 2⁻²³，float32 机器 epsilon）。
- **不是删 Type 造成的**：用改动**前**的二进制（`before_chk`）跑同一输入，节点 0 同样有 trs 行、
  数值逐位相同（`s=(1.0, 0.999999881, 0.999999881)`）⇒ 既有行为。原因与 `Type` 无关：
  导入边界的共轭 `M' = R·M·R⁻¹` + `DecomposeTransform`（列模长）在单位矩阵上引入 1 ULP 的 scale 偏差，
  而 `TRS::empty()` 是**精确比较**（`scale == vec3(1)`）⇒ 判不出恒等。
- **结论**：`Type`（`enum{None,TRS}`）从来不是表达"单位变换"的手段 —— 表达它的是 `empty()`；
  而现在的 `empty()` 因为比较方式而失效。想让这类节点真正零成本（不占行、引擎侧拿到精确单位矩阵），
  要在导入边界做**带容差的恒等规约**（见 §7 第 10 条），不是再加一个状态枚举。
- 检查脚本新增 **[F] 不变式**：源节点无任何变换键 ⇒ 导出不得有 trs 条目**或**该条目必须 ≤ 容差地等于
  单位矩阵（数值残差），且 `localM` 必须是单位矩阵；同时把"残差占行"的个数打进每行输出（现在会打印
  `无变换=N(残差占行=N)`），一旦它和"无变换"数相等，就说明规约没发生。

**⑤ 新发现（既有缺陷，与本次改动无关）："required 扩展未启用"的资产**失败方式不干净** —— 挂住或 abort**

**⑤-已修（2026-09-29，A3）："必需扩展"改为**自己预筛**，不再走进那条坏掉的失败路径**

- **改动**：新增 `gltf/import/GLTFExtensions.{h,cpp}`：
  - `EnabledExtensions()` —— 转换器**启用解析**的扩展集合（fastgltf 在**构造期**接收位掩码：
    `fastgltf::Parser(Extensions)`，该版本**没有** `enableExtensions` setter）；
  - `ReadRequiredExtensions()` —— 自己读 `extensionsRequired`（`.gltf` 扫 JSON、`.glb` 定位 JSON chunk），
    不引第三方 JSON 库；
  - `CheckRequiredExtensions()` —— 分类：**拒绝**（几何会缺失/错误或根本无法解析）/ **启用但效果未实现**（告警）。
  - `GLTFImporter.cpp` 在构造 Parser 前先跑预筛：拒绝的先 `return false`（**不碰** fastgltf 的失败路径），
    未实现效果的逐条打印 `[Import] 警告：源文件要求扩展 X，转换器已启用解析但未实现其效果…`（不静默降级）。
- **启用集合**（能消费或可安全忽略）：`KHR_materials_{unlit,ior,specular,iridescence,volume,transmission,`
  `clearcoat,emissive_strength,sheen,anisotropy,dispersion,diffuse_transmission,variants}`、`KHR_texture_transform`、
  `KHR_lights_punctual`、`KHR_mesh_quantization`、`EXT_texture_webp`、`KHR_texture_basisu`、`MSFT_texture_dds`、
  `GODOT_single_root`。其中**真实现了效果**的只有 `unlit` / `mesh_quantization` / `variants` / `single_root`。
- **拒绝集合**（明确报错 + 理由）：`KHR_draco_mesh_compression` / `EXT_meshopt_compression`（无解压 ⇒ 几何错误）、
  `EXT_mesh_gpu_instancing`（未实现 ⇒ 实例几何整体丢失）、`KHR_accessor_float64`、
  `KHR_materials_pbrSpecularGlossiness`、`MSFT_packing_*`（打包约定未实现 ⇒ PBR 通道错误）。
- **验证（此前 15 个"被跳过"的资产）**：**14 个 rc=0**（0–3 秒，含明确告警；`UnlitTest` 0 告警 = 真实现了），
  **1 个干净拒绝**：`SpecGlossVsMetalRough` → `rc=1 / 0 秒`，信息
  `源文件要求的扩展无法转换 —— KHR_materials_pbrSpecularGlossiness（已废弃的材质模型，转换器未实现）`。
  修前这 15 个全是"挂住 >120s 或 abort rc=3、且不打印 `[Error] Conversion failed:`"。
- **检查脚本同步**：预筛改为镜像同一套清单（`ENABLED_EXT`/`HANDLED_EXT`）——只跳过**会被拒**的资产，
  "有未实现效果"的照常检查并在结果后打印 ⚠ 行。

**⑥-已修（2026-09-29，A4）：多场景**全部**导出**

- **改动**（`export/ExportPureModel.cpp`）：
  - 原来只导出**一个**场景（默认场景），现在 `for si in scenes` 逐个导出 `BuildSceneExportData(sm,si,…)`；
  - **命名**：多场景（`scenes.size()>1`）用 `scene<N>`（用户拍板口径）；单场景沿用源场景名
    ⇒ 现有产物文件名**零变动**（`BasicModel.Scene.*`、无名场景仍 `AnimatedCube.unnamed.*`）；
  - **踩坑**：`SanitizeName("")` 会返回 `"unnamed"`（`export/SanitizeName.cpp:21`，函数保证非空），
    所以判断"是否用 sceneN"必须看**源场景名是否为空**，不能看 SanitizeName 的结果 ——
    否则多个无名场景会撞成同一个文件名（`MultipleScenes` 就是两个无名场景）；
  - 图像过滤的 `CollectSceneIndices` 原来只统计默认场景 ⇒ 改为**所有场景的并集**
    （否则"只被非默认场景引用的贴图"会被漏导出）。
- **验证**：`MultipleScenes` → `MultipleScenes.scene0.{json,scene}` + `MultipleScenes.scene1.*`，
  两份**内容各自正确**（scene0 引用 `MultipleScenes.0.geometry`、scene1 引用 `.1.geometry`，各 1 节点）；
  单场景 `BasicModel`/`AnimatedCube` 命名与修前逐字相同。
- **检查脚本同步**（`check/verify_transform_chain.py`）：`find_scene_jsons()` 收全部场景产物、
  `check_exports_of_scene()` **逐场景**校验（按 `scene<N>` 反解源场景序号，用**该场景**的可达性判定），
  并新增不变式 **"导出场景数 == 源场景数且序号覆盖 0..N-1"**。
  **反证**（证明判据非空）：人为造出 `unnamed`+`scene1`+`scene9` 混排 ⇒
  `[场景] 导出场景数 4 ≠ 源场景数 2（导出序号 [0, 1, 1, 9]）` 立即失败。

**⑤-历史（修前证据，保留说明为什么必须预筛）**：

官方样本 142 个资产里有 **14 个**带 `extensionsRequired`（`KHR_texture_transform`、`KHR_materials_unlit`、
`KHR_lights_punctual`、`KHR_materials_*`、`EXT_texture_webp`、`KHR_animation_pointer` 等；
典型为 `AnimationPointerUVs` / `UnlitTest` / `DirectionalLight` / `CommercialRefrigerator` / `SheenChair` …）。
由于 `gltf/import/GLTFImporter.cpp:73` 用的是**裸 `fastgltf::Parser{}`**（没有 `enableExtensions`），
这些资产在解析阶段就失败：

```
[GLTFConvert] Loading model: .../AnimationPointerUVs.gltf
[Import] Parse failed: One or more extensions are required by the glTF but not enabled in the Parser.
   ← 之后**没有** [Error] Conversion failed 行，进程也不退出
```

实测失败形态（`--no-images --no-meshlet`，`timeout 120`）：

| 资产 | 耗时 | rc |
|---|---|---|
| `AnimationPointerUVs` | 57s（自行结束） | **3**（abort） |
| `DirectionalLight` | 39s | **3**（abort） |
| `UnlitTest` | >120s（被外部 kill） | **124** |
| `CommercialRefrigerator` | >120s（被外部 kill） | **124** |

⇒ 既不是"干净地 rc=1 + 报错"（`core/GLTFConvertCore.cpp:155-161` 的错误路径根本没走到），
也不是单纯的慢：**解析失败后的返回路径会挂住或 abort**（怀疑在 `ImportFastGLTF` 的局部
`Parser`/`GltfDataBuffer`/`Expected` 析构上，需调试器确认）。
**影响**：任何自动化（含本检查脚本）碰到这类资产都会白等超时；且调用方拿不到任何错误文本。
**已做**：检查脚本现在**先读 `extensionsRequired` 再决定是否转换**，这类资产报 `[SKIP]` 并计入
"跳过 N"，不再吃 300s 超时（`check/verify_transform_chain.py: required_extensions()`）。
**建议（未做，待拍板）**：① 在 `GLTFImporter.cpp` 的 Parser 上用 `enableExtensions(...)` 打开转换器
实际支持的那些（`KHR_materials_*`/`KHR_texture_transform`/`KHR_lights_punctual`/…）；
② 对 fastgltf 不支持的（如 `KHR_animation_pointer`、`KHR_materials_pbrSpecularGlossiness`）
先查 `extensionsRequired` 并**干净地** fail-fast 报错，而不是走进坏掉的返回路径。


**⑥ 全库扫描：**`check/verify_transform_chain.py` 扫过全部 **142 个官方资产**（逐个转换 + 逐节点对拍）：

| 结果 | 原始 | ⑦ 交错修复后 | ⑧ 四元数归一化后 | **⑨ 多场景修复后** | 说明 |
|---|---|---|---|---|---|
| PASS | 117 | 124 | 126 | **127** | 变换链逐节点在容差内 |
| SKIP | 15 | 15 | 15 | 15 | `extensionsRequired` 非空（见 §⑤），转换器不支持 ⇒ 预筛跳过 |
| FAIL | 10 | 3 | 1 | **0** | —（142 个资产里可转换的全部通过） |

⇒ **142 个官方资产：127 PASS / 15 SKIP（扩展未启用）/ 0 FAIL**。

- ⑦ 修复（交错读取）⇒ `fail → pass` **7 个**：`AnisotropyStrengthTest`、`BoxInterleaved`、`ClearCoatTest`、
  `InterpolationTest`、`IridescenceDielectricSpheres`、`IridescenceMetallicSpheres`、`MandarinOrange`。
- ⑧ 修复（源 `rotation` 归一化）⇒ `fail → pass` **2 个**：`IridescentDishWithOlives`、`TextureEncodingTest`。

失败分解（**修前** 10 个）：

| 资产 | 症状 | 定性 |
|---|---|---|
| `AnisotropyStrengthTest`, `BoxInterleaved`, `ClearCoatTest`, `InterpolationTest`, `IridescenceDielectricSpheres`, `IridescenceMetallicSpheres`, `MandarinOrange` | [C] 世界 AABB 差 0.5–4.2 | **同一个真 bug：交错顶点缓冲被当紧凑读**（见 ⑦） |
| `IridescentDishWithOlives` | `[Import] 错误：node 5 ("Camera001") 的局部变换无法用 TRS 表示…残差 0.000377644 > 0.0001` ⇒ rc=1 拒转 | **分解/共轭链在此节点上有损**（源是 TRS、`det=1`、`|M|max=1.0` ⇒ 不是 float32 量级问题，也不是剪切） |
| `TextureEncodingTest` | [A] 节点 14 局部变换误差 **3.625e-03**（`|M|max=12`，容差 1.3e-4） | 同上：接近 90° 旋转 + 各向异性缩放（源 S=(12,1,3)）时 TRS 往返有损 |
| `MultipleScenes` | rc=1，`Failed to export pure model` | 该资产有 **2 个 scene 且都没有名字**，导出阶段失败（几何已写出，scene 打包失败） |

**⑦ 真 bug：交错（interleaved）顶点缓冲被当作紧凑 12 字节读取**

- **行为**：`gltf/import/GLTFImportPrimitives.cpp:62-93` 的 `CopyAccessorToBytes` 用
  `elemSize*count` **连续**拷贝（起点 = `bufferView.byteOffset + accessor.byteOffset`），
  **完全没有看 `bufferView.byteStride`**（全仓 `grep byteStride` 在 import 侧 0 命中）。
  ⇒ 只要 POSITION 落在交错 buffer view 里（`byteStride` = 24/32/36/48），顶点数据就是错的
  （不只是 bounds：`geo.positions` 本身就错，导出几何与 bounds 同源）。
- **证据（4/4 复现到 1e-8）**：把同一段 buffer 用"紧凑 12 字节（含 accessor 偏移）"解释，
  算出的 `worldM·(R·v)` AABB 与导出 bounds **逐轴吻合**；用正确步长则差 0.5–4.2：

  | 资产 | `byteStride` | 正确解释差 | 紧凑12解释差 |
  |---|---|---|---|
  | `BoxInterleaved`（acc 偏移 12） | 24 | 5.000e-01 | **1.4e-14** |
  | `InterpolationTest` | 32 | 4.219e+00 | **1.2e-07** |
  | `ClearCoatTest` | 48 | 9.400e-01 | **2.4e-07** |
  | `AnisotropyStrengthTest` | 48 | 6.000e-01 | **1.8e-08** |

- **分布**：官方样本里 **10 个资产有交错 POSITION**，其中 7 个 [C] 失败、2 个因扩展跳过、
  **1 个"通过"是假通过**（`RecursiveSkeletons`：节点全是蒙皮 ⇒ [C] 全跳过）。
  132 个紧凑资产里只有 3 个失败（⑧ 的另外三项）。
- **修复点**：`CopyAccessorToBytes` 需按 `bv.byteStride`（缺省 = `elemSize`）逐元素拷贝到紧凑目标缓冲；
  **修完必须重跑全库扫描**（预期 7 个 [C] 失败全部转 PASS）。**→ 已于 2026-09-29 修掉，见 ⑦-已修**
- **注意**：这不是"bounds 算错"，是**顶点数据整体读错** ⇒ 这些资产的几何在引擎里是错的
  （`BoxInterleaved` 这种标准测试资产都中招，说明此前没人拿交错资产验过导入链）。

**⑦-已修（2026-09-29）：`CopyAccessorToBytes` 现在按 `byteStride` 摘元素**

- **改动**：`gltf/import/GLTFImportPrimitives.cpp` 的 `CopyAccessorToBytes` 改为
  `stride = bv.byteStride.value_or(elemSize)`——紧凑（`stride == elemSize`）仍整段 `memcpy`，
  交错时**逐元素**拷进紧凑目标缓冲；越界检查改为 `startByte + (count-1)*stride + elemSize`
  （原来用 `elemSize*count`，交错时会漏检尾部）。三个 `sources::*` 分支收成一个 `copy_from` lambda。
  索引 accessor 按 glTF 规范不带 stride ⇒ `value_or` 自然退化为原行为。
  依据：`fastgltf::BufferView::byteStride` 是 `Optional<size_t>`（`fastgltf/tools.hpp:584` 已有同样用法）；
  `byteOffset` 原本就是生效的，**只有 stride 被忽略**。
- **验证（同一套"两种解释对拍"手法，前后正好反转）**：

  | 资产 | 修前 正确步长 / 紧凑12 | 修后 正确步长 / 紧凑12 |
  |---|---|---|
  | `BoxInterleaved` | 5.000e-01 / **1.4e-14** | **1.4e-14** / 5.000e-01 |
  | `InterpolationTest` | 4.219e+00 / **1.2e-07** | **3.0e-08** / 4.219e+00 |
  | `ClearCoatTest` | 9.400e-01 / **2.4e-07** | **2.4e-07** / 9.400e-01 |
  | `AnisotropyStrengthTest` | 6.000e-01 / **1.8e-08** | **8.9e-08** / 6.000e-01 |

- **检查套件**：`PASS (12/12)`（7 个原失败资产 + `BasicModel` + `VirtualCity` + 合成 3 例），
  `max[C]` 从 0.5~4.2 降到 **1.4e-14 ~ 8.8e-07**；构建 `0 error / 0 warning`；行尾/BOM 保持 BOM+CRLF。
- ⚠ **交错资产的转换产物从此与修前不同**（修前是错的）⇒ 仓库/引擎里若存有这些资产的旧产物，需重新转换。

**⑧ 已修（2026-09-29）：fail-fast 误拒 —— 根因是源 `rotation` 不是单位四元数**

- **症状**：`IridescentDishWithOlives` 被拒（`node 5 ("Camera001")` 残差 3.78e-4 > 1e-4），
  `TextureEncodingTest` 节点 14 报 [A] 3.625e-03（`|M|max=12`）。两者都**不是**剪切、也不是 float32 量级问题
  （该节点源是 TRS、`det=1`、`|M|max=1.0`）。
- **根因**：源 `rotation` 是**非单位四元数** —— `Camera001` 的 `rotation=[x,y,z,w]=[-0.162,0.688,0.162,0.688]`，
  `|q| = 0.9995879`（小数位截断的产物，偏离单位 4.1e-4）。而 `FastTRSToGlmMat4` 把它直接喂给
  `glm::mat3_cast`：**对非单位四元数，该公式不是"旋转 × 比例"**（对角项 `1-2(y²+z²)` 与交叉项
  `2(xy+zw)` 的缩放不一致）⇒ 矩阵带进 ~(1-|q|²) 的**各向异性**：
  实测 `mat3_cast(源四元数)` 的列模长 `0.99917634 / 0.99991350 / 0.99917634`、
  **列间最大 |dot| = 3.674e-04**（= 那些"残差"的量级）；归一化后列模长全 1、|dot| = 0。
  于是矩阵**真的不是 TRS 可表示的** ⇒ 保真自检（正确判据）把它判死 ⇒ 整个资产被误拒。
- **修复**：`gltf/ToNodeTransform.cpp` 新增 `NormalizedRotation(src)`（`glm::normalize`，全零四元数退化单位），
  `FastTRSToGlmMat4` 改用它。glTF 规范本就要求 rotation 为单位四元数 ⇒ 归一化是"按文件本意解释"，
  非单位值属非法输入；真正的畸形矩阵仍会被保真自检拦住（阈值不动）。
- **验证**：`IridescentDishWithOlives` **rc=0 且检查全过**（[A] 1.84e-07、[C] 1.35e-08）；
  `TextureEncodingTest` [A] **3.625e-03 → 2.02e-06**（同一根因连带修好）；
  全库 **124 → 126 PASS（3 → 1 FAIL）**；引擎回归门 42 PASS / 0 FAIL、三测试 rc=0；构建 0 error / 0 warning。
- **经验（已写进技能）**：残差异常大（>1e-5）但矩阵看起来正常时，**先查源四元数的模长**。
- **影响面（官方样本实测）**：142 个资产里带 `rotation` 的节点 347 个，其中 `|q|` 偏离 1 超过 1e-6 的 **5 个（1.44%）**，
  分布在 4 个资产：`IridescentDishWithOlives`（4.12e-04）、`TextureEncodingTest`（1.51e-04）、
  `BrainStem`（4.39e-06）、`Cameras`（1.53e-06，后两者只是浮点噪声）。⇒ 绝大多数资产不受影响，
  但"小数位截断的四元数"确实存在于真实资产中，且此前会让**整个资产**转换失败。

**⑨ 已修（2026-09-29）：`MultipleScenes` —— 导出文件命名不一致 + 默认场景被忽略**

修前症状：几何/网格全部写出，但在"打包场景"这一步失败
（`[Export] pack v2 write fail: Cannot open geometry file for ScenePayloadV2: .../MultipleScenes.geometry`），rc=1。

- **根因 1：同一文件被算出两个名字。** `MakeGeometryFileName(base, idx, total)` 在 `total==1` 时**不带索引**，
  而两侧传的 `total` 不同：**写出侧** `ExportGeometries.cpp:21` 用 `model->geometry.size()`（全局，=2 ⇒ 写 `.0./.1.`），
  **场景记录侧** `SceneExportGeometries.cpp:15` 用 `ci.geometries.size()`（本场景，=1 ⇒ 记录 `MultipleScenes.geometry`）
  ⇒ 打包时按记录名开文件，文件不存在。
- **根因 1b（同源、更隐蔽，且是运行时 bug）**：`SceneExportPrimitives.cpp` 用默认 `total=-1` 自己又算了一份
  （**永远带索引**），而这个字段 `pe.geometryFile` **被写进场景包、由引擎在加载时按它打开几何文件**
  （`SceneExportPack.cpp:226/516` 写入，`LoadStaticMesh.cpp:394/401` 读出并 `base_dir + "/" + name`）。
  ⇒ **单几何资产**的包内名 `X.0.geometry` 与磁盘 `X.geometry` 不符（实测 `BoxInterleaved`）⇒ **引擎加载必然失败**。
  属既有缺陷，此前没被发现。
- **修法（定为"写出侧参数是唯一规范"，并消灭第二处计算）**：
  ① `BuildGeometries` 新增 `totalGeometryCount` 形参，调用点传 `model.geometry.size()`（与写出侧同源）；
  ② `BuildPrimitivesExport` **不再计算文件名**（删掉该形参与 `ExportFileNames.h` 依赖），
  几何文件名在 `SceneExportBuild` 的链接步直接取自几何表 `data.geometries[geoIt->second].file`
  ⇒ 包内名字与几何表、磁盘三者必然一致（单一真源）；
  ③ `SceneExportMaterials` 去掉 `ci.materials.size()`（写出侧 `MaterialExporter.cpp:53` 用默认 `-1` ⇒ 永远带索引），
  两侧一致（材质是同类不一致，只是 `MultipleScenes` 恰好 0 材质没暴露）。
- **根因 2：导出写死 `sceneIndex=0`（`ExportPureModel.cpp:40` 原注释`// first scene only`）。**
  glTF 规范要求客户端优先使用 `scene`（默认场景），该资产是 `scene=1` ⇒ 修前导出的是**非默认场景**的内容。
  修法：`GLTFModel`/`pure::Model` 增加 `default_scene`/`defaultScene`（导入时取 `asset.defaultScene.value_or(0)`），
  导出用 `SelectDefaultScene()`（越界回落 0）。
- **检查脚本**：新增**源节点可达性判据** —— 多场景资产里非默认场景的节点本就不导出，
  现在跳过而非判失败（可达却缺失才报错）。
- **验证**：`MultipleScenes` **rc=0**，包内引用变为 `MultipleScenes.1.geometry`（= 默认场景 scene 1 的几何，修前是 `.0.`）；
  `BoxInterleaved` 包内名与磁盘一致（`BoxInterleaved.geometry`）；检查套件 `PASS (12/12)`；全库见 ⑥ 表。


### T5 `matrixTable` 退役（TRS-only 导出）——依赖 T2 + T4

- **目标**：导出侧每节点只留 TRS（identity 用 `trsIndex < 0` 表达），删 `matrixTable` 里的 local/world 双份。
- **文件**：`src/Tools/GLTFConvert/export/SceneExportNodes.cpp:27-33`、`export/SceneExportData.h:20-22, 53-54`、`export/SceneExportPack.cpp:189-191, 350-352, 457-459, 478-484`、`export/SceneExportJson.cpp:76-78`；主仓 `example/Geometry/LoadScene/LoadStaticMesh.cpp`（`PackedNode`/`NodeList` 解析、`StaticMeshNode::{localMatrix,worldMatrix}`）、`inc/hgl/graph/mesh/StaticMesh.h:22-23`。
- **注意**：世界变换有真实消费者（`SceneTest.cpp` 的扁平化烘焙）⇒ TRS-only 后世界变换要在加载期自行连乘得到（一次，不逐帧）。
- **验收**：3 个模型转换+加载正常；pack 体积下降（记录改前后数字）；门 42 PASS / CSM 契约不变。

**实测结论（2026-09-30，已完成；用户拍板"JSON + pack 一起去"）**

- **爆炸半径比预估小**：`matrixTable` 的**引擎运行时消费者 = 0**（`src/` + `inc/` 全仓 grep 命中 0）；
  产物消费者只有 `example/Geometry/LoadScene/LoadStaticMesh.cpp` 一处（内含**两条**装载路径：
  SCN2 chunk 路径 + MiniPack 路径 —— 漏改一条就会字段错位）。
- **产物侧改动**：`SceneExportData.h`（删 `matrixTable` / `localMatrixIndex` / `worldMatrixIndex`）、
  `SceneExportNodes.cpp`（删矩阵填充）、`SceneExportJson.cpp`（删 `matrixTable` 与节点 `localM`/`worldM` 键）、
  `SceneExportPack.cpp`（删 `MatrixTable` chunk + 枚举重排、`PackedNode` 去两字段、NodeList 去两 int、
  删 MiniPack 的 `MatrixTable` 条目）、`SceneExportTransforms`（删 `GetOrAddMatrix`）。
  **`ComputeWorldMatrices` 保留**：导出内部仍需它算 world AABB，只是不再写进产物。
- **消费者改动**：`LoadStaticMesh.cpp` 新增 `ComposeNodeMatrices()`（两条路径共用）：
  `local = TranslateMatrix(t)·ToMatrix(q)·ScaleMatrix(s)`（与 `hgl::math::Transform::GetMatrix()` 同一组合式）、
  `world = 父world × local`、DFS 前序；**无 trs 行 ⇒ 单位变换**（这条契约被数据对拍实测确认）。
- **验证（数字）**：
  - 数据对拍（`vulkan_logo`：旧 pack 的矩阵表 ↔ 新产物 TRS 组合）：`max|Δlocal| = max|Δworld| = 1.94e-07`；
  - pack 体积：**247,409 → 246,780** B（该资产只有 4 节点；节省 ≈ 2 矩阵/节点 + 8 B/节点）；
  - 示例 `LoadScene` 真跑：`ABeautifulGame` 棋盘/棋子渲染正确（组合约定错会立刻表现为飞散/错位）；
  - 检查脚本：[B] 判据随 `matrixTable` 一起删除，[C] 的 worldM 改为脚本自己按父链组合；PASS(8/8)；
    全库 142 资产 **141 PASS / 1 SKIP / 0 FAIL**；
  - 引擎回归：门 42/0、四测试 rc=0；构建 0 error / 0 warning；改动文件 BOM+CRLF 合规。
- **零兼容代价（已拍板接受）**：旧格式产物新装载器读不了 ⇒
  `res/model/vulkan_logo/*`、`res/ABeautifulGame.StaticMesh/*` 已用新转换器重生成。

### T6 局部真源唯一化（组件三份副本 → 一份）

- **目标**：`TransformComponent` 的 `local_pos/local_rot/local_scale`（`inc/hgl/ecs/components/TransformComponent.h:46-48`）删除，读写全部直落 `TransformDataStorage`（唯一真源）。
- **文件**：`src/ecs/components/TransformComponent.cpp`（`:104`、`:111/114`、`:146/149`、`:182/185`、`:210-213`、`:223-231`、`MigrateStorage :599`、`UpdateWorldMatrix :664-`）+ 头文件。
- **验收**：`TestTransformFlatStorage`、`TestCSMIncrementalPass`（含 D4 静态写入告警 Test 15 契约）、门；示例 `ClockUse`（static/movable 混合）、`RecursiveCube`（层级）、`ComputeTransformHierarchy` 冒烟。

**实测结论（2026-09-30，已完成）**

- **删掉的副本**：`local_pos/local_rot/local_scale`（局部 TRS 的第二份）、`cachedWorldMatrix`（世界矩阵的第二份）、
  `matrixDirty`（脏标记的第二份）；`TransformComponent::GetLocalMatrix()` 随之**无调用者 ⇒ 一并删**。
  现在 `TransformComponent` 只用 `storageHandle` 寻址，读写全部直落 `TransformDataStorage`（含 `IsDirty()`：
  由 `storage->IsDirty(handle)` 判定）。
- **顺带修掉的两个隐患**：① `GetWorldRotation/GetWorldScale/SetWorld*` 原来直接用 `storageHandle`（可能还是
  `INVALID_HANDLE`）⇒ 未分配时读越界，现在统一走 `GetStorageHandle()`（惰性分配）；
  ② `OnAttach()` 换存储时原来从组件副本抄 TRS，现在**从旧行搬**（组件已经没有副本可抄）。
- **死代码清理**（同批，全部 0 引用）：匿名命名空间的 `TransformRecord`/`ToArray*`/`ToVec*`/`ToQuat`、
  `GetLocalMatrix()/SetLocalMatrix()`、`GetAllPositions/Rotations/Scales/WorldMatrices`、
  `GetLocalMatrices/GetParentIndices/GetHierarchyDepths/GetEvalOrder/GetLevelOffsets`（ValueArray 读取口）、
  `GetLevelHandles`、`GetLevelOffsetsData`、`GetEvalOrderCount`、`UpdateMovableDirtyMatrices`、
  `UpdateAllDirtyMatrices`、`MarkTopologyDirty`。
- **验证**：`TestTransformFlatStorage` / `TestRenderItemDataStorage` / `TestCSMIncrementalPass`（含 Test 15 D4 契约、
  Test 23 相机槽）**rc=0**；门 **42 PASS / 0 FAIL**；三个示例 `ClockUse`/`RecursiveCube`/`ComputeTransformHierarchy`
  跑满 10s 不崩（rc=124=被 timeout 杀）；构建 0 error / 0 warning。
- **坑（写进技能）**：把头里的 `inline bool IsDirty() const` 改成外部定义后，**陈旧 obj** 会报
  `LNK2005: IsDirty() already defined in <Test>.obj`（老 TU 按 inline 发射了符号）⇒ 先 purge
  `build/src/ecs/**/*.dir/Debug` 再编，不要怀疑代码。

### T7 `local_matrices` 缓存去留（与 T6 同批判定）

- **现状消费者只有三处**：`TransformComponent::GetLocalMatrix`（`TransformComponent.cpp:221-226`）、`TransformDataStorage::UpdateAllLocalMatrices/GetLocalMatricesData`（`TransformDataStorage.h:234-251`）、示例 `example/Basic/ComputeTransformHierarchy.cpp:314/360`（把 local 矩阵当 mat4 上传给 compute shader 做层级求值 demo）。
- **决策**：若 `ComputeTransformHierarchy` 的定位是"未来 GPU 层级求值的探路"，缓存保留（并在注释写明"仅该示例消费"）；若暂不做 GPU 层级求值，缓存与示例一起删（零兼容口径）。
- **验收**：删除后每行省 64B（T0 探针复测）；示例/测试全绿。

**判定结果（2026-09-30，用户选择"执行 B2"⇒ 按证据保留缓存）**

- `local_matrices` **不是副本，是求值中间量**：`UpdateAllWorldMatricesFlat` / `UpdateDirtyWorldMatricesFlat`
  都要读 `local_matrices[idx]`（`world = 父world × local`）；删掉就得在每个节点重算 `T*R*S`。
  它由 TRS 唯一决定、**没有任何写入口**（T6 已删 `SetLocalMatrix` 与两个 `GetLocalMatrix`），
  已是纯派生数据 ⇒ **保留**，并在头里写明"求值中间量，不是第二真源"。
- 外部读取口只剩 `GetLocalMatricesData()` / `UpdateAllLocalMatrices()` 两个，消费者**只有**
  `ComputeTransformHierarchy`（`example/Basic/ComputeTransformHierarchy.cpp`，GPU 层级求值探路）——
  该示例仍在，且自检有效 ⇒ 一并保留（**这是"示例定位"那一问的现状，未改动**）。
- **示例的真实自检数字**（比 T7 原本的验收更强）：`ComputeTransformHierarchy` 自报
  `PASS: 212 nodes across 8 levels verified successfully! max_diff=1.9e-06/1.0e-06/5e-07`
  （CPU 从 TRS 组合出的世界矩阵 ↔ GPU compute 层级求值）⇒ T6 的"单一真源"在两条独立实现上对得上。

### T8 `TransformComponent` → `TransformID`（阶段一主体）

- **爆炸半径（已量化）**：75 文件 / 347 处、`AddComponent<...TransformComponent>` 74 处、示例 30+ 文件。
- **顺序**：① 引入 `TransformID`（= 现有 `HandleID` 语义）+ 带世界上下文的 `TransformAccessor`；② 组件瘦身为"只有 id"（去掉三份副本、`cachedWorldMatrix`、`child_ids` 容器）；③ 示例批量改调用点；④ 删 `inc/hgl/ecs/components/TransformComponent.h` + `src/ecs/components/TransformComponent.cpp`。
- **隐藏工作量（必须预先评估）**：**组开关 = 组件计数**（`Context::RegisterComponentInstance` 自动装组/开开关，见技能 `ulre-ecs-layer` 不变量）⇒ 删掉组件类会连带改系统组的激活条件；`RenderItem::GetTransform()` 返回 `shared_ptr<TransformComponent>`（`inc/hgl/ecs/core/RenderItem.h:51`）也要一起换成 ID。
- **验收**：每步 build + 四个测试 + 门 + 5 个代表性示例 0 VUID；`grep -rn TransformComponent src inc example` 最终**零命中**（用户会逐文件复查是否删干净）。
- **状态：✅ 已完成（T8-1..T8-5，本地未提交）**
  - `TransformID`（`inc/hgl/ecs/support/TransformID.h`）= **世界内**变换行号（非全局 ID；`Mobility`、`TransformChange`/`ToChangeMask` 一并搬到这里）；
  - `TransformAccessor`（`inc/hgl/ecs/support/TransformAccessor.{h,cpp}`）= 值类型薄句柄（storage + 行号 + context），**全 const**（句柄语义：const 指手柄不能换座，指向的行仍可变）、零副本；局部 TRS 直落存储、world 由父链组合；`fixed-pixel` 状态入存储、算法在访问器；
  - `TransformComponent.{h,cpp}` 已 `git rm`；`grep -rn TransformComponent src inc example` **零命中**（仅 doc 保留历史记录）；`RenderItem::GetTransform()` 返回访问器；`Context` 的变换列表改 `TransformID` 向量 + 实体级 API（`CreateTransform/GetTransform/GetTransformByEntity/GetTransformID/DestroyTransform`）；
  - 组开关：**有变换出现即自动装 `TransformSystem`**（`Context::RegisterTransform`），不再依赖"组件计数"；
  - **搬迁中发现并修掉一个真 bug**：访问器 setter 原先只写存储、不累积 `change_masks`，而 `TransformSystem::ShouldUpdateTransform` 以掩码为硬判据 ⇒ 只经访问器写入的行（连同子孙）**永不上传 GPU**；现由 `TransformAccessor::MarkLocalChanged` 统一记账（版本 +1、掩码累积、逐行 bump 子孙 WorldMatrix）；
  - 验证：全仓 Debug 构建 **0 真错误**（唯一错误是 `doc/backlog.md:109` 已登记的 TexConvCore 存量 LNK1104）；`TestTransformFlatStorage` / `TestRenderItemDataStorage` / `TestCSMIncrementalPass` **rc=0**；门 **42 PASS / 0 FAIL**；Test 15 = **13 源码契约 + 6 行为检查**（新增"经访问器写入必累积变更掩码"的契约与行为检查，专门防上面那个 bug 复活）；示例 ClockUse / RecursiveCube / ComputeTransformHierarchy / GizmoUsageExample / BasicLitSunDirection / IBLEnvironment / LoadScene / RayPicking 等 **0 error 行**；
  - **未验证项（诚实标注）**：**动效的视觉确认未完成**——引擎在窗口隐藏/最小化时整帧跳过（`src/Work/WorkManager.cpp:93` `if(!has_window || win->IsVisible())`），把窗口前置需要用户同意（本次已超时未获同意）；而截图工具能截隐藏窗口 ⇒ 曾把"隐藏窗口的静止帧"误判成"画面冻结"。需要一次前台可见的 `RecursiveCube`/`ClockUse` 目视确认（掩码修复的直接观感是"物体真的会动"）。
  - **用户实测发现的真回归（已修）**：`RecursiveCube` 里 92 个实例全画在原点且不动。根因是组件退役时**丢了三条"对世界的副作用"**：
    ① `SetMobility` 不再通知世界把该行在静态/可动列表之间换边（`Context::MigrateTransform`）⇒ 该类行停在旧通道，渲染侧的实例→行索引映射取不到它 ⇒ 取默认行（单位矩阵）⇒ 全在原点且永不更新；
    ② `SetParent` 不再维护存储**子表**（HEAD 的组件在 `SetParent` 里做 `AddChild/RemoveChild`）⇒ 子孙标脏/版本 bump 的遍历失效；
    ③ 实体销毁不释放变换行（HEAD 由组件 `OnDetach` → `UnregisterTransform` + 摘父表 + 释放）⇒ 残行留在列表里把索引映射整体串位。
    修在 `TransformAccessor::{SetMobility,SetParent}` 与 `ECSContext::DestroyEntity`（现在会先 `DestroyTransform(GetTransformID(id))`）。
  - **防复活**：新增 `TestTransformFlatStorage` **Test 8「变换行生命周期契约」**（Mobility 换边 / SetParent 子表 / 销毁注销），并做了反证——临时抽掉 `context->MigrateTransform(...)` ⇒ Test 8 精确报"该行未进可动列表（渲染侧取不到它的行 ⇒ 实例画在原点）"、`rc=10`。顺带修好该测试**从未 `logger::InitLogger`**：此前它的失败信息全不可见（静默通过/静默失败），现已能打印。
  - 数值迹象：`RecursiveCube` 修前 `LocalToWorld required=122`（结构冻结），修后 `required=962`（递归结构正常增长）。

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
6. ~~检查脚本是否收编~~ **已收编（2026-09-29）**：`src/Tools/GLTFConvert/check/verify_transform_chain.py`（独立可用）
   + `check/README.md` + CMake 选项 `GLTF_BUILD_TRANSFORM_CHECK`（默认 OFF → ctest `GLTFConvertTransformChain`）。
   后续若要做成更独立的工具：把它挪到独立的 check 目录/包，接受 `--exe`、`--models`、`--report`，已具备。
7. ~~剪切告警还是 fail-fast~~ **已定为 fail-fast（2026-09-29）**：残差 > 1e-4 直接判失败 → `ImportNodes` 返回 false
   → CLI `return 1` 并打印节点名/残差。**单轴 scale=0 不算失败**（用另两轴叉积补齐正交基，精确重建）；
   两轴以上退化才失败。
8. **样本模型只能用 glTF 2.0**：`res/model/color_teapot_spheres.gltf` 是 assimp 导出的 **glTF 1.0**
   （`asset.version="1.0"`、`buffers` 是对象不是数组），fastgltf 不支持 ⇒ 转换器会长时间卡住/异常退出，
   检查脚本现在会**先校验版本再转换**并给出明确错误。要把 1.x 资产纳进来就得在转换器入口做版本判定 + 明确报错
   （目前是 fastgltf 的行为，属既有缺口，与本次改动无关）。
9. ~~**最小体积 OBB 的 run-to-run 非确定性**~~ → **A1 已修（2026-09-29）**：
   - **根因**：`math/OBB.cpp` 的四处并行归约（粗搜索的线程局部与 `omp critical`、两次 refine 的同两处）都用
     `if(vol < bestVol)` —— **只比体积、且是严格小于**。平面/旋转对称形状上大量朝向的体积**完全相等**（tie），
     赢家于是取决于"哪个线程先进入 `critical`"（到达顺序）⇒ 同一二进制度连跑两次得到不同朝向
     （实测 `obbAxis*` 最大 Δ=2.0、`obbCenter` ~1e-6，并让 `*.geometry`（内嵌 BoundingVolumes）与 `*.scene`
     的字节随运行变化）。
   - **修法**：新增**严格全序**判据 `better(volA,yawA,pitchA,rollA, volB,yawB,pitchB,rollB)`
     —— 先比体积，体积相等再按 `(yaw,pitch,roll)` 字典序；四处归约（含串行 `#else` 分支，保证 OpenMP ON/OFF 一致）
     全部改用它。全序下 min 与归约顺序无关 ⇒ 结果与线程数、分块、到达顺序无关。
   - **验证**：同一二进制连跑两次、3 个模型共 **249 个产物逐字节一致（0 处差异）**（修前 `BasicModel` 20 个文件里 7 个不同）；
     **AABB 逐位不变（差 0.00e+00）**；OBB 体积相对变化 ≤ **9.2e-07**（只在不同 tie 候选之间换朝向，
     属旋转对称形状的规范自由度，不是质量退化）。
   - **影响**：转换器回归**重新可以用字节比对**（同机同二进制）；跨编译器/跨环境仍建议字段级比对。
10. ~~**单位变换规约（是否做）**~~ → **A2 已修（2026-09-29）**：
   - **背景**：源文件里"没有变换键"的节点，经共轭 `R·M·R⁻¹` + 分解后留下 **1 ULP** 的 scale 残差
     （实测 `|s−1| = 1.19e-07 = 2⁻²³`，`t`/`r` 精确为 0），而 `TRS::empty()` 是**精确比较** ⇒ 判不出单位变换。
   - **修法**：`gltf/ToNodeTransform.cpp` 在分解之后、保真自检**之前**加"恒等规约"——
     `|t| ≤ 1e-6`、`|s_i−1| ≤ 1e-6`、`|q.xyz| ≤ 1e-6`、`||q.w|−1| ≤ 1e-6` ⇒ 收敛为规范 `TRS{}`
     （eps=1e-6：远离 1.19e-07 的 ULP 噪声，又远小于真实几何尺度差异）。**不引入状态枚举**
     （`empty()` 仍是唯一出口）。放在自检前 ⇒ 自检校验的就是最终存下来的值。
   - **验收**：检查脚本的"残差占行"计数 **120 → 0**（合成 `ok` 1→0、`RecursiveSkeletons` 84→0、
     `CarConcept` 17→0、`ABeautifulGame` 1→0、`MultipleScenes` 1→0）；[A] 不退化
     （`BasicModel` 9.54e-07 不变；`MultipleScenes` 的恒等节点 4.77e-07 → **4.44e-16**）；
     全库 **127 PASS / 15 SKIP / 0 FAIL**；门 42 PASS / 0 FAIL、三测试 rc=0。
   - **⚠ 更正此前的估计**：原文写"每个无变换节点白占一行 trsTable（`RecursiveSkeletons` 84 行）"**不准确** ——
     `GetOrAddTRS` 是**按值去重**的，那些"单位 + 同值 ULP 噪声"的行本来就被合并成 **1 行** ⇒
     `trsTable` 实际只省 **1 行/资产**（`RecursiveSkeletons` 10→9、`CarConcept` 40→39、`ABeautifulGame` 36→35）。
     **真实收益在节点级**：那些节点不再引用 trs 行（`trsIndex = -1`），且**引擎侧拿到的是精确单位矩阵**
     （`matrixTable` 的 0 号条目）而不是"几乎单位"的矩阵 —— 这一条对后续 T6（局部真源唯一化）与
     T10（行号冻结）才有意义。

---

## 8. 外部架构评审整合（2026-09-30）

> 来源：`doc/future/ULRE_Architecture_Technical_Review_20260930.md`（另一工具生成，代码基线 `b941570` = **删组件之前**）。
> 本节把它逐条对照**当前**状态判定：已解决 / 仍成立（采纳）/ 不成立（给依据），并把数字重测于本批之后。

### 8.1 判定表

| # | 评审意见 | 现状（本仓实测） | 处置 |
|---|---------|-----------------|------|
| R1 | `TransformComponent` 过渡期外壳的双轨风险（老路径建的实体 `entity_rows` 无 owner ⇒ `GetTransformByEntity` 无效） | 组件已 `git rm`，`grep -rn TransformComponent src inc example` **零命中**，双轨不存在。但**该类风险真的发生过**，且比评审列的更宽——见 8.3 | ✅ 已解决；其"类"已扩写为 T9 的强制前置检查 |
| R2 | `Allocate()` 动态扩容 ⇒ 行号漂移 = Phase 3 最大前置阻塞 | 成立：`TransformAssignmentBuffer::EnsureCapacity` + `"L2W recreated"`（`src/ecs/support/TransformAssignmentBuffer.cpp:163`、`:511`）确实重建 GPU buffer | ✅ 采纳 → T10 具体化（8.2） |
| R3 | 347 处引用 / 75 文件，迁移体量大；建议脚本生成清单分批改 | 引用面已归零（本批 84 文件迁完：示例 42 + gizmo 17 + 引擎侧）；**但"脚本批量替换"实测有害**——同名成员（`transform`）、带前缀表达式（`gizmo->root_transform`）、inline→外部定义的 const 变化会反复生成新错误，最终必须"逐文件读→改→编译" | ✅ 已解决（方法上否掉脚本建议） |
| R4 | `MaterialComponent` 不是纯数据（recipe/重试/降级）⇒ 不能按纯数据表迁移 | 成立：`inc/hgl/ecs/components/MaterialComponent.h` 含 `recipe_hash`、`cached_normalized_recipe`、`shadow_cached_normalized_recipe` 等运行期状态 | ✅ 采纳 → T9 的 Material 分两层（8.2） |
| R5 | `BoundingBoxComponent` 仍 OOP，视锥剔除无法向量化 | 成立：`src/ecs/support/PrimitiveBatchPipeline.cpp:176`、`src/ecs/support/line/LineRenderPipeline.cpp:454`、`src/ecs/systems/tick/LineBoundsUpdateSystem.cpp:37` 均逐实体 `GetComponent<BoundingBoxComponent>()` | ✅ 采纳并**前置**：BoundingBox 连续化排在其余组件 ID 化之前（热路径） |
| R6 | 层级求值"版本号漏传"风险 ⇒ 建议封装统一入口 | **已落地**：`TransformAccessor::MarkLocalChanged` 就是该统一入口（版本 +1 + 掩码累积 + 逐行 bump 子孙 `WorldMatrix`），并有 Test 15（13 源码契约 + 6 行为）与 Test 8 护栏 | ✅ 已解决（与建议同向） |
| R7 | `Context.h` 691 行过重 ⇒ 拆 Public/Impl | 事实成立（689 行、30+ include） | ⏸ **延后**：T11（64B Entity）会重写该文件的实体/组件部分，先拆必返工 |
| R8 | `eval_order` 构建是 O(n²) ⇒ 改 Kahn | 部分成立：实现是"节点 × 层深"，实测 `ComputeTransformHierarchy` 场景 **212 节点 / 8 层**（自检 `max_diff ≤ 1.9e-06`）⇒ 现值 O(8n)，非瓶颈 | ⏸ 延后（低优先、非阻塞）；若改，Kahn 顺带得到天然环检测 |
| — | 评审"偏差表"7 条（Accessor 24 B vs 文档"4 B 零成本"目标、禁全局 storage、不引 48 B TRS SSBO、ActiveRowLease 禁动、`M'=R·M·R⁻¹`、fastgltf 分解关闭、动静物理分区待 T10） | 逐条与实仓一致（`sizeof(TransformAccessor)` 探针实测 **24 B**） | ✅ 保留；"文档 4 B 口径需修订"应写进 Phase 3 文档 |

### 8.2 采纳项落点

- **T10（容量与行号冻结）**
  - `TransformDataStorage::Initialize(max_statics, max_dynamics)` 预分配；`Allocate()` 超容量 **fail-fast**（与"材质行 arena 1024 行不扩容、超限=项目 bug"同一口径）；
  - `TransformAssignmentBuffer` 去掉 `"L2W recreated"` 路径（`TransformAssignmentBuffer.cpp:511`）；
  - **行字节瘦身（评审未见，本批引入）**：`fixed_pixel`（`FixedPixelState` **32 B/行**）与 `children`（`std::vector`，MSVC x64 Release 24 B / Debug 32 B，且绝大多数为空）被做成了**每行数组** ⇒ 每行 189 B → **约 267 B（Release 口径；Debug 约 275 B）**（+41%），与 Phase 3 的 48 B TRS + 64 B L2W 目标背道而驰。建议两者改**稀疏侧表**：fixed-pixel 只有 gizmo 行需要；子表用 CSR（`child_offset/child_count` + 共享 child pool）。这是 T10 该顺手做掉的。
- **T9（其余组件 ID 化）**
  - **先做 BoundingBox 连续化**（`BoundingBoxDataStorage` 已有头文件，补齐实现；`PrimitiveCullSystem` 改连续数组批处理）；
  - **MaterialComponent 分两层**：数据层（MaterialID → SSBO 行）ID 化；状态层（recipe/重试/降级）留在 MaterialManager / 专属 System，不入纯数据表；
  - 其余（Primitive / Renderable / Visibility / Camera）按 T9 既定顺序。

### 8.3 T9 前的强制前置检查（本批血的教训）

删一个组件类时，必须把它**全部"对世界的副作用"**搬走，而不只是数据面。做法：逐条对照
`git show HEAD:<被删文件>` 里的每个 `ctx->` / `owner_context->` / `storage->` 调用。本批漏过并已修的：
1. `SetMobility` 通知世界把该行在静态/可动列表间换边（`Context::MigrateTransform`）—— 漏掉 ⇒ 行停在旧通道，渲染侧实例→行索引取不到它 ⇒ 取默认行（单位矩阵）⇒ **92 个实例全画在原点且不动**（用户实测 `RecursiveCube`）；
2. `SetParent` 维护存储子表（存储的 `SetParent` 只写 `parent_indices`）—— 漏掉 ⇒ 子孙标脏/版本 bump 的遍历失效；
3. 实体销毁回收变换行（原组件 `OnDetach`）—— 漏掉 ⇒ 残行留在列表里把索引映射整体串位。

### 8.4 数字重测（本批之后，`ProbeTransformDiagnostics`）

| 指标 | 评审（删组件前） | 现在 | 说明 |
|------|----------------|------|------|
| 单 transform 实体边际内存 | 939.9 B / 9 次分配 | **678.5 B / 7 次分配** | T6/T7/T8 的直接收益：−261 B、−2 次 |
| `sizeof(TransformComponent)` | 336 B | **不存在**（组件已删） | — |
| `sizeof(TransformDataStorage)` | 488 B | **832 B** | 世界级元数据（owners/versions/masks/children/fixed_pixel）的一次性开销 |
| 每行字节 | 189 B | **275 B（Debug 实测）**；探针已改为**由存储自列** 18 条平行数组（`TransformDataStorage::GetPerRowFields`）⇒ 不再手工维护字段清单 | 其中 `children` 32 + `fixed_pixel` 32 是 T8 新增；Release 口径 ≈ 267 B（`std::vector` 24）；见 8.2 的瘦身建议 |
| `sizeof(TransformAccessor)` | 24 B | 24 B | 与评审一致（文档"4 B 零成本"口径需修订） |

---

## 9. 接下来的工作总清单（2026-09-30 整理）

> 口径：每项给"目标 / 落点 / 验收"。**顺序硬约束：T10 必须早于 T11**。T9 与 T10 彼此不阻塞，
> 但都改 `Context.h` / 存储层 ⇒ 建议串行做，避免同文件双写。
>
> **设计总纲以 `doc/future/ULRE_FINAL_TARGET_v2_设计约束.md` 为准**（2026-09-30 定稿，逐条标注
> 已覆盖/需新增/待细化，含三条不可动摇规则：单一真源、预算制、CPU 权威 + GPU 派生视图）。

### 9.0 收尾（本批 84 文件，未提交）

| 项 | 落点 | 验收 |
|---|---|---|
| 提交本批 | —— | ✅ **已由用户本地提交（2026-09-30）**；此后 T9-0.5（commit `034f7486d`）与 T9-1（bbox 连续化，16 文件）也已提交 |
| **前台复验 `RecursiveCube`**（窗口隐藏时引擎整帧跳过，我无法前置窗口） | `RecursiveCube.exe` | 92 个实例散开且各自转动；顺带扫 `RayPicking`/`GizmoUsageExample` |
| ~~修探针"每行字节"统计~~ **✅ 已完成（2026-09-30）** | `TransformDataStorage::GetPerRowFields/PerRowBytes/FindPerRowCountMismatch` + `ProbeTransformDiagnostics.cpp` | 探针改由**存储自列**（18 条平行数组，新增字段只改存储一处）；Debug 实测 **275 B/行**（`children` 32 + `fixed_pixel` 32 单列）；`TestTransformFlatStorage` **Test 9** 钉住不变量"每条平行数组元素数 == 行数"（检查前先结算拓扑，否则 `eval_order` 会误报） |

### 9.1 T9 其余组件 ID 化（阶段二主线）

顺序：**前置检查 → 纯数据先行 → 带状态机最后**（2026-10-01 改版为 **stage A 组成形 → stage B 存储形**，见下方改版说明）

0. **前置检查（强制）**：删组件类前，逐条对照 `git show HEAD:<被删文件>` 里的每个 `ctx->` / `owner_context->` / `storage->` 调用，把"对世界的副作用"全部搬走（T8 血泪：Mobility 换边 / 子表维护 / 销毁回收）。
0.5 **泛化地基（v2 约束 §1/§2，趁只有 Transform 一个消费者时做最省）**：① 类型 → (scope, arena, 行宽) **静态表**；② 句柄 **revision 校验**（泛化现有 `versions`，释放/重分配时 bump；句柄仍 24 B）；③ **CPU 权威 / GPU 派生视图**的存储布局（版本号增量同步，单写者）。
   - **状态：✅ ①②已落地（2026-09-30，commit `034f7486d`）**
     - `inc/hgl/ecs/support/ComponentTypeTable.h`（新）：`ComponentType`(5 类) + `ComponentScope`(Global/World，静态) + 静态表；**表与枚举同序由 `static_assert` 强制，漏登记表项 = 编译错误**（该检查当场抓到"漏 `None=0` 占位"的疏漏）。其余四类的 scope/行宽标注为待定稿（现记 World 作保守默认）。
     - 存储新增 `generations`（行账目第 19 条 ⇒ 探针现报 **279 B/行**）；**世代编码：0 = 死/未分配，正奇数 = 活；复用须 +2 保持奇数（ABA 免疫）**。
     - `TransformAccessor` 建柄时捕获世代（用掉原 4 B 填充 ⇒ **仍 24 B**）；`IsValid()` = 存世 + id 有效 + **世代非零且与行一致** ⇒ 释放后旧柄与"死行上的新柄"都无效。
     - **Test 10「句柄失效契约」**：活柄有效 / 释放即失效 / 死行新柄也失效 / 空柄写入被忽略；**反证已做**（抽掉 `Deallocate` 的世代归零 ⇒ Test 10 精确报"行释放后旧句柄仍报有效（悬垂写入窗口）"、`rc=12`）。
     - 验证：build 0 error/0 warning；三测试 rc=0；门 rc=0；四个示例 0 真 error 行；探针"各数组元素数 == 行数：是"。
     - ⚠ 过程记录：改存储**成员布局**后必须清 `build/**/*.dir/Debug` 再编——陈旧 obj 会造成 TU ABI 错位（本次表现是 `TestCSMIncrementalPass` 段错误 rc=139，清 obj 后消失；用"临时退回旧 `IsValid`"的受控二分排除了悬垂解引用假设）。
     - ⏳ ③ CPU 权威/GPU 派生的**存储布局**尚未动（它跟各组件 ID 化一起做）。
1. `BoundingBoxComponent` → `BoundingBoxDataStorage` 连续化（评审 R5：剔除是热路径；当前 `src/ecs/support/PrimitiveBatchPipeline.cpp:176`、`src/ecs/support/line/LineRenderPipeline.cpp:454`、`src/ecs/systems/tick/LineBoundsUpdateSystem.cpp:37` 全是逐实体 OOP）⇒ `PrimitiveCullSystem` 改连续数组批处理。
   - **状态：✅ 已完成（2026-10-01）**
     - **存储**：`inc/hgl/ecs/support/BoundingBoxDataStorage.h` 重写为世界私有 SoA 行存储（并行 `hgl::ValueArray`）：`local_min/local_max/world_min/world_max`(各 `glm::vec3`) + `world_valid`(uint8) + `owners`(EntityID) + `generations`(uint32) + `entity_rows` 反查表 ⇒ **每行 77 B**（旧的 `std::vector<vec3>` 实现是 64 B，但**没有世代/owner 反查**，且句柄走 free list 复用 ⇒ ABA 隐患）。
     - **句柄**：新增 `BoundingBoxAccessor`（值类型，`static_assert(sizeof==24)`），建柄捕获世代、`IsValid()` 三重判据 ⇒ 与 Transform 完全同范式。
     - **世界接入**：`Context.h/.cpp` 加 `bounding_box_storage` 与 `GetBoundingBoxByEntity/GetOrCreateBoundingBox/DestroyBoundingBox`；`DestroyEntity` 追加行回收（T8 教训：销毁必回收行）。
     - **删组件**：`BoundingBoxComponent.{h,cpp}` 已 `git rm`（含它的**全局静态共享存储** `sharedStorage` —— 违反 v2 §1 世界私有/单一真源）；`GetSerializationType()`（零调用者）与 `IsDirty/ClearDirty/dirtyFlags`（零消费者）一并删除。全仓 `grep BoundingBoxComponent inc src example` = **0**。
     - **调用点**：`PrimitiveBatchPipeline` 两个函数签名改为传数据（`const math::AABB&` / `const glm::vec3& ×2`），`LineRenderPipeline`、`LineBoundsUpdateSystem` 改 accessor；无兼容层。
     - **⚠ 行布局硬教训**：行内**不得**存 `math::AABB` —— 本仓全局 `GLM_FORCE_DEFAULT_ALIGNED_GENTYPES` 下 `sizeof(glm::vec3)=16` 且 AABB 自带 352 B 派生缓存（6 face center + 6 plane）⇒ 实测每行 **717 B**（比旧实现大 10 倍）；改存 min/max 后 **77 B（≈9.3× 缩减）**。另：不要为"让 `ValueArray<重型类型>` 能实例化"而给第三方类型打全局 `operator==` 补丁（ODR 风险）——改行布局即可消除该需求。
     - **验证**：清除整棵 obj 树重编；build 0 error（仅 4 条既有 `src/InlineGeometry` C4715）；`TestBoundingBoxStorage`/`TestTransformFlatStorage`/`TestRenderItemDataStorage`/`TestCSMIncrementalPass` 全 rc=0；门 rc=0；Test 4 实测每行 77 B / 7 条平行数组。
     - **反证（含防假绿）**：抽掉 `Deallocate` 的 `generations[id] = 0;` ⇒ 清该目标 `.dir` 重编（obj mtime 00:18:57 晚于头 00:18:34）⇒ `rc=1`、文案"释放后世代未归 0（行未失效）"；恢复后重编全绿。**第一次反证曾是假绿**（MSBuild 未重编该 TU，跑的旧二进制全 Passed）——本仓头文件依赖跟踪不可靠，见 §9 教训。
2. `VisibilityComponent`（最接近纯数据）。
   - **状态：✅ 已完成（2026-10-01）**
     - **真值唯一化**：`VisibilityDataStorage` 收编为**世界私有**成员（`ECSContext::visibility_storage`，构造体里 `SetContext(this)`）；删掉组件里的第二份 `bool visible`（原来组件与存储双写 ⇒ 违反 v2 §1 单一真源）。
     - **删组件 + 删接线系统**：`VisibilityComponent.{h,cpp}` 与 `VisibilitySystem.{h,cpp}` 已 `git rm`（该系统唯一职责就是"把存储指针注入组件"）；同时删掉 `Context.cpp` 里的自动注册块与 CMake 组。全仓 `grep VisibilityComponent|VisibilitySystem inc src example` **仅剩注释**，零代码引用。
     - **API**：`GetVisibilityStorage()` / `SetEntityVisible(id,bool)` / `IsEntityVisible(id)`（O(1) 直查不可见集合 + 变换父链上溯）；`DestroyEntity` 追加可见性回收（T8 同类副作用）。
     - **调用点**：`RenderPrimitiveCollectSystem` 不再经 `GetSystem<VisibilitySystem>()->GetStorage()` 取存储，改为直取世界存储；`LineRenderPipeline` 改 `context_->IsEntityVisible()`（**行为统一**：此前查组件的"直接"标志，现与 Primitive 路径一致地走祖先继承）；`GizmoUnified` 改 `world->SetEntityVisible(root,...)`。
     - **保留项**：`GizmoECS::root_visible` 保留 —— 它是 gizmo 的**本地门控标志**，被 `GizmoUnified.AssetCore.inl:47-49`、`AssetUpdate.inl:86` 与 `modes/{Move,Rotate,Scale}GizmoMode.Input.inl:52` 读取（与"世界可见性真值"是两个用途）。
     - **测试**：新增 `src/ecs/support/TestVisibilityStorage.cpp` + CMake target（3 项：默认可见 / 祖先不可见⇒后代不可见且后代非"直接"不可见 / 销毁回收）。**反证**：抽掉 `DestroyEntity` 里的回收 ⇒ `rc=12`、文案"实体销毁后不可见标记未回收（同索引新实体会继承旧状态）"（obj mtime 00:45:09 > 头 00:44:49）。
     - **验证**：清除整棵 obj 树重编；build 0 error（非 C4715 警告 0）；`TestVisibilityStorage`/`TestBoundingBoxStorage`/`TestTransformFlatStorage`/`TestRenderItemDataStorage`/`TestCSMIncrementalPass` 全 rc=0；门 rc=0；`GizmoUsageExample`/`LineRenderTest`/`RecursiveCube` 各 8 秒 0 真 error 行且按时被 taskkill。
     - **⚠ 教训**：判定"某字段无读者"必须**扫整个目录、含 `.inl` 分片**（本仓 gizmo 把实现放在 `GizmoUnified.AssetCore.inl` / `AssetUpdate.inl` / `modes/*.Input.inl`）——只 grep `.cpp` + 头文件会误判为死字段，删掉即 C2039。
> **顺序改版（用户拍板 2026-10-01）**：原来"逐个组件直接 ID 化"改为 **stage A（组成形）→ stage B（存储形）** 两阶段，
> 设计细节见 `doc/future/ULRE_FINAL_TARGET_v2_设计约束.md` **§9**（五条护栏 P1–P5、材质三层共享语义与 CoW、中间态纪律）。
> 理由：T8 的代价证明"责任迁移"与"存储搬迁"必须分开——混在一起时，"对世界的副作用"丢失会伪装成渲染 bug。
> **硬纪律（写进每一步验收）**：① 每步独立可运行（build 0 error → 固定验证集 → 示例抽跑）；
> ② 每步**同批删掉被取代的旧代码**，`grep` 零残留为验收项（不许"新的有了旧的还在"）；③ **禁止 if 特例/新旧并存分支**，语义收敛一律走判定表；
> ④ 做不到"一步内新旧都跑得通" ⇒ 说明该步太大，继续切分（而不是加兼容分支）；⑤ 行为等价的重排以**同输入同结果**（T5 口径）验收。
- **stage A 步骤（每步一批，独立可运行）**：
  A0 **地基（纯新增、零行为变化）**：类型表 scope 定稿（`Geometry`/`Texture`/`MaterialData` ⇒ Global；`Transform`/`MaterialRuntime` ⇒ World）+ `implies` 规则表（如 `MaterialRuntime ⇒ MaterialData`）+ **Entity 组件类型位掩码**（单一写者 = 挂载/卸载；顺手把 §9.5 的"组启停双写者"收敛为"仅 gather"）+ 不变量测试。
  A1 **策略判定表**（`组件集合 → pass/收集器需求`）：先**只读不驱动**，与现有判据**对拍同值**；此后每步把旧 if 链改读表并删掉对应段落。
  A2 **材质 Data 层**：新增 `MaterialData`（资源级；按 (definition, 参数指纹) 去重）；把 `PrimitiveComponent` 的 5 项 authoring 状态（`hasMaterialRecipeOverride`/`materialRecipeOverride`/`namedMaterialTextureResources`/`materialDataResource`/`material_authored_generation`）搬进去 ⇒ **搬完即删旧字段**。
  A3 **材质 Variant 层**：新增 `MaterialVariant`（解析结果缓存）；**键只放"静态且取值有界"的维度** —— `PassType`（**仓里已含 `ForwardDither`/`ForwardA2C`，dither 直接复用既有维度，不新造 feature**）、`MaterialRecipe::compile_defines`（**必须归一化+哈希成有界键**：去序、去重、trim——现在是 `vector<string>` 无界）、quality、未来的材质 LOD 档；**每帧动态的东西一律不许进键**（见 A4 的选择器）。把 `MaterialComponent` 的 `shadow_program`/`shadow_program_build_context_hash`/`shadow_tracked_material_authored_generation`/`shadow_retry_frames` 收敛成"变体表里的一项"（先 N=2，行为不变）。
  A4 **材质 Runtime 层（共享行 + 每实例选择器 + 持久差异才 CoW）**：`MaterialRuntime` 支持**多实体引用同一行**（interned）；行里带**每实例选择器**（当前 pass / LOD 档 / 是否 dither 等**每帧可变、基数小**的状态 ⇒ 零解析成本地切，**禁止每帧改共享行、禁止每帧 CoW**）；**只有持久差异**（改了参数/纹理覆盖/自有 SSBO 行）才 CoW 出自有行；共享/独占行数可观测；释放走 refcount，**行号复用按 §2 世代约定 `+2 保持奇数`** ⇒ **删 `MaterialComponent`**。（三层是 **1:N:N 扇出**，不是恒等链：同 Data 可出多变体、同变体可对多运行时——详见 v2 §9.3）
  A5 **Primitive 拆分**：`Geometry`（引用资源行 + draw range + 变体索引）/ `MaterialBinding`（→Data/Variant/Runtime）/ `ShadowProxy`（现 `ShadowComponent` 正名）/ LOD 钩子；同时删两处 OOP 缓存（`cached_shadow_component`、`PrimitiveComponent.cpp:643` 的 `bound_render_item_storage`）⇒ **删 `PrimitiveComponent`/`RenderableComponent`/`InstancedPrimitiveComponent`**。
   - **★ 追加靶子（2026-10-01 侦察发现）**：现在"可见性"语义有**三份真值** —— ① 实体级 `VisibilityDataStorage`（读侧 `RenderPrimitiveCollectSystem.cpp:1348/:1458`、`LineRenderPipeline.cpp:441`）；② `RenderableComponent::visible`（`PrimitiveComponent` 继承；读侧同文件 `:1344/:1446/:1448`、`PrimitiveComponent.cpp`、`GizmoUnified.AssetVisual.inl`、示例）；③ `LinesComponent::visible`（读侧 `LineRenderPipeline.cpp:431`）。渲染剔除实际是 **① OR ②** / **③ OR ①**。本项要**收敛成一份真值**：实体级 `visible` 作为唯一可见性真值，组件侧只保留"能力/可渲染性"语义（如 `CanRender()`、几何有效性），同义字段删除或改名，不留双写。另需确认**阴影收集链**是否也应查可见性（当前未见）。
  A6 **`CameraComponent`**（独立；注意深层虚继承）。
  A7 **stage A 收口**：全仓 `grep` 零残留（doc 除外）+ 策略判定表成为唯一判据（旧 if 链零残留）+ 记录"stage B 仍欠什么"（行/ID/`EntityGPU`/预算/T10-T11）。
- **stage B（存储形）**：按 (scope, 类型) 行 arena + 访问器 + `EntityGPU` 128B + 预算冻结（T10/T11）；**纯机械替换 + 对拍同值**。

- 验收（本阶段统一口径，取代原来的"每组件一批"）：见上方**硬纪律**五条；每批跑 §5 固定验证集与 §9.7 口径。

### 9.2 T10 容量与行号冻结（T11 的强前置）

1. `TransformDataStorage::Initialize(max_statics, max_dynamics)` 预分配 + 分区 `[0, static_count)`；`Allocate()` 超限 **fail-fast**（与"材质行 arena 1024 上限不扩容"同一口径）。**v2 约束下更准确的说法**：**按 (scope, 类型) 各自的 arena 声明式预算** + 预分配；超限给**一次性明确告警**（"预算没调够 ⇒ 行号已移动 ⇒ 固化 ID/存档失效"）；**不引入 Editor/Release 分支**（Editor 例外永远开着）。
2. 去掉 `TransformAssignmentBuffer` 的 `"L2W recreated"` 路径（`src/ecs/support/TransformAssignmentBuffer.cpp:511`）。
3. **行字节瘦身**：`fixed_pixel`（32 B/行）与 `children`（24–32 B/行，绝大多数为空）改**稀疏侧表**（fixed-pixel 只有 gizmo 行需要；子表用 CSR：`child_offset/child_count` + 共享 child pool）⇒ 每行从 ≈267 B 回到 ≈211 B（只留 owners/change_masks/versions/两标志）。
4. 探针与 3 同步复核行字节（口径一致后再冻结）。

- 验收：现有示例在固定容量下正常；构造超限场景验证 fail-fast 报错明确；日志不再出现 L2W 重建。

### 9.3 T11 `EntityGPU`（128 B）+ `.ulrescene` 直载（阶段三）——**已延后（用户拍板 2026-09-30）**

> **顺序变更**：场景直载推迟到**全部 Component ID 化 + 访问器完成**、以及**子场景树的快速插入/展开**等问题解决之后再谈。
> 也就是说 T9 之后先做子场景树（插入/展开/局部重排）相关的设计，T11 顺位往后；本节的实现要点先原样保留备查。

`inc/hgl/ecs/core/Entity.h` 重写为 GPU 派生视图 **`EntityGPU`：`flags(4) + type[16](16) + row[16](64) + work_flags(4) = 88 B` ⇒ `alignas(64)` ⇒ 128 B**（16 槽 × (类型 1 B + 行号 4 B)；**`persistent_id` 已删**；旧文档"64 B Entity"作废）；CPU 侧权威为 `EntityRecord`。`SceneHeader` 对齐 + StringPool 外置、离线 Cooker、Windows 侧用 `CreateFileMapping`/一次性 `fread`（**不可** alias 文件页进可写 GPU 缓冲，静态段仍要拷一次）、DMA 一次推 GPU。

- 验收：离线导出工具 + 直载器；场景还原时间与显存直推链路（度量并记录）。

### 9.3b T12 两级展开 + 动画 bank（v2 约束 §4/§5）

1. **离线压平**（Editor/GLTFConvert 产物）：模型内每个 node 的 TRS 预组合成"**相对模型根**"并连续排列 ⇒ 运行时 `World(模型实例) × node_相对` = 永远 2 级；**动画同样压平**，动画剪辑也必须预组合到模型根相对；骨骼动画走独立 skin palette 通道。验收：压平前后世界矩阵**对拍同值**（T5 口径）。
2. **动画 bank**（海量 NPC）：离线把基础动画烘成 `node_count × TRS(48 B)` 的**只读段**（例：30 fps × 2 s × 20 node ≈ 57 KB/动作）；行支持"**引用段**（零拷贝，海量背景 NPC）/ **自有行**（主角、被逻辑改的对象）"两种模式；更新率分档；海量 NPC 可在 compute 里直接从 bank 算 L2W（不落 TRS）。
3. 近期**不做 HLOD/prefab**（留钩子）。

### 9.3c 视口列表（RenderList = 视口）——阴影与多视口同一套

每条视口 = **相机行 + 视口矩形/裁剪 + 收集过滤器（layer / caster 标记）+ 输出目标**；主视图 / 阴影视图 / 分屏 / 离屏 RT 共用。阴影侧配 `ShadowProxyComponent`（"它产生什么阴影"、代理下多子 entity 还是一个）。顺带覆盖 backlog A3（跨 RT pass 链）/ A7（离屏 RT in-flight 槽）的需求。

### 9.4 文档口径修订（跟任务同步做）

- Accessor 目标"4 B 零成本" ⇒ **24 B**（携带世界上下文是多世界安全的必要代价）；Phase 3 内存预算按实测口径改写。
- Phase 2 文档里"`MaterialAccessor` 假设材质是纯数据" ⇒ 按 9.1-5 的分层重写。
- `doc/backlog.md` C.2 的十篇待更新文档，**再加 T8 影响的**：`ecs-layer-architecture-and-frame-flow`、`ecs/transform-data-management`（变换真源/组件退役）、`simple-sphere-ecs-render-chain`、`gpu-driven-4id`。

### 9.5 待拍板 / 延后

| 项 | 状态 |
|---|---|
| `ComputeTransformHierarchy` 示例定位（GPU 层级求值探路 vs 死代码）—— 决定 `local_matrices`/SoA 平行数组去留 | **待拍板**（§7.2） |
| `Context.h`（689 行 / 30+ include）拆 Public/Impl | 延后到 T11 之后（否则返工） |
| `eval_order` 构建改 Kahn | 延后（实测 212 节点 / 8 层，非瓶颈） |
| 组启停双写者（组件挂卸计数 vs scene gather 全量） | T9 会正面撞上（删组件类即删掉计数路径）⇒ 一并收敛为"仅 gather" |
| `DetachAllComponents(bool)` 参数无效 | 顺手（与 9.3 的销毁路径一起） |

### 9.6 仓库既有 backlog（触发条件型，非本线）

- **A 线**：A1 GPU 提交原语（semaphore 链 / per-frame 资源多份化）、A3 RenderGraph 跨 RT pass 链（`Pass::renderTarget` 死字段）、A6 cubemap/CSM/MSAA、A7 离屏 RT in-flight 槽。
- **C 线**：**TexConvCore 链接 `out\Windows_64_Release\TexImage.lib`** —— ✅ **已修（2026-09-30）**。根因是**设计使然**而非笔误（`src/Tools/TexConv/CMakeLists.txt:179-196`：`TexImage.dll` 恒按 Release 构建，Debug 宿主必须链它的 import lib；`image/CMakeLists.txt:25` 也写了"Debug 构建前需先完成一次 Release 构建"），而本树从未做过 Release 构建 ⇒ 每次全量 Debug 必吃一条 LNK1104。处置：① 在 `texconv_link_teximage()` 加 **configure 期明确警告**（缺库时打印路径/原因/修复命令，已受控验证"缺则打印、在则静默"）；② 实跑一次 `--config Release --target TexImage`（rc=0），此后**全量 Debug 构建 EXIT=0（编译错误 0、链接/工具错误 0）**，`TexConv.exe`/`TexConvCore.dll` 正常产出。
- **D 线**：D5/D6 已完成；**D7 = 性能账目 → EnvironmentSystem 拆分 → 4 级联合并**（大组按序）。
- **B 线**：RenderContext 类移除、FreeCameraMode 空实现且为默认值、LineStatsSystem 默认注册、WorkManager 序列空架子、示例干净退出时 LEAK + 间歇 CRT abort（判据看日志内容，**不看退出码**）。

### 9.7 固定验证集（口径重申，脚本见 §5）

build **0 error** → 门 **42 PASS / 0 FAIL** → 三测试 **rc=0** → 示例抽跑 **0 真 error 行** → 行尾/BOM 逐文件保持 → 删类型类任务 `grep` 零残留。

### 9.8 Visibility 演示示例（DEMO）—— 2026-10-01 追加

**动机**：实体级可见性（`VisibilityDataStorage`）目前唯一的消费者是两条渲染收集路径、引擎侧唯一写入者是 gizmo 内部的显示开关 ⇒ **太隐蔽，看不出这机制在干什么**。用一个能肉眼看出效果的演示把它的用途显式化。

**目标**：新建一个 Visibility 功能演示示例 —— **10×10 个球，按时间 + 某种规律开关可见性，形成动画**（功能演示，能看出效果即可）。

**落点**：新建 `example/Basic/VisibilityDemo.cpp`；在 `example/Basic/CMakeLists.txt` 里按既有惯例注册 `CreateProject(VisibilityDemo VisibilityDemo.cpp)`（必要时动顶层 `example/CMakeLists.txt`）。
参照 `example/Basic/PBRSpheres.cpp`（它本身就是 10×10 球体：`Sphere_M<col><row>` + `CreateSphere` + `PrimitiveComponent` + `CreateTransform`）的搭建方式，只保留"球 + 光 + 相机"最少必要部分。

**实现要点（硬要求）**
- **可见性开关一律走实体级新 API**：`world->SetEntityVisible(entity_id, bool)`（或 `IsEntityVisible` 查询）。**不要**用组件级 `PrimitiveComponent/RenderableComponent::SetVisible` —— 那是第二份真值，正是 §9.1-3 要收敛掉的。
- 规律自己选一个看得出效果的：棋盘滚动 `((col + row + (int)(t*speed)) & 1)`、按到中心的距离做**涟漪/波浪**相位、或按行/列扫描。100 个实体逐帧 O(1) 写入，代价可忽略。
- 演示里顺带打印**逐帧可见球数**（便于无窗口环境下做机器判据，不能只靠肉眼）。

**验收**
1. 构建 0 error（清 obj 重编口径同 §9）;
2. 前台跑起来能看出"球按规律成片亮灭/滚动"（**动效必须由用户前台确认** —— 引擎在窗口隐藏时整帧跳过，我这边只能给 build 0 error + 示例跑满 8 秒 0 真 error + 开关计数日志）;
3. 演示同时作为"实体级可见性有真实可见用途"的证据（不再只是 gizmo 的内部开关）。
