# 材质数据 Arena+BDA 重构工作计划

日期：2026-09
配套技术方案：[material-ssbo-arena-bda-refactor-plan-2026-09.md](material-ssbo-arena-bda-refactor-plan-2026-09.md)（里程碑 M1-M4、决策 D1-D8、风险 R1-R8）
状态：待执行
基线分支：SharedOneSSBO → 工作分支 `material-arena-bda`

---

## 0. 总览

| 项 | 内容 |
|---|---|
| 工作包 | W0 准备 → W1 基础设施 → W2 双路径切通 → W3 迁移与删除 → W4 收尾 |
| 任务数 | 约 28 个（每任务一 commit，可独立回滚） |
| 工作量 | **13.5-16.5 人日**（W0:0.5 / W1:3-4 / W2:5-6 / W3:4-5 / W4:1） |
| 节奏 | W1 完全 inert 可先行合入主线；W2 起双路径运行时开关隔离；W3.3 一次性原子删除旧路径 |
| 关卡 | W2 验收（新旧路径逐像素一致）不过不进 W3；W3.3 删除提交前要求全部示例走查 + gate 全绿 |

### 本轮规划新增决策（并入决策表）

- **D9 双路径 flag = 运行时开关**：环境变量 `ULRE_MATERIAL_ARENA`（默认 off），
  ResourceDomainManager 初始化时读一次，经全局查询函数（如
  `graph::IsMaterialArenaBDAEnabled()`）供 ShaderGen 发射 / ECS 行表写入 /
  `AllocateArrayAccessor` 后端三处共读。依据仓库惯例：CMake `ULRE_*` option 只管构建面
  （ULRE_ENABLE_QT / ULRE_ECS_DEBUG_API / ULRE_ENABLE_RELEASE_SHELL），
  渲染路径切换一律运行时（先例：`ULRE_SHADER_CACHE_MODE`，
  src/SceneGraph/module/ShaderProgramManager.cpp:210-221）。
- **R1（SPV 缓存失效）降级为验证项**：缓存键已含最终 GLSL 全文哈希
  （`ShaderStageKey.definition_hash`，inc/hgl/mtl/ShaderStageKey.h:14-43；
  加载时 metadata `generated_source_digest` 二次校验，
  src/ShaderGen/compile/ShaderProgramArtifactBuilder.cpp:38-61）——
  GLSL 发射一变，digest 变 → 文件名变 → 旧缓存自然 miss。
  保险动作（W2.3）：在 `GetShaderCompilerProfileHash`
  （inc/hgl/mtl/contract/ShaderGenProfileTargetVersion.h:64-77）哈希输入中加
  `SHADERGEN_CODEGEN_GENERATION` 常量，一次性整体换键。

---

## 1. 任务分解

### W0 准备（0.5 天）

| # | 任务 | 内容 | 验收 |
|---|---|---|---|
| W0.1 | 切工作分支 | 从 SharedOneSSBO 切 `material-arena-bda` | 分支存在 |
| W0.2 | 采集基线 | 旧路径下截图基线：SimpleCube、PBRSpheres、TextureBlinnPhongMeshes、AutoMergeMaterialInstance、SingleSphereMaterialSwitch、Environment 组（AtmosphereSkyAmbient / BasicLitSunDirection）；存放 `doc/baseline/`（或测试资产目录，执行时定）。同时记录 `ctest -L shadergen` 38 case 全绿状态 | 基线资产入库，gate 基线绿 |

依赖：无。

### W1 基础设施（3-4 天，inert 可先行合入）

