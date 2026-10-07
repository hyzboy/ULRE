# GPU 剔除「每 3 帧一张交换链图整帧空白」问题技术文档

> **版次**：v1（2026-10-06）。本文是**叙事性技术文档**（问题史 + 分析过程 + 各方案结局），
> 按「初次出现 → 逐轮分析 → 各方案 → 当前边界 → 教训」组织，供接手者一次读完即可接手。
> **范围**：只覆盖「每 3 帧一帧内容异常」这一族症状（三例），不覆盖 meshlet/实例化等其他线。
> **关联文档**：
> - `doc/future/entity-rows-gpu-cull-plan.md` —— **真源（raw record）**：§11.1~§11.14 是本文的取证底稿，
>   本文负责把它讲成一条线；两档分工 = 计划档存**原始测量**，本文存**推理链与方案取舍**。
> - 本文与计划档**不重叠**：计划档是"要做什么（T4c 任务）"，本文是"出了什么问题、怎么查的"。
> - 相关的既有档：`doc/shadow-ubo-inflight-overwrite.md`（另一族：UBO 在飞覆写，非本症状）、
>   `doc/shadow-mask-and-hybrid-shadows-design.md`（阴影设计，非本症状）。

---

## 0. 摘要

三个症状同源家族，**三例全部结案（都是真 bug 且已修）**：

| # | 症状 | 状态 | 根因 |
|---|---|---|---|
| 1 | `ShadowMap` 每 3 帧缺一帧（整帧近乎空） | **已修并验证** | `RenderPassRequest.is_shadow_pass` 是**可漏设**的旗标；示例渲染主世界进 shadow RT 时漏设 ⇒ 离屏 pass 被判成主帧 pass ⇒ 改写了**两 pass 共享**批对象的 `gpu_slice[Main]` ⇒ 主帧读**别的帧槽**的计数（0）⇒ 整帧空 |
| 2 | `AlphaTestShadow`/`CascadeShadowMap` 每图内容不同（地面消失） | **已修并验证** | **批标签表缺 pass 维度**（批分区表/计数表都按 pass 分块，唯独标签表只有帧槽）+ `pc.batch_count` 单份共享 ⇒ 后装配的 pass 盖掉先装配 pass 的标签 ⇒ 静止物体按**别 pass 的批索引**写行 ⇒ 挤进 `capacity=1` 的批 0 ⇒ 溢出丢行 |
| 3 | **所有**走 GPU 剔除的示例每 3 帧一帧**纯清屏** | **已修并验证** | **实体 4-ID 表的设备地址下发漏了 pass 分半的槽步长**（T4c3a 把表按 pass 分半、分配与上传都改用 `kRowStride`=8192 行/槽，唯独 `SyncWorldAddresses` 下发 `addr_entity_render_ids` 仍按 `kCapacity`=4096 行步进 ⇒ 槽 1 的着色器读进**全零的阴影半区** ⇒ `geometry_id==0` ⇒ 每实体在视锥测试前 `return` ⇒ `DrawCount=0` ⇒ 纯清屏）。详见 §4.7 |

**一句话结论**：三例全是**真缺陷且都已修复**；第 3 例的根因与第 2 例是**同一条坑**
（"分块/分半改造时漏改下发地址的步长"，§11.10 坑 1 的第二个实例），由特殊 dump 版
（`CULL_SSBO_DUMP=1`：每次 compute 派发后 `WaitLastSubmitFence` 再全量回读）一锤定音。
排查过程本身（§4.2~§4.6 的逐项排除表）保留 —— 它定义了此类问题的完整排除清单。

---

## 1. 初次出现与症状界定

### 1.1 最早的形态（用户报告）

- 最早症状：「接收阴影的示例，**每 3 帧有一帧画面不对**」；`ShadowMap` 自己看起来正常，
  而 `AlphaTestShadow`（ATS）/`CascadeShadowMap`（CSM）**会闪**。
- 用户最初给的落点是绘制命令侧：`vkCmdDrawMeshTasksIndirectCountEXT` 的
  **`countBufferOffset` 指向的计数为 0**，`maxDrawCount=1`。
- ⚠ 症状有一个**极危险的特性**：**改动看似无关的提交（甚至只加一行注释）会翻转判定** ——
  这直接导致了后面数轮的"修好了/又坏了"误判（真因见 §2.2）。

### 1.2 量化：从「闪一下」到「整帧近乎空」

用引擎的帧摘要自检把一个"看起来像闪一下"的症状变成可判定数字：

```
SM_SELFCHECK=1 FRAMEDIGEST_PERFRAME=12
frame=1/2 distinct=256 ✓
frame=3 distinct=4  fnv=0x43D07F2DF9290383  <== 与首帧不同
frame=4/5 ✓ → frame=6 又变 → …
PERFRAME FAIL：12 帧，与首帧不同的帧数=4      （12/3 = 4 ⇒ 完全吻合"每 3 帧一次"）
```

