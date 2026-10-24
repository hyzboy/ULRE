#include<hgl/common/DebugChannel.h>

#include<string>

#include<hgl/log/Log.h>
#include<hgl/utf.h>
#include<hgl/type/Smart.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/filesystem/Path.h>
#include<hgl/filesystem/FileSystem.h>

// toml11（header-only）。**全工程只有本 TU 把 toml11 拉进 Vulkan / ECS 层**，
// 头文件侧不再引它，也别在别处再引一次。
#include<toml/toml.hpp>

namespace hgl::debug
{
    const ChannelInfo *GetChannelTable()
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
            { "perf",      "计时（prepass / pbr 性能账）" },
            { "vksync",    "Vulkan 队列/fence/信号量" },
        };
        return table;
    }

    AnsiString MakeChannelTokenList()
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

    namespace
    {
        uint32_t AllChannelsMask()
        {
            return (kChannelCount >= 32) ? ~0u : ((1u << kChannelCount) - 1u);
        }

        bool TokenEqual(const char *begin, const char *end, const char *literal)
        {
            while (begin != end && *literal)
                if (*begin++ != *literal++)
                    return false;
            return begin == end && *literal == '\0';
        }

        int32_t IndexOfToken(const char *begin, const char *end)
        {
            const ChannelInfo *table = GetChannelTable();

            for (uint32_t i = 0; i < kChannelCount; ++i)
                if (TokenEqual(begin, end, table[i].token))
                    return static_cast<int32_t>(i);

            return -1;
        }

        /// 读整个文件到 std::string（toml11 只吃 std::string，此处是唯一让步）
        bool ReadWholeFile(const OSString &path, std::string &out)
        {
            hgl::io::OpenFileInputStream opener(path);

            if (!opener)
                return false;

            const int64 size = opener->GetSize();

            if (size <= 0)
                return false;

            hgl::AutoDeleteArray<char> buffer(static_cast<size_t>(size) + 1);

            if (!buffer || opener->Read(buffer.data(), size) != size)
                return false;

            buffer[size_t(size)] = 0;
            out.assign(buffer.data(), size_t(size));
            return true;
        }

        OSString JoinFilename(const OSString &dir)
        {
            if (dir.IsEmpty())
                return OSString();

            return (hgl::filesystem::Path(dir) / ToOSString(kDebugConfigFilename)).ToOSString();
        }

        /// 从 dir 起逐级向上找；找到返回完整路径，找不到返回空
        OSString SearchUpwards(OSString dir)
        {
            for (uint32_t depth = 0; depth < 64 && !dir.IsEmpty(); ++depth)
            {
                const OSString candidate = JoinFilename(dir);

                if (!candidate.IsEmpty() && hgl::filesystem::FileExist(candidate))
                    return candidate;

                const hgl::filesystem::Path current(dir);
                const hgl::filesystem::Path parent = current.GetParent();

                if (parent == current)
                    break;

                dir = parent.ToOSString();
            }

            return OSString();
        }

        /// 查找顺序：cwd → exe 目录 → 两者各自向上。返回空 = 没有配置文件。
        OSString LocateConfigFile()
        {
            OSString cwd;
            hgl::filesystem::GetCurrentPath(cwd);

            const OSString direct = JoinFilename(cwd);

            if (!direct.IsEmpty() && hgl::filesystem::FileExist(direct))
                return direct;

            OSString program_dir;
            hgl::filesystem::GetCurrentProgramPath(program_dir);

            const OSString beside_exe = JoinFilename(program_dir);

            if (!beside_exe.IsEmpty() && hgl::filesystem::FileExist(beside_exe))
                return beside_exe;

            OSString found = SearchUpwards(cwd);

            if (!found.IsEmpty())
                return found;

            return SearchUpwards(program_dir);
        }

        /// 解析失败时给出 TOML 库自带的行号 + 原文
        void ReportSyntaxError(const OSString &path, const toml::syntax_error &error)
        {
            int line = 0;
            const char *detail = nullptr;

            if (!error.errors().empty())
            {
                const toml::error_info &info = error.errors().front();

                if (!info.locations().empty())
                {
                    line = static_cast<int>(info.locations().front().first.first_line_number());
                    detail = info.locations().front().second.c_str();
                }
                else
                    detail = info.title().c_str();
            }

            GLogWarning("[DebugChannel] %ls 语法错误（第 %d 行）：%s；全部通道按关闭处理",
                        path.c_str(), line, detail ? detail : error.what());
        }

        /// 配置文件根表，整个进程只解析一次；文件缺失或解析失败时是空表
        const toml::value &ConfigRootRef()
        {
            static const toml::value root = []() -> toml::value
            {
                const toml::value empty(toml::table{});

                const OSString path = LocateConfigFile();

                if (path.IsEmpty())
                    return empty;

                std::string text;

                if (!ReadWholeFile(path, text))
                {
                    GLogWarning("[DebugChannel] %ls 读取失败；全部通道按关闭处理", path.c_str());
                    return empty;
                }

                try
                {
                    const toml::value parsed = toml::parse_str(text);

                    if (!parsed.is_table())
                    {
                        GLogWarning("[DebugChannel] %ls 根不是表；全部通道按关闭处理", path.c_str());
                        return empty;
                    }

                    return parsed;
                }
                catch (const toml::syntax_error &error)
                {
                    ReportSyntaxError(path, error);
                    return empty;
                }
                catch (const toml::exception &error)
                {
                    GLogWarning("[DebugChannel] %ls 解析失败：%s；全部通道按关闭处理",
                                path.c_str(), error.what());
                    return empty;
                }
            }();

            return root;
        }

        /// 解析字符串形式的通道列表：`all` / `*` 全开，`none` 全关，前缀 `-` 减法、`+` 加法
        uint32_t ParseTokenMask(const char *list)
        {
            uint32_t mask = 0;

            if (!list)
                return mask;

            const char *cursor = list;
            int32_t sign = 1;

            while (*cursor)
            {
                if (*cursor == ',' || *cursor == ';' || *cursor == '|'
                 || *cursor == ' ' || *cursor == '\t')
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

                while (*cursor && *cursor != ',' && *cursor != ';' && *cursor != '|'
                                && *cursor != ' ' && *cursor != '\t')
                    ++cursor;

                const char *end = cursor;

                if (TokenEqual(begin, end, "all") || TokenEqual(begin, end, "*"))
                {
                    const uint32_t all = AllChannelsMask();
                    mask = (sign > 0) ? (mask | all) : (mask & ~all);
                    continue;
                }

                if (TokenEqual(begin, end, "none"))
                {
                    mask = 0;
                    continue;
                }

                const int32_t index = IndexOfToken(begin, end);

                if (index < 0)
                {
                    GLogWarning("[DebugChannel] %s 里未知通道 '%.*s'，已忽略（可用令牌：%s）",
                                kDebugConfigFilename, int(end - begin), begin,
                                MakeChannelTokenList().c_str());
                    continue;
                }

                const uint32_t bit = 1u << static_cast<uint32_t>(index);
                mask = (sign > 0) ? (mask | bit) : (mask & ~bit);
            }

            return mask;
        }

        /**
         * 解析 `[channels]` 表：
         *   all = true            作为基线（还可逐通道覆盖）
         *   batch = true          开
         *   vksync = false        关
         * 键名写错会告警（拼错可见），未知键不影响其它通道。
         */
        uint32_t ParseConfigMask(const toml::value &root)
        {
            if (!root.is_table() || !root.contains("channels"))
                return 0;

            const toml::value &channels = root.at("channels");

            // 字符串形式：channels = "all,-vksync"
            if (channels.is_string())
                return ParseTokenMask(channels.as_string().c_str());

            if (!channels.is_table())
            {
                GLogWarning("[DebugChannel] %s 的 channels 必须是表或字符串，已忽略", kDebugConfigFilename);
                return 0;
            }

            uint32_t mask = 0;

            if (channels.contains("all"))
            {
                const toml::value &all = channels.at("all");

                if (all.is_boolean())
                    mask = all.as_boolean() ? AllChannelsMask() : 0;
                else
                    GLogWarning("[DebugChannel] channels.all 必须是布尔值，已忽略");
            }

            const ChannelInfo *table = GetChannelTable();

            for (uint32_t i = 0; i < kChannelCount; ++i)
            {
                if (!channels.contains(table[i].token))
                    continue;

                const toml::value &value = channels.at(table[i].token);

                if (!value.is_boolean())
                {
                    GLogWarning("[DebugChannel] channels.%s 必须是布尔值（true/false），已忽略",
                                table[i].token);
                    continue;
                }

                if (value.as_boolean())
                    mask |= (1u << i);
                else
                    mask &= ~(1u << i);
            }

            // 键名写错 = 静默不生效是最难排查的一种失效 ⇒ 逐个报出来
            for (const auto &item : channels.as_table())
            {
                if (item.first == "all")
                    continue;

                if (IndexOfToken(item.first.c_str(),
                                 item.first.c_str() + item.first.size()) >= 0)
                    continue;

                GLogWarning("[DebugChannel] %s 里未知通道 'channels.%s'，已忽略（可用令牌：%s）",
                            kDebugConfigFilename, item.first.c_str(),
                            MakeChannelTokenList().c_str());
            }

            return mask;
        }

        /// 配置文件决定的掩码，整个进程只解析一次
        uint32_t &ConfigMaskRef()
        {
            static uint32_t mask = []()
            {
                const uint32_t parsed = ParseConfigMask(ConfigRootRef());

                // 开着的通道在日志里留一行，归档时能回看当时用的是哪份配置
                if (parsed != 0)
                {
                    AnsiString list;
                    const ChannelInfo *table = GetChannelTable();

                    for (uint32_t i = 0; i < kChannelCount; ++i)
                    {
                        if ((parsed & (1u << i)) == 0)
                            continue;

                        if (list.Length() > 0)
                            list += ",";

                        list += table[i].token;
                    }

                    GLogInfo("[DebugChannel] %s 启用通道：%s", kDebugConfigFilename, list.c_str());
                }

                return parsed;
            }();

            return mask;
        }

        uint32_t &OverrideMaskRef()
        {
            static uint32_t mask = 0;
            return mask;
        }

        uint32_t &OverrideKnownRef()
        {
            static uint32_t known = 0;
            return known;
        }
    }//anonymous namespace

    bool IsEnabled(Channel channel)
    {
        const uint32_t bit = Bit(channel);

        if (OverrideKnownRef() & bit)
            return (OverrideMaskRef() & bit) != 0;

        return (ConfigMaskRef() & bit) != 0;
    }

    void SetEnabled(Channel channel, bool enabled)
    {
        const uint32_t bit = Bit(channel);

        OverrideKnownRef() |= bit;

        if (enabled)
            OverrideMaskRef() |= bit;
        else
            OverrideMaskRef() &= ~bit;
    }

    void ClearOverrides()
    {
        OverrideKnownRef() = 0;
        OverrideMaskRef() = 0;
    }

    uint32_t GetEnabledMask()
    {
        uint32_t mask = 0;

        for (uint32_t i = 0; i < kChannelCount; ++i)
            if (IsEnabled(static_cast<Channel>(i)))
                mask |= (1u << i);

        return mask;
    }

    int64_t ConfigInt(const char *table, const char *key, int64_t default_value)
    {
        const toml::value &root = ConfigRootRef();

        if (!root.is_table() || !root.contains(table))
            return default_value;

        const toml::value &table_value = root.at(table);

        if (!table_value.is_table() || !table_value.contains(key))
            return default_value;

        const toml::value &value = table_value.at(key);

        if (!value.is_integer())
        {
            GLogWarning("[DebugChannel] %s.%s 必须是整数，已用默认值 %lld",
                        table, key, static_cast<long long>(default_value));
            return default_value;
        }

        return value.as_integer();
    }

    void LogEnabledChannels(const char *tag)
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
}//namespace hgl::debug