| # | 任务 | 内容（锚点） | 验收 |
|---|---|---|---|
| W1.1 | 硬件要求 + 设备特性 | ① `inc/hgl/vk/VKDeviceCreater.h:9-117` VulkanHardwareRequirement 加 `SupportLevel shaderInt64;` / `SupportLevel bufferDeviceAddress;`，ctor 默认 Must；② RequirementCheck（src/Vulkan/VKDeviceCreater.cpp:555-668）加 `VHRC_F10(shaderInt64);` 与 `VHRC(bufferDeviceAddress, physical_device->GetFeatures12().bufferDeviceAddress);`（features12 在 :627 已取得）；③ SetDeviceFeatures（:86-105）加 `REQURE_FEATURE_COPY(shaderInt64);`；④ vk12 特性块（:247-271）加 `vk12_features.bufferDeviceAddress = dev12.bufferDeviceAddress;` | 现有示例在支持设备上行为不变；caps 输出确认两特性启用 |
| W1.2 | 内存分配 flags + arena buffer 工厂 | ① `VulkanDevice::CreateMemory`（src/Vulkan/VKMemory.cpp:8-26）加可选 `VkMemoryAllocateFlags` 参数，分配前把 `VkMemoryAllocateFlagsInfo{.flags=...}` 链到 alloc_info.pNext（全仓库无先例，此处为唯一注入点；`MemoryAllocateInfo` 包装在 inc/hgl/vk/VKStruct.h:57-61）；② 新 `VulkanDevice::CreateArenaBuffer(name, size)`：usage=`STORAGE\|SHADER_DEVICE_ADDRESS`、**强制 CPUVisible 策略**（必须绕开 `ResolvePolicy` 的 Auto→StagedUpload 分支，src/Vulkan/VKDeviceBuffer.cpp:13-22——staged 路径会破坏持久映射假设）、分配 flags=`VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT`、`vkMapMemory` 持久映射（DeviceMemory::Map 自带缓存语义，VKMemory.cpp:63-77）、`vkGetBufferDeviceAddress` 取址返回 | 单测：arena buffer Map() 返回非空且两次调用指针相同；device address 非 0 |
| W1.3 | MaterialDataArena | 新文件 `inc/hgl/graph/module/MaterialDataArena.h` + `src/SceneGraph/module/MaterialDataArena.cpp`：① `MappedArenaAllocator` 实现 `AbstractMemoryAllocator`（CMCore/inc/hgl/type/MemoryAllocator.h:9-57），`CanRealloc()=false`，`Reserve` 只校验容量不增长，`Get(off)=mapped+off`，`Write=memcpy`；② 持 arena buffer + adapter + `BlockPool`（Init(64MB/16=4M 块, 16)，容量经 env `ULRE_MATERIAL_ARENA_MB` 可配 16-512）；③ block0 memset 0（默认行，不变量 I4）；④ `arena_base` 缓存、`AddressOf(block)=base+block*16`、`GetAllocator<T>()` 懒建 `TypedBlockAllocator<T>`、`AcquireRange<T>(count)`（底层 `BlockPool::Acquire(count*slot_blocks)`，连续语义）；⑤ 注释写入架构约束 I3（块号不得进入跨帧结构） | 单测：`AddressOf(b)==base+b*16`（与 CPU `GetData` 同构）、block0 全零、耗尽 AcquireRange 返回 0、Reserve 拒绝超容量 |
| W1.4 | 分配器守卫 + 补测 | ① `TypedBlockAllocator::Release`（CMCore/inc/hgl/type/TypedBlockAllocator.h:100-107）加 debug 校验：free list 查重 + 块号属于本类型 batch 段；② TypedBlockAllocatorTest.cpp 补三测：完全合并断言（全 ReleaseAll 后 `pool.Acquire(free_count)` 返回 1，覆盖 BlockAllocator.cpp:139-250 四条合并分支）、double-release 守卫、碎片楔死（大 slot_blocks 交错后超大连续 Acquire 干净返回 0） | CMCore 测试全绿（含新三测） |

依赖：W1.1→W1.2→W1.3→W1.4（严格顺序：特性→分配→池→守卫）。
里程碑验收：全部 inert——现有渲染路径零变化，可整包合入主线。

### W2 双路径切通（5-6 天，核心攻关）