⭐ **症状的真实形态是「整帧近乎空」**（`distinct` 256 → 4 ⇒ 只剩清屏色），
不是"某个物体缺失"⇒ 嫌疑应指向**整帧的绘制/呈现路径**，而非单个批的行集/切片。

⚠ 顺带查出一个真缺陷：**`FRAMEDIGEST_PERFRAME=N` 一直是失效的** ——
读回钩子是**一次性**的（`VKSwapchainRenderTarget.cpp` 执行后即清除）⇒ 逐帧模式必须**自续装**。
修前只打第 1 帧就停摆，这也解释了"两份二进制第 8 帧摘要总是一样"的怪现象
（`FrameDigest.h` 自己也记着"每 3 帧缺一帧的摘要恰好被第 8 帧采样避开"——8 不是 3 的倍数）。

### 1.3 最终界定：三张交换链图里有一张从来没被画出内容

把逐帧摘要**接上 image 序号**后，症状被钉死：

```
image=0: 帧[2,5,8,…] distinct=[256] ✓
image=1: 帧[3,6,9,…] distinct=[4]   ← 5/5 全空
image=2: ✓
```

⇒ **空白帧恒为同一张交换链图（`image=1`）**，即"三张图里有一张从来没被画出内容"。
这一条与最早那条独立测量（**槽 1 的每批计数读出来恒为 0**）是同一件事（**槽号 = 图号**）。

---

## 2. 第一例：`ShadowMap`（已结案）

### 2.1 定位过程（三轮）

1. **逐帧读数**：用引擎现成的 `SyncForParityReadback` + `MapCurrentBatchCounters` 读计数 ⇒
   主帧 pass 的 0 号批计数在**槽 1 上恒为 0**，槽 0/2 恒为 14 ⇒ 与"3 张交换链图"一一对应。
2. **排除项**：槽 1 的标签（本槽有效标签 14 ✓）、布局（`max=14`、计数偏移 2056 = 槽 1 正确算术 ✓）、
   批数（`批=1` ✓）、三个按槽基址（`pc.batch_*_addr = 基址 + batch_slot × 槽步长` ✓）。
   ⚠ 此处我自己犯过一次测量错：数标签时用 `Map(0, …)` 读了**第 0 槽**，据此得的结论作废；
   改成 `Map(slot × slot_bytes, …)` 后才是真值。**教训：先确认量的是不是那个槽。**
3. **最强线索**：`[TAGTRACE]` 实测**离屏（帧号=4）那一路的批装配 `pass_index=0`、`shadow_flag=0`**，
   而 `EnvironmentSystem` 对真阴影 pass **确实**签发了 `req.is_shadow_pass = true`
   ⇒ 说明帧号 4 的那次装配**不是（或不只是）阴影 pass**。

### 2.2 根因（已结案）

`RenderPassRequest.is_shadow_pass` 是**调用方设置、可以漏设**的旗标：

- `ShadowMapApp::RenderShadowMap()` 把**主世界**渲染进 shadow RT 时**没设它**；
- 便捷重载 `Context::RenderTo(rt, clear, dt, cull)` 同样**不设**它 ⇒
  那次离屏渲染的 `is_current_pass_shadow` 为 false ⇒ 批装配取 `pass_index = Main`
  ⇒ **把两 pass 共享的批对象的 `gpu_slice[Main]` 改写成离屏自己的切片**
  ⇒ 主帧绘制随后读到它、去读**别的帧槽**的计数（0）⇒ 该图整帧空。

⭐ **"只加注释也翻转判定"的机理**：**两条路径写同一份切片，谁最后写由时序决定**；
每份二进制的代码布局固定了时序 ⇒ 每个提交的判定"稳定但不同"。
⇒ 这正是我数轮误判的根源：**不是代码变了，是竞争赢家变了。**

### 2.3 修复与验证

- **修复**：调用方声明 `req.is_shadow_pass = true`（示例侧一行）。
- **引擎侧收口**（`46e2da935`；⚠ 该推导的**判据**后被 §2.5 修正）：
  `pass 身份由目标推导`（当时口径：非交换链目标 ⇒ 非主帧 pass），消掉"调用方忘设旗标"的口子。
  方向正确，但"非交换链"这个判据把**独立离屏世界**也误判成了 shadow pass（§2.5）。
- **验证（全套绿）**：逐帧摘要 30 帧 ×3 次 ⇒ 三图取值集合全为 `[256]`（闪烁消除）；
  `ShadowMap 0xA8A369516CB04571` / `CascadeShadowMap 0x1CDAADEBF18B7CE4` **逐位不变**；
  `AlphaTestShadow` 契约 rc=0；门 45/0。

### 2.4 遗留

