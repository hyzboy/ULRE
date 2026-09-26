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
| 示例**干净退出**时泄漏 + CRT abort（退出码 3） | `CascadeShadowMap` 关窗后：133 条 `[LEAK]`（TransferSrcBuffer 27 / Texture_ 21 / UBO:ShadowUBO 8 / Shader stage 7 / VAB_ 6 / SSBO:ECS:Batch:* …）+ `BufferManager::Release` 清理 → 弹 "Microsoft Visual C++ Runtime Library" abort 对话框，进程退出码 **3** | 2026-09-26 冒烟时发现（两次复现）。**与 D1/D8 无关**：示例自身未改，D8 diff 零资源生命周期行（已 grep 核对）。`shutdown_object_tracker` 只 delete tracker、不 abort ⇒ 来源在设备/静态析构期。**注意**：拿 CSM 示例做冒烟时退出码 3 不是回归信号，判据是日志内容（路由/缓存/0 VUID） |

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

### D2. pipeline 缓存键纳入 shader 内容（新卡②）+ 同构排查（新卡③）

- **现状**：`PipelineResolver::HashShaderStages` 用 VkShaderModule **指针值**
  做 `FinalPipelineKey::shader_stages_hash`——module 销毁后新建同地址会错误
  命中缓存（与已修复的 `resolvedRuntimePipelineMap` 无 program 键控同构）。
- **做法**：ShaderModule 创建时算一次 SPIRV 内容 hash 存下来，key 用内容
  hash；顺带排查 LineRenderPipeline/TextRenderPipeline 等自持 pipeline
  缓存的同类模式。
- **规模**：核心 ~20 行 + 排查半天。

### D3. receive_shadow / bias_multiplier 落地或删除（A5）

- **现状**：`ShadowComponent` 四旋钮中 `receive_shadow`/`bias_multiplier`
  零消费者（`CanCastShadow`/`GetShadowMaxDistance` 已消费）；文档
  （shadow-component-and-automated-pipeline-design.md:19）把它们写成已生效。
- **做法**：二选一——接收侧开关需接 shader/材质行（成本高于预期则先删
  字段留接口，文档同步标注）。
- **触发条件**：需要决策。规模：落地 ~2-3 天，删除 ~1 小时。

### D4. TransformComponent 同值短路（新卡①）

- **现状**：`SetLocalPosition` 等 setter 无同值短路，每帧重复 set 同值会被
  A3 链判为变更 → 静态级联每帧全量重绘（示例网格吸附已修，引擎级未修）；
  同时它也是"每帧全量上传静态矩阵"的隐性带宽浪费源。
- **做法**：三 setter 加同值短路（语义变更，需排查依赖"重复 set 触发 dirty"
  的调用点）。
- **规模**：~20 行 + 排查半天。**触发条件**：出现静态级联每帧重绘且日志指向
  同值 set 的场景（SKILL §9 已有诊断条目）。

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

### D9. 行未就绪跳过路径的告警与收敛（A1-4 残留）

- **现状**：`RenderPrimitiveCollectSystem` 阴影分支对 "masked caster 行未就绪"
  的处理是静默 `BumpStaticSceneRevision()` + `continue`——设计意图是首帧收敛
  （下帧行就绪即恢复）。但若 forward 链对该 primitive **持续**失败/行永不就绪，
  就退化为"每帧 bump → 静态级联每帧全量重画"（`100% Cached` 再不出现）且
  该 caster 的影子长期缺席，全程无日志。
- **做法**：该分支加一次性告警（每材质一次，含 primitive 名与原因）+ 收敛
  上限（或仅当 `last_materialize_epoch != 0` 时才 bump，避免首帧前的空转）。
- **规模**：~20 行。**触发条件**：随下一次阴影/物化链改动。

### 留置

- **T5 ScenePipelineMode 空壳**：用户明确留置（未实现模式不加告警）。

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
  → D2（pipeline 键正确性）→
  D9（行未就绪路径告警/收敛）→ D3/D4（决策项）→ **T8 量测** →
  T6 拆分 → A6 合并。D 线与 A 线 A1/A7（提交原语/in-flight 槽）强相关：
  A6 的 4 次全槽排空问题在 A1 的 per-frame 多份化落地后可能自然消失，
  两者做前先对齐。
