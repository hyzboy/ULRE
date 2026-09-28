# 全局地址统一：GlobalAddresses 表接管全局表地址（执行计划）

> **注（2026-09-28）**：本文的 S1–S3 已全部落地（Scene 集整体退场、地址归口 GlobalAddresses）。
> **后续定稿已改为"两张表"**：新增世界表 `WorldAddresses`（相机行 / 渲染项表 / DrawItemID 表 /
> sky / shadow / env 随世界；viewport 留全局表），相机存储下沉世界级 —— 见
> `doc/world-addresses-and-camera-model-plan.md`（含事实订正清单）。本文保留为 S 系列执行记录。
> 另：本文 §3 写的"门基线 38 PASS"当时正确，现在基线为 **39 PASS**（S2d 新增
> `S.global-addresses-struct-parity`）。

> 决策（用户，2026-09-27）：**所有表 BDA 全放 GlobalAddresses 表**，pc_root 只保留
> 「表地址」这类根入口。配套口径：sky 现在"全动态"只是因为要改太阳；正式版不会天天动，
> 不构成反对理由。shadow 同理。viewport 数据可缩成 `uint16[2]`，其余算得出来。

## 1. 实测现状（这决定了可行形状）

### pc_root（`RootAddresses`，**现 80B**：9×uint64 + 2×uint32，**每 MaterialBatch push 一次**）9 个地址 + 相机行号的归属

| 字段 | 真实归属 | 依据 |
|---|---|---|
| `addr_mesh_draw_params`（**现已改名 `addr_batch_mesh_draw_params`**） | **每 DrawBatch** | `inc/hgl/ecs/core/MaterialBatch.h:55`、`src/ecs/support/PrimitiveBatchPipeline.cpp:635` |
| `addr_l2w` | 每批（`l2w_buffer` 覆盖）或每世界（`TransformAssignmentBuffer`） | `MaterialBatch.h:76`、`PipelineMaterialRenderer.cpp:160-171` |
| `addr_l2w_index` | **每 DrawBatch** | `MaterialBatch.h:60` |
| `addr_mtl_data_addrs` | **每 DrawBatch** | `MaterialBatch.h:65` |
| `addr_texture_references` | **每材质** | `MaterialBatch.h:67`（`texture_reference_base_addr`） |
| `addr_text_char_info/style/instance` | 每字体（文本路径自己 push） | `src/ecs/support/text/TextRenderPipeline.cpp:283-291` |

### GlobalAddresses（S1 前实测快照：当时是 64B UBO、Set 0 binding 2）8 个字段

> 下面这段是**改造前的实测**，是「可行形状」的推导依据，不是当前状态：S1 已把本表改成
> SSBO（地址经 `pc_root.addr_global_addresses`），S2 已把表按帧槽分份（8×128B）并把
> sky/shadow 地址收进表、两绑定退出 Set 0。

全部是**全局 / 长期有效**：三个材质私有池、render_items、draw_item_ids、camera_info、
mesh_draw_params（与 pc_root **重复**）、color_palette（本会话已 BDA 化）。
出处：`inc/hgl/graph/ssbo/SSBOTypes.h:12`、`inc/hgl/graph/ubo/GlobalAddresses.h:17-29`。

### 结论（对"全放一张表"的一个修正）

「一张表装下所有地址」对 **每批 / 每材质 / 每字体** 这 5+3 个值不成立——同一帧内它们
逐批不同，一张标量表装不下，除非给表再加一个「批号」索引通道（等于把 push 换成
索引+表内跳转，不省反增）。因此可达终态是：

- **表（GlobalAddresses）**：接管**全部全局地址**——现 8 个字段 + sky / viewport / shadow 的地址；
  因 sky/viewport/shadow 是每帧变化，表按 `HGL_FRAME_SLOT_TOTAL` 多份（每帧写本槽）。
- **pc_root**：`addr_global_addresses`（本帧槽表基址）+ 每批/每材质/每字体地址 + `camera_id`
  ⇒ 9×uint64 + 2×uint32 = **80B**（≤128B 硬顶）。pc_root 从「8 张表地址」变成
  「1 张表地址 + 本批地址」。
- **GLSL**：`#define global_addresses GlobalAddressesRef(pc_root.addr_global_addresses)`；
  sky/viewport/shadow 宏改 `XxxRef(global_addresses.addr_xxx)`。宏名不变 ⇒ shader 正文与
  生成侧发射字符串零改动（与调色板同一手法）。