1. `Context::RenderTo(rt, clear, dt, cull)` 便捷重载**不设旗标**，其调用者 `OffscreenWorld.cpp`
   仍会被判成主帧 pass（同一后果，只是尚未以闪烁形式暴露）；
2. `TestCSMIncrementalPass` 的契约只断言"**默认值必须为 false**"，**没守"离屏渲染必须声明自身 pass 身份"**
   —— 这正是该 bug 能长期存活的**制度原因**。


### 2.5 后续：§2.3 的推导判据修坏了离屏世界（2026-10-07，已修正）

**症状**：`RenderToTexture` / `RenderToTextureColorDepth` 离屏 RT 全空——RenderDoc 截帧见离屏
cmd 只有 `vkCmdBeginRendering/vkCmdEndRendering`、**中间零绘制指令**；ShadowMap 的离屏却正常。

**根因**：`is_current_pass_shadow = req.is_shadow_pass || 非交换链目标` 这条推导的**判据选错了**——
第一例的病源是"**同一个世界**渲进第二个目标 ⇒ 与主帧共享批对象 ⇒ 必须错开 Main 块"；
而 RTT 的离屏是 `OffscreenWorld` **独立子世界**（自己的批表/自己的 GPUCullSystem，无任何共享），
它的 `RenderTo` 目标就是**它自己的主目标**，本该走完整 forward 收集。被判成 shadow 后
collect 走阴影精简链（只收 ShadowCaster）⇒ 子世界物体无投射阴影属性 ⇒ **收集为空 ⇒ 0 批 ⇒
cmd 只剩 Begin/End**。ShadowMap 离屏本来就只该收 caster ⇒ 它"正常"恰好掩盖了这条误判。

**修正**（`Context.cpp::RenderTo`）：推导判据改为 `req.target != render_target`
（**目标是否本世界主目标**）：`== render_target` ⇒ 本世界正常 forward pass（含独立离屏世界
对自有 RT 的渲染）；`!= render_target` ⇒ 同一世界渲进 shadow/mirror 等别的目标 ⇒ 判 shadow，
第一例兜底不变；`req.is_shadow_pass` 显式声明仍是正交条件。

**验证**：RTT 两例离屏世界每帧恢复完整装配/派发（`pass=0 几何批=1`、flush 逐帧、0 ERROR）；
阴影三例 + ATS 契约 + AsyncTextureUpload + 门 45/0 + CSM 契约全部对基线逐字节不变。

**教训**：兜底推导要按「**共享资源争用**」建模，不能按「目标长什么样（离屏/交换链）」建模——
后者把拓扑上完全无关的独立世界卷进来了。

---

## 3. 第二例：`AlphaTestShadow` / `CascadeShadowMap`（已结案，但**是另一个缺陷**）

### 3.1 症状

同一张交换链图内的各帧内容**完全一致**（同图 FNV 唯一 ✓），但 **image1 与 image0/2 不同**
（`distinct 254 vs 255`）。⚠ 粗细判据（`distinct<50`）**看不见**它。

### 3.2 定位手段的转折：从"单帧对单值"到"横向判据"

**这是本项目最重要的方法论转折**：此前的判据都是"单帧对单值"（把当前帧的摘要和已知基线比），
**结构上抓不到"三张图内容不同"**。改用**横向判据**（冻结取景下三张图的 FNV 必须彼此相等）后，
症状第一次被稳定照出来。

### 3.3 判据盲区：引擎自己的对拍也在骗自己

`CPUCullReferenceSystem`（CPU↔GPU 剔除对拍）报 `mismatch=0` —— 看似健康。实测发现：
**采样周期 60 与交换链图数 3 同倍（60 % 3 == 0）⇒ 判据永远只落在图槽 0**。
改成 **61**（与 3 互质）后采样扫过所有槽，两本账立刻给出决定性数据：

```
Σcount=2 < global=3、overflow=1、可见性对拍 CPU=3 GPU=3
```

⇒ 即：**可见性判定正确，但 gather 少写一行、且有实体被当"切片溢出"丢弃**。

### 3.4 根因（代码层不对称：两处"两 pass 同源"假设不成立）

- **批分区表（`batch_info`）与计数表都按 pass 分块**（`kArenaPasses × kBatchBlockStride`），
  **唯独批标签表只有帧槽维度**：`batch_tag_addr_ + slot * kMaxVisibleEntities * 4`；
  `BeginBatchTagWrite(pass)` 更是**整表清空重填** ⇒ 一帧内**后装配的 pass 会盖掉先装配 pass 的标签**；
  着色器查表也不带 pass 项（同文件的 **4-ID 表已**按 `blk * pc.max_visible_count + entity_index` 分半，
  标签表漏了同款处理）。
- **`pc.batch_count` 也是单份共享值**，而着色器**一次派发内自己循环写两个 pass 块**
  ⇒ 后 `EndBatchLayout` 的批数会作用到两个 pass。

