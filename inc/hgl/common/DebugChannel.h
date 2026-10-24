#pragma once

// ============================================================================
// 文件级 DEBUG 通道开关（由仓库根目录的 ULRE_DEBUG.toml 驱动）
//
// 背景：此前每个诊断点各自现读一个环境变量当开关，导致
//   1) 变量名散落、语义重叠（PBR_PERF / PREPASS_TIMING 都是计时）
//   2) 有的写在函数体内、每次调用都重新读一次环境（每帧数万次）
//   3) 新增诊断点必须再想一个名字
// 本接口把它们收敛成一张 **32 位通道掩码**；掩码来源是配置文件
// `ULRE_DEBUG.toml`（TOML），全工程只有 DebugChannel.cpp 读该文件。
//
// 用法（代码侧不变）：
//   if (ULRE_DEBUG_ON(Batch))
//       GLogInfo(u8"[T3i] ...");
//
// 打开方式（编辑 ULRE_DEBUG.toml）：
//   [channels]
//   batch  = true
//   cull   = true
//
// 也接受字符串形式（等价于旧 `ULRE_DEBUG=...` 的语法）：
//   channels = "all,-vksync"      # all / * 全开，none 全关，- 减法，+ 加法
//
// 查找顺序（找到第一个即用）：当前工作目录 → 可执行程序目录 →
// 由这两处逐级向上（最多 64 级）。文件不存在 / 解析失败 ⇒ 全关。
//
// 注意：开关是**运行时**的，Release 构建同样有效（这是与 LogVerbose / LogDebug
//       的关键区别——那两个在非 _DEBUG 下被编译掉，无法用于线上排障）。
//
// 本头只放**类型与声明**：找文件 / 读文件 / 解析 TOML / 告警的实现全部在
// src/Vulkan/Debug/DebugChannel.cpp（唯一 TU）。因此诊断点所在的 TU 不必再
// 吃 toml11、文件 IO 与日志头。分类口径见 doc/debug-log-channels.md。
// ============================================================================

#include<cstdint>

#include<hgl/type/String.h>

namespace hgl::debug
{
    /**
     * 诊断通道。**只增不改**：新增通道加在 Count 之前，令牌（token）一经发布不再改。
     */
    enum class Channel : uint32_t
    {
        Material = 0,   ///< 材质定义解析 / 配方解析 / 模板路由输入
        Route,          ///< 渲染模板路由决策（[SkyRoute]）
        Program,        ///< 材质程序获取 / 清单解析 / SPV 缓存命中
        ShaderGen,      ///< 生成器内部：相位、模块注入、varying、GLSL 落盘
        Binding,        ///< 材质行 / SSBO 绑定 / 纹理引用落盘
        Fallback,       ///< 错配回退（含 Checkboard 兜底）细节
        Pipeline,       ///< PSO 创建 / 复用 / 缓存查找
        Collect,        ///< 图元收集明细
        Batch,          ///< 批次接管 / 间接绘制 / [T3i] 追踪
        Cull,           ///< 剔除派发与 CPU 参考剔除对拍
        CullDump,       ///< 剔除 SSBO / 计数回读转储
        Shadow,         ///< 阴影 / CSM 逐 pass 诊断
        Line,           ///< 线渲染
        Text,           ///< 文本渲染
        Transform,      ///< 变换提交 / 脏标记
        Upload,         ///< 缓冲上传 / 资源行同步
        Perf,           ///< 计时（原 PBR_PERF / PREPASS_TIMING）
        VkSync,         ///< Vulkan 队列 / fence / 信号量

        Count
    };

    constexpr uint32_t kChannelCount = static_cast<uint32_t>(Channel::Count);

    /// 通道表就是一个 uint32 位掩码，超过 32 个必须改成多字
    static_assert(kChannelCount <= 32, "debug channel count exceeds 32-bit mask");

    constexpr uint32_t Bit(Channel c)
    {
        return 1u << static_cast<uint32_t>(c);
    }

    /// 配置文件：仓库根目录（= 通常的运行工作目录）下的这一个文件
    constexpr const char *kDebugConfigFilename = "ULRE_DEBUG.toml";

    struct ChannelInfo
    {
        const char *token;  ///< ULRE_DEBUG.toml 里使用的键名（小写，无分隔符）
        const char *title;  ///< 说明
    };

    /// 通道表（kChannelCount 项，下标 = Channel 的枚举值）
    const ChannelInfo *GetChannelTable();

    /// 供错误提示使用：拼出全部可用令牌
    AnsiString MakeChannelTokenList();

    /// 通道是否打开。判定本身只是读一个 static 掩码，无系统调用。
    bool IsEnabled(Channel channel);

    /// 程序内强制覆盖（测试用）：SetEnabled 后该通道不再看配置文件
    void SetEnabled(Channel channel, bool enabled);

    /// 清掉全部 SetEnabled 覆盖，恢复按配置文件判定
    void ClearOverrides();

    /// 当前全部打开通道的掩码
    uint32_t GetEnabledMask();

    /**
     * 值语义配置的读取口（如 `[perf] prepass_interval`），集中在此避免各自读文件。
     * 缺失或类型不对 ⇒ 返回默认值。
     */
    int64_t ConfigInt(const char *table, const char *key, int64_t default_value);

    /// 启动时打印一行启用的通道清单，便于日志归档时回看当时开了什么
    void LogEnabledChannels(const char *tag);
}//namespace hgl::debug

/**
 * 通道开关判定。**不要**在热路径用 `hgl::debug::IsEnabled()` 之后又去构造函数、
 * 拼字符串——判定本身只是读一个 static 掩码，但真正的开销在参数准备上，
 * 所以宏形式（先判定、后求值）比先组串再判断更省。
 */
#define ULRE_DEBUG_ON(channel)  (::hgl::debug::IsEnabled(::hgl::debug::Channel::channel))
