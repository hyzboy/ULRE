# 纹理资产 ID 化与全异步加载（TextureStreaming）实施方案 v2

> **版次**：v2（吸收外部评审四条 + 自纠 v1 风险编号错引）。相对 v1：① `TextureAssetID` 封装为**不透明强类型**且 `AllocateHandle` 补容量 fail-fast（复核事实：`kMax=8192`、当前**无上限守卫**，见 E-2/R-7）；② T4 解码像素**直写 staging ring 映射块**，消主线程大缓冲拷贝尖峰（§3.4）；③ `Request()` 等入口显式 `[MainThreadOnly]`；④ 数组记录加 `layer_ready_mask`，就绪广播**逐层**粒度（§2.2/§3.3/T5）。范围 = 用户层纹理只需声明文件名即得 ID；状态/基础信息/坏图可查询；加载全程异步；
> 材质授权旁表**零指针**（彻底数据化，为 TOML/BINARY 序列化做好前提）。
> **分支基线**：`TextureStreaming`（基点 `e74514c2c`，与事项同名）。
> **关联文档**：
> - `doc/async-texture-upload-and-bindless.md` —— 上传队列 + Bindless 热换的机制真源。本计划**延伸它、不取代它**（队列语义不变，在其上立"资产记录"层）。
> - `doc/future/ULRE_ECS_Component_To_ID_Architecture_Roadmap.md` —— 阶段二"材质子系统 ID 化"的纹理侧由本计划落地（注意该档顶部"时点规范"警示：其示例代码是设计示意，以本档为准）。
> - `doc/texture2darray-unified-texture-binding.md` —— 数组纹理事件级绑定；本计划 T5 的逐层 API 与其口径对齐。
> - 引用一律写 `文件:符号`，不写行号（行号属本基点提交）。

---

## 0. 设计输入

### 0.1 已定输入 —— 用户决策

| # | 决策 | 落点章节 |
|---|---|---|
| U-1 | 所有纹理在用户层**只声明一个文件名**即可得到一个 ID | §2、T1 |
| U-2 | ECS 层专开一个管理器：管"是否加载 / 是否坏 / 基础信息"，并驱动异步加载 | §3.1（归属修正见 P-2：记录表落**设备级**，ECS 侧为驱动器+访问器） |
| U-3 | 用户层材质部分**彻底数据化**，为全面 TOML/BINARY 化做好准备 | §2.4、T2/T3（旁表零指针） |
| U-4 | 零兼容容忍：裸 `Texture*` 材质 API 与 `resource_id` 字符串通道**整删**，不留过渡层 | T2 触点表 |
| U-5 | glTF 导入链本计划**不碰**（用户指示：未来重写） | §1 非目标 |

### 0.2 已定输入 —— 引擎不变量（违反即崩/编译失败，出处按符号复核）

