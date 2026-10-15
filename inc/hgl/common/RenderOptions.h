/**
 * RenderOptions.h —— **ULRE 编译期开关总台账**（2026-10-13 起）
 *
 * 本项目一切「编译期开关 / 容量常量」集中在此声明与定默认值：要找某个开关、想知道它默认
 * 关还是开、想知道**怎么打开它** ⇒ 只看本文件，不用全仓翻。
 * CMake 侧只允许**覆盖**（`-D<名字>=...` / 对应 option）；别处不得再 `#define` 这些名字。
 *
 * 三条规矩：
 *  1) **所有开关永远有定义，值域只有 0/1** ⇒ 判断一律写 `#if XXX`。
 *     **禁 `#ifdef XXX` / `#if defined(XXX)`**——值为 0 时 `defined()` 依然为真。
 *     实测泄漏：`DefaultSystems.cpp` 曾写 `defined(ULRE_ECS_DEBUG_API)`，把 `LineStatsSystem`
 *     在 Release 里也注册了（生产没做到零开销）。
 *  2) **打开方式写在每条的注释里**（CMake option / `-D` 名 / 目标名）。
 *  3) 只读判断建议用同名 `kXxx`/`constexpr` 走 `if constexpr`（两侧都编译 ⇒ 不会 rot）；
 *     只有「整段不该存在」（如打印、对拍块）才用 `#if`。
 *
 * 平台宏不在本台账：`HGL_OS` / `HGL_OS_Windows` / `HGL_OS_Linux` / `HGL_OS_Android` 由
 * CMCoreType 子仓提供。
 *
 * ══════════════════════════════════════════════════════════════════════════════
 * §1 帧槽 / 资源容量常量（**值不许改**：动它会改 GPU 行布局与 ring 深度）
 * ══════════════════════════════════════════════════════════════════════════════
 */
#pragma once

/**
 * per-frame 数据槽空间（A1/A7）：所有「每帧重写、GPU 异步读」的 CPU 侧数据
 * （L2W ring、CameraInfo 行、Viewport UBO、阴影 UBO …）共用这一段索引空间，
 * 索引一律取 `IRenderTarget::GetCurrentFrameIndex()`。
 *
 * 划分（固定上限，不为极端场景预留增长，超出即 fail-fast）：
 *   [0, HGL_FRAME_SLOT_MAIN)            —— 交换链主帧槽（= image_count，须 < MAIN）
 *   [HGL_FRAME_SLOT_MAIN, TOTAL)        —— 离屏 pass 槽（按 RT 顺序整带分配）
 *
 * 主帧槽与离屏槽**不相交**是关键：改造前离屏 RT 恒返回 0，prepass 的每帧数据
 * 会写坏在途主帧的槽 0（旧实现靠 RenderTo 开头的全槽排空兜住，见
 * RenderSystemCore::BeginFrame 的 T10 注释）。
 */
#ifndef HGL_FRAME_SLOT_TOTAL
#define HGL_FRAME_SLOT_TOTAL 8
#endif

#ifndef HGL_FRAME_SLOT_MAIN
#define HGL_FRAME_SLOT_MAIN 4
#endif

/**
 * L2W（local→world 矩阵 ring）深度。
 *
 * 必须与 per-frame 槽空间**一致**：L2W ring 的索引就是 frame_slot（主帧 0..3、离屏 4..7）。
 * ring 深度小于槽总数时（历史上是 3），离屏槽 4..7 取模后会与主帧槽别名（4%3=1、5%3=2、
 * 6%3=0、7%3=1），prepass 覆写主帧在途的矩阵 ⇒ 主帧 draw 读到退化矩阵、整帧只剩清屏色
 * （间歇性、竞态：实测 D3 转储三帧恒为清屏色 mean_lum=200.0）。
 */
#ifndef HGL_L2W_RING_FRAMES
#define HGL_L2W_RING_FRAMES HGL_FRAME_SLOT_TOTAL
#endif

/* ══════════════════════════════════════════════════════════════════════════════
 * §2 校验面开关（对拍器 / 诊断打印）
 *
 * 默认规则一律是 **AUTO：Debug 开、Release 关**（按 `NDEBUG` 判定；MSVC Release 定义 NDEBUG）。
 * 覆盖方式：`-D<名字>=ON`／`=OFF`（CMake 侧见 `src/ecs/CMakeLists.txt`）。
 * 只有**对拍与诊断**可以挂在这里；功能分支不许用这些开关门住。
 * ⚠ 宏开着只表示「允许付校验的钱」：Release 下 `assert()` 仍会被 NDEBUG 抹掉 ⇒
 *   判据一律用 `GLogError` + 计数/退出码，不要用 `assert`。
 * ══════════════════════════════════════════════════════════════════════════════
 */

/**
 * ECS 调试 / 校验面：对拍器（掩码↔行号、实体行表↔组件行）、越界守卫、
 * `CreateEntity`/`DestroyEntity`/`Clear` 的诊断打印。
 *
 * 打开：`cmake -DULRE_ECS_DEBUG_API=ON`（Release 下也开，用于"带校验跑一次验收"）。
 * 默认（不在 CMake 里显式开）= 本文件按配置决定：Debug 开、Release 关。
 *
 * **口径已确认（2026-10-14，用户拍）**：保留上述 AUTO 语义（option 只表示「全配置强制打开」，
 * 不再下发 `=0`，否则压过 Debug 默认）。⇒ **验收集合必须含一条 `-DULRE_ECS_DEBUG_API=ON`**
 *   （Release 下强制开）的构建 + 门/契约针/台架跑，否则这部分对拍代码 Release 侧长期不编译会 rot。
 *   配方与脚本：`scripts/ci/verify_debug_api_release.ps1`（账本 §7②）。
 */
