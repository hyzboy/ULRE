> **注（2026-09-28）**：本文的 T1–T6 计划已由 S1–S3 实际落地（Scene 集整体退场），现状口径见 doc/world-addresses-and-camera-model-plan.md；本文保留为分析记录。

## 先说结论

这份交接文档**任务定性正确、验证清单可信，但它的核心前提（pc_root 容量）是错的**，而且照它的口径做会在迁移到一半时撞上 128B 硬顶。它把 `GlobalAddresses` 的 56B 当成了 `RootAddresses`（pc_root）的大小（真实 **72B**，`inc/hgl/graph/ShaderBufferSources.h:265-323`），于是"加 4~5 个地址 ⇒ 约 96B"的估算是少算了一倍；按真实清单算是 **144B > 128B**。另外它的"读取面 7/4/2/2/2（文件数）"是朴素 grep 的结果，含声明文件、include 路径与同名局部变量的假阳性，不可用于排期。

---

## 1. 已核对的现状事实（全部与出处一致者打 ✓）

| 文档断言 | 实测 | 出处 |
|---|---|---|
| 待退役绑定 sky=0…shadow=4 | ✓ | `inc/hgl/common/DescriptorSetTypeDef.h:12-22` |
| GLSL 声明行 61/75/83/88/112 | ✓ 逐行精确 | `ShaderLibrary/ubo/scene_ubo.glsl` |
| 每帧写入 :400/402/404/406 | ✓ 逐行精确 | `src/ecs/systems/render/RenderSceneUBOSystem.cpp:400-406` |
| 第二写入点 ColorPaletteSystem:143 | ✓（每 GraphicsContext 一次，非每帧） | `ColorPaletteSystem.cpp:141-145` |
| 两处 layout builder + push descriptor | ✓（:34/:132，push 缓冲 :190-255） | `src/Vulkan/VKGlobalSceneUBOSet.cpp` |
| descriptor_macros.glsl 是生成物、禁止手改 | ✓（构建期 ALL 门 + ctest 双保险） | `src/Tools/ShaderGen/CMakeLists.txt:44-59` |
| L2W 范式可照抄 | ✓ | `l2w_ssbo.glsl:20-26`；相机同模式 `scene_ubo.glsl:56/123` |
| 前提 A1/A2（表 buffer 带地址位、16B 对齐 fail-fast） | ✓ | `VKDevice.h:330/378-388`、`DeviceBufferFactory.cpp:61-64` |
| gate 名称/基线机制存在 | ✓ `ShaderResourceSchemaRegressionGate` 目标 + `golden/`（5 个文件 53 行） | `CMakeLists.txt:82-123`、`src/Tools/ShaderGen/golden/` |
| 坑清单（陈旧 shader-cache、purge、MSBuild 不重链、golden CRLF） | ✓ 与既有技能/前一份文档一致 | 见 `doc/camera-ubo-removal-plan.md:41-44,80` |

**失效项**：§0 说起点 HEAD = 删相机 UBO 的提交；实际 HEAD 已是 `a8ea3b321`（其后还有 4 个提交，含写入本文的 `bcc528f80`、帧外读回两条 7f98be858/a8ea3b321）。不影响方案，但"起点状态"要更新。

---

## 2. 三处失真数字（都源自同一批陈旧注释）

| 文档 | 实际 | 来源 |
|---|---|---|
| "pc_root 现 56B" | **72B** = 8×uint64 + 2×uint32，有 `static_assert` 钉死 | `ShaderBufferSources.h:304-323`；56B 其实是 `GlobalAddresses`（`ubo/GlobalAddresses.h:25`） |
| "7 张全局表" | **8**（多了 `addr_texture_references`） | 同一批注释谎报：`RootAddressPush.h:3,5,7`、`VKPipelineLayoutData.cpp:56`、`MeshShaderHeaderGen.h:25` |
| 读取面 7/4/2/2/2 文件 | 见下表 | 朴素 grep 命中声明文件/`#include "sky/…"`/同名局部变量 |

| 宏 | 真读文件数 | 说明 |
|---|---|---|
| sky | **5** | 文档多算了声明文件与 `indirect_sky_ambient.glsl:19` 的 include 路径 |
| shadow | **1** | `shadow/pcf_shadow.glsl`；另 3 个是局部变量/注释（`shadow_receiver_source.glsl:233` 的 `float shadow`） |
| viewport | **3** | 文档漏了 `helpers/scale_fixed_pixels.glsl:22` |
| color_palette | **0** | 唯一提及是注释 `s1_palette_index.glsl:8` |
| global_addresses | **2** ✓ | 另需加上生成侧 `MTL_ROW`（`MaterialShaderEmitter.cpp:291-297`），文档没算 |

顺带两处会诱导误判的注释建议一并订正：`FragmentTemplateComposer.cpp:563` 说 MTL_ROW 读 `global_addresses.addr_pbr_surface`（半对——宏体是"池基址 + `pc_root.addr_mtl_data_addrs` 的 payload_index×stride"）；`RenderSceneUBOSystem.cpp:380`、`GlobalAddresses.h:9` 的绑定号注释仍是旧编号。