⇒ 静止物体被按**另一个 pass 的批索引**写行 ⇒ 全部挤进批 0（`capacity=1`）⇒ 溢出 ⇒ 该行被丢。

### 3.5 修复（3 文件 6 处）与踩到的两个坑

| 文件 | 改动 |
|---|---|
| `GPUCullSystem.h` | `batch_count_` → `batch_count_[kArenaPasses]`；`GetBatchCount()` 返回**主帧**那份；`GetBatchLayout` 界检查按 pass |
| `GPUCullSystem.cpp` | 标签表 `tag_slot_bytes` 乘 `kArenaPasses`；`BeginBatchTagWrite(pass)` 只 Map/只清**本块**；`EndBatchLayout` 存按 pass；下发 `pc.batch_count = batch_count_[0]`；**`pc.batch_tag_addr` 槽步长同步改为 `kArenaPasses × kMaxVisibleEntities`**；溢出槽偏移与逐批前置校验同步 |
| `entity_gpu_cull.glsl` | 标签查表改为**两个 pass 各查自己那一块**（与同文件 4-ID 表同式）；早退语义拆分：**两块都无**才 `return`，单块无则 `continue`（否则会连带跳过材质行写入） |

**坑 1：漏改 `pc.batch_tag_addr` 的槽步长** —— 缓冲已改为"每帧槽两块"，但下发基址仍用旧步长
⇒ 槽 ≥ 1 时指进上一个槽的块 1 ⇒ 标签全错（表现：D3 契约 `receive_shadow` 直接 FAIL、坏图 `distinct` 塌到 4）。
⇒ **改寻址类改动必须把「缓冲尺寸 / 写入映射 / 下发基址 / 着色器索引」四处列成清单逐一对账。**

**坑 2：枚举使用点被 `head` 截断** ⇒ 漏掉两处 `batch_count_`（编译器只报第一个）
⇒ **改成员形态时用 `rg <名>` 去重列出全部，不靠编译器逐个发现。**

### 3.6 验证

- ATS 逐帧摘要按图号分组：**图 0/1/2 的 FNV 种类各为 1**（修复前坏图恒 2 种）；`distinct` 回到 255/254/255；
- ATS 契约：**D1 c0 PASS / D3 receive_shadow PASS（变红 18187 px）/ D3 bias_multiplier PASS / D4 行池写入 PASS / selfcheck PASS**；
- `TestCSMIncrementalPass`（自仓库根）：All CSM Incremental Pass Contract Tests PASSED；
- 回归面：`AlphaModes`/`CoverageVariants`/`PBRSpheres`/`LoadScene`/`PlaneGrid3D` 各图 FNV 唯一；
  **`GPUDrivenCull` 冻结取景对基线逐位一致**（`0x90E0745C2C7B8599`）。

---

## 4. 第三例：**所有** GPU 剔除示例每 3 帧一帧纯清屏（**已结案**，见 §4.7）

### 4.1 症状的扩散

修完第 2 例后，用户报：**不只 ATS/Cascade，所有走 GPU 剔除的示例都每 3 帧有 1 帧整幅空白**
（`PBRSpheres`/`AlphaModes`/`CoverageVariants`/`LoadScene`/`PlaneGrid3D`）。
且 `LoadScene` 与 `PlaneGrid3D` 的空白帧 **FNV 完全相同** ⇒ **纯清屏、与场景无关**。
`Cascade` 的空白帧形态不同（`distinct=256` 的满图）⇒ 说明该族症状至少有两种表现形态。

### 4.2 已逐项**实测**排除的输入（不是代码推断，是测量）

| 输入 | 实测值 |
|---|---|
| 标签表（坏槽） | 前 100 项 = 批 0（有效），之后 `0xFF` |
| 布局表 BatchInfo | 偏移 0 与 8224 均为 `(0,100,0,192)`（正常帧/空白帧一致） |
| 计数/标签基址 | 逐槽 `0/2056/4112`、`0/32768/65536` |
| 派发槽号 | `dispatch_slot` = 0/1/2 逐图正确（与图号一致） |
| 相机行 | 行号 0/1/2；`CommitRow` 全成功；内容三槽**逐位相同**（`1.358 0 0 0 0 -2.414 0 0`） |
| 实体行表（8 份） | 每份 `Active/Visible` 计数均 **`101/101`** |
| world addresses 基址 | 逐帧 `pc == expect`（`13021216768/896/1024` 随槽变） |
| 派发规模 | `entity_count=4096`、`batch_count=1` 逐帧恒定 |
| 接管是否发生 | `出口=ok`、`就绪=1`；所有拒绝路径探针**从未触发** |
| 可见性对拍 | `match`、`不一致=0` |

### 4.3 被提出并已被**实测否定**的假说（逐条：内容 / 方法 / 结果）

> 这一节是本问题最贵的部分：**每条都真的做过、都有实测数据、都回退了**。接手者不必重走。