#ifndef ULRE_ECS_DEBUG_API
    #ifdef NDEBUG
        #define ULRE_ECS_DEBUG_API 0
    #else
        #define ULRE_ECS_DEBUG_API 1
    #endif
#endif

/**
 * CPU 剔除参考台架（每 60 帧 GPU 读回对拍；读回会 stall ⇒ 生产管线不付这笔钱）。
 *
 * 打开：`cmake -DULRE_ENABLE_CPU_CULL_PARITY_TEST=ON`（`src/ecs/CMakeLists.txt:4`）。
 */
#ifndef ULRE_ENABLE_CPU_CULL_PARITY_TEST
#define ULRE_ENABLE_CPU_CULL_PARITY_TEST 0
#endif

/**
 * 渲染策略判定表 vs 参考实现的**每帧对拍**（A7a 反向守卫：不一致即 `GLogError`）。
 *
 * 打开：`-DULRE_STRATEGY_PARITY_ENABLED=ON`；`TestRenderStrategyTable` 目标强制 =1。
 * 默认 AUTO（Debug 开 / Release 关 —— Release 下整装置编成空 inline）。
 */
#ifndef ULRE_STRATEGY_PARITY_ENABLED
    #ifdef NDEBUG
        #define ULRE_STRATEGY_PARITY_ENABLED 0
    #else
        #define ULRE_STRATEGY_PARITY_ENABLED 1
    #endif
#endif

/**
 * `TransformSystem` 诊断打印（脏变换 / 静态运行时写入等逐帧追踪）。
 *
 * 打开：`-DHGL_TRANSFORM_DEBUG_LOGGING=1`；默认关（逐帧打印代价大）。
 */
#ifndef HGL_TRANSFORM_DEBUG_LOGGING
#define HGL_TRANSFORM_DEBUG_LOGGING 0
#endif

/**
 * Vulkan：DescriptorPool 兼容回退（**给 RenderDoc 用**）。
 * RenderDoc 不支持 Descriptor Buffer ⇒ 在 RenderDoc 下启动程序需要回退到传统 DescriptorPool。
 * 未来/正式版可关掉它，只保留纯 Descriptor Buffer 路径。
 *
 * 打开/关闭：`-DHGL_VK_DESCRIPTOR_POOL_FALLBACK=0` 关（默认 1 = 开，保持历史行为）。
 * 原定义点 `inc/hgl/common/RenderOptions.h` 已于 2026-10-13 收归本台帐；站点（7 档共 **22 处** `#ifdef`：`VKBindlessTextureManager.h/.cpp`、`VKDevice.h`、
 * `VKDeviceAttribute.h/.cpp`、`VKDeviceCreater.h/.cpp`）已改成 `#if`，且各自显式 include 本台帐。
 */
#ifndef HGL_VK_DESCRIPTOR_POOL_FALLBACK
#define HGL_VK_DESCRIPTOR_POOL_FALLBACK 1
#endif

/* ══════════════════════════════════════════════════════════════════════════════
 * §3 只用 CMake、无 C++ 消费者（备查；别去代码里找它们）
 * ══════════════════════════════════════════════════════════════════════════════
 * · `ULRE_ENABLE_QT`（根 `CMakeLists.txt:30`）—— 是否构建 CMQT/Qt6 模块；只影响构建图。
 * · `ULRE_ENABLE_RELEASE_SHELL` —— **已于 2026-10-13 删除**（只有 CMake 声明与
 *   `target_compile_definitions`、全仓无 C++ 消费者 = 空开关）。
 */

/* ══════════════════════════════════════════════════════════════════════════════
 * §4 处置记录（都已完成，留痕防再踩）
 * ══════════════════════════════════════════════════════════════════════════════
 * · `HGL_VK_DESCRIPTOR_POOL_FALLBACK` —— 已收归 §2（默认 1 = 开）。它是**活开关**，不是死代码：
 *   定义原在 `inc/hgl/common/RenderOptions.h`（默认开），用途见 §2。站点 7 档共 22 处已由 `#ifdef` 改 `#if`（并各自显式 include 台帐）。
 * · `ULRE_ENABLE_LINE_STATS` —— 死名（全仓仅 `DefaultSystems.cpp` 一处 `defined()`、无定义点），
 *   2026-10-13 已从代码删除。那行 `defined(...) || defined(ULRE_ECS_DEBUG_API)` 就是 §2 规矩 1) 的由来。
 * · `AssimpImport`（`src/Tools/ModelConvert/`）—— 旧转换器，与现引擎不完全兼容 ⇒ 2026-10-13
 *   已 `EXCLUDE_FROM_ALL` **屏蔽**（不参与默认构建）；其源码里的 `HGL_OS_Window`（少个 s，正确名
 *   `HGL_OS_Windows`）笔误随之不再编入，按"不处理"处置。
 */