- **Set 0 归零**：`SCENE_SET` / `VKGlobalSceneUBOSet` / push descriptor 路径 / 目录行 /
  语义 / 绑定枚举 / 生成物一并删。

> 若以后要让 pc_root 连每批地址也不背（pc_root → 16B），唯一办法是把每批 buffer 并进
> **一张全局 arena** 并给每批一个「行基址偏移」通道（偏移仍需 per-batch 下发，只是从 8B
> 降到 4B）。这是另一场深改（行结构 ABI + 生成侧 + ICB 携带），不在本计划内。

## 2. 任务（每步自身可编译 + 测试绿）

| 步 | 内容 | 判据 |
|---|---|---|
| **S1 ✅ 已完成** | `GlobalAddresses` UBO → **SSBO**（`CreateSSBO` + `GetBufferDeviceAddressAligned16` fail-fast），pc_root 增 `addr_global_addresses`（72B→**80B**），GLSL 宏改经 pc_root 读表；Set 0 该 binding / `SBS_GlobalAddresses` / 语义 / 目录行 / 宏表 / 生成物同批清；`RenderSceneUBOSystem` 的 `ResolveGlobalAddressesUBO` 改为 `SyncGlobalAddressesTable`（只同步表内会变的两个字段） | 实测：build rc=0 / 0 errors；门 **38 PASS / 0 FAIL**（golden 无变化——SceneGlobal 行不入 golden）；TestCSMIncrementalPass 21 Passed；TestRenderItemDataStorage rc=0；LineRenderTest 0 校验层消息且 `ColorPaletteSSBO addr=0x308080000`、无 `addr_global_addresses=0` 警告 |
| **S2 ✅ 已完成** | 地址表按 `HGL_FRAME_SLOT_TOTAL` 多份（每帧只写本槽）；**sky / viewport / shadow 的地址全部进表**（S2a/S2b/S2c/S2d）且三者均已退出 Set 0（S2e + S2d）——**Set 0 现已无任何绑定项** | 全绿：门 **39 PASS/0 FAIL**（含新增 `S.global-addresses-struct-parity`）；`TestRenderItemDataStorage` 五 Stage；`TestCSMIncrementalPass` 21 Passed；ATS 填充率 57.6% / selfcheck PASS / 0 校验层 / 0 设备丢失；CSM 不一致=0 / 0 校验层 / 0 设备丢失；LineRenderTest、DrawMultiLineText 0 校验层 / 0 设备丢失 |
| **S3 ✅ 已完成** | Scene 集**整体退场**：`SCENE_SET` / `VKGlobalSceneUBOSet` / push descriptor 路径 / `SceneBinding` 枚举 / 资源目录 / `SBS_*` / 三个语义枚举项 / `ubo_requirements` 字段 / 生成物里的 SCENE_SET 行全删；`DescriptorSetType` 收敛为唯一集合 **`Bindless=0`**；删面 **44 文件 +132/−1285**；`scene_ubo.glsl` 改为**无条件包含**（原先挂在会被清空的 UBO 需求集上——见 §2.5.1 第 3 条） | 全绿：门 **39 PASS / 0 FAIL**；RIDS 五 Stage；TCSM 21 Passed；ATS 填充率 57.6% / selfcheck PASS（`receive_shadow` 18189 px、`bias_multiplier` 600662 px，**与 S2d 基线逐位一致**）/ 0 校验层 / 0 设备丢失 / 材料编译失败 0；CSM 不一致=0 / 0 校验层；Line、DrawMultiLineText 0 校验层 / 0 设备丢失 |
| S4 | viewport 收敛（**待用户拍板**）：(a) 地址进 pc_root（每 pass push）或 (b) 缩成 `uint16[2]×2` 进 pc_root 当数据、`ortho_matrix` 在 shader 里算 | 文本/线/常规示例 0 校验层消息；golden 无资源行变化 |
| S5 | 收尾：陈旧注释（"7 张表/56B"、"Set 0 binding 4"）、门夹具、`doc/` 与技能同步 | 全仓 grep 零残留 |

## 2.1 S1 期间的坑（已归档到技能 `ulre-ssbo-vertex-input/references/descriptor-binding-retirement.md`）

- `DescriptorMacroGen --emit` **输出到 stdout**：`--emit > 产物路径` 才是重新生成；`--verify` **必须带路径参数**。
- 生成器 TU 在 `src/Tools/`：改 `inc/hgl/common/DescriptorSetTypeDef.h` 后 purge 的 src 列表必须含
  `src/Tools`，否则生成器用旧头 emit 出旧宏表，而构建/verify 全绿（产物与真源分叉）。