| # | 假说 | 方法 | 结果 |
|---|---|---|---|
| A | 阴影 pass 相关（`ENV_NO_SHADOW` 是否影响） | 跳过**整个**阴影 pass | ATS 依旧每图不一致 ⇒ **与阴影 pass 无关** |
| B | 竞技场未按帧槽分份（跨帧覆写） | 三表 + ICB 按 `HGL_FRAME_SLOT_TOTAL` 扩份 | 逐帧摘要**逐位不变**（`D=4` 不变）⇒ 不是病因 |
| C | **L2W 环被渲染目标切换推进** | 把环改为"只跟主帧序号推进"，重建 | 六台架空白帧 FNV **逐字节相同** ⇒ 假说否定 |
| D | 计数写入/清零时序 | **实验 A**：每帧绘制前把 8 槽计数直写为 7 | 画面**逐字节不变** ⇒ **绘制内容不依赖该计数** |
| E | `buffer_reference_align=16` ⇒ 命令数组步长应为 16 | **实验 B**：绘制侧步长改 16 + 放大 ICB | 图 0/2 逐帧乱变（24 帧 5 种）、图 1 仍恒空 ⇒ **步长 12 是正解**（该对齐约束**基址**、不约束元素步长） |
| F | `batch->items(400) > info.capacity(100)` ⇒ 接管被拒 | 在守卫处插拒绝探针 | **探针从未触发** ⇒ 假说否定 |
| G | 标签表缺 pass 维度（第 2 例的发现） | 已修（保留） | 是**另一个**真缺陷，**与本症状无关**（修完本症状仍在） |
| H | 相机行未激活/未提交/值为零 | 探针读 `active_camera_row` 与行内容 | 三者均正常 ⇒ 否定 |
| I | 实体行表某槽那份未写入 | 引擎内探针统计 8 份的 `Active/Visible` | 8 份全为 `101/101` ⇒ 否定 |
| J | world addresses 某槽未写/写错 | 探针比对 `pc.addr_world_addresses` 与应为值 | 逐帧一致 ⇒ 否定 |
| K | 派发规模为 0（无线程执行） | 探针读 `pc.entity_count` | 恒为 4096 ⇒ 否定（但这条**方向对了**：确实每实体在派发内被 early-return，只是出口不是规模而是 §4.7 的 4-ID 半区） |
| L | 缺 barrier 导致读到被清的 0 | 复查两条 barrier 与掩码 | 两条都在且掩码正确 ⇒ 否定（但源码注释"跨 cmd 靠 submission order 保证可见性"**本身是错的**，已改正） |
| M | 槽就绪门拦住 | 读四槽 `就绪` | 全为 1 ⇒ 否定 |

### 4.4 决定性数据：GPU 自己的幸存计数（`CULL_SSBO_DUMP` 特殊 dump 版）

用引擎现成访问器（`GetDrawCountBuffer()` + `SyncForParityReadback()`，**无需改着色器**）逐帧回读：

```
图0: draw_count=100  counters=100  ✓
图1: draw_count=0    counters=0    ★  ← GPU 自己的幸存计数就是 0
图2: draw_count=100  counters=100  ✓
```

⇒ **GPU 的剔除在槽 1 上产出 0 个可见实体**，而 CPU 参考（对拍）说 3。
⇒ 两侧在**可见性判定**上分歧，而**能读到的每一项输入都正确且逐槽一致**（§4.2）。

### 4.5 收敛结论与当时的"自相矛盾"（已解）

**收敛结论**：**CPU 侧与所有按帧槽分份的资源都是正确的**，而 GPU 在其中一个槽上产出 0
⇒ 差异只能发生在 **GPU 执行侧**。

**当时的"自相矛盾"**（`draw_count` 单份 ⇒ image=1 帧为何读到 0）后来被证明是**问题问错了**：
`draw_count` 与批计数**每帧都被派发重写**，"上一帧的值"这个概念本就不存在——
真正要问的是"**本帧派发为什么产出 0**"。特殊 dump 版（§4.7）直接回答了它。

### 4.6 当时留下的"下一步"（事后回看：方向部分正确）

当时写的下一步是"给 `draw_count` 缓冲记录写入/读回帧号"与"GPU 侧诊断 SSBO"。
事后看，**真正奏效的是第三条**：用户提出的**特殊 dump 版**——每次 compute 派发后
`WaitLastSubmitFence()` 同步等待，再把**所有帧槽的计数/布局/标签/命令**全量回读（`CULL_SSBO_DUMP=1`）。
它的价值在于把观测点从"帧首"搬到"**派发后**"，第一次看到了 GPU 在真实执行时刻的状态。

> 教训：**帧首读回永远看不到"本帧派发写了什么"**。此类跨帧时序问题，
> 观测必须锚在派发之后（dump 版），或者干脆串行化（dump 版自带的 `WaitFence` 效果）。