| # | 不变量 | 出处 | 对本计划的约束 |
|---|---|---|---|
| E-1 | 纹理 GPU 侧身份 = **bindless handle**（1-based，0 = 无效/无阴影哨兵）；描述符机制已整体退役，无第二通道 | `VKBindlessTextureManager.h:RegisterTexture/AllocateHandle`；`inc/hgl/graph/ubo/ShadowInfo.h`（`shadow_tex` 语义） | ID 体系必须收敛到 handle（P-1） |
| E-2 | `AllocateHandle` = `next_handle_++` **单调不复用**且**无 `kMax=8192` 上限守卫**（溢出 = 描述符缓冲越界写，静默 VU）；`RegisterTexture` 按 `Texture*` 键去重（`tex_cache_`），`AllocateHandle` 无去重；`UpdateTextureHandle` 的守卫是新鲜度（`>= next_handle_`）不是容量 | `src/Vulkan/VKBindlessTextureManager.cpp:AllocateHandle/RegisterTexture/UpdateTextureHandle`；`inc/hgl/vk/VKBindlessTextureManager.h:kMax` | handle 天然可做"永不重排的持久引用"；T1 必须补容量 fail-fast（R-7）；驱逐/复用若引入必须先加 slot+世代（P-6） |
| E-3 | 帧入口自动泵上传队列：`BeginManagedRenderFrame` 内 `tm->UpdateUploadQueue(bindless_mgr)`，**任务完成后即 delete** | `src/ecs/core/Context.cpp:BeginManagedRenderFrame`；`src/SceneGraph/module/TextureManager.cpp:UpdateUploadQueue` | 状态必须住**常驻记录**，不能住一次性 task（§2.2） |
| E-4 | 跨队列同步 = 单条 **upload lane**（timeline），主帧提交前 wait 最新值；不得回退逐任务信号量 | `inc/hgl/graph/module/TextureUploadQueue.h:upload_lane_`（注释含两处历史事故） | worker 只产像素（CPU 块或 staging ring **映射块**——取段是纯 CPU 记账，无 Vulkan 对象创建）；VkImage/tci/派发留在主线程泵（T4、§3.4） |
| E-5 | 材质数据行归**设备级**行号（B4 拍板"路 B"）；重类型落**世界级旁表**，行内只 POD | `inc/hgl/ecs/components/MaterialData.h`（B4-iii 注）；`inc/hgl/ecs/support/MaterialAuthoringSideTable.h:MaterialTextureAuthoringResource` | 旁表条目换成 `uint32_t` 记录 ID 后即满足"行/条目全 POD"（T2） |
| E-6 | 收集链纹理获取现按 `resource_id` 字符串匹配，缺失即 `GLogError + return false`（每帧每实体重复报错） | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:AcquireDeferredResources`（"DeferredResource" 分支） | Loading≠错误：需状态化静默 + 就绪重试通道（T3） |
| E-7 | 材质解析跳过靠每实例 `tracked_material_data_generation` 比对 | `inc/hgl/ecs/support/MaterialRuntimeTable.h:MaterialRuntimeSlot` | 纹理就绪是**第二个 generation 源**（§3.3） |
| E-8 | Static transform 重复 set 同值仍会触发 revision 链（教训：CSM 地面吸附加同值守卫） | `example/Shadow/CascadeShadowMap.cpp:UpdateMovableAnimation` 注释；`ECSContext::static_scene_revision` | 就绪广播必须**值变才 bump**，不得每帧全量重解析 |
| E-9 | `Task 2.2 / MaterialAccessor(4B)` 是 Roadmap 阶段二既定目标 | `doc/future/ULRE_ECS_Component_To_ID_Architecture_Roadmap.md` §三 | 本计划即其纹理侧首批，不与之冲突 |

### 0.3 待拍板项（每项：选项 + **推荐** + 影响；不定也能开工的部分已标注）

| # | 问题 | 选项 | 推荐 | 影响 |
|---|---|---|---|---|
| P-1 | 运行时 ID 用什么 | a) 独立记录索引+世代 / b) **ID = bindless handle**（记录表以 handle 为主键） | **b** | b 零额外间接、与 E-1/E-2 自洽；但对外以**不透明强类型**发布（§2.1）——业务代码拿不到裸数值做算术，未来驱逐引入 slot+世代（P-6）时只换内部。TOML 里仍写**名字**（ID 不落盘） |
| P-2 | 记录表归属层 | a) ECS 世界级 / b) **设备级**（GraphicsContext 下，`TextureManager` 扩生），ECS 只加驱动器访问器 | **b** | 纹理跨世界共享（DualWorldShadows 同图应只一条记录），与 E-5 同理由。U-2 的"ECS 专开管理器"读作=驱动器+访问器门面 |
| P-3 | 未就绪时画面表现 | a) 实体整帧不收集 / b) **Loading 占位图**（常驻 1x1 图，handle 恒就绪） | **b** | b 管线零分支、与 bindless 热换同构；坏图另备 Broken 品红占位。a 作为 required 未给时的退路保留 |
| P-4 | 引擎内直调 `LoadTexture2D`（≈31 处，Gizmo/UI/加载器等）本批是否一并迁 | a) **全留同步 API，只迁示例+材质链** / b) 全迁 | **a** | 这些调用点不经材质旁表，不阻塞数据化目标；b 会把 T2 体量翻倍换不到判据 |
| P-5 | Broken 后的修复语义 | a) **重新 Request(同名) 复用原 ID 重启加载** / b) ID 永久作废 | **a** | a 让持有旧 ID 的材质无需任何动作即可自愈；b 需广播引用者换 ID（无必要复杂度） |
| P-6 | 驱逐/引用计数 | 本计划**不做**（记录常驻）；接口预留 `Release(record)` 空壳 | — | 触发条件 = 资产量超显存预算（未来 virtual texture/mipmap streaming 一并议）。E-2 约束：届时要世代 |

---

## 1. 目标与非目标

### 目标
1. 用户层唯一入口：`TextureAssetID Request(const OSString &filename, 优先级, 就绪回调可选)`；ID 立即可用，内容后到。**`[MainThreadOnly]`**（§3.4，入口断言）。
2. 每张纹理有**常驻记录**：加载态（Requesting/Decoding/Uploading/Ready/Broken）、基础信息（宽高/层数/mip 级数/格式，**入队前即可查**——现状 `CreateTexture2DAsync` 同步建 VkImage 已满足）、失败原因。
3. 材质旁表条目零指针零字符串：纹理项 = `uint32_t record_id`（+ kind/array_layer/required 选择器，本就是 POD）。
4. 未就绪不是错误：Loading 静默、就绪 ≤1 帧自动重解析、Broken 一次性告警 + 错误占位。
5. 全流程真异步：文件读 + 解码 + 压缩解包离开主线程（T4），主线程只剩"记账 + 打包 + 入队"（沿用 E-4 泵）；解码像素默认直写 staging ring 映射块，主线程零二次拷贝（§3.4）。

### 非目标（防止架构走死）
- **不写** TOML/BINARY 的解析与落盘本体（本计划只交付"可序列化"的零指针前提与 name-table 语义）。
- **不做** mipmap streaming / virtual texture / 记录驱逐（P-6）。
- **不碰** glTF 导入链（U-5）。
- **不动** upload lane、staging ring、背压、优先级调度等队列机制（E-4；`doc/async-texture-upload-and-bindless.md` 继续是真源）。
- **不动** `PBRSurfaceRow` 等材质**参数**行空间（那是 SSBO 域，与纹理身份无关）。

---

## 2. 数据模型

### 2.1 记录与 ID

```cpp
// inc/hgl/graph/module/TextureAssetManager.h（新增）
namespace hgl::graph {