| # | 任务 | 内容（锚点） | 验收 |
|---|---|---|---|
| W2.0 | 运行时开关 | `ULRE_MATERIAL_ARENA` env（D9）：ResourceDomainManager 构造时读一次存全局；提供 `graph::IsMaterialArenaBDAEnabled()`；三消费点（W2.2/W2.4/W2.5）共读 | flag off 时一切如旧 |
| W2.1 | 行结构 C++ 真源 | ① `inc/hgl/graph/ssbo/` 新增 `PBRSurfaceRow`(80B/5块)、`EmissiveSurfaceRow`(64B/4块)、`TextureRectArrayRow`(64B/4块)、`TransmissionRow`(16B/1块)：材质字段 + `uint32 tex[10]`（TextureSlot::RANGE_SIZE=10）尾，`static_assert(sizeof(Row)%16==0)`；② `MaterialSSBOLayout.h` 表升级：C++ 行结构 + 对应 GLSL buffer_reference 声明文本（含 `tex_<slot_name>` 字段名生成）+ sizeof 镜像断言 | 编译期断言全过；行布局文档化 |
| W2.2 | ShaderGen flag 后发射 | ① `BuildMaterialSSBODeclarations`（src/ShaderGen/compile/MaterialShaderEmitter.cpp:120-169）重写：发射 `layout(buffer_reference, scalar, buffer_reference_align=16) buffer <Row>` 声明 + `#define MTL_ROW(i) <Row>(mtl_data_addrs.values[(i)])`，不再发射 `readonly buffer…data[]` / `#define MTL_DATA` / set+binding；② `mtl_private_data_index` 声明与 `ResolveMaterialPrivateDataIndex` 删除 → `mtl_data_addrs`（uint64 行表，仅 Fragment 可见）声明（:281-309）；③ `BuildFSIndexTableDecls` 的 `mtl_texture_layer_rows` 注入删除（:323-356）；④ `ShaderBuildContext::AddSSBOMaterialPrivateData*`（src/ShaderGen/compile/ShaderBuildContext.cpp:176-191）→ `AddSSBOMaterialDataAddresses`；⑤ `AddMaterialPrivateDataSlotDescriptor`（src/ShaderGen/compile/MaterialShaderCompiler.cpp:119-135）→ `AddMaterialRowReference`（纯 struct 声明，无描述符）；能力规则表（:192-228）MaterialPrivateData/MaterialTextureLayerTable/MaterialPrivateDataIndex 三行改 flag 条件；⑥ DescriptorContract（src/ShaderGen/contract/DescriptorContract.cpp:73-83）与 `EnsureMaterialPrivateDataIndexTable`（src/ShaderGen/builder/DescriptorBuilderCommon.h:395-410）flag 后剔除相关条目；pipeline layout 不含 set 2；⑦ mesh 模板 varying 直传 item 序号：MeshShaderModeVertexPassthrough.h:74 / LineQuad.h:55-57 / CharQuad.h:67-69；⑧ ShaderBufferSources.h:15-20 新增 `SBS_MaterialDataAddresses{PerObject,"mtl_data_addrs",…}` | flag on：SimpleCube 生成 GLSL 含 buffer_reference/MTL_ROW/mtl_data_addrs，不含 MTL_DATA/mtl_private_data/set2；flag off：GLSL 与基线完全一致 |
| W2.3 | 缓存失效验证 | flag on 首跑：确认 SPV 重新生成（缓存键含 GLSL 哈希，自动 miss）；保险：`GetShaderCompilerProfileHash` 加 `SHADERGEN_CODEGEN_GENERATION` 常量整体换键 | 旧缓存目录存在时 flag on 正常重建 |
| W2.4 | ECS flag 后路径 | ① `EnsureBatchIndexRows` 行宽 4B→8B（src/ecs/support/PrimitiveBatchPipeline.cpp:630-689）；② `WriteBatchIndexRows` 写 `arena->AddressOf(material_comp->data_index_values[0])`（:691-750，0 号哨兵块同样映射 base+0 合法地址）；③ 批 key 删 SSBO 签名（:807-809，flag 后）；④ RDBS：`resolve_recipe_batch_struct_ssbo_id` 删除（src/ecs/systems/render/RenderDescriptorBindingSystem.cpp:618-711）、MaterialPrivateData/MaterialTextureLayerTable 两分支删除（:831-883）、MaterialPrivateDataIndex 分支改绑 mtl_data_addrs（:898-928）、`ensure_batch_mp` 对 set2 退役（:538-570）；⑤ Collect 侧 `MaterializeRecipeRowsForPrimitive` 缩水（src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:771,814-935：删 ssbo_id 解析 / MakeECSSSBOId 兜底 / 纹理行表 binding，纹理句柄改写行尾 `tex[]`）；⑥ `MaterialComponent::resolved_ssbo_bindings` 退役（src/ecs/components/MaterialComponent.cpp:50-78）；⑦ PipelineMaterialRenderer set2 批次覆盖路径删（src/ecs/support/PipelineMaterialRenderer.cpp:384-403） | flag on：PBRSpheres 100 实例数据正确（metallic/roughness 梯度、纹理数组层正确） |
| W2.5 | 访问器后端切换 | `AllocateArrayAccessor`（inc/hgl/graph/module/ResourceDomainManager.h:96-126）flag 后走 `arena->AcquireRange<T>(count)`（不再建独立 DeviceBuffer）；`SSBOArrayAccessor` 行距解耦：`element_stride=slot_blocks*16`，`operator[]=mapped+idx*stride`；`Commit()` 保留为 no-op（API 兼容）；新增 `GetBlockBase()`；`GetSSBOId/GetSSBOBinding` 标 deprecated | flag on：21 示例的 accessor 用法编译通过、数值写入正确 |
| W2.6 | 对拍验收 | flag on/off：SimpleCube + PBRSpheres 截图**逐像素一致**（对比工具用现有截图基线流程）；记录粗测数据（draw call 数、每帧 descriptor bind/update 计数，供 §7 收益核对） | 逐像素一致报告；不一致则修复后重验，**不过不进 W3** |