---

### 4.7 结案：根因、修复与验证（特殊 dump 版一击命中）

**dump 版给出的逐帧数据**（`[BatchCounters]` 显示槽 1 恒 0、标签/布局全部有效、
`[COUNT_DUMP]` 显示绘制端读到与剔除端一致的 0；再配合 `[WADUMP]` 打印每帧下发的
`addr_entity_render_ids` 基址）：

```
render_ids 基址: slot0=0x308700000 slot1=0x308710000 slot2=0x308720000
                        相邻差 = 0x10000 = 4096 行 × 16 B   ← 用了 kCapacity
缓冲实际布局（分配/写侧）: 每槽 kRowStride = 2 pass × 4096 = 8192 行 = 0x20000 B  ← 差一倍
```

**根因**（`Context.cpp` `SyncWorldAddresses`，T4c3a 的漏改点）：

```cpp
want.addr_entity_render_ids = entity_render_id_table_addr
    + (uint64_t)slot * EntityRenderIDTable::kCapacity * 16;   // ✗ 应为 kRowStride
```

T4c3a 把 4-ID 表**按 pass 分半**（flat 行号 = `PassBase(pass)+index`，每帧槽 = 8192 行），
**分配（`CreateSSBO`）与上传（`UploadEntityRenderIDDirtyRows`）都改成了 `kRowStride`，
唯独下发地址没跟上** ⇒ 槽 s 的着色器指向"半份错位"的地址：
- **槽 1 → 槽 0 的阴影半区**：无阴影 pass 的示例（PBRSpheres 等）那半**从未被写 = 全零**
  ⇒ `geometry_id == 0` ⇒ 每个实体在视锥测试**之前** early-return ⇒ `DrawCount=0` ⇒ **纯清屏**；
  有阴影 pass 的示例（Cascade）那半有阴影风味数据 ⇒ 满图但内容不同 = §11.11 记录的"另一种形态"；
- **槽 2 → 下一份的 pass-0 半区**：上传把每份都写成相同内容 ⇒ 恰好读到**正确数据** ⇒ 正常。

⇒ 与全部观测严丝合缝：只坏一张图、CPU 侧每样都对、标签布局无罪、对拍看不见（CPU 参考不走设备地址）。
**与第 2 例同一条坑**：§11.10 坑 1"改寻址必须对账『缓冲尺寸/写映射/下发基址/着色器索引』四处"——
这次漏的正是第三处"下发基址"，**该纪律的第三个实例**。

**修复**（`src/ecs/core/Context.cpp` 3 处同口径）：`addr_entity_render_ids` 下发、
整表清零循环、就绪日志，`kCapacity` → `kRowStride`。

**验证（全部实测）**：
- `PBRSpheres`：12 帧逐图 ⇒ 三图 FNV 全等 `0x3DD88A0999CDC486`（= image0/2 原基线，
  **空白图消失且好图逐字节未变**），`PERFRAME PASS`；
- 回归 7 台架：`AlphaModes`/`CoverageVariants`/`LoadScene`/`PlaneGrid3D`/`ShadowMap`/
  `CascadeShadowMap`/`GPUDrivenCull` **每图唯一 + 横向相等 + 全部回到各自基线**
  （Cascade 修复后的多帧摘要三图全等 `0x1CDAADEBF18B7CE4` ⇒ 旧基线本来记录的是 image0/2 的正常帧）；
- `AlphaTestShadow` 契约 D1/D3/D4 全 PASS（receive_shadow 变红 18187 px）；
  `TestCSMIncrementalPass` 42 项契约 PASSED；`ShaderResourceSchemaRegressionGate` **45 PASS / 0 FAIL**。

**一个插曲（调试件污染的实例）**：修复初验时 Cascade 一度显示 `0x1BAB…` ≠ 基线，
追踪发现是 `PipelineMaterialRenderer` 里排查期加的**无门控同步回读**（每批每帧
`ReadbackBuffer` = GPU stall，改变帧间节奏）所致；把它收进 `CULL_SSBO_DUMP` 门控并撤除
`current_count_val == 0` 调查分支后，Cascade 立刻回到基线。**"未门控的观测本身会改变被观测系统"**
——收尾清理因此不只是卫生问题，它是判据的一部分。

---

## 5. 过程中我犯的取证与工程错误（含纪律）

这一节保留下来，因为**它们比结论更容易重犯**。

