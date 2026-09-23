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