依赖：W2.0 → W2.1 → W2.2 →（W2.3、W2.4、W2.5 可并行）→ W2.6。
高风险任务 W2.2 / W2.4 各自独立 commit，出问题可二分定位。

### W3 迁移与删除（4-5 天）

| # | 任务 | 内容（锚点） | 验收 |
|---|---|---|---|
| W3.1 | 回归门更新 | src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp：改造 `mtl_private_data` 引用 ~14 处（:568,:3305,:3686,:3760,:3766,:3774,:3795,:3911,:4272,:4317,:4333,:4378,:4723 等）、`MTL_DATA` 2 处（:3760,:3774）、`DescriptorSetType::Material` 6 处（:2475,:3928,:4331,:4376,:4711,:4723）；golden 重生成（src/Tools/ShaderGen/golden/，首跑自动生成 + `.actual` 差异落盘）；`module-invariants` 组只能经全量入口跑，勿漏 | `ctest -L shadergen` 全量 + 7 组 ctest 全绿（38 case） |
| W3.2 | 21 示例迁移 | 逐示例删 `UpsertRecipeSSBOAssetBinding` 调用（函数暂留 no-op），flag on 走查，与 W0 基线对比；重点：TextureBlinnPhongMeshes、AutoMergeMaterialInstance、SingleSphereMaterialSwitch、Environment 组（纹理切换/材质切换路径） | 21 示例全部可视一致 |
| W3.3 | 旧路径原子删除 | 单 commit 删除（技术方案附录 A 清单）：旧发射（MTL_DATA/set2 管线）、`ResolveMaterialPrivateDataIndex`、`SBS_MaterialPrivateDataIndexRows`/`SBS_MaterialTextureLayerRows`、RDBS 旧分支、`resolved_ssbo_bindings`、`ensure_batch_mp` set2、ResourceDomainManager 材质域 EnsureBuffer/copy-on-grow 路径、`UpsertRecipeSSBOAssetBinding`/`RecipeSSBOAssetBinding`、SSBOArrayAccessor deprecated 接口、SSBOTypes.h 材质 ID 命名空间注释 | 全示例重走查 + gate 全绿（删除后） |
| W3.4 | 删 flag | `ULRE_MATERIAL_ARENA` 开关与双路径分支删除，arena 成为唯一路径 | 编译无死代码警告；全示例绿 |

