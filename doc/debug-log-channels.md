# 日志分类与 DEBUG 通道开关

本文是 `inc/hgl/common/DebugChannel.h` 的配套说明，也是**全工程 DEBUG 门开关的唯一口径**。

## 1. 为什么要有它

改造前的问题（都是实测踩到的）：

1. 开关名散落，语义重叠——`PBR_PERF` 与 `PREPASS_TIMING` 都是计时，各自一个名字。
2. 有的门写在函数体里，**每次调用都重新读一次环境变量**（`TextRenderPipeline::Render()` 每帧一次、
   `RenderPrimitiveCollectSystem` 每个图元一次）。
3. 新增诊断点时没有地方可挂，只能再想一个变量名，几年下来攒了十几个。
4. 一部分高频日志根本**没有门**，正常渲染时也一直刷（`[SkyRoute]`、`[LineStats]`、
   `[RenderBufferUpload] skip`、`[ColorPaletteSystem] Flush`、`[GlobalSSBOBufferRegistry] 行写入`）。

现在：**一张 32 位通道掩码 + 一份仓库根 TOML 配置 `ULRE_DEBUG.toml` + 全工程唯一读取点**。

## 2. 用法

```cpp
#include <hgl/common/DebugChannel.h>

if (ULRE_DEBUG_ON(Batch))
    GLogInfo("[T3i][TRACE] ...");
```

- 判定本身只读一个函数级 `static` 掩码（进程内最多解析配置文件一次），**不做文件 IO**；
  参数求值被 `if` 挡住，所以"先判定、后组串"是必须的写法。
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

## 4. `ULRE_DEBUG.toml` 写法

配置文件放在**仓库根目录**（`ULRE_DEBUG.toml`），随代码一起进版本库；改完直接重启被调试的
exe 即可，不需要 `set` 环境变量、也不污染全局环境。

查找顺序（`detail::LocateConfigFile()`）：`cwd/ULRE_DEBUG.toml` → `exe 目录/ULRE_DEBUG.toml`
→ 从 `cwd` 逐级向上 → 从 exe 目录逐级向上（各 ≤64 级）。所以从 `build/out/...` 直接起
exe 也能找到仓库根那份。

**写法一：表形式（默认，推荐）**

```toml
[channels]
batch  = true      # 只开这两个
cull   = true
shadow = false     # 其余显式写 false，便于对照
# all = true       # 想全开就打开这行
```

**写法二：字符串形式（与早先 `ULRE_DEBUG` 环境变量的语法完全一致）**

```toml
channels = "batch,cull"          # 只开这两个
channels = "all"                 # 全开
channels = "all,-vksync"         # 全开但关掉 vksync
channels = "none"                # 全关（等价于不写）
```

- 分隔符：`,` `;` `|` 空格 TAB，随意混用；前缀 `+` 是加法（默认），`-` 是减法；
  `all` 与 `*` 等价；`none` 清空已有掩码。
- 未知**令牌**（`"batch,bogus"` 里的 `bogus`）：打一条 `GLogWarning` 后忽略，其余照常生效。
- 未知**键**（`[channels] batchh = true` 这类拼错）：逐个 `GLogWarning` 报出键名并列出可用令牌，
  不会静默吞掉——这是最容易被忽略的一类配置错误。
- 有任一通道被打开时，启动会打一行公告：`[DebugChannel] ULRE_DEBUG.toml 启用通道：batch,cull`；
  全关则完全静默。

**失败降级**：文件不存在 / 读失败 / 根不是表 / TOML 语法错误 ⇒ **全部通道按关闭处理**（日志量回落到
生产基线），语法错误额外打一条带文件路径与行号的 `GLogWarning`。调试开关自身绝不能让程序崩。
`detail::ConfigRootRef()` 是进程内**唯一**解析点（函数级 `static`）。

## 5. 旧变量已废弃

早先那 8 个环境变量（`ULRE_ARENA_DEBUG` / `CSM_PASS_LOG` / `ECS_TAKEOVER_TRACE` /
`CULL_SSBO_DUMP` / `PBR_PERF` / `PREPASS_TIMING` / `VK_SYNC_TRACE` / `ULRE_DUMP_GLSL`）
**已整条删除**，设了也不再生效。等价写法：

| 旧变量 | 现在写进 `ULRE_DEBUG.toml` 的 `[channels]` |
|--------|--------------------------------------------|
| `ULRE_ARENA_DEBUG`   | `binding = true` + `batch = true` + `collect = true` + `text = true`（原本一个变量干了四类活，现已拆分） |
| `CSM_PASS_LOG`       | `shadow = true` |
| `ECS_TAKEOVER_TRACE` | `batch = true` + `cull = true` |
| `CULL_SSBO_DUMP`     | `culldump = true` |
| `PBR_PERF`           | `perf = true` |
| `PREPASS_TIMING`     | `perf = true` + `[perf] prepass_interval = N`（打印间隔，缺省 60） |
| `VK_SYNC_TRACE`      | `vksync = true` |
| `ULRE_DUMP_GLSL`     | `shadergen = true`（**不再需要 `_DEBUG` 构建**，Release 也能落盘 GLSL） |