| # | 错误 | 后果 | 纪律 |
|---|---|---|---|
| 1 | 数标签时 `Map(0, …)` 读了第 0 槽 | 结论全部作废 | **先确认量的是不是那个槽** |
| 2 | 把两个**长度不同**的日志列表**按位置 zip** | 得到"槽恒差 1"的**假恒定偏移** | 不同长度的列表先对齐再比较 |
| 3 | 在**截断样本**上断言"没有 pass_index=1"（只看了前 26 行） | 假阴性 | 用**全量分布**下结论 |
| 4 | 用会**回绕的帧号**当字典键 | 不同周期的同号帧被合并 | 帧键要含周期/唯一化 |
| 5 | 按"触发次序"计数当锚点 | 随时序漂移 | 改锚**确定帧**（如"第 45 帧"） |
| 6 | 按条数切帧（224/30 非整数） | 帧边界错 | 用天然帧键（**相机行**随帧槽轮转；摘要行每帧恰一条且在帧末） |
| 7 | 探针走了"对象为空"的**静默失败分支** | 输出 0 行，白等一个构建周期 | **探针第一条语句无条件打印**（自证能出声）；失败路径不许静默 |
| 8 | 长跑输出套 `tail` | 缓冲让健康运行看起来像**卡死**（误判两次） | **长跑不套 `tail`**，后台 + 通知 |
| 9 | 一整块替换切坏了花括号 | `C1075` | 改探针只用**纯插入 / 整行区间替换** |
| 10 | 正则贪婪 `^.*\] ` 吃掉了标记本身 | 搜不到标记 ⇒ 假"0 次落表" | 在**原始行**上搜，别在格式化后的字符串上搜 |
| 11 | `⇒` 误写成 `=>`、单行替换漏掉后续两行 | 锚点失配 / 留下引用已删变量的语句 | 脚本**写盘前全量预检锚点**（全中才落盘） |
| 12 | 构建失败后仍想拿运行结果下结论 | 差点拿旧二进制定案 | **构建失败即不采信运行结果**（本轮已成型为守卫） |
| 13 | 对拍"健康"被当成证据 | 采样盲区（周期与图数同倍）让判据只看一个槽 | 判据的**采样/覆盖**本身要受检（周期与图数互质） |
| 14 | 相邻改动踩到**未 purge 的 obj** ⇒ 假回归 | 一度误判真回归 | 结构性头文件改动后 purge 相关 TU 再构建 |

---

## 6. 方法论收获

1. **判据必须横向**。冻结取景下多张交换链图的内容**应当相等**——"单帧对单值"的基线
   **结构上抓不到"三张图内容不同"**。本项目直到改判据才第一次照出本类缺陷。
   ⇒ 新示例的回归口径应默认包含"**同图 FNV 唯一 + 各图彼此相等**"。
2. **症状要量化到不能再量化**（`distinct 256→4`、`12 帧差异 4 帧`、`恒为 image=1`）。
   量化的过程本身会暴露新缺陷（如 `FRAMEDIGEST_PERFRAME` 一次性钩子失效）。
3. **"改了不相关的代码，判定就翻转"是竞争条件的签名**，不是玄学。
   遇到"稳定但每个提交都不同"的判定，要在**同一份二进制**上重复测量，而不是跨提交比较。
4. **构建失败即不采信运行结果**。探针类工作尤其如此（否则会拿旧二进制"证明"一个假说）。
5. **每条假说都要有"实测否定"的落点**（不是"我觉得不对"）。本文 §4.3 的表格就是产物：
   它的价值不在于列出的结论，而在于**下一个人不必重走**。
6. **只读探针的编辑纪律**：`printf` 通路、纯插入/整行区间替换、锚点用文件原文、
   写盘前全量预检、失败路径必须出声。

---

## 7. 当前代码状态与提交索引

**分支**：`DrivenSplit`（本地领先 upstream，**未 push**）。**工作树干净**（仅 2 份未跟踪文档）。

| 提交 | 内容 |
|---|---|
| `1aa8e0292` | **判据**：修 CPU↔GPU 剔除对拍的采样盲区（周期 60 与图数 3 同倍 ⇒ 永远只采图槽 0） |
| `2ad2b48af` | **修复**：批标签表与批数补齐 pass 维度（第 2 例真因） |
| `7eca553b1` | 计划 §11.10：补记修复后的回归面（含 `GPUDrivenCull` 对基线逐位一致） |
| `3be9aeb74` | 计划 §11.11：负结果——每图一帧全空、L2W 环假说被实测否定 |
| `189edc7ec` | 计划 §11.12：CPU 侧全部输入逐字节排除，收敛到 GPU 执行侧 |
| `4207da6e8` | 计划 §11.13：实验 A/B 结果（计数无关、步长 12 正确） |
| `1265c43a1` | 计划 §11.14：逐项实测收尾（含 §4.5 的矛盾） |
| `84dcf57e5` | 本文 v1（结案前的版本） |
| `7b385d025` | **修复**：`addr_entity_render_ids` 下发槽步长 `kCapacity`→`kRowStride`（+清零/日志同口径）；SSBO DUMP 设施（`CULL_SSBO_DUMP` + `DumpCullSSBOs` + `VKBufferReadback`）入库 |
| `219dd8802` | **修复**：阴影 pass 漏设 `is_shadow_pass`（第 1 例真因） |
| `46e2da935` | 引擎侧收口：pass 身份由目标推导 |
| `d60622e58` | 计划：补记第 1 例真根因、修复、全套验证与两处遗留 |