    // 不透明强类型：当前值域 = bindless handle（P-1b）；0 = 无效。
    // 封成 struct 的原因：业务代码一旦拿裸数值做算术/比较/持久化，
    // P-6（slot+世代复用）改造就无处下刀 —— 现在封死，未来只换内部。
    struct TextureAssetID {
        uint32_t v = 0;
        bool IsValid() const { return v != 0; }
        bool operator==(const TextureAssetID &o) const { return v == o.v; }
    };

    enum class TextureAssetState : uint8_t {
        Requesting,   // 已登记，解码未开工（T4 前此态即同步排队）
        Decoding,     // worker 在读文件+解像素（仅 T4 后存在）
        Uploading,    // 已入 TextureUploadQueue，GPU DMA 在飞
        Ready,
        Broken,       // 文件缺失/解码失败/尺寸格式不符——终态，可 P-5 重启
    };

    struct TextureAssetInfo {                 // 基础信息：.Tex2D 头读出，入队前即有效
        uint32_t width = 0, height = 0, depth_or_layers = 1;
        uint32_t mip_levels = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        bool     block_compressed = false;
    };

    struct TextureAssetRecord {
        TextureAssetID  id;                   // == bindless handle（AllocateHandle 预分配）
        OSString        name;                  // 归一化文件名 = 持久身份（序列化只写它）
        TextureAssetState state = TextureAssetState::Requesting;
        TextureAssetInfo info;
        Texture        *texture = nullptr;     // GPU 对象（内部字段；**不再经材质 API 暴露**）
        uint64_t        upload_task_id = 0;    // 关联队列 task（Uploading 期）
        uint32_t        generation = 0;        // P-5 重启 +1；引用者可选校验（默认不校验，E-2 保证 id 不复用）
        int32_t         error_code = 0;        // Broken 根因（枚举，唯一口径）
        uint64_t        layer_ready_mask = 0;  // Array 记录：bit i = 层 i 已就绪；单图恒 bit0
        uint32_t        layer_total = 1;       // Array 记录：总层数（state=Ready ⇔ mask 全覆盖）
    };
}
```

要点：
- **id 先于内容存在**：`Request()` 里 `AllocateHandle(loading_placeholder)` 立刻拿到槽位 ⇒ 材质行当天起只有整数。
- 记录表 = `id→record` + `归一化 name→id` 双索引；同图第二次 Request 返回既有 id（引用计数留给 P-6，本计划记录常驻不淘汰）。
- 文件名归一化**唯一出处**一个函数（大小写/分隔符/相对路径折叠），否则双记录双上传（风险 R-3）。
- `Request / RequestIntoArray / GetState / GetInfo` 全部 `[MainThreadOnly]`：入口断言（主线程 id 于 GraphicsContext 初始化登记），记录表与双索引因此**无锁**（§3.4）。
- T1 顺手给 `AllocateHandle` 补容量守卫：`next_handle_ >= kMax ⇒ GLogError + return 0`（现状无守卫 = 越界写，R-7）。

### 2.2 状态与 task 状态的映射（E-3 修正）

`UploadTaskState` 是 task 的一次性视角；`TextureAssetState` 是记录的常驻视角：

```
Requesting ──(T4: worker 领取)──▶ Decoding ──(主线程泵: 建 VkImage+Enqueue)──▶ Uploading
    │(同步路径：T1..T3 直接建 tci+Enqueue)                                        │
    └────────────────────────────────────────────────────────────────────────────┤
 失败任意环节 ─────────────────────────────▶ Broken ◀──(upload 失败/头校验拒)   Ready ◀── task Completed
                                                 ▲                                │
                                                 └──── P-5 重新 Request(同 id) ◀──┘（Ready 后内容换=重 Request 覆盖描述符，generation++）
```

task `Cancelled/Discarded` 不进记录状态机（记录语义 = "这个文件要不要"，撤销是调度细节；撤销 API 直接落回 Requesting→Broken 之间由调用方决定，暂不暴露）。

**Array 记录的逐层就绪**：`state=Ready ⇔ layer_ready_mask` 覆盖全部 `layer_total` 层；单层完成**不**升整体态、只进逐层广播（§3.3）。引用特定层（材质条目 `array_layer`）的 slot 在该层就绪时即脏，**不等整图**——IconWall 逐格上墙语义保留；无"半层撕裂"（每层是独立拷贝单元，层内不存在部分就绪）。层数 > 64 时掩码换扩展容器（实现期定，`.Tex2D` 数组当前规模远未触及）。

### 2.3 占位图（P-3）

设备级常驻两张极小纹理（`TextureManager` 初始化时建，不走队列）：
- `loading_placeholder`（中性灰）：Request 返回的 handle 初绑它；
- `broken_placeholder`（品红棋盘）：Broken 时 `UpdateTextureHandle` 热换上去。
"该槽永不空"是收集链能零分支的前提——`AcquireDeferredResources` 里 `texture==nullptr` 整类分支随之删除。

### 2.4 材质旁表条目（T2 后形态，全 POD）

```cpp
// inc/hgl/ecs/support/MaterialAuthoringSideTable.h:MaterialTextureAuthoringResource 改为
struct MaterialTextureAuthoringResource {
    hgl::graph::TextureAssetID texture_id;       // 0/无效 = 未绑定；原 resource_id 字符串 + Texture* 双通道整删
    hgl::graph::Sampler       *sampler  = nullptr; // T5 sampler 命名化后再消
    MaterialTextureResourceKind kind    = MaterialTextureResourceKind::Texture2D;
    uint32_t array_layer  = 0;
    bool     required     = false;
};
```

- `resource_id`/`BuildTextureResourceId`（`src/ecs/support/RenderResource.cpp`）全链删除；
- `materialization_resource_handles(rid→handle)` 映射消失——记录 id **就是** handle，行内取 handle 一步到位；
- 序列化就绪判据（自动化）：`MaterialAuthoringSideTable.h` 中 grep `std::string|resource_id|Texture \*` = 0 命中（sampler 指针 T5 前豁免，在变更记录中挂账）。

---

## 3. 通路

### 3.1 分层与驱动点

```
用户层        TextureAssetManager::Request("res/image/a.Tex2D", High)  → id（立即）
                │ 去重 / 建记录 / AllocateHandle(loading)
设备级        T1..T3: 同步建 tci（复用 VkTextureLoader 前半）→ TextureManager::CreateTexture2DAsync
              T4:     记录入 worker 待解码池 → worker 解像素**直写 ring 映射块**+info → 主线程泵记账、打包 tci → 入队