> 历史档案（`doc/backlog.md`、`doc/ecs-quality-audit-2026-10.md`、`doc/future/ULRE_Phase0-1_*` 等）
> 里仍会出现这些旧变量名——那是**当年测量的原始记录**，保留原样以示可追溯，不是现在可执行的用法。
> 要复现这些测量，请按上表换成 `ULRE_DEBUG.toml` 写法。

## 6. 不属于本机制的开关（明确排除）

这些**不是**日志开关，改它们不会改变日志量，因此保留原样：

| 变量 | 性质 |
|------|------|
| `HGL_FORCE_DESCRIPTOR_POOL` / `HGL_DISABLE_DESCRIPTOR_BUFFER` / `ENABLE_VULKAN_RENDERDOC_CAPTURE` | 设备创建期的行为开关（`VKDeviceCreater.cpp`），跨平台宽/窄字符两套环境读取 |
| `ULRE_SHADERLIBRARY_PATH` / `ULRE_SHADER_CACHE_PATH` | 路径覆盖（`ShaderLibraryPath.h` / `ShaderCacheRoot.h`，宽字符） |
| `ULRE_SHADER_CACHE_MODE` | 缓存模式（`readonly` / `ro`）取值语义 |
| 各示例自己的 `GC_*` / `PG_*` / `PBR_*` / `CSM_*` / `AM_*` / `CV_*` / `LS_*` / `ATS_*` / `DWS_*` / `EF_*` / `SKYMIN_*` / `IBLENV_*` / `RTT*_SELFCHECK` | 示例的相机定位、定帧退出、自检参数，被既有测试脚本按名调用 |

值语义的配置（打印间隔之类的数值）不走通道掩码，用
`hgl::debug::ConfigInt("perf", "prepass_interval", 60)` 从同一份 TOML 的表里取，
避免再在别处出现第二处环境读取。

## 7. 按场景推荐的组合

| 症状 | 打开 |
|------|------|
| 材质不生效 / 贴图没上 / 走了兜底 | `binding` + `material` + `route` + `fallback` |
| 材质编译或程序没拿到 | `program` + `shadergen` |
| 每帧重复建 PSO | `pipeline` |
| 批次没接管 / 间接绘制没发生 | `batch` + `cull` |
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

1. 先看 `Channel` 里有没有合适的通道；没有就新增（加在 `Count` 前 + 表里加一行 + 本文加一行
   + `ULRE_DEBUG.toml` 的 `[channels]` 里加一行 `xxx = false`）。
2. 写 `if (ULRE_DEBUG_ON(Xxx)) { ... }`，**不要把字符串拼装放在 `if` 外面**。
3. 不要在函数内做 `static const bool s = ULRE_DEBUG_ON(...)` 缓存——掩码本身已经是
   `static`，多一层缓存只会在测试里用 `SetEnabled()` 时失效。
4. 单测要在进程内强开某通道：`hgl::debug::SetEnabled(Channel::Xxx, true)`，
   结束时 `ClearOverrides()`（见 `DebugChannel.h` 的 `OverrideKnown/OverrideMask`）。

## 10. 相关文件

- `inc/hgl/common/DebugChannel.h` —— **只有类型与声明**（`Channel` 枚举、`ChannelInfo`、函数原型、`ULRE_DEBUG_ON` 宏）。
  使用者不需要吃 toml11 / 文件 IO / 日志头。
- `src/Debug/DebugChannel.cpp` —— 通道表与**全工程唯一的 `ULRE_DEBUG.toml` 读取点**的实体（唯一携带 toml11 的 TU）。
- `src/Debug/CMakeLists.txt` —— 独立的 `ULRE.Debug` 静态库目标。单独成库的原因见文件头注释：
  这个符号被 Vulkan / ShaderGen / SceneGraph / ECS 四个互相 include 的库（强连通分量）与只链
  `ULRE.Runtime` 的最小工具共同使用，放进任何一个现有库都会造成循环或把 toml11 传染给最小工具。
- `ULRE_DEBUG.toml` —— 仓库根的配置文件本体（带逐通道中文注释）
- `inc/hgl/ecs/support/PerfTimer.h` —— `perf` 通道的消费方范式（每 240 帧一窗均值）
- `doc/future/shadergen-material-chain-review.md` —— 材质链缺陷闭环与本次日志降噪的验收记录
