#pragma once

// ============================================================================
// 文件级 DEBUG 通道开关（全工程唯一环境变量读取口）
//
// 背景：此前每个诊断点各自 `getenv("XXX")`，导致
//   1) 环境变量名散落、语义重叠（PBR_PERF / PREPASS_TIMING 都是计时）
//   2) 有的写在函数体内每次调用都 getenv（每帧数万次）
//   3) 新增诊断点必须再想一个名字
// 本头把它们收敛成一张 **32 位通道掩码**，全工程只有这一处 getenv。
//
// 用法：
//   if (ULRE_DEBUG_ON(Batch))
//       GLogInfo(u8"[T3i] ...");
//
// 打开方式（任选）：
//   set ULRE_DEBUG=batch,cull          -- 只开这两个
//   set ULRE_DEBUG=all                 -- 全开
//   set ULRE_DEBUG=all,-vksync         -- 全开但关掉 vksync
//   旧变量（ULRE_ARENA_DEBUG / CSM_PASS_LOG / ECS_TAKEOVER_TRACE /
//           CULL_SSBO_DUMP / PBR_PERF / PREPASS_TIMING / VK_SYNC_TRACE /
//           ULRE_DUMP_GLSL）继续生效，见 GetLegacyAliasTable()。
//   ULRE_DEBUG 一旦设置就是唯一权威来源，旧变量被忽略。
//
// 注意：开关是**运行时**的，Release 构建同样有效（这是与 LogVerbose / LogDebug
//       的关键区别——那两个在非 _DEBUG 下被编译掉，无法用于线上排障）。
// ============================================================================

#include<cstdlib>
#include<cstdint>

#include<hgl/log/Log.h>