**修复后的帧摘要基线**（冻结取景）：

```
GPUDrivenCull     0x90E0745C2C7B8599   （需 GC_STATIC=1）
ShadowMap         0xA8A369516CB04571
CascadeShadowMap  0x1CDAADEBF18B7CE4
AlphaModes        0x2FE87B15EE8B8A16
CoverageVariants  0x8EED38BAD9B8181E
LoadScene         0xC9E38ED2F49A069B
PlaneGrid3D       0x4125C3C830CB92D3
PBRSpheres        0x3DD88A0999CDC486   （结案后 = 三图全等；旧注"image1 仍空"作废）
```

**已知遗留（非本症状，供决策）**：`src/ecs/CMakeLists.txt` 给若干测试只设了
`VS_DEBUGGER_WORKING_DIRECTORY`（仅 VS 调试器生效），**未设 `ctest` 用的 `WORKING_DIRECTORY`**
⇒ 凡按相对路径读 `ShaderLibrary/...` 的测试在 `ctest` 下必红（实测从仓库根跑均 RC=0）。

---

## 附录 A：帧槽 / 槽算术速查

- **帧槽空间**：`HGL_FRAME_SLOT_TOTAL = 8`（主帧 4 / 离屏 4..7）。
- **不变量**：per-frame ring 深度**必须 ==** 槽空间（L2W 环曾为 3 ⇒ 离屏槽与主帧槽别名 ⇒
  prepass 覆写主帧矩阵 ⇒ 整帧只剩清屏色）。
- **计数偏移**：`/ 2056 = 槽号`（2 pass × 257 行 × 4 B）。实测槽 0/1/2 = `0/2056/4112`。
- **标签偏移**：实测槽 0/1/2 = `0/32768/65536`（= `slot × kArenaPasses × kMaxVisibleEntities × 4`）。
- **`CullPushConstants` = 112 B**：`batch_count` 偏移 100、`batch_block_stride` 偏移 104。
- **`kBatchBlockStride = 257`**（= `kMaxBatches 256` + 1 溢出槽）；块 0 = 主帧、块 1 = 阴影。
- **`BatchInfoRow` = 16 B**（`row_base`/`capacity`/`topology`/`group_size`）；`topology` 0→192、1→64。
- **命令步长 = 12 B**（`VkDrawMeshTasksIndirectCommandEXT`）；`buffer_reference_align=16` 由**基址**兑现。

## 附录 B：环境开关一览

| 开关 | 用途 |
|---|---|
| `<PREFIX>_SELFCHECK=1` | 帧摘要自检（退出码即契约）；例：`SM_`/`CSM_`/`ATS_`/`PBR_`/`LS_`/`PG_`/`GC_`/`AM_`/`CV_` |
| `FRAMEDIGEST_PERFRAME=N` | 逐帧摘要（须配 `_SELFCHECK=1`，且**已修**一次性钩子） |
| `ENV_NO_SHADOW=1` | 跳过整个阴影 pass（隔离实验） |
| `GC_STATIC=1` | 冻结 `GPUDrivenCull` 相机（对基线回归用） |
| `FORCE_COUNT=N` | 实验 A：绘制前直写批计数 |
| `ECS_TAKEOVER_TRACE=1` / `BATCH_LAYOUT_TRACE=1` / `TAG_READ_TRACE=1` | 接管/布局/标签追踪 |
| `SM_FREEZE=0` / `CSM_AUTOWALK=1` | 敏感性验证（改值即使摘要变） |

## 附录 C：关键文件索引

| 主题 | 文件 |
|---|---|
| 剔除着色器 | `ShaderLibrary/ecs/entity_gpu_cull.glsl` |
| 剔除系统 | `src/ecs/support/GPUCullSystem.cpp`、`inc/hgl/ecs/support/GPUCullSystem.h` |
| 绘制侧（间接命令） | `src/ecs/support/PipelineMaterialRenderer.cpp` |
| CPU 对拍参考 | `src/ecs/support/CPUCullReferenceSystem.cpp` |
| pass 判定与帧槽 | `src/ecs/core/Context.cpp`（`is_current_pass_shadow`、`SetFrameIndex`、`SyncWorldAddresses`） |
| 实体行表 | `inc/hgl/ecs/core/EntityGPU.h`、`inc/hgl/ecs/support/EntityGPUTable.h` |
| 变换行（L2W） | `src/ecs/systems/tick/TransformSystem.cpp`、`src/ecs/support/TransformAssignmentBuffer.cpp` |
| 帧槽常量 | `inc/hgl/common/RenderOptions.h` |
| 真源记录 | `doc/future/entity-rows-gpu-cull-plan.md` §11.1~§11.14 |