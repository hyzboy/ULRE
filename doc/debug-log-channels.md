# 日志分类与 DEBUG 通道开关

本文是 `inc/hgl/common/DebugChannel.h` 的配套说明，也是**全工程 DEBUG 门开关的唯一口径**。

## 1. 为什么要有它

改造前的问题（都是实测踩到的）：

1. 环境变量名散落，语义重叠——`PBR_PERF` 与 `PREPASS_TIMING` 都是计时，各自一个名字。
2. 有的门写在函数体里，**每次调用都 `getenv`**（`TextRenderPipeline::Render()` 每帧一次、
   `RenderPrimitiveCollectSystem` 每个图元一次）。
3. 新增诊断点时没有地方可挂，只能再想一个变量名，几年下来攒了十几个。
4. 一部分高频日志根本**没有门**，正常渲染时也一直刷（`[SkyRoute]`、`[LineStats]`、
   `[RenderBufferUpload] skip`、`[ColorPaletteSystem] Flush`、`[GlobalSSBOBufferRegistry] 行写入`）。

现在：**一张 32 位通道掩码 + 一个主环境变量 `ULRE_DEBUG` + 全工程唯一的 `getenv` 出口**。

## 2. 用法

```cpp
#include <hgl/common/DebugChannel.h>

if (ULRE_DEBUG_ON(Batch))
    GLogInfo("[T3i][TRACE] ...");
```

- 判定本身只读一个函数级 `static` 掩码，**不做 `getenv`**；参数求值被 `if` 挡住，
  所以"先判定、后组串"是必须的写法。
- 开关是**运行时**的，Release 同样有效。这是与 `LogVerbose` / `LogDebug` 的关键区别
  ——那两个在非 `_DEBUG` 下被 `CMCore/inc/hgl/log/Log.h` 展开成 `{}`，线上排障用不了。

## 3. 通道表

| 令牌 | 枚举 | 归类 | 覆盖内容 |
|------|------|------|----------|
| `material`  | `Material`  | 材质 | 材质定义解析、配方解析、模板路由输入 |
| `route`     | `Route`     | 材质 | 渲染模板路由决策（`[SkyRoute]`） |
| `program`   | `Program`   | 材质 | 材质程序获取、清单解析、SPV 缓存命中 |
| `shadergen` | `ShaderGen` | 材质 | 生成器内部：相位、模块注入、varying、GLSL 落盘 |
| `binding`   | `Binding`   | 绑定 | 材质行、SSBO 绑定、纹理引用落盘（`[MaterialTextureReferences]`、`[BatchTextureRef]`、`[ArenaTrace]`） |
| `fallback`  | `Fallback`  | 绑定 | 错配回退与 Checkboard 兜底的细节（错误本身始终输出，不受门控） |
| `pipeline`  | `Pipeline`  | 渲染 | PSO 创建 / 复用 / 缓存查找（`[RenderPass::CreatePipeline]`、`[PipelineResolver]`） |
| `collect`   | `Collect`   | 渲染 | 图元收集明细 |
| `batch`     | `Batch`     | 渲染 | 批次接管、间接绘制（`[T3i][TRACE]`、`[IndirectMeshDraw]`） |
| `cull`      | `Cull`      | 渲染 | 剔除派发、CPU 参考剔除对拍 |
| `culldump`  | `CullDump`  | 渲染 | 剔除 SSBO 与计数回读转储（**会插同步等待**，只在台架用） |
| `shadow`    | `Shadow`    | 渲染 | 阴影 / CSM 逐 pass 诊断（`[S6-PASS]`、`[S6-COLLECT]`） |
| `line`      | `Line`      | 渲染 | 线渲染（`[LineStats]`） |
| `text`      | `Text`      | 渲染 | 文本渲染（`[TextTrace]`） |
| `transform` | `Transform` | 数据 | 变换提交、脏标记（编译期宏 `HGL_TRANSFORM_DEBUG_LOGGING` 的运行时替代） |
| `upload`    | `Upload`    | 数据 | 缓冲上传、资源行同步（`[RenderBufferUpload]`、`[ColorPaletteSystem] Flush`、`GeometryAABB 行写入`） |
| `perf`      | `Perf`      | 计时 | `[PBR-PERF]`、`[PREPASS-TIMING]` |
| `vksync`    | `VkSync`    | Vulkan | 队列 / fence / 信号量（`[QUEUE][SYNC]`） |

**只增不改**：新增通道加在 `Channel::Count` 之前；令牌一经发布不再改名。
上限 32 个（掩码是 `uint32_t`，`DebugChannel.h` 有 `static_assert`）。

## 4. `ULRE_DEBUG` 语法

```
set ULRE_DEBUG=batch,cull        # 只开这两个
set ULRE_DEBUG=all               # 全开
set ULRE_DEBUG=all,-vksync       # 全开但关掉 vksync
set ULRE_DEBUG=none              # 全关（等价不设，便于脚本里显式清空）
```

- 分隔符：`,` `;` `|` 空格 TAB，随意混用。
- 前缀 `+` 是加法（默认），`-` 是减法。
- `all` 与 `*` 等价；`none` 清空已有掩码。
- 未知令牌：打一条 `GLogWarning` 后忽略，不影响其余令牌生效。

## 5. 旧变量迁移表

`ULRE_DEBUG` **一旦设置就是唯一权威来源**，旧变量被完全忽略；未设置时才回落旧变量。
这样避免"新旧语义打架"这种最难查的错。旧名字永远保留（脚本不用改）。