- `RootAddresses` 的布局断言已改成**按 X 列表自动推导**（字段 offset == 前面字段大小之和、
  总大小 == 各字段之和）——手列下标会在加字段时静默过期。

## 2.2 S2 实测（sky / viewport / shadow 全部落地）

| 子步 | 内容 | 实测 |
|---|---|---|
| a | 表切 8 槽（槽步长 128B，`GlobalAddresses` 64→88B，`static_assert` 锁「≤ 槽步长 + 16B 对齐」）；`GraphicsContext::GetGlobalAddressesAddress(帧槽)`；三个 push 站点按 `GlobalAddressesSlotFromCameraRow(camera_row)` 取**本帧槽**表地址（行号 = camera_id×槽总数 + 槽） | build 0 errors；门 **38 PASS/0 FAIL**；`TestRenderItemDataStorage` rc=0；`TestCSMIncrementalPass` 21 Passed；ATS selfcheck PASS 填充率 57.6%；CSM 不一致=0 / 0 校验层 |
| b | sky 地址进表：`MaterializeSkyUBO` 改 `CreateSSBO`（BDA 前提 = SHADER_DEVICE_ADDRESS usage）；`SyncGlobalAddressesTable` 每帧幂等写 `SetSkyAddress(addr)`（单份 buffer ⇒ 写满全槽）；GLSL `SkyInfoRef` + `#define sky SkyInfoRef(global_addresses.addr_sky)`；停更 sky 绑定 | 同上全绿；SPIR-V 缓存 5 个 stage 含 `addr_sky` ⇒ 新路径确在生效 |
| c | shadow 地址**按帧槽**进表：ring 改 `CreateSSBO`，`ring[i]` → 第 i 槽（新增 `static_assert(kShadowUboRing == HGL_FRAME_SLOT_TOTAL)`）；GLSL `ShadowInfoRef` + 宏；停更 shadow 绑定 | 同上全绿（ATS selfcheck PASS 57.6%、CommitRow 拒绝 0；CSM 两轮 不一致=0 / 0 校验层） |
| d | viewport 地址进表（用户拍板：**走 GlobalAddresses**）：viewport UBO 改 `CreateSSBO`，在 `EnsureViewportUBO` **物化处**注册地址（fail-fast + `viewport addr=0x… 入表` 日志）；**地址全帧槽同址**（单份 buffer、内容按 pass/RT 覆盖写、地址恒定）；GLSL `ViewportInfoRef` + `#define viewport …`（宏名 / 成员名不变 ⇒ `viewport.*` 读点零改动）；停更 viewport 绑定；11 个材质 TOML 去 `ubos=["ViewportInfo"]`；门夹具与 4 个 golden 同步（definition 已无 UBO 需求 ⇒ `resource_count` 1→0） | 全绿：门 **38 PASS/0 FAIL**（golden 更新后）；`TestRenderItemDataStorage` 五 Stage；`TestCSMIncrementalPass` 21 Passed；ATS 57.6% / selfcheck PASS（`receive_shadow` 18189 px、`bias×1000` 600662 px，与基线逐位一致）/ 0 校验层 / 0 设备丢失；CSM 不一致=0 / 0 校验层；Line、Text 0 校验层 / 0 设备丢失 |
| e | 两绑定退出 Set 0：`SceneBinding` 只剩 `Viewport=0`（ABI 断言/别名/宏表同步）、目录删 SkyInfo/ShadowInfo 行、`kBindingCount=1`、两处 layout builder 与 init 日志同步；**并删掉「能力子集授权规则表」**（唯一行 SkyInfo 的 UBO 需求规则随 sky 退出而失效，`CapabilityRulesMatchCatalog` 交叉断言一并删）——只留「无条件内置」授权。同时清**声明侧**：4 个 `*.material.toml` 的 `ubos`、`UBOShaderSources.h` 的 `SBS_SkyInfo/SBS_ShadowInfo`、builder 的 `PushSky/PushShadow/switch` 分支与只服务它的 `BuildDescriptorOptions`；门夹具（`S.material-definition-file-schema` 的 `ubo_requirements.size() == 1` 与内联 TOML）与 `SD.structure-dump-golden-pilot` 的 golden 同步 | build 0 errors；门 **38 PASS/0 FAIL**（用例数不变）；`TestRenderItemDataStorage` 五 Stage 全过；`TestCSMIncrementalPass` 21 Passed；ATS 填充率 57.6% / selfcheck PASS / CommitRow 拒绝 0 / 0 校验层；CSM 不一致=0 / 0 校验层；LineRenderTest、DrawMultiLineText 0 校验层；`DescriptorMacroGen --verify` OK |

