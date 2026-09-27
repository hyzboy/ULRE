# 全局地址统一：GlobalAddresses 表接管全局表地址（执行计划）

> 决策（用户，2026-09-27）：**所有表 BDA 全放 GlobalAddresses 表**，pc_root 只保留
> 「表地址」这类根入口。配套口径：sky 现在"全动态"只是因为要改太阳；正式版不会天天动，
> 不构成反对理由。shadow 同理。viewport 数据可缩成 `uint16[2]`，其余算得出来。

## 1. 实测现状（这决定了可行形状）

### pc_root（`RootAddresses`，72B，**每 MaterialBatch push 一次**）8 个地址的归属

| 字段 | 真实归属 | 依据 |
|---|---|---|
| `addr_mesh_draw_params` | **每 DrawBatch** | `inc/hgl/ecs/core/MaterialBatch.h:55`、`src/ecs/support/PrimitiveBatchPipeline.cpp:635` |
| `addr_l2w` | 每批（`l2w_buffer` 覆盖）或每世界（`TransformAssignmentBuffer`） | `MaterialBatch.h:76`、`PipelineMaterialRenderer.cpp:160-171` |
| `addr_l2w_index` | **每 DrawBatch** | `MaterialBatch.h:60` |
| `addr_mtl_data_addrs` | **每 DrawBatch** | `MaterialBatch.h:65` |
| `addr_texture_references` | **每材质** | `MaterialBatch.h:67`（`texture_reference_base_addr`） |
| `addr_text_char_info/style/instance` | 每字体（文本路径自己 push） | `src/ecs/support/text/TextRenderPipeline.cpp:283-291` |

### GlobalAddresses（64B UBO，Set 0 binding 2）8 个字段

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
| S2 | sky / viewport / shadow 的地址进表，表按帧槽多份（每帧写本槽），三个绑定退出 Set 0 | 门 + ATS（D1/D3 契约）+ CSM 多轮不一致=0 + 0 校验层消息 |
| S3 | `SCENE_SET` / `VKGlobalSceneUBOSet` / push descriptor / `descriptor_macros.glsl` 的 SCENE_SET 行整体删除；`scene_ubo.glsl` 的 include 与材质语义列表解耦（宏已全走 BDA） | 门 38→N PASS/0 FAIL；全部示例载入材质 0 errors |
| S4 | viewport 数据缩成 `uint16[2]`（其余按尺寸算），并按帧槽多份 | 文本/线/常规示例 0 校验层消息；golden 无资源行变化 |
| S5 | 收尾：陈旧注释（"7 张表/56B"、"Set 0 binding 4"）、门夹具、`doc/` 与技能同步 | 全仓 grep 零残留 |

## 2.1 S1 期间的坑（已归档到技能 `ulre-ssbo-vertex-input/references/descriptor-binding-retirement.md`）

- `DescriptorMacroGen --emit` **输出到 stdout**：`--emit > 产物路径` 才是重新生成；`--verify` **必须带路径参数**。
- 生成器 TU 在 `src/Tools/`：改 `inc/hgl/common/DescriptorSetTypeDef.h` 后 purge 的 src 列表必须含
  `src/Tools`，否则生成器用旧头 emit 出旧宏表，而构建/verify 全绿（产物与真源分叉）。
- `RootAddresses` 的布局断言已改成**按 X 列表自动推导**（字段 offset == 前面字段大小之和、
  总大小 == 各字段之和）——手列下标会在加字段时静默过期。

## 3. 验证清单（沿用既有门）

1. 构建含 `descriptor_macros_verify`（改真源 → `DescriptorMacroGen --emit` → `--verify`）。
2. `ShaderResourceSchemaRegressionGate`：基线 38 PASS / 0 FAIL（每次改动按预期增删用例）。
3. `TestCSMIncrementalPass` 21 Passed、`TestRenderItemDataStorage` 通过。
4. `ATS_SELFCHECK=1 AlphaTestShadow`：D1 57.6% / D3 18189 px / bias 600662 px / D4 拒绝 0 / selfcheck PASS。
5. `CSM_CACHE_DIFF=1 CSM_AUTOWALK=4 CascadeShadowMap`：不一致=0。
6. `LineRenderTest` / `DrawMultiLineText`：0 校验层消息；调色板地址非 0。
7. 每步都清 `build/cache-hot/shader-cache` 后再验（陈旧产物会冒充结果）。