| 旧变量 | 等价新写法 | 备注 |
|--------|-----------|------|
| `ULRE_ARENA_DEBUG`   | `ULRE_DEBUG=binding,batch,collect,text` | 原本一个变量干了四类的活，现已拆分 |
| `CSM_PASS_LOG`       | `ULRE_DEBUG=shadow`   | |
| `ECS_TAKEOVER_TRACE` | `ULRE_DEBUG=batch,cull` | |
| `CULL_SSBO_DUMP`     | `ULRE_DEBUG=culldump` | |
| `PBR_PERF`           | `ULRE_DEBUG=perf`     | |
| `PREPASS_TIMING`     | `ULRE_DEBUG=perf`     | 数值（打印间隔）仍读 `PREPASS_TIMING=<N>` |
| `VK_SYNC_TRACE`      | `ULRE_DEBUG=vksync`   | |
| `ULRE_DUMP_GLSL`     | `ULRE_DEBUG=shadergen`| **不再需要 `_DEBUG` 构建**，Release 也能落盘 GLSL |

## 6. 不属于本机制的开关（明确排除）

这些**不是**日志开关，改它们不会改变日志量，因此保留原样：

| 变量 | 性质 |
|------|------|
| `HGL_FORCE_DESCRIPTOR_POOL` / `HGL_DISABLE_DESCRIPTOR_BUFFER` / `ENABLE_VULKAN_RENDERDOC_CAPTURE` | 设备创建期的行为开关（`VKDeviceCreater.cpp`），跨平台宏双路径（`GetEnvironmentVariableA` + `getenv`） |
| `ULRE_SHADERLIBRARY_PATH` / `ULRE_SHADER_CACHE_PATH` | 路径覆盖（`ShaderLibraryPath.h` / `ShaderCacheRoot.h`，宽字符） |
| `ULRE_SHADER_CACHE_MODE` | 缓存模式（`readonly` / `ro`）取值语义 |
| 各示例自己的 `GC_*` / `PG_*` / `PBR_*` / `CSM_*` / `AM_*` / `CV_*` / `LS_*` / `ATS_*` / `DWS_*` / `EF_*` / `SKYMIN_*` / `IBLENV_*` / `RTT*_SELFCHECK` | 示例的相机定位、定帧退出、自检参数，被既有测试脚本按名调用 |

值语义的环境变量（`PREPASS_TIMING=<N>` 的间隔、缓存模式字面量）统一通过
`hgl::debug::EnvValue(name)` 读取，避免再在别处出现裸 `getenv`。

## 7. 按场景推荐的组合

| 症状 | 打开 |
|------|------|
| 材质不生效 / 贴图没上 / 走了兜底 | `binding,material,route,fallback` |
| 材质编译或程序没拿到 | `program,shadergen` |
| 每帧重复建 PSO | `pipeline` |
| 批次没接管 / 间接绘制没发生 | `batch,cull` |
| 剔除结果和 CPU 参考不一致 | `cull`（要逐槽比对再加 `culldump`，**会插同步等待**） |
| 阴影帧间闪烁 / 条带差异 | `shadow` |
| 卡顿但不知道卡在哪 | `perf` |
| 线 / 文字不显示 | `line` / `text` |
| 变换没跟上 | `transform` |
| 缓冲内容像是没刷上去 | `upload` |
| 队列 / fence 死锁 | `vksync` |

## 8. 日志等级分工（分类的另一半）

门管"要不要说"，等级管"说多重"。约定：

| 等级 | 用途 | 是否受门控 |
|------|------|-----------|
| `Error` | 契约/ABI 违约、绑定无效、必需资源缺失、回退发生 | **否**，始终输出 |
| `Warning` | 可继续但可疑（未知令牌、路径不合法、忽略的配置） | **否** |
| `Info` | 生命周期与就绪（`Initialize*` / `就绪` / `created` / 注册表清单） | **否**，一次性 |
| `Info`（经过门） | 逐帧 / 逐图元 / 逐批的诊断追踪 | **是** |
| `Verbose` / `Debug` | 开发期细粒度 | 非 `_DEBUG` 编译期裁掉；线上排障请改用通道 + `Info` |

判据很简单：**这条日志在正常跑 1000 帧后还会刷屏吗？** 会，就必须套门。

## 9. 新增诊断点的流程

1. 先看 `Channel` 里有没有合适的通道；没有就新增（加在 `Count` 前 + 表里加一行 + 本文加一行）。
2. 写 `if (ULRE_DEBUG_ON(Xxx)) { ... }`，**不要把字符串拼装放在 `if` 外面**。
3. 不要在函数内做 `static const bool s = ULRE_DEBUG_ON(...)` 缓存——掩码本身已经是
   `static`，多一层缓存只会在测试里用 `SetEnabled()` 时失效。
4. 单测要在进程内强开某通道：`hgl::debug::SetEnabled(Channel::Xxx, true)`，
   结束时 `ClearOverrides()`（见 `DebugChannel.h` 的 `OverrideKnown/OverrideMask`）。

## 10. 相关文件

- `inc/hgl/common/DebugChannel.h` —— 实现与通道表
- `inc/hgl/ecs/support/PerfTimer.h` —— `perf` 通道的消费方范式（每 240 帧一窗均值）
- `doc/future/shadergen-material-chain-review.md` —— 材质链缺陷闭环与本次日志降噪的验收记录