---

## 3. 文档漏掉的前置与连带项（每条都会"第一个就炸"）

1. **这 5 个 buffer 现在都取不到设备地址**。`CreateUBO` 的 usage 位是 `0`，地址位只加在 SSBO 上（`VKDevice.h:329-330`），而内存的 `DEVICE_ADDRESS` flag 由 usage 位派生（`DeviceBufferFactory.cpp:61-64`）。BDA 化必须先让它们带 `SHADER_DEVICE_ADDRESS`（改 `CreateUBO` 或改走 `CreateSSBO`）并满足 16B 对齐，否则 `GetBufferDeviceAddressAligned16` 恒返 0 ⇒ shader 拿 0 基址解引用。文档 A1/A2 只提"表 buffer"，没提这条。
2. **`camera` 宏的 include 被 ubos 语义列表卡着**：mesh 头是 `if (!ubos.IsEmpty()) include "ubo/scene_ubo.glsl"`（`MeshShaderHeaderGen.h:86-89`），而 `#define camera …` 就住在这个文件里（`scene_ubo.glsl:123`）。把 ViewportInfo/SkyInfo 语义删空 ⇒ mesh 阶段不再 include ⇒ `camera` 未定义 ⇒ 全材质编译失败。这正是 `camera-ubo-removal-plan.md:34-36` 坑 1 的同一形态，文档 §4.3 完全没提。**Set 0 整体退场必须先把这个 include 从语义列表解耦。**
3. **测试夹具硬编码了该绑定**：`TestRenderItemDataStorage.cpp:207-214`（`sizeof(GlobalAddresses)==56` + 7 个 offsetof）与 `:245/:485/:729`（三份 GLSL 里 `layout(set=SCENE_SET, binding=GLOBAL_ADDRESSES_BINDING) uniform …`）。文档只在验证清单里要求它"通过"，没把它列进同批改动。
4. **0 地址不是"安全默认"，是可炸设备**：sky 在 schema 里是 `required=0 fallback=1`（`golden/lit-forward-opaque.txt:14`），现状"没绑也不读（PARTIALLY_BOUND）"；BDA 化后变成"地址 0 + shader 解引用 = UB/设备丢失"。仓库已有对策范式：`pcf_shadow.glsl:171` 用 `if (pc_root.addr_mtl_data_addrs == uint64_t(0))` 分流。**必须为可选表加显式 0 判定 + 一条契约**，否则故障形态是 GPU 挂死而非数值差异，`0 VUID` 判据抓不到。
5. 其余同批清单按技能 `references/descriptor-binding-retirement.md` 走：toml 名表与语义值表是并行数组、必须成对删（`MaterialDefinitionFile.cpp:856-869`）；目录位图断言 `SceneBindingsFullyCovered`（`DescriptorResourceCatalog.h:137-147`）在 RANGE_SIZE 归零时有边界要处理；两处 layout builder + push descriptor 路径同批清。

---

## 4. 真正的核心：pc_root 容量的真账

现有 72B，硬顶 **128B**（compute 路径显式拒绝 `>128`，`ShaderProgramManager.cpp:466-472`；图形路径直接 `sizeof(RootAddresses)` 无检查，`VKPipelineLayoutData.cpp:62-69`）。\n**全量平铺所需新增槽位：**

| 来源 | 新增槽 | 消费方 |
|---|---|---|
| sky | `addr_sky` | 5 个 GLSL 文件 |
| viewport | `addr_viewport` | 3 个文件 |
| shadow | `addr_shadow` | `pcf_shadow.glsl`；UBO 是 per-frame **ring**（`EnvironmentManager.cpp:108-139`）⇒ 地址每 pass 不同 |
| global_addresses | `addr_pbr_surface` / `addr_emissive_surface` / `addr_transmission_surface` / `addr_global_render_items` / `addr_draw_item_ids` / `addr_camera_info` | `MTL_ROW` 生成宏用 3 个池基址；`RenderItemResolve.glsl:69-70` 用 2 个；`scene_ubo.glsl:123` 用 camera_info（`addr_mesh_draw_params` 与 pc_root 重复，可去重） |

**计 9 槽 = 72B ⇒ 合计 144B，超顶 16B**（palette 若也迁 = 152B）。文档 §4.1 的"退化保留 1 个 UBO"虽然是阀门，但配套的容量前提错了，先把错数修掉再谈。

**我推荐的是第 ① 条（唯一能让 Set 0 整体退场、又留 24B 余量、且 shader/生成侧零改动的做法）：**