依赖：W3.1/W3.2 可并行 → W3.3 → W3.4。
W3.3 前置门槛：W3.1 gate 全绿 + W3.2 全示例通过。

### W4 收尾（1 天）

| # | 任务 | 内容 |
|---|---|---|
| W4.1 | 文档归档 | 技术方案标记"已实施"并回填实测数据（§7 收益表）；本工作计划标记完成；两文档互链 |
| W4.2 | 遗留标注 | `DescriptorSetType::Material` 枚举 deprecated 注释（值保留，契约兼容）；SSBOTypes.h 命名空间注释更新 |
| W4.3 | 历史整理 | commit rebase/squash 整理为按工作包分组的提交序列；合并回主线 |

---

## 2. 依赖与提交序列（DAG）

```
W0.1 ─ W0.2
W1.1 ─ W1.2 ─ W1.3 ─ W1.4          (inert，可整包先合主线)
W2.0 ─ W2.1 ─ W2.2 ─┬─ W2.3
                     ├─ W2.4 ──────┐
                     └─ W2.5 ──────┤
                                   W2.6 (关卡)
W3.1 ─┐
W3.2 ─┴─ W3.3 ─ W3.4
W4.1 ─ W4.2 ─ W4.3
```

提交粒度：每任务 1 commit；W1.1-W1.4 可合为 4 个连续 commit 一次 PR；W2.2、W2.4 强制独立 commit。

## 3. 回滚点

| 时点 | 回滚手段 |
|---|---|
| W1 期间 | 回退分支（inert 改动无影响面） |
| W2 期间 | `ULRE_MATERIAL_ARENA=0` 立即回旧行为；或回退单个任务 commit |
| W3.3 之前 | 关 flag 回旧路径（双路径共存期至少覆盖一个完整验证周期） |
| W3.3 之后 | 回退该原子 commit（单 commit 设计保证可整体 revert） |

## 4. 验证矩阵（累计）

| 层 | 手段 | 引入时点 |
|---|---|---|
| 分配器 | CMCore 单测（5 组原有 + 3 项补测） | W1.4 |
| Arena | MaterialDataArena 单测（AddressOf/零块/耗尽/Reserve 边界） | W1.3 |
| 设备 | 特性启用日志 + Must 拒绝路径（模拟不支持设备） | W1.1 |
| ShaderGen | RegressionGate 38 case（M2 期间 flag off 断言不变；M3 更新断言 + golden） | W0.2 / W3.1 |
| 端到端 | SimpleCube + PBRSpheres 逐像素对拍 | W2.6 |
| 全量回归 | 21 示例可视走查 vs W0 基线 | W3.2 / W3.3 后复测 |
| 缓存 | 旧缓存目录存在下的重建验证 | W2.3 |

## 5. 风险对照（执行期视角）

| 技术方案风险 | 工作计划落点 |
|---|---|
| R1 SPV 缓存 | 已降级：键含 GLSL 哈希自动失效；W2.3 验证 + generation 常量保险 |
| R2 double-release | W1.4 守卫 + 补测，前置到 inert 阶段 |
| R3 块号跨帧悬空 | W1.3 注释约束 + W2.4 review 检查项（行表为唯一通道） |
| R4 双路径漂移 | W2.6 关卡 + W3 限时收口（W2-W3 连续执行不跨长期分支） |
| R5 无 BDA 设备 | W1.1 Must 拒绝启动并列明缺失特性 |
| R8 C++/GLSL 布局漂移 | W2.1 static_assert + 镜像断言；W3.1 golden |
| （新）staged 策略误选 | W1.2 强制 CPUVisible + 单测断言 Map 稳定 |

## 6. 完成定义（DoD）

- [ ] `ULRE_MATERIAL_ARENA` 路径成为唯一路径（W3.4），旧代码零残留（附录 A 清单核对）；
- [ ] `ctest -L shadergen` 全绿；CMCore 单测全绿；
- [ ] 21 示例与 W0 基线可视一致；
- [ ] 技术方案 §7 收益表回填实测值（描述符绑定次数、RDBS 净删行数、批合并变化）；
- [ ] 两份文档归档互链，合并回主线。