**更正（S2d 实测推翻初判）**：先前判「viewport 属每 pass / 每 RT 各一份 UBO ⇒ 一帧内同槽会有多个
不同地址 ⇒ 塞不进按帧槽分份的表」，**错**——实测 `RenderSceneUBOSystem::viewport_ubo` 只有**一份**
buffer（`CommitViewportUBO()` 仅把当前 RT 的 w/h 覆盖写进去）⇒ **地址恒定**，与 sky 同形，直接进表；
变的只是内容，更新路径原样不动。故不需要 S4 的 (a)/(b) 两条收敛路。教训：判「某数据是每 pass 变化的」
之前先读它的**持有方式**（单份 buffer + 覆盖写 ≠ 每 pass 一个地址）。

### 2.3 S2 期间定下的三条硬规矩（写进技能 `ulre-ssbo-vertex-input/references/descriptor-binding-retirement.md`）

- **表内地址必须在「buffer 物化处」注册，不能只放「每帧同步函数」里**。把 sky 地址只写在
  `SyncGlobalAddressesTable()` 里时，某条路径没跑到 ⇒ 表里 `addr_sky` 恒 0 ⇒ 读 sky 的 shader
  解引用 0 基址 ⇒ `vkQueueSubmit2 failed with result -4`（**VK_ERROR_DEVICE_LOST**），而校验层
  只报次生错误（fence in use / semaphore 已 signal）看不到真因。现改为在 `MaterializeSkyUBO`
  （地址唯一会变的地方）注册 + 取不到地址即 fail-fast + 打一行 `sky addr=0x… 入表` 日志。
- **删「授权规则表」必须同批清声明侧**。能力规则表删掉后，材质定义 TOML 仍写
  `ubos = ["ViewportInfo","SkyInfo"]` ⇒ 授权失败 ⇒ 材质编译被拒 ⇒ 深度图全空 / 画面空，
  而**静态 schema 门当时 37/38 全绿**，抓不到。⇒ 判据里必须含**渲染型**门（ATS/CSM），
  且每次都要单独 grep `result -4`（不能只看 `[Validation]` 计数）。

- **C++ 表结构与 GLSL 侧结构必须同改**（顺序 / 数量 / 名字全一致）。S2d 只给 C++ 加了
  `addr_viewport`、GLSL `GlobalAddressesRef` 漏加 ⇒ 材质 shader 编译失败 ⇒ **静默回退默认材质**：
  画面照常有内容（639 色、有几何）、ATS 的 D1 深度门 57.6% 照常 PASS、0 校验层消息、CSM 不一致=0，
  唯一破口是 D3 颜色契约门（开关前后 0 px 变化）⇒ 极易误判成「颜色读回坏了」（我确实先这么判了）。
  ⇒ 已加门用例 `S.global-addresses-struct-parity`（源级对表，反向验证过会 FAIL）；遇到「渲染契约门
  莫名 0 px」时先查**材质是否被静默回退**，再怀疑读回通道。

### 2.4 验证环境纪律（本轮踩到，代价最大）

- `taskkill //F //IM x.exe` 在本环境（MSYS 路径转换关闭）被**原样**传给 taskkill ⇒
  `ERROR: Invalid argument/option - '//F'`；再被 `2>/dev/null` 吞掉就成了完全静默的空操作。
  后果：4 个 GUI 进程残留 ⇒ 链接 `LNK1168`、两个 Vulkan 应用互相当对方是「设备丢失」⇒ 假回归。
  正确写法 **单斜杠** `taskkill /F /IM x.exe`，构建前核验 `tasklist | grep -c` 归零。
- `timeout -k 5 <秒> <exe>` 只杀 bash 侧子壳，GUI 进程会活下来 ⇒ 每次运行后**立即再 taskkill**。
- 被强杀的构建会留下半写 `.lib` ⇒ `LNK1236: corrupt or invalid COFF sections`；删掉该 lib 与
  对应 obj 重编即可（不是代码问题）。

## 2.5 S3 实测（Scene 集整体退场）