namespace hgl::debug
{
    /**
     * 诊断通道。**只增不改**：新增通道加在 Count 之前，令牌（token）一经发布不再改。
     * 分类口径见 doc/debug-log-channels.md。
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
        Transform,      ///< 变换提交 / 脏标记（编译期宏 HGL_TRANSFORM_DEBUG_LOGGING 的运行时替代）
        Upload,         ///< 缓冲上传 / 资源行同步
        Perf,           ///< 计时（PBR_PERF / PREPASS_TIMING 归此处）
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

    /// 主开关环境变量名
    constexpr const char *kDebugEnvironmentVariable = "ULRE_DEBUG";

    struct ChannelInfo
    {
        const char *token;  ///< ULRE_DEBUG 里使用的令牌（小写，无分隔符）
        const char *title;  ///< 说明
    };

    inline const ChannelInfo *GetChannelTable()
    {
        static const ChannelInfo table[kChannelCount] =
        {
            { "material",  "材质定义/配方解析、模板路由输入" },
            { "route",     "渲染模板路由决策 [SkyRoute]" },
            { "program",   "材质程序获取、清单解析、SPV 缓存命中" },
            { "shadergen", "生成器内部：相位/模块注入/varying/GLSL 落盘" },
            { "binding",   "材质行、SSBO 绑定、纹理引用落盘" },
            { "fallback",  "错配回退与 Checkboard 兜底细节" },
            { "pipeline",  "PSO 创建/复用/缓存查找" },
            { "collect",   "图元收集明细" },
            { "batch",     "批次接管、间接绘制 [T3i]" },
            { "cull",      "剔除派发、CPU 参考剔除对拍" },
            { "culldump",  "剔除 SSBO 与计数回读转储" },
            { "shadow",    "阴影/CSM 逐 pass 诊断" },
            { "line",      "线渲染" },
            { "text",      "文本渲染" },
            { "transform", "变换提交、脏标记" },
            { "upload",    "缓冲上传、资源行同步" },
            { "perf",      "计时（PBR_PERF / PREPASS_TIMING）" },
            { "vksync",    "Vulkan 队列/fence/信号量" },
        };
        return table;
    }

    struct LegacyAlias
    {
        const char *variable;   ///< 旧环境变量名
        uint32_t    channels;   ///< 映射到的通道位
        const char *note;       ///< 迁移说明
    };

    inline const LegacyAlias *GetLegacyAliasTable(uint32_t &count)
    {
        static const LegacyAlias table[] =
        {
            { "ULRE_ARENA_DEBUG",   Bit(Channel::Binding) | Bit(Channel::Batch)
                                  | Bit(Channel::Collect) | Bit(Channel::Text),
              "已拆分：binding,batch,collect,text" },
            { "CSM_PASS_LOG",       Bit(Channel::Shadow),                    "= shadow" },
            { "ECS_TAKEOVER_TRACE", Bit(Channel::Batch) | Bit(Channel::Cull), "= batch,cull" },
            { "CULL_SSBO_DUMP",     Bit(Channel::CullDump),                  "= culldump" },
            { "PBR_PERF",           Bit(Channel::Perf),                      "= perf" },
            { "PREPASS_TIMING",     Bit(Channel::Perf),                      "= perf" },
            { "VK_SYNC_TRACE",      Bit(Channel::VkSync),                    "= vksync" },
            { "ULRE_DUMP_GLSL",     Bit(Channel::ShaderGen),                 "= shadergen" },
        };
        count = static_cast<uint32_t>(sizeof(table) / sizeof(table[0]));
        return table;
    }

    namespace detail
    {
        inline const char *Env(const char *name)
        {
            return std::getenv(name);
        }

        inline bool IsSeparator(char c)
        {
            return c == ',' || c == ';' || c == '|' || c == ' ' || c == '\t';
        }

        inline bool TokenEqual(const char *begin, const char *end, const char *literal)
        {
            while (begin != end && *literal)
                if (*begin++ != *literal++)
                    return false;
            return begin == end && *literal == '\0';
        }

        inline int32_t IndexOfToken(const char *begin, const char *end)
        {
            const ChannelInfo *table = GetChannelTable();

            for (uint32_t i = 0; i < kChannelCount; ++i)
                if (TokenEqual(begin, end, table[i].token))
                    return static_cast<int32_t>(i);

            return -1;
        }

        /// `"0"` / `""` / `"false"` / `"no"` 视为关闭，其余非空值视为开启
        inline bool IsTruthy(const char *value)
        {
            if (!value || !*value)
                return false;
            if (value[0] == '0' && value[1] == '\0')
                return false;
            if ((value[0] == 'f' || value[0] == 'F')
                && (value[1] == 'a' || value[1] == 'A'))
                return false;
            if ((value[0] == 'n' || value[0] == 'N')
                && (value[1] == 'o' || value[1] == 'O'))
                return false;
            return true;
        }

        inline uint32_t AllChannelsMask()
        {
            return (kChannelCount >= 32) ? ~0u : ((1u << kChannelCount) - 1u);
        }
    }

    inline uint32_t LegacyAliasMask()
    {
        uint32_t alias_count = 0;
        const LegacyAlias *table = GetLegacyAliasTable(alias_count);

        uint32_t mask = 0;

        for (uint32_t i = 0; i < alias_count; ++i)
            if (detail::IsTruthy(detail::Env(table[i].variable)))
                mask |= table[i].channels;

        return mask;
    }

    /// 解析 `ULRE_DEBUG`。`all` / `*` 全开，`none` 全关，前缀 `-` 是减法、`+` 是加法。
    inline uint32_t ParseEnvironmentMask()
    {
        const char *master = detail::Env(kDebugEnvironmentVariable);

        if (!master)
            return LegacyAliasMask();

        uint32_t mask = 0;
        const char *cursor = master;
        int32_t sign = 1;

        while (*cursor)
        {
            if (detail::IsSeparator(*cursor))
            {
                ++cursor;
                sign = 1;
                continue;
            }

            if (*cursor == '+')
            {
                ++cursor;
                sign = 1;
                continue;
            }

            if (*cursor == '-')
            {
                ++cursor;
                sign = -1;
                continue;
            }

            const char *begin = cursor;

            while (*cursor && !detail::IsSeparator(*cursor))
                ++cursor;

            const char *end = cursor;

            if (detail::TokenEqual(begin, end, "all") || detail::TokenEqual(begin, end, "*"))
            {
                const uint32_t all = detail::AllChannelsMask();
                mask = (sign > 0) ? (mask | all) : (mask & ~all);
                continue;
            }

            if (detail::TokenEqual(begin, end, "none"))
            {
                mask = 0;
                continue;
            }

            const int32_t index = detail::IndexOfToken(begin, end);

            if (index < 0)
            {
                GLogWarning("[" "ULRE_DEBUG" "] 未知通道 '%.*s'，已忽略（可用令牌见 hgl/common/DebugChannel.h）",
                            int(end - begin), begin);
                continue;
            }

            const uint32_t bit = 1u << static_cast<uint32_t>(index);
            mask = (sign > 0) ? (mask | bit) : (mask & ~bit);
        }

        return mask;
    }

    /// 环境变量决定的掩码，整个进程只解析一次
    inline uint32_t &EnvironmentMaskRef()
    {
        static uint32_t mask = ParseEnvironmentMask();
        return mask;
    }

    /// 程序内强制覆盖（测试用）：SetEnabled 后该通道不再看环境变量
    inline uint32_t &OverrideMaskRef()
    {
        static uint32_t mask = 0;
        return mask;
    }

    inline uint32_t &OverrideKnownRef()
    {
        static uint32_t known = 0;
        return known;
    }

    inline void SetEnabled(Channel channel, bool enabled)
    {
        const uint32_t bit = Bit(channel);

        OverrideKnownRef() |= bit;

        if (enabled)
            OverrideMaskRef() |= bit;
        else
            OverrideMaskRef() &= ~bit;
    }

    inline void ClearOverrides()
    {
        OverrideKnownRef() = 0;
        OverrideMaskRef() = 0;
    }

    inline bool IsEnabled(Channel channel)
    {
        const uint32_t bit = Bit(channel);

        if (OverrideKnownRef() & bit)
            return (OverrideMaskRef() & bit) != 0;

        return (EnvironmentMaskRef() & bit) != 0;
    }

    inline uint32_t GetEnabledMask()
    {
        uint32_t mask = 0;

        for (uint32_t i = 0; i < kChannelCount; ++i)
            if (IsEnabled(static_cast<Channel>(i)))
                mask |= (1u << i);

        return mask;
    }

    /// 值语义环境变量的读取口（如 PREPASS_TIMING 的打印间隔），集中在此避免再散 getenv
    inline const char *EnvValue(const char *name)
    {
        return detail::Env(name);
    }

    /// 启动时打印一行启用的通道清单，便于日志归档时回看当时开了什么
    inline void LogEnabledChannels(const char *tag)
    {
        const uint32_t mask = GetEnabledMask();

        if (mask == 0)
            return;

        AnsiString list;
        const ChannelInfo *table = GetChannelTable();

        for (uint32_t i = 0; i < kChannelCount; ++i)
        {
            if ((mask & (1u << i)) == 0)
                continue;

            if (list.Length() > 0)
                list += ",";

            list += table[i].token;
        }

        GLogInfo("[DebugChannel] %s 启用通道：%s", tag ? tag : "", list.c_str());
    }

    /// 供错误提示使用：拼出全部可用令牌
    inline AnsiString MakeChannelTokenList()
    {
        AnsiString list;
        const ChannelInfo *table = GetChannelTable();

        for (uint32_t i = 0; i < kChannelCount; ++i)
        {
            if (i)
                list += ",";

            list += table[i].token;
        }

        return list;
    }
}//namespace hgl::debug

/**
 * 通道开关判定。**不要**在热路径用 `hgl::debug::IsEnabled()` 之后又去构造函数、
 * 拼字符串——判定本身只是读一个 static 掩码，但真正的开销在参数准备上，
 * 所以宏形式（先判定、后求值）比先组串再判断更省。
 */
#define ULRE_DEBUG_ON(channel)  (::hgl::debug::IsEnabled(::hgl::debug::Channel::channel))
