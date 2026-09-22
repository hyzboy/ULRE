# 远期工作清单（Backlog）

> 本文档收录两轮 ECS/WorkObject 深度清理（2026-09-20 ~ 09-22，批次 1–5d，
> 提交 ffea8bb78…e417d5ab3）之后**有意留下**的远期项：要么需要设计讨论，
> 要么有明确的触发条件（等用例出现再做）。每项标注现状、做法、触发条件
> 与规模。完成一项划掉一项，不再回写正文细节——实现说明属于提交信息。
>
> 关联文档：`doc/render-target-standardization-design.md`（RT 标准化主线）。

## A 线：渲染架构演进

### A1. GPU 提交原语升级（semaphore 链）

- **现状**：三个前置缺陷——`DeviceQueue::Submit` 只支持单个 wait semaphore；
  `SwapchainRenderTarget::Submit(Semaphore*)` 显式忽略外部 wait_sem；
  离屏 `RenderTargetData::Submit` 在 `wait_sem==nullptr` 时不 signal。
  当前离屏逐帧同步是 CPU 侧 fence 等待（5a，`RenderTo` 提交后
  `rt->WaitFence()`），每帧一次 CPU-GPU 同步点。
- **做法**：wait semaphore 列表化（或 timeline semaphore）；离屏提交恒
  signal；`RenderTo` 把待等信号量传递给主帧提交。
- **规模**：6–7 文件，~150 行，触及队列提交原语。
- **触发条件**：出现多个逐帧离屏 RT（fence 等待逐个累积）或级联 pass 链。
  单 shadow map 场景 fence 方案已够。

### A2. RenderPassRequest（RT 标准化收官）

- **现状**：`RenderTo(rt, clear, dt)` 无 camera_override——ShadowMap 靠
  两相机 matrix_dirty 脏标记 hack 切换（`ActivateCamera`）；RenderTo 期间
  viewport/aspect 归属未定义（示例手动 `SetViewportInfo` 防污染）；
  CameraSystem 单份共享 camera_data/camera_info 是多相机问题的共同根源。
- **做法**：`RenderPassRequest{world, target, clear, camera_override,
  viewport}` 作为 pass 级一等描述（设计文档 §3.4 已有规划）；ShadowMap
  的 hack 与 viewport 防护随之删除。
- **规模**：Context/CameraSystem，~100–150 行。
- **触发条件**：任意多相机/多视口特性之前；A3/A4 的前置。
- **性价比最高的入口**：验证用例（ShadowMap）现成。

### A3. RenderGraph 跨 RT pass 链

- **现状**：`RenderGraph::Pass::renderTarget` 是死字段（执行器只
  LogWarning 不生效）；真正多段渲染需要命令缓冲跨 RT 切换点
  （EndRendering/BeginRendering）+ 跨 RT 提交同步（依赖 A1）。
- **触发条件**：后处理链、多视口同帧输出等用例出现时。

### A4. Shadow 的材质/光照集成

- **现状**（`example/Basic/ShadowMap.cpp` 文件头 TODO a–d 仍有效）：
  - Shadow UBO 缺失——示例靠"ViewModel 看原点 + shader 重建光源相机"
    绕过 light VP（Scene UBO 五个 binding 需加 Shadow）；
  - `ShaderLibrary/shadow/` 只有 identity.glsl（`GetShadowFactor` 恒 1.0）；
  - `forward_lit.glsl` 主流程不调 GetShadowFactor，LightingInput 无 shadow 字段；
  - `lit.material.toml` 无 shadow_map 槽——阴影只在专用 ShadowReceiver
    材质生效，环上网格互不投影（"看起来对、其实不全对"）。
- **做法**：SceneBinding 加 Shadow UBO → shadow provider（PCF）→
  forward_lit 模板接线 → lit 材质加槽。纯 ShaderLibrary/材质线，
  与 RT 层解耦。
- **触发条件**：随时可做；建议在 A2 之后（light VP 进 UBO 后一并清理
  示例 hack）。

### A5. 深度比较采样

- **现状**：shadow map 只保证普通采样（深度统一转 SHADER_READ_ONLY，
  与 bindless descriptor 的 imageLayout 绑定）；`sampler2DShadow` 需要比较
  采样器与 DEPTH_STENCIL_READ_ONLY_OPTIMAL 路径。
- **触发条件**：A4 做 PCF 软阴影时一并。

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

### A8. CameraInfo 全局 SSBO 化与 PushConstants 索引（多相机同帧并发）

- **现状**：CameraInfo 目前作为全局 Scene 集（set=0）的固定 UBO（单份绑定），
  多相机/子 pass（如 shadow map、反射、多视口）需在 pass 间串行覆盖写入并排空/同步。
- **做法**：将所有 CameraInfo 注册到 `GlobalSSBOBufferRegistry` 中的全局 SSBO 数组，
  渲染各 pass 或 draw item 时通过 `vkCmdPushConstants` 传递 `camera_id`，
  shader 索引读取对应相机的矩阵与视口参数，彻底解耦 pass 间覆写和 UBO 资源串扰。
- **规模**：影响面广，涉及 ShaderGen（UBO 语义转 SSBO）、材质管线 Layout、
  PushConstants 布局与相关 RenderSystem 改造。
- **触发条件**：当场景需要多视口同帧输出、并发多相机渲染或消除 pass 间相机 UBO 同步点时。

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