删除面分 8 类（每类漏一处即编译或断言失败）：真源绑定枚举 + 集合宏表行；集合枚举收敛 ⇒
`Bindless` 由 1 变 0（连带 `dsl[]` 下标、`vkCmdBindDescriptorSets` 的 set 号——全走枚举名就不会漏）；
生成物重发射；资源目录 / `SBS_*` / 三个语义枚举项与它们的**全部**消费方；`ubo_requirements`
字段与其门内比较（随集退场的字段不留空壳）；运行时（集类 + `GraphicsContext` 成员/创建/销毁 +
设备侧占位 layout + push descriptor 的函数指针与设备特性 + 三条渲染路径 include）；门夹具与 golden。

**收益**：资源目录删空后「能力子集授权」失去真源 ⇒ 校验退化成一条**硬不变量**——
`definition` 声明的描述符必须为空（TOML 的 `ubos` 键已删、viewport/sky/shadow 走 BDA、
纹理/采样器走 bindless），出现任何一条即回归。

### 2.5.1 这一轮踩到的四个坑（都已写进技能 `ulre-ssbo-vertex-input/references/descriptor-binding-retirement.md`）

1. **生成器构建失败时 `--emit` 会静默用旧 exe**：产物留旧宏表，而 `--verify` 比对的是同一个
   旧 exe 的期望值 ⇒ **通过**（两边都旧就自洽，假绿灯）。判据必须加**产物内容断言**
   （`grep -c 'SCENE_SET' 产物 == 0` 且 `#define BINDLESS_SET 0`），且先看生成器目标 rc 再看 verify。
2. **改了生成器的输出模板后必须先重生成再构建**：否则构建期的 `descriptor_macros_verify`
   自定义步报 `MSB8066` 并**中断构建** ⇒ 该轮 exe 完全没重链。此时随后跑的任何示例都是
   「旧二进制 + 新宏表」（`BINDLESS_SET` 1→0 变了）⇒ 出现 `0 px` 的**假回归**。
   **规矩：见到 `MSB8066` 之后，本轮所有门结果一律作废，先修构建再整体复跑。**
3. **「以需求集驱动公共 include」是定时炸弹**：`scene_ubo.glsl`（pc_root 之后的 BDA 地址表声明 +
   `camera`/`vp`/`viewport`/`shadow` 宏）原先只在 `if (!ubos.IsEmpty())` 时被包含，而那个需求集的
   唯一成员就是 viewport——viewport BDA 化后需求集空 ⇒ include 静默消失 ⇒ `camera.vp` 未定义 ⇒
   材质 shader 编译失败 ⇒ **静默回退默认材质**（画面仍有几何、0 校验层、D3 契约门报 0 px 才暴露）。
   已改为**无条件包含**，并把整条死掉的 `MeshUboResolver` / `resolve_ubos` / 三个空 resolver /
   模式表项一起删净。**公共依赖必须无条件发射，绝不能挂在会变空的需求集合上。**
4. **删语义枚举的连带**：`IsSemanticOptional` 的 case、`GetDefaultStructNameBySemantic`、
   meshgen 的 resolver、builder 里只为剔该语义而存在的 `remove_if` —— 删一个语义会牵动一串
   switch/表，报错点通常在共享头的一行而真因在多处消费方。先全仓 grep 该语义再动手。

## 3. 验证清单（沿用既有门）

1. 构建含 `descriptor_macros_verify`（改真源 → `DescriptorMacroGen --emit` → `--verify`）。
2. `ShaderResourceSchemaRegressionGate`：基线 38 PASS / 0 FAIL（每次改动按预期增删用例）。
3. `TestCSMIncrementalPass` 21 Passed、`TestRenderItemDataStorage` 通过。
4. `ATS_SELFCHECK=1 AlphaTestShadow`：D1 57.6% / D3 18189 px / bias 600662 px / D4 拒绝 0 / selfcheck PASS。
5. `CSM_CACHE_DIFF=1 CSM_AUTOWALK=4 CascadeShadowMap`：不一致=0。
6. `LineRenderTest` / `DrawMultiLineText`：0 校验层消息；调色板地址非 0。
7. 每步都清 `build/cache-hot/shader-cache` 后再验（陈旧产物会冒充结果）。
8. **每个渲染门都单独 grep `result -4`**（VK_ERROR_DEVICE_LOST）：校验层消息数看不出「shader 解引用坏地址」，
   计数为 0 也可能是已经崩了之后的次生状态。地址进表类改动必须同时确认 `sky addr=0x… 入表` 这类日志出现。
9. **跑渲染门前 `tasklist` 核验残留=0、跑完立即 taskkill**（单斜杠 `/F /IM`）；否则跑的是旧 exe，结论全假。