- ① **地址表降一级**：pc_root 只加 `addr_sky`/`addr_viewport`/`addr_shadow`/`addr_addresses` 四个槽（32B ⇒ **104B**），把那 6 个池/表地址留在 `GlobalAddresses` 表里，表本身由 UBO 改为 SSBO，GLSL 里写 `#define global_addresses GlobalAddressesRef(pc_root.addr_addresses)`。因为 GLSL 正文与 ShaderGen 生成的 `MTL_ROW` 拼的都是 `global_addresses.addr_*` 字符串（`MaterialShaderEmitter.cpp:293-297`），**正文零改动这条卖点连生成侧一起成立**。代价：那 6 个地址多一级解引用（每帧稳定，代价可忽略）。
- ② 合并三个 1024 行材质私有池（`GlobalSSBOBufferRegistry.cpp:16-18` 现为三个独立池）成一个 arena ⇒ 省 16B，但那是池机制改造，不该混进本批。
- ③ 全平铺到 128B（零余量）：必须同时判死删掉 palette 且此后不再加表。作备用。

另外 §4.4 的"PushRootAddresses 每帧调用成本"问错了方向：调用点只有 3 处（`PipelineMaterialRenderer.cpp:173`、`LineRenderPipeline.cpp:764`、`TextRenderPipeline.cpp:291`），**成本可忽略，真问题是谁来解析"本 pass 的槽"**——shadow ring 的槽要按当前 RT 取（同铁律 2 的 `PublishCameraRows` 模式），且 prepass 与主帧各取自己的槽，不能缓存在 tick 阶段。

---

## 5. 各 UBO 的处置建议

| 绑定 | 建议 | 依据 |
|---|---|---|
| color_palette | **删除，不迁移** | 零读取（`s1_palette_index.glsl:15` 实际读 `draw_params.addr_color`）；写侧 `ColorPaletteSystem.cpp:141-145` + catalog `DescriptorResourceCatalog.h:49` + toml `vertex_palette_color.material.toml:21` + golden 行同批清。动手前按判死三步用一次 SPIR-V 反汇编坐实。 |
| global_addresses | **保留表、换寻址形态** | 字段实际全活（3 个池基址被生成宏用），不适用"删绑定"，只换 set/binding → pc_root 地址 + buffer_reference（方案 ①）。 |
| sky / shadow / viewport | 迁移到 pc_root（sky 需 0 地址分支） | 见上；sky 的 fallback 语义必须显式化。 |

---

## 6. 建议的执行顺序（每步独立可验）

1. **T1 打底**：`CreateUBO`/缓冲创建 + 16B 对齐这条前置打通（拿一个缓冲验证地址非 0，再往下走）；顺带订正 §2 的陈旧注释与本文档错数。
2. **T2 删 palette 死绑定**（走 `descriptor-binding-retirement.md` 18 项清单，含 toml/语义/catalog/golden/夹具）——绑定号整体下移到 0..3。
3. **T3 迁移 shadow**（读点最少，只有 1 个文件，且必须验证 prepass/主帧各取自己 ring 槽）。
4. **T4 迁移 sky（带 0 地址分支 + 契约）→ T5 viewport**（viewport 的每帧多份化建议一并拍板：走"一个 slotted buffer + 槽号"，槽号可以复用 pc_root 里现成的 `_pad_camera` 填充位，不额外吃容量）。
5. **T6 地址表 BDA 化 + Set 0 整体退场**：先解耦 `scene_ubo.glsl` 的 include（§3.2），再删 SCENE_SET/`VKGlobalSceneUBOSet`/push descriptor 路径/`kDescriptorBindingMacros` 相应行，收 `TestRenderItemDataStorage` 夹具，最后按 `diff --strip-trailing-cr` 逐条核对 golden（预期只少 Scene 资源行：`lit-depth-only.txt:8`、`lit-forward-opaque.txt:14-15`、`vertex-palette-color-forward.txt:10-11`）。
6. 每步：清 `shader-cache` → purge → 构建（含 `descriptor_macros_verify`）→ gate → Test/ATS/CSM。

---

## 7. 本次没验证的（诚实交代）

- `TestCSMIncrementalPass` 21 Passed、ATS 的 D1/D3/D4 契约数值、CSM 多轮"不一致=0"：**没复跑**，且 `build/out/Windows_64_Debug/` 下没有这两个 exe，需先构建目标。
- ShaderGen gate 的 39 PASS 也没复跑：`ShaderResourceSchemaRegressionGate.exe` 尚未构建（只在 build 里留了中间产物）。按文档 §0 要求，动工前应跑一次把这几个数坐实。
- `maxPushConstantsSize` 的真机值没实测（`VulkanPhysicalDeviceProfileCollector` 是现成路径）；本仓 compute 门把 128 当硬顶，所以容量结论先按 128 算——与我上面的账一致。

---

需要你拍板三件事，然后我就可以按 T1 开工：**(a)** 走方案 ①（地址表降一级，104B、Set 0 全退场）还是 ③（全平铺 128B、零余量）；**(b)** palette 按死绑定删除（我推荐）还是照迁；**(c)** viewport 的每帧多份化是本次一并做还是留到迁移完成后单独一批。