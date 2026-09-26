# 远期工作清单（Backlog）

> 本文档收录两轮 ECS/WorkObject 深度清理（2026-09-20 ~ 09-22，批次 1–5d，
> 提交 ffea8bb78…e417d5ab3）之后**有意留下**的远期项：要么需要设计讨论，
> 要么有明确的触发条件（等用例出现再做）。每项标注现状、做法、触发条件
> 与规模。完成一项划掉一项，不再回写正文细节——实现说明属于提交信息。
>
> 关联文档：`doc/render-target-standardization-design.md`（RT 标准化主线）。

## A 线：渲染架构演进

### A1. GPU 提交原语升级（semaphore 链 / per-frame 资源多份化）

- **现状**：三个前置缺陷——`DeviceQueue::Submit` 只支持单个 wait semaphore；
  `SwapchainRenderTarget::Submit(Semaphore*)` 显式忽略外部 wait_sem；
  离屏 `RenderTargetData::Submit` 在 `wait_sem==nullptr` 时不 signal。
  **且 Camera/Viewport UBO 与 L2W ring 段是单份 host-visible 内存**
  （`CriticalPerFrame` 仅为内存分类标签，无轮转副本）——RenderTo 覆写
  它们前必须等在途主帧（上沿 fence），离屏提交后等离屏 fence 确保 GPU 消费完
  光源 UBO 后才还原主相机数据（下沿 fence），每帧 2 次 CPU-GPU 同步点即源于此。
- **做法**：wait semaphore 列表化（或 timeline semaphore）；离屏提交恒
  signal；`RenderTo` 把待等信号量传递给主帧提交。**或** camera/viewport
  UBO 与 L2W 段 per-frame 多份化（按 frame_index 轮转副本）——任一路径
  均可解除 fence 等待。
- **规模**：6–7 文件，~150 行，触及队列提交原语。
- **触发条件**：出现多个逐帧离屏 RT（fence 等待逐个累积）、级联 pass 链，
  或 shadow pass 双向 fence 等待成为帧率瓶颈时。单 shadow map 场景
  当前方案正确且可接受。

### A2. ~~RenderPassRequest（RT 标准化收官）~~ **已完成（f99f8dda1，2026-09-23）**

- **已落地**：`RenderPassRequest{target, clear, use_target_clear, delta_time, camera}` +
  `ECSContext::RenderTo(request)`（三参重载保留为包装）；CameraSystem
  pass 级相机覆盖（override_camera，RenderTo 期间只解算覆盖相机、结束
  自动恢复）；常规更新中主从相机严格隔离（仅主相机响应输入与写入共享数据）；
  viewport 随 RT 切换由 RTSystem SyncSubsystems 内建同步。
  离屏提交后立即 WaitFence 彻底根治 UBO 踩踏造成的闪烁。
  ShadowMap 已迁移（ActivateCamera hack 与 viewport 手动防护删除，
  sun 向量与迁移前一致且阴影稳定无闪烁）。
- **衍生待办**：CameraSystem 多相机共享数据（单份 camera_data）仍是
  "串行 pass"模型——真并行多视口（同帧多相机同时渲染）需要共享数据
  实例化或全局 SSBO 化（见 A8），留待 A3/多视口用例出现时评估。

### A3. RenderGraph 跨 RT pass 链

- **现状**：`RenderGraph::Pass::renderTarget` 是死字段（执行器只
  LogWarning 不生效）；真正多段渲染需要命令缓冲跨 RT 切换点
  （EndRendering/BeginRendering）+ 跨 RT 提交同步（依赖 A1）。
- **触发条件**：后处理链、多视口同帧输出等用例出现时。

### A4. ~~Shadow 的材质/光照集成~~ **已完成（c293b895c，2026-09-23）**

- **已落地**：SceneBinding 补齐 ShadowInfo UBO（Set 0, Binding 5）；新增 `pcf_shadow.glsl`；
  标准 Lit / Lit IBL 材质原生集成阴影计算，移除专用 ShadowReceiver 材质；地面与全场景网格均支持自遮挡与互投。

### A5. ~~深度比较采样~~ **已完成（2026-09-23）**

- **已落地**：`sampler.toml` 中配置 `ShadowPCF` 采样器为 Reversed-Z `GreaterOrEqual` 硬件深度比较（Linear 滤波 + ClampToEdge）；
  `bindless_textures.glsl` 补充 `Sample2DArrayShadow`；`pcf_shadow.glsl` 全面切换至硬件深度比较采样（Hardware PCF），消除软件手动比较并享受硬件 2x2 面积双线性平滑。

### A6. cubemap / CSM / MSAA

- **现状**：`CreateFBO` 的 `fb_info.layers=1`、`BeginRendering` 的
  `layerCount=1` 硬编码；`RenderTargetDesc::samples != 1` 被 `IsValid()`
  直接拒绝（不假装支持）。
- **触发条件**：点光源阴影（cubemap 6 面）、CSM（array）、前向 MSAA。
  desc 字段已预留位置。
- **CSM 滚动缓存 Step 1**：已建立固定 4 级的 `ShadowInfo` 扩展契约，
  保留旧单级字段作为 fallback，并追加级联 split、光空间 texel snap、
  环形 offset、有效区域和 CPU 缓存状态。当前尚未启用 array attachment
  或增量条带绘制；后续仍需完成 layer view、静态物体筛选和失效重建。

### A7. 离屏 RT in-flight 槽

- **现状**：5a 评估"单做不解决跨 pass 采样竞态"而搁置；fence 等待后仍
  每帧一次 CPU 等待。
- **触发条件**：与 A1 同做才有意义（GPU 侧链路通了，N 份
  cmd_buf+queue+fence 轮转才换来真重叠）。

### A8. ~~CameraInfo 全局 SSBO 化与 PushConstants 索引（多相机同帧并发）~~ **已完成（c7430f92f，2026-09-23）**

- **已落地**：在 `GlobalSSBOBufferRegistry` 中注册 CameraInfo（656B 步长），主相机固定第 0 行，从属相机按需分配；
  PushConstants `RootAddresses` 末尾加入 `camera_id` 与 `_pad_camera`（72B 自然对齐）；
  GLSL BDA `CameraInfoBufferRef` 经宏 `#define camera` 透明解引用；
  ECS `CameraComponent` / `CameraSystem` / `Context::RenderTo` 全链路打通多相机隔离与 `camera_id` 下发，彻底解耦多 Pass 相机矩阵踩踏。

## B 线：ECS/框架残余小项（低优先，顺手做）

| 项 | 现状 | 备注 |
|---|---|---|
| RenderContext 类整体移除 | 只剩 `{GraphicsContext*}` 一个指针，src 25 处纯转发 | 波及 20+ 文件系统成员；下次动系统基类时顺带 |
| 组启停双写者 | 组件挂卸按计数开关 vs scene gather 全量重算 | 5d 已消全局 enabled；剩余两套可收敛为"仅 gather" |
| `RegisterGroup` 相位区间互写无校验 | 同名组不同相位区间静默覆盖 | 5–10 行 fail-fast 校验 |
| destroy/IsDestroy 机制 | 恒 false 但 CMQT 仍 polling | 实现 `RequestExit()` 或删机制+改 CMQT |
| FreeCameraMode 空实现且为默认值 | 未设 control_mode 的相机零输入响应 | FirstPerson/LookAt 零使用，枚举可瘦身 |
| LineStatsSystem 默认注册 | 每 120 帧一条日志，`SetLogInterval` 零调用 | 降为按需注册 |
| WorkManager"序列"空架子 | `OnChangeWorkObject(old,new)` 0 重写 | 与 destroy 同属旧工作流残留 |
| TickObject 双名同物 + 继承趋零 | `hgl/type/TickObject.h` 旧版无人用仍在编译 | 跨 CMCore 子仓库 |
| AppFramework 空虚函数/死参 | OnActive/OnClose/Tick 空、`(void)argc` | 化妆级 |
| `DetachAllComponents(bool)` 参数无效 | 只进日志不进分支 | 顺手 |
| 示例**干净退出**时泄漏 + CRT abort（退出码 3） | `CascadeShadowMap` 关窗后：133 条 `[LEAK]`（TransferSrcBuffer 27 / Texture_ 21 / UBO:ShadowUBO 8 / Shader stage 7 / VAB_ 6 / SSBO:ECS:Batch:* …）+ `BufferManager::Release` 清理 → 弹 "Microsoft Visual C++ Runtime Library" abort 对话框，进程退出码 **3** | 2026-09-26 冒烟时发现（两次复现）。**与 D1/D8 无关**：示例自身未改，D8 diff 零资源生命周期行（已 grep 核对）。`shutdown_object_tracker` 只 delete tracker、不 abort ⇒ 来源在设备/静态析构期。**注意**：拿 CSM 示例做冒烟时退出码 3 不是回归信号，判据是日志内容（路由/缓存/0 VUID）。**2026-09-26 D3 冒烟第 3 次实跑 exit=0 且 0 VUID/ERROR** ⇒ 该 abort 是**间歇性**的（不是每次干净退出都触发），更不能用退出码当判据 |

## C 线：构建与文档

1. **TexConvCore 存量问题**：链接引用 `out\Windows_64_Release\TexImage.lib`，
   clean Debug 树后必失败——工具链配置问题，一次修复永久消音
   （本系列工作期间全量构建的 EXIT=1 全部源于它）。
2. **doc 十篇待更新**（逐篇清单见维护记忆）：
   - 命名类 5 篇：`GetMaterialSSBOBinding→GetGlobalSSBOBinding` 等
     （simple-sphere-material-shadergen-overview、
     ShaderGen_MaterialGLSL_DesignAnalysis、
     material-lit-recipe-to-shadergen-dataflow、ENVIRONMENT_SYSTEM、
     ShaderGen_ComposableTemplate 加"已实现"标记）；
   - 结构类 5 篇：ecs-layer-architecture-and-frame-flow（相位行号/
     RenderTo/OnRenderPass）、simple-sphere-ecs-render-chain（4-ID 基线
     标注）、gpu-driven-4id（直通已落地标注）、
     ecs/transform-data-management（传输层改 push constants）、
     ecs/primitive-geometry-vdm（结构图）。

## D 线：CSM/阴影后续（2026-09-26 review 消化 + masked 链深挖新卡）

> 来源：`doc/csm-review-2026-09-25.md`（A5/A6/A7-A10/T 系列剩余）+
> `doc/alpha-test-shadow-masked-caster-fix-chain-2026-09-26.md`（masked 链深挖
> 新卡）。快赢组（A9/A7/T10/T2/T7 + demote feature）已完成不再列。
> 深度图读回报证工具见 `example/Basic/AlphaTestShadow.cpp` 的 DumpCascadeDepth。

### ~~D1. 深度镂空判读升级为自动契约~~ ✅ 已完成（2026-09-26）

- **现状**：depth-only 通道的 FS 剥除豁免（`IsFragmentShaderRequired`）与
  masked 链各层修复目前只有手动验证（`AlphaTestShadow` + 深度图读回，
  棋盘投影填充 ~57%=镂空）。豁免被改回时契约测试全绿但功能坏。
- **交付**（两段式，原计划"全收进 `TestCSMIncrementalPass`"不可行——该可执行
  文件**无 GraphicsContext/设备**，0 处设备引用，做不了读回）：
  1. `TestCSMIncrementalPass` **Test 11**：masked 链**源码契约**（10 条 needle）
     —— 剥离点必须检查 `keep_fragment_shader`、recipe 语义（`alpha_test`/
     `dither`）必须参与判据、`fragment_shader_required` 必须有生产者、masked
     caster 模板必须真评估 alpha（`ShadowCasterMasked`/`EvalAlpha`/
     `HGLApplyAlpha`）、forward 本体必须接线 alpha test、阴影 pass 必须判定
     `MaterialRequiresRecipeRuntimeRows`。诊断信息直指"哪条判据没了+后果"。
  2. `AlphaTestShadow` **自判自检**：第 45 帧读回深度图后立即算 c0 包围盒填充率
     并打印 `[D1-CONTRACT] c0 PASS/FAIL`；`ATS_SELFCHECK=1`（或 `--selfcheck`）
     时按契约退出码结束（0=PASS / 1=FAIL）。回归门 = 一条命令看退出码。
- **判读口径（2026-09-26 实测，已在代码里写死）**：填充率必须**相对非零
  像素的包围盒**统计，不能相对整图——c0 实测包围盒 112x58（占全图仅 0.62%），
  **框内填充 57.6%**；整图口径只有 0.4%，当断言用会立刻失败。只对 c0 断言
  （c1 静态层在本场景无 caster，全空是预期值）。判读带 50-65%：实心 ~100%、
  全空 ~0% 都在带外。
- **破坏验证（已验证非空洞）**：① 测试里把某条 needle 改错 → `Test 11 Failed`
  点名文件+needle+后果，退出码 11；② 把判读带下限临时改 0.90 → `[D1-CONTRACT]
  c0 FAIL: 包围盒填充率 57.6% < 90%`，selfcheck 退出码 1。两处均已还原并复跑绿。
- **剩余**：本契约覆盖"判据链还在（源码层）+ 链路端到端生效（图像层）"。D8 已用
  其破坏验证现场裁决并落地（删程序级扫描）→ Test 11 的 needle 随之收敛为
  9 条正向（recipe 判据 + masked 链）+ 2 条禁复活（见 D8）。

### D2. pipeline 缓存键纳入 shader 内容（新卡②）+ 同构排查（新卡③） ✅ 2026-09-26

- **原状**：`PipelineResolver::HashShaderStages` 用 VkShaderModule **句柄值**做
  `FinalPipelineKey::shader_stages_hash`——句柄在 module 销毁后可被新建模块复用，
  两个不同的 shader 会算出同一个 key → 错误复用 pipeline。
- **落地（身份改用内容，两处）**：
  1. `VulkanDevice` 新增 SPIRV 内容 hash 注册表（`Register/Unregister/GetShaderModuleHash`，
     键=句柄值、值=内容 hash、带互斥）；`CreateShaderModule` 创建成功后
     `FNV1aHasher64::AppendBytes(spv_data,spv_size)` 登记，`~ShaderModule` 注销
     （防句柄复用后残留旧身份）。全仓唯一 `vkCreateShaderModule` 调用点就在此处。
  2. `HashShaderStages(device,stages)` 改为查内容 hash；查不到 → `GLogError` 点名
     句柄+stage 并返回 0（键不完整 → fail-fast 重建，绝不静默退回句柄身份）。
  3. **同构**：`PrimitiveComponent` 的已解析管线条目收敛为单 map
     `{Pipeline*, ShaderProgramKey, has_program_key}`（原先 pipeline/program 两张
     平行 map + **program 指针比较**）；复用校验改为比对结构化 digest——program
     释放后新对象落到同一地址不再被误判成"同一个 program"。
- **同构排查（全仓）**：Line/Text 自持 pipeline **无**句柄/指针键控缓存（各自持单个
  pipeline / 按 `FontSource*` 分资源，统一走 `RenderPass::CreatePipeline` → 随本卡修复）；
  `g_device_map` 在设备析构里 erase；其余 `(uintptr_t)` 命中全是命名/日志。
  **遗留（记录待办，不在本卡）**：`resolvedRuntimePipelineMap` 以 `RenderPass*` 为键、
  值为该 pass 拥有的 `Pipeline*`，而组件侧只在材质/配方变化时清空、pass 析构无人通知。
  当前 `RenderPassManager` 按结构化 key 缓存 RenderPass 且只在 `Release()` 全清 →
  运行期不复用地址，**暂无实害**；一旦 RT 在运行期重建（A6/D5 级联重配置路径）就必须补
  「查 pass 名 + 校验 pipeline 归属」或「pass 析构时失效」，否则命中悬垂 Pipeline*。
- **验证**：
  | 门 | 结果 |
  |---|---|
  | `TestCSMIncrementalPass` | exit 0；Test 11 = 11 checks；**Test 12 = 7 checks**（pipeline 键身份契约：内容 hash 登记/消费/注销/全字节覆盖 + 2 条禁复活） |
  | `ATS_SELFCHECK=1 AlphaTestShadow` | exit 0，`c0 PASS: bbox=112x58 filled=3740 57.6%`——与 D8 基线逐字节同值 = **行为 no-op** |
  | `CascadeShadowMap` 冒烟 | 路由 105/100/4、0 `[ERROR]`、0 VUID；管线 **4 Created / 205 Reuse**（与 D2 前基线同值，无膨胀无复用退化）；`has no registered SPIRV hash` = 0（登记不漏、fail-fast 未触发） |
- **破坏验证**：把 resolver 的 `module_hash` 临时改回 `(uint64_t)(uintptr_t)stages[i].module`
  → `Test 12 Failed ... 'GetShaderModuleHash(' not found in VKPipelineResolver.cpp`，
  退出码 **12**；还原后复跑绿。
- **构建事故（记录，非代码缺陷）**：给 `VKDevice.h` 加成员的那次编辑**落在构建进行中**，
  MSBuild 的 tlog 此后不再重编 `VKDevice.obj`/`VKDeviceCreater.obj`（obj mtime 比头文件新、
  内容却是旧布局）→ 构造函数没构造新成员、使用者按新布局读 → 首次建 pipeline 键走**空 vptr**
  崩（cdb 栈：`ThreadMutexLock::ThreadMutexLock` ← `VulkanDevice::RegisterShaderModuleHash`）。
  清掉整棵 `find build -type d -path "*.dir/Debug"` obj 树重编后全绿。判定法：
  `ls -la <header> <obj>` 看 obj 是否早于头文件（详见技能 crlf-safe-editing）。

### ~~D3. receive_shadow / bias_multiplier 落地~~ ✅（2026-09-26）

- **原状**：`ShadowComponent` 四旋钮中 `receive_shadow`/`bias_multiplier` 零消费者
  （`CanCastShadow`/`GetShadowMaxDistance` 已消费）；文档
  （shadow-component-and-automated-pipeline-design.md:19）把它们写成已生效。
- **裁决**：**落地**（用户拍板）——两者都是**逐图元**着色决策，既不是材质业务
  数据也不是逐批共享量，所以随 per-draw 行表到达着色器。
- **做法**：
  1. **载体**：per-draw 行 `MaterialInstanceAddresses` **8B → 16B**，新增
     `shadow_flags`（bit0 = 不接收）与 `shadow_bias_multiplier`（局部偏差倍率）；
     行结构改为**唯一真源 X 列表** `HGL_MATERIAL_INSTANCE_ADDRESSES_FIELD_LIST`
     （`inc/hgl/graph/ShaderBufferSources.h`），CPU struct 与 GLSL struct 发射
     同源遍历——新增/改名/调序字段只改一处，没有手写漂移面。
  2. **写入端**：`PrimitiveBatchPipeline` 逐图元从 `ShadowComponent` 取
     `CanReceiveShadow()` / `GetBiasMultiplier()` 写行；未挂载组件时行保持
     零值 = 引擎默认（接收 + 倍率 1.0），与 D3 之前**逐字节一致**。
  3. **着色端**：`pcf_shadow.glsl` 新增 `GetShadowReceiveParams(data_index)`
     读行；`receive=false` 直接返回 1.0（不受影）；倍率逐级乘进
     `EvalCascadePCF` 的深度 bias 与法线偏移，并沿
     `EvalPCFShadowAt → EvalCascadeChain → EvalCascadeShadowAt → EvalCascadePCF`
     逐点穿线；`forward_lighting/flat_lighting/*.glsl.tmpl/identity.glsl` 四处
     签名同步传 `data_index`。
  4. **零值语义**：行内倍率 0 视为"不调节"（回落 1.0），保证旧零值行为不变。
- **规模**：代码 11 文件（`: `ShaderBufferSources.h` / `MaterialShaderEmitter.cpp` /
  `pcf_shadow.glsl` / `forward_lighting.glsl` / `flat_lighting.glsl` /
  `forward_lit.glsl.tmpl` / `forward_unlit.glsl.tmpl` / `identity.glsl` /
  `PrimitiveBatchPipeline.cpp` / `TestCSMIncrementalPass.cpp` / `AlphaTestShadow.cpp`），
  工作区累计 diff **+650/−38**（含 `AlphaTestShadow.cpp` 与 `TestCSMIncrementalPass.cpp`
  里前序 D1/D9 的未提交改动）+ 4 份文档。
- **证据**：
  - 源码契约：`TestCSMIncrementalPass` **Test 14 = 22 checks**（行结构单源、
    发射遍历、写入端取真值、片元端早退/缩放偏差，3 条"禁复活"needle）。
  - 运行期契约（`ATS_SELFCHECK=1 AlphaTestShadow`，**逐像素对比**三帧颜色）：
    默认帧 A → `SetReceiveShadow(false)` 帧 B：受影区 **18189 px 由"灰蓝"变回
    "红地面"**、反向 0 px（影子整体消失，方向 + 位置都锁定）；
    `SetBiasMultiplier(1000×)` 帧 C：外观变化 **600662 px**（倍率确实进入偏差计算）。
  - **对照组**（`ATS_D3_NOKNOB=1`：照常读回三帧但一个旋钮都不拨）：外观变化
    **0 px** ⇒ "读回+对比"链噪声底为 0，上面的差异只能归因于旋钮。
  - **破坏验证**：把 `params.receive` 写死 `true` + 深度 bias 不乘倍率
    → 运行期 exit **1**（两项都 0 px，并提示读回无差异）、源码契约
    **Test 14 exit 14** 点名 needle；还原后复绿。
  - 回归：`ATS_SELFCHECK` c0 仍 **57.6%**、`CascadeShadowMap` 冒烟路由/统计不变。
- **踩坑**：本场景地面是**饱和红**（R≈198,G≈1,B≈31）、影子压上去呈灰蓝——
  用"亮度（RGB 均值）"判方向会得出反直觉结论（红的均值比灰影低）；
  判据必须按**通道**（"是否变红"）而不是亮度。另：交换链颜色图实际布局是
  `PRESENT_SRC_KHR`，`Texture2D::GetImageLayout()` 停在 `SHADER_READ_ONLY_OPTIMAL`，
  拿它当 oldLayout 会被校验层判非法转换（拷贝不可信）。

### ~~D4. TransformComponent 同值短路~~ ✅（2026-09-26，改判为"静态写入语义化"）

- **原状**：`SetLocalPosition` 等 setter 无条件 `MarkDirty`（版本 +1），
  `ShouldUpdateTransform` 用"版本变了"当变更判据 ⇒ **同值**写也被判为变更 ⇒
  A3 链 `BumpStaticSceneRevision()` ⇒ 整段静态矩阵重写 + 全部静态级联缓存失效
  （当帧 4 级全量重绘）+ `MarkDirty` 连带标脏整棵子树。
- **关键澄清（先量化再动手）**：动静分离本来就在，且**没有**"静态每帧刷新 TRS/L2W"
  这回事——`Movable` 每帧全量写 ring 段、`Static` 走版本门控的**增量**写
  （`TransformSystem.cpp:207-281`），静态 L2W 只在被判脏时算。所以真问题不是
  "缺同值短路"，而是**"静态被写"这件事没有值比较、也没有留痕**。
- **裁决（用户拍板）**：**A′ 语义化**——不做同值短路。理由：短路只治"同值每帧写"
  这一种写法，救不了**每帧写变化的值**这种真误用（那时值在变，短路毫无作用）；
  而这类误用的正解是引擎早已有的 `SetMobility(Mobility::Movable)`（迁到 movable
  通道：每帧 ring 写 + 动态级联）。所以把"运行期写静态"做成 **API 语义**：
  一次性告警 + 把开发者引到正解，而不是替他猜。
- **做法**：
  1. `TransformComponent` 加 `static_runtime_write_armed`/`static_runtime_write_warned`
     + `ArmStaticRuntimeWriteWarning()` / `IsStaticRuntimeWriteArmed()` /
     `HasWarnedStaticRuntimeWrite()`；`WarnStaticRuntimeWrite(what)` 一条一次性
     `GLogWarning`（含 setter 名 + 实体名 + id + 代价说明 + 正解指引）。
  2. **八条写入路径**全部留痕：`SetLocalPosition/Rotation/Scale`、`SetLocalTRS`、
     `SetWorldPosition/Rotation/Scale`、`SetParent`（改父级同样让全部静态级联失效）。
  3. arming 点放在 `TransformSystem::SubmitTransformUpdates` 的 `has_dirty_static`
     分支（**在 `transform_buffer` 早退之前** ⇒ 无图形设备的单元测试路径同样成立；
     只在真有静态变更时付 O(N)，稳态仍然早退）——语义：**搭建/首次写入不告警，
     之后的写入才被 warn**（"运行期"的判定基准就是"已被渲染侧识别过一次"）。
  4. `Movable` 组件永不 arm / 永不告警；`SetMobility(Movable)` 之后写入不再是静态写入。
- **规模**：3 文件（`TransformComponent.h/.cpp`、`TransformSystem.cpp`）+ 测试 + 文档。
- **证据**：
  - 契约：`TestCSMIncrementalPass` **Test 15 = 11 源码契约 + 5 行为**（搭建期不告警、
    消费后 arm、运行期写入告警一次且不改写入语义、Movable 不受影响、迁移后不误报；
    8 个 setter + 实现 + 守卫 + arming 共 11 条 needle）。
  - **代价实测（根因钉死）**：注入探针 `ATS_D4_PROBE=1`（每帧对**静态**地面写**同值**）跑
    15 秒 ⇒ 告警 **1 条**（每组件一次，不刷屏）+ `invalidating static cascade` **872 次**
    （≈每帧一次）+ `100% Cached` **0 行** ⇒ 静态级联缓存彻底失效。探针已还原。
  - **误报门**：`ATS_SELFCHECK=1` 正常场景告警 **0 条**；`CascadeShadowMap` 冒烟告警
    **0 条**、缓存统计与基线一致 ⇒ 现有引擎/示例代码没有运行期写静态的调用点。
- **未采用（明确记录）**：B′ 同值短路、C′ 写静态自动迁移到 Movable（渲染行为可见变化）。

### D5. A8 scissor 增量分支：实现条带滚动或删除

- **现状**：`RenderMainLightShadowPass` 的 scissor 增量分支永不执行
  （`ShadowDirtyRect` 池恒全图矩形）——与环形寻址（A 线 A6 的 CSM 部分）绑定。
- **触发条件**：决定做 Toroidal 条带滚动（保留改造）或确认长期整级重建（删除
  分支与 rect 池）。**依赖路线决策。**

### D6. prepass 入口防御（T4）

- **现状**：自动化 shadow prepass 仅在 `RenderGraph.cpp:171` 一条入口被驱动，
  新增/绕过入口时阴影静默失效（无断言无日志）。
- **做法**：挂到 `BeginManagedRenderFrame` 并显式区分主帧/离屏帧，或入口
  缺失时告警。
- **规模**：~30 行。**触发条件**：新增第二条渲染入口时。

### D7. 性能账目 → EnvironmentSystem 拆分 → 4 级联合并（T8 → T6 → A6，大组按序）

- **T8**：量测 prepass GPU 时间 / 4 次 submit+全槽排空的 CPU 等待 / masked
  caster 保留 FS 后的片元开销（A1 修复后收益才真实可测）。
- **T6**：CSM 从 `EnvironmentSystem`（瘦转发层，现堆满控制器/4 RT/bindless/
  光相机/固化防御）拆出 `MainLightShadowSystem`；A10（重复 Enable 重建
  4 RT + 头文件重量）随 RAII 一并解决。
- **A6**：4 级联合并单 command buffer / 单次 prepass（4 次 submit、4 次
  全槽排空、Update 每帧 5 次的问题一并解决）。**依赖 T8 数据决定收益**。
- **规模**：T8 半天（工具已有 DumpCascadeDepth 基础）；T6 2-3 天；A6 1-2 天。

### ~~D8. FS 剥除豁免的判据收敛~~ ✅ 已完成（2026-09-26，实测裁决：**删程序级扫描**）

- **实测证据（三层判据探针，AlphaTestShadow 实跑 `[D8-PROBE]`）**：所有程序
  `spirv-scan=0`、`text-scan=0` → `IsFragmentShaderRequired()` **恒 false**；
  `keep_fs=1` 只出现在 `recipe_at=1` 的程序上 ⇒ **唯一真正生效的判据是 recipe
  语义**，此前文档/提交声称的"SPIRV 扫描撑住 masked 影子"不成立：
  - SPIRV 扫描：`OpKill = 101`（真值 **252**）、cap `5407`（真值 **5379**）全错
    → 从不命中；且**唯一调用点在 stage 缓存命中分支**（冷缓存首编译根本
    不扫）。
  - FinalGLSL 文本扫描：结构性失明——实测 `shader-cache/stage/stage-16-*.frag`
    含 `HGLApplyAlpha(...)` 调用而 **0 处 `discard` 字面量**（include 的
    alpha_compositor 不在该文本里；glsl_len 12551/16673/16732 全部 0 命中）。
  - 且该处是**硬赋值**（`fragment_shader_required = ...find("discard")`），会把
    SPIRV 扫描的 true 覆盖成 false ⇒ **修常量也无效**，必须同时改三处。
- **裁决依据**：① 两层程序级实现在实测中都是死的；② 引擎全部路径的模板选择
  与 FS 保留用**同一输入**（normalize 后的 recipe `alpha_test`，见
  `ShaderProgramManager.cpp` 的 `masked ? ShadowCasterMasked : ShadowCasterOpaque`）
  ⇒ 未声明 alpha_test 的材质根本不会生成带 discard 的影子程序（ShadowCasterOpaque
  里没有 discard），判据完备；③ 符合零兼容偏好，不留恒假机制。
- **落地**（净 −45/+24）：删 `ScanSPVHasDiscard`、文本扫描块、
  `ShaderProgramManager::SetFragmentShaderRequired` 中转、
  `ShaderProgram::fragment_shader_required` 与 `IsFragmentShaderRequired()`；
  `RenderPass::CreatePipeline(ShaderProgram*, const MaterialRecipe&)` 判据收敛为
  `render_state.alpha_test || render_state.dither`；config 重载（line/text 颜色
  通道）显式传 `false` 并注明"depth-only 需求须走 recipe 重载"；原位置留注释
  说明被删原因（**勿再引入第二套判据**）。
- **禁复活契约**：Test 11 新增 2 条反向 needle（`ScanSPVHasDiscard`、
  `FragmentShaderRequired` 必须不存在）。
- **验收**：Debug 构建通过；`TestCSMIncrementalPass` 全绿（Test 11 = 11 checks）；
  `ATS_SELFCHECK=1` 跑 `AlphaTestShadow` → c0 仍 **57.6% PASS**（**删除 = 行为
  no-op**，实测证实）；`CascadeShadowMap` 冒烟无回归。
- **若改走"程序级扫描"**：属**重做**而非修常量——需扫在模块创建处并覆盖编译+
  缓存两条路径、OR 语义、补 `OpTerminateInvocation=4416`/
  `OpDemoteToHelperInvocation=5380`/cap `5379`，且删掉文本扫描（它只会覆盖结果）。

### D9. 行未就绪跳过路径的告警与收敛（A1-4 残留） ✅ 2026-09-26

- **原状**：`RenderPrimitiveCollectSystem` 阴影分支对 "masked caster 行未就绪" 的处理
  是**静默** `BumpStaticSceneRevision()` + `continue`（设计意图首帧收敛）；程序解析/
  几何/管线三条失败路径虽各有告警，但**逐帧刷屏**。四条路径都**无上限地每帧 bump**
  ——持续失败 = 静态级联每帧全量重画（`100% Cached` 再不出现）+ 该 caster 影子长期
  缺席，且（行未就绪那条）全程无日志。
- **落地**：per-primitive 计数 `MaterialComponent::shadow_retry_frames` +
  统一收敛入口 `RenderPrimitiveCollectSystem::AdvanceShadowRetry(material_comp, reason, primitive)`：
  1. 四条跳过/失败路径（行未就绪、程序解析、几何、管线）全部改走该入口，
     **原因字符串随调用点传入**（行未就绪 = `masked caster runtime rows not ready
     (forward chain has not materialized them)`）；程序解析失败的逐帧告警**移入
     forward 分支**，阴影侧只由收敛入口输出一条（不再双重刷屏）。
  2. 首次跳过 → `GLogWarning` **一次**（primitive 名 + 原因 + "前 120 帧每帧 bump 以收敛"）。
  3. 连续 `kShadowRetryFullBumpFrames = 120` 帧仍跳过 → `GLogError` **一次**，并把
     bump 降频为每 `kShadowRetryBumpPeriod = 60` 帧一次（**限速自愈**：不再每帧全量
     重画，但保留周期性重试，避免影子永久缺席）。
  4. 该 caster 成功产出本帧 render item（`shadow_program` 非空）→ 计数清零，
     下次失败重新告警、bump 回到每帧。
- **验证**：
  | 项 | 结果 |
  |---|---|
  | `TestCSMIncrementalPass` | exit 0；**Test 13 = 7 checks**（收敛入口/原因串/上限判定/降频公式/计数字段 + 2 条禁复活"逐帧刷屏告警"） |
  | `ATS_SELFCHECK=1 AlphaTestShadow` | exit 0，c0 仍 `57.6%`（bbox=112x58 filled=3740）；**跳过告警恰好 2 条**（MaskedCube / FallbackCube 各一条，均在第 1 帧）、达上限 0 条 → 一帧内收敛、无刷屏 |
  | `CascadeShadowMap` 冒烟 | 路由 105/100/4、0 VUID/ERROR、管线 4 Created/205 Reuse 与基线同值；告警 4 条（4 个 masked caster 各一条，首帧）、达上限 0 条、静态级联失效全程 2 次 |
  | **注入验证（门有牙）** | 把条件临时改成恒 `shadow_needs_rows`（强制持续跳过）→ **每 caster 恰好 1 条告警 + 1 条达上限报错**（共 4+4，无刷屏）；静态级联 revision 终值 **565** ≈ 4 caster × [120 + (N−120)/60]，N≈1380 次跳过帧 ⇒ 降频公式逐字命中（不限频应为 4×1380≈5520 次），即"限速自愈"确实生效。还原后复跑绿 |
- **教训**：`GLogInfo/GLogWarning/GLogError` 展开为 **`{...}` 块**（`CMCore/inc/hgl/log/Log.h:164-168`）
  → `if (x) GLogWarning(...); else ...` 触发 **error C2181: illegal else without matching if**
  （宏自带花括号 + 调用处 `;` 变空语句 ⇒ if 已闭合）。**if/else 链里用日志宏必须给分支加花括号。**
- **规模实际**：核心 ~50 行（含注释）；契约 Test 13 ~50 行。

### 留置

- **T5 ScenePipelineMode 空壳**：用户明确留置（未实现模式不加告警）。

## E 线：未来项（诊断工具下沉，待排期）

### E1. 附件读回落盘下沉为引擎基础功能 + 图像写出改用 CM2D（2026-09-26 用户留置）

- **现状**：`example/Basic/AlphaTestShadow.cpp` 自带两套一次性取证工具，形态都是
  「RT 附件 → staging buffer → CPU → **手写 BMP**」，且各自重复实现了一遍
  immediate submit / 首尾 barrier / 布局还原 / staging 生命周期：
  - `DumpCascadeDepth(rt, name, DepthFillStats*)`（`:167-322`）：D32 深度附件（DEPTH
    aspect）→ 读回 → 8bit 灰度 BMP（reversed-Z：近亮远暗，镂空=clear 值）+
    同遍算 c0 包围盒内填充率（D1 契约）；
  - `DumpColorTarget(rt, filename, out_lum)`（`:324-470`）：`rt->GetColorTexture(0)`
    （COLOR aspect）→ 读回 → BMP + 逐像素低三字节均值（D3 契约）。
  - 两处都手写 BMP 头 + 底行翻转，且都带着同一条教训：**交换链颜色图在提交时点的
    真实布局是 `PRESENT_SRC_KHR`**，`Texture2D::GetImageLayout()` 停在
    `SHADER_READ_ONLY_OPTIMAL`，拿它当 oldLayout 会被校验层判非法转换。
- **目标（两件事）**：
  1. **读回下沉**：把「immediate submit + old/new layout barrier（含提交时点真实布局
     修正）+ staging 创建/回收 + aspect 选择」封装成引擎侧一次调用
     （挂在 `RenderTarget`/`Texture2D` 上的 `ReadbackImage(aspect, ...)` 之类，
     `src/Vulkan/` 内，目前全仓只有 `VKCommandBuffer.h` 出现过
     `vkCmdCopyImageToBuffer`，没有可复用的读回工具），示例侧只留
     「取哪张图 + 怎么判读」。
  2. **图像写出改用 CM2D**：删掉示例里手写的 BMP 写入，改用
     `CM2D/inc/hgl/2d/TGA.h` 的 `hgl::bitmap::SaveTga(filename, bmp)`
     （模板适配 `hgl::bitmap::Bitmap<T,N>`：`GetData/GetWidth/GetHeight/GetChannels/
     GetChannelBits`），或 `CM2D/inc/hgl/2d/BitmapSave.h` 的
     `SaveBitmapToTGA(os, data, w, h, channels, bits)`；容器/头/字节序全部交给 CM2D，
     不再自研。
     **坑**：BMP 是底行在前，TGA 用 `TGAImageDesc::direction` 表达方向
     （0=lower-left / 1=upper-left）⇒ 迁移时必须显式选对方向，否则读回的图上下翻转
     （现有 BMP 代码正是靠「底行在前」翻转过一次）。
- **规模估计**：引擎侧读回下沉 ~150-200 行；示例删 ~250 行；CM2D 写出替换 ~30 行。
- **触发时机**：不影响引擎正确性，**不排在本轮**；下一次需要图像/深度取证时顺手做，
  或与 **T8 量测**（性能账目，届时需要多次读回取证）一并做。
- **验收**：示例删掉自带 BMP 代码后仍能产出 D1/D3 契约（`ATS_SELFCHECK=1` exit 0、
  c0 填充率 57.6%、D3 `18189 px / 600662 px` 不变）；新 API 配
  `TestCSMIncrementalPass` 源码契约或单测；`res/`（用户自管）不动。

## 关联顺序

```
A4(shadow 集成) ──建议在──► A2(RenderPassRequest) ──前置──► A3(跨RT pass链)
                                │                          │
A5(比较采样) ◄──同做───────────┘                          ▼
                                                   A1(提交原语) ──► A7(in-flight槽)
```

- **性价比最高的入口是 A2**：解锁 ShadowMap hack 清理（验证用例现成）
  与 A4 的光照矩阵通路，且不依赖任何其它项。
- B/C 线与 A 线无耦合，随手清。
- **D 线内部顺序**：~~D1~~ ✅（已锁死 masked 链：Test 11 源码契约 + AlphaTestShadow
  自判）→ ~~D8~~ ✅（实测裁决：程序级扫描恒 false → 删除，判据收敛为 recipe 语义）
  → ~~D2~~ ✅（pipeline 键改 SPIRV 内容 hash + 已解析管线身份改 program digest；
  Test 12 = 7 checks。同构遗留：`resolvedRuntimePipelineMap` 的 `RenderPass*` 键
  在**运行期 RT 重建**时才会变成悬垂，锁 A6/D5）→
  → ~~D9~~ ✅（阴影跳过路径统一收敛：一次性告警 + 120 帧上限后降频 bump，
  Test 13 = 7 checks）
  → ~~D3~~ ✅（接收侧旋钮落地：per-draw 行 8B→16B +
  行结构唯一真源 X 列表；Test 14 = 22 checks + 运行期逐像素对照/破坏验证）
  → ~~D4~~ ✅（静态写入语义化 A′：运行期写 Static 一次性告警 + 引导
  `SetMobility(Movable)`；Test 15 = 11 源码 + 5 行为契约，探针实测每帧同值写
  ⇒ 15s 内 872 次级联失效）→ **D5（决策项）** → **T8 量测** →
  T6 拆分 → A6 合并。D 线与 A 线 A1/A7（提交原语/in-flight 槽）强相关：
  A6 的 4 次全槽排空问题在 A1 的 per-frame 多份化落地后可能自然消失，
  两者做前先对齐。