E-3 既有泵    BeginManagedRenderFrame → UpdateUploadQueue → Completed → UpdateTextureHandle(id, tex)  ← 机制零改动
记录层        泵后扫描：task 完成 → 记录 state=Ready → 投入「本帧就绪批」
世界级        ECSContext 帧内消费就绪批 → 反查表置脏 → 相关 slot 重解析（§3.3）
```

- `TextureAssetManager` 挂 `GraphicsContext`（P-2b），`TextureManager` 是其唯一 GPU 对象生产者；
- ECS 侧门面（U-2 落点）：`ECSContext::RequestTexture(name,…)` 透传 + 本世界就绪消费；不新设独立 System——消费点并入现有帧入口（与 E-3 泵同一位置，顺序=泵之后）。

### 3.2 同步等待路径（Immediate 语义保留）

`RequestAndWait(id, 上限)`：主线程泵循环至 Ready/Broken（复用队列 `WaitTask`）。用于 Gizmo/UI/引擎内 31 处直调（P-4a）——它们不改语义，只是内部换 ID 通道。

### 3.3 就绪传播（T3 核心，现状缺口）

新增世界级（不进设备记录）：
1. **反查表**：`world.record_refs: TextureAssetID → ValueArray<MaterialRuntimeSlotId>`，slot 解析成功注册引用时登记、slot 释放时摘除；
2. **就绪批**：manager 每帧输出 `GetNewlyReadyBatch()`（仅"从无到有变 Ready"的记录，**值变才非空**，E-8 教训）；Array 记录额外携带**本帧新增就绪层掩码**（v2/§2.2），广播粒度到层；
3. 世界消费：对批内 id 查反查表 → 对应 slot 置脏（复用现有 `InvalidateRecipeRuntime` 的 `reset_row_in_place=true` 形态：program 不变、只重物化绑定）；Array 引用按 `array_layer` 与就绪层掩码**求交**——只脏"本帧该层新就绪"的 slot；
4. `authored_generation` 不动——纹理就绪不是作者变更，是**第二个代际源**（E-7）。

判据：Loading→Ready 的实体在 **≤1 帧** 内出现在渲染结果且日志 `Required texture handle missing` 命中数 = 0（该分支删除）。

### 3.4 线程契约（v2 新增，T4 前提）

| 边界 | 契约 |
|---|---|
| 用户/ECS 入口 | `Request / RequestIntoArray / GetState / GetInfo / 就绪批消费` = **`[MainThreadOnly]`**（入口断言 `HGL_ASSERT_IS_MAIN_THREAD`）；记录表与双索引无锁 |
| worker 池 | 只消费待解码池：读文件、解头、解像素。**像素落点默认 = staging ring 映射块**：`StagingRingBuffer::Allocate` 是纯 CPU 偏移记账 + 互斥锁（环缓冲启动期一次性建好，无 Vulkan 对象创建 ⇒ 不违反 R-1 铁律）；取段失败（满）回退 CPU 块——**背压退化，不是兼容分支** |
| 主线程泵 | 一切 VkImage / tci / Enqueue / `UpdateTextureHandle`（E-3/E-4 机制原样） |

零拷贝通路队列里现成：`DispatchGpuTransferDma` 的 `tci->buffer` 整条接管分支（源码注释"零拷贝直通"）。T4 做的事 = 把"buffer 在调用线程现取现填"挪成"worker 取环段直接填"，**Enqueue 之后的一切不变**。

---

## 4. 分阶段实施

> 切分原则（用户工作流）：**每批自身可编译 + 测试绿**；API 变更与全部调用点同批（故 T2 体量最大）。每阶段给回滚 tag。

### T1 —— 记录层立起（纯加法，不动任何现有 API）
- **内容**：新增 `TextureAssetManager.{h,cpp}`（记录表、Request/GetState/GetInfo/GetNewlyReadyBatch、两张占位图、文件名归一化、双索引去重；`TextureAssetID` 即不透明强类型 §2.1）；挂 GraphicsContext；**给 `AllocateHandle` 补容量 fail-fast** + 主线程 id 登记与入口断言宏；`Request` 内部即"AllocateHandle(loading) + 同步建 tci + 入队 + 泵后热换"。新增契约台架 `TestTextureAsset.cpp`（`src/ecs/support/`，宏开关仿 `TestCSMIncrementalPass`）：同文件二次 Request 同 id；占位初绑可查；Broken（不存在文件）态与错误占位；info 入队前可读。
- **判据**：台架全 PASS；`CSM_SELFCHECK=1` CascadeShadowMap 摘要 FNV 不变（纯加法证明）；现有上传三契约（ATS）基线逐项一致。
- **回滚**：revert 即净删除，无触点残留。

### T2 —— ID 迁移批（API 变更 + 全调用点，一批完成）
- **内容**：旁表条目换 `TextureAssetID`（§2.4）；`MaterialData::SetTextureResource` 指针重载整删、新增 id 重载（U-4）；`RenderPrimitiveCollectSystem` 的 Acquire/materialize 链改 id 直取 handle（删 `BuildTextureResourceId`、`resource_id` 匹配、rid→handle 映射、"DeferredResource" 字符串比对分支）；配方 `RecipeTextureBinding.resource_id` 字段**保留但改由加载器在登记时填 id**（TOML 里写的仍是文件名——解析属未来，见 §1 非目标）；示例材质链 ≈25 文件 83 处 `SetTextureResource` 改 `Request()` 一行；引擎内非材质直调按 P-4a 不动。
- **判据**：grep 全仓 `MaterialTextureAuthoringResource` 条目内 `Texture \*|resource_id` = 0；`AsyncTextureUpload` 示例改用新 API 且 150 帧冒烟 rc=0；CascadeShadowMap/TextureQuad/TextureRectArray/AlphaTestShadow `CSM_SELFCHECK` 式帧摘要判据重建并 PASS（新 FNV 常量按实跑录）。
- **回滚**：本批是**不可逆点**（动全局结构），回滚 = revert 整个 merge commit；tag `pre-T2-id-migration`。

### T3 —— 就绪传播 + 重试语义
- **内容**：§3.3 三件套（反查表/就绪批/slot 置脏）；Loading 静默（P-3b 下本应无此分支——显式核对收集链无任何"未就绪"错误路径）；Broken 一次性告警闩（模式同 `disabled_light_reported`，`MainLightShadowSystem.cpp`）。
- **判据**：台架新增用例——先 Request 后挂材质，Ready 帧起 ≤1 帧物化 handle 与记录 id 相等；全程 `GLogError` 计数 = 0（加载中）；就绪批连续两帧空 ⇒ 零重解析（E-8 自证）。
- **回滚**：纯增量消费点，revert 安全。

### T4 —— 解码 worker 化（真异步兑现）
- **内容**：拆 `VkTextureLoader` 为 CPU 半（读文件+解头+解像素，零 Vulkan 调用）与主线程半（建 tci/入队）；worker 用 `CMCore/inc/hgl/thread/Workflow.h` 池 + `SpscQueue` 回投；**像素交接默认 ring 直写**（§3.4）：worker 经锁取 `StagingRingBuffer::Allocate` 段、解码产出直写其映射块，主线程泵只记 offset 入队——命中队列现成的 `tci->buffer` 零拷贝直通分支，**消掉主线程 4K/HDR 大缓冲 memcpy 尖峰**；环分配失败 = CPU 块 + 主线程拷贝的背压退化路径；新增 `Decoding` 态与待解码池内存预算（默认 256MB，待拍板随实现）。**Immediate 优先级绕过 worker 直走 T1 同步路径**。
- **判据**：主线程单帧内不再出现文件 IO（`RenderDoc`/`ETW` 目视一次即可，判据以日志为主：worker 领取/回投时间戳均在 Tick 之外）；台架用例"同帧 Request 100 文件 → 泵 N 帧后全 Ready"无卡顿断言改为状态断言；上传三契约基线不劣化。
- **回滚**：`TextureAssetManager` 加环境开关退回同步解码路径（属应急阀，非兼容层——一版后删）。

### T5 —— Sampler 命名化 + 数组逐层 + 收尾
- **内容**：sampler 记录（名字→`VkSamplerCreateInfo` 预设，绑定现有 bindless binding=1 索引体系，`ShadowPCFSampler 4u` 编译期宏不变）；旁表 `Sampler*` 删除 ⇒ **材质条目全 POD 达成**；数组纹理：`CreateTexture2DArrayAsset(name, w,h,layers,fmt,mips)` 记录 kind=Array，逐层 `RequestIntoArray(array_id, layer, src_file)`（内部走 `VKTexture2DArrayLoader.cpp:LoadTexture2DLayerFromFile` 的 mip/fail-fast 策略，不动）；就绪按 `layer_ready_mask` **逐位**置位、广播与 slot 置脏到层粒度（§2.2/§3.3）；`TextureArrayStreaming` 示例迁移验证。
- **判据**：旁表 grep 指针类型 = 0；IconWall 全 50 层流式上墙 rc=0；`grep -rn "SetTextureResource.*Texture\s*\*" example/ src/` = 0。
- **回滚**：sampler 与数组两卡独立提交，可单 revert。

---

## 5. 接线面

### 5.1 新增文件

| 文件 | 内容 | 引入阶段 |
|---|---|---|
| `inc/hgl/graph/module/TextureAssetManager.h` | §2 全部类型 + API | T1 |
| `src/SceneGraph/module/TextureAssetManager.cpp` | 记录表、Request、泵后扫描、就绪批 | T1 |
| `src/ecs/tests/TestTextureAsset.cpp` | 契约台架（宏开关 + 注册） | T1，逐阶段追加用例 |

### 5.2 改动触点（缺一不可）

| 文件:符号 | 改动 | 阶段 | 为什么缺一不可 |
|---|---|---|---|
| `GraphicsContext`（`inc/hgl/graph/core/GraphicsContext.h`） | 持有 TextureAssetManager + 访问器 | T1 | 记录表设备级归属点（P-2） |
| `TextureManager::CreateTexture2DFromFileAsync`（`src/SceneGraph/texture/VKTexture2DLoader.cpp`） | 被 manager 复用为同步建 tci 通道；T4 拆 CPU/GPU 半 | T1/T4 | 解码唯一现场；不拆则"异步"仍是假的 |
| `ECSContext::BeginManagedRenderFrame`（`src/ecs/core/Context.cpp`） | 泵后接"就绪批消费" | T3 | E-3 泵所在=唯一合法驱动点 |
| `MaterialAuthoringSideTable.h:MaterialTextureAuthoringResource` | 指针/字符串 → id | T2（sampler T5） | 数据化本体 |
| `MaterialData.{h,cpp}:SetTextureResource/GetTextureResource` | id 重载替代指针重载 | T2 | API 与调用点同批（切分铁律） |
| `RenderPrimitiveCollectSystem.cpp:AcquireDeferredResources/materialize` | id 直取 handle；删字符串匹配与空指针分支 | T2/T3 | E-6 的错误风暴源头在此 |
| `RenderSceneUBOSystem:RegisterTextureResource/materialization_resource_handles` | rid→handle 映射退役（记录 id==handle） | T2 | 留双源=违反唯一真值 |
| `MaterialRuntimeTable.h:MaterialRuntimeSlot` | 引用登记进反查表 + 脏标记入口 | T3 | 就绪传播的落点 |
| `mtl::RecipeTextureBinding.resource_id`（`MaterialRecipe.h`） | 语义改为"加载器登记后的文件名字段"，匹配判据换成 id | T2 | 不删则旁表删了它仍在比对 |
| `example/Texture/AsyncTextureUpload.cpp` | 重写为 Request/id 演示（30 帧轮询段作废 → 记录状态查询） | T2 | 原演示的 callback 通道即被记录层吸收 |
| `example/Texture/TextureArrayStreaming.cpp` | T5 数组逐层 API 迁移 | T5 | 逐层语义唯一台架 |
| 示例材质链 ≈25 文件 83 处 `SetTextureResource` | 机械替换 `LoadTexture2D` → `Request` | T2 | 调用点同批 |

---

## 6. 验证矩阵（全部可自动判定）

| # | target | 判据 | 阶段 |
|---|---|---|---|
| V1 | `TestTextureAsset` | 去重/占位/Broken/info/就绪批 用例全 PASS，计数写死断言 | T1/T3 |
| V2 | grep `resource_id\|BuildTextureResourceId` | 旁表/收集链命中 = 0（配方字段按 T2 语义豁免注记） | T2 |
| V3 | grep `Texture \*`（`MaterialAuthoringSideTable.h`） | 0 命中（T5 后 `Sampler` 亦 0） | T2/T5 |
| V4 | `AsyncTextureUpload` rc | 150 帧 rc=0，日志 0 error | T2 |
| V5 | 帧摘要自检（`FrameDigest`，`example/Shared/`） | CascadeShadowMap/TextureQuad/AlphaTestShadow 新 FNV 常量录后连续两次运行一致 | T2/T3 |
| V6 | 加载中日志 | 挂 required 未就绪场景下 `GLogError` 计数 = 0 | T3 |
| V7 | 就绪时延 | 台架断言 Ready→物化 ≤1 帧 | T3 |
| V8 | 主线程 IO | T4 后 worker 时间戳日志齐全、Immediate 走同步路径用例 PASS | T4 |
| V9 | 上传三契约（ATS）+ `TestCSMIncrementalPass` | 与既有基线逐项一致（各阶段复测） | 全阶段 |
| V10 | IconWall | 50/50 层上墙 rc=0；台架断言"前 N 层就绪 ⇒ 仅前 N 格被脏"（逐层掩码粒度，无越层触发） | T5 |
| V11 | handle 容量 | 台架循环 Request 至 `kMax` ⇒ `AllocateHandle` 返回 0 + 恰 1 条错误（无越界写）；强类型封装后 grep「ID 裸数值算术/比较」在 manager 外命中 = 0 | T1 |
| V12 | T4 零拷贝 | 日志断言 ring 直写命中率（除退化用例）= 100%；主线程泵单帧耗时不劣于 T3 基线 | T4 |

---

## 7. 风险与默认选择

| # | 风险 | 默认选择 |
|---|---|---|
| R-1 | worker 触碰 Vulkan 对象（staging/DeviceBuffer/manager 容器）引发线程安全深坑 | T4 铁律：worker 零 Vulkan；产出仅 CPU 像素块+头信息，主线程泵打包入队（E-4 泵不动） |
| R-2 | 就绪广播每帧触发全量重解析（E-8 同族事故） | 就绪批"值变才非空"；反查表按 slot 精确置脏；V6/V7 兜住 |
| R-3 | 文件名归一化双口径 ⇒ 同图双记录双上传 | 归一化唯一函数 + 台架断言"两种写法同 id" |
| R-4 | T2 体量失控（83+ 调用点） | 机械替换脚本化（全命中才落盘，见技能 crlf-safe-editing 批量替换规程）；材质链与示例拆两个可独立编译的提交序列、同一批验证 |
| R-5 | 记录常驻显存无界（P-6 未做） | 本计划资产量级可控（示例/单机场景）；`Request` 计数日志暴露增长，触发条件写死在 P-6 |
| R-6 | `auto_mipmaps` 分支依赖设备查询（`CheckFormatSupport`） | 支持位是设备静态量：启动期一次求得后缓存进 manager，worker 期纯 CPU 判定 |
| R-7 | **`kMax=8192` 句柄耗尽，且现状 `AllocateHandle` 无上限守卫**（溢出 = 描述符缓冲越界写、静默 VU；`RegisterTexture` 有去重缓存、`AllocateHandle` 没有 ⇒ "每记录一句柄"下资产数直贴天花板） | T1 补 fail-fast（耗尽返回 0 + 一次性错误，V11 判据）；容量上调 = descriptor pool/layout 改动，**与 P-6（驱逐 + slot/世代复用）同批议**——"记录常驻 + 每记录一句柄"在几千图规模有余量，项目规模资产表必须先解驱逐 |
| R-8 | ring 直写的跨线程锁争用 | 取段 = 纯偏移记账，临界区 O(1) 无分配；worker 数 × 每帧投递量级下争用可忽略；退化路径保留（§3.4），V12 兜性能回归 |

---

## 8. 变更记录

| 旧说法 | 新结论 | 原因 |
|---|---|---|
| v1：`using TextureAssetID = uint32_t`（裸别名） | v2：不透明强类型 struct | 评审点 1。复核加剧：`kMax=8192` 属实且 `AllocateHandle` **无上限守卫**（E-2/R-7）——裸数值依赖 + 静默越界写双重不可接受；封死数值语义，P-6 时代只换内部 |
| v1：T4 像素交接触主线程 `CreateTransferSourceBuffer` + memcpy | v2：worker 直写 staging ring 映射块，命中队列既有"零拷贝直通"分支 | 评审点 2。4K/HDR 的主线程拷贝尖峰正是本事项要消灭的东西；ring 取段是纯 CPU 记账，不违反 R-1"worker 零 Vulkan 对象创建" |
| v1：`Request()` 线程约束未明文 | v2：`[MainThreadOnly]` + 入口断言（§3.4），记录表无锁 | 评审点 3。选断言而非加锁：ECS 与用户调用今天全在主线程，断言在开发期即抓违例，锁是给不存在的并发付的税 |
| v1：Array 记录只有整体 state | v2：`layer_ready_mask` + 逐层广播/置脏（§2.2/§3.3/T5） | 评审点 4。粒度定在"层"：单图 state 等整图（语义正确），**引用不整图等待**（IconWall 逐格上墙保留）；层间互不撕裂（每层独立拷贝单元），>64 层掩码扩展容器留实现期 |
| v1：§2.1 要点引用"风险 R-5" | 更正为 R-3 | 自纠：风险表里归一化双口径 = R-3，R-5 = 记录常驻无界 |

挂账：T5 前旁表仍余 `Sampler*` 一处指针（§2.4 注），届时清零。

---

## 附录 A：关键文件索引

- 队列机制真源：`doc/async-texture-upload-and-bindless.md`；`inc/hgl/graph/module/TextureUploadQueue.h`、`TextureUploadTask.h`
- bindless：`inc/hgl/vk/VKBindlessTextureManager.h`（AllocateHandle/UpdateTextureHandle）
- 解码现场：`src/SceneGraph/texture/VKTextureLoader.h`、`VKTexture2DLoader.cpp`、`VKTexture2DArrayLoader.cpp`
- 材质侧：`inc/hgl/ecs/components/MaterialData.h`、`inc/hgl/ecs/support/MaterialAuthoringSideTable.h`、`MaterialRuntimeTable.h`
- 收集链：`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp`、`src/ecs/support/RenderResource.cpp`
- 帧入口：`src/ecs/core/Context.cpp:BeginManagedRenderFrame`
- 线程件：`CMCore/inc/hgl/thread/Workflow.h`、`SpscQueue.h`
- 台架样板：`src/ecs/tests/TestCSMIncrementalPass.cpp`；帧摘要 `example/Shared/FrameDigest.h`

## 附录 B：术语

- **记录（record）**：以归一化文件名为身份、以 bindless handle 为运行时 ID 的常驻纹理资产条目。
- **就绪批（newly-ready batch）**：设备级每帧输出、仅含"本帧从无到有变 Ready"的记录 ID 列表。
- **反查表（record_refs）**：世界级 `record id → 引用它的材质 slot` 索引，T3 引入。
