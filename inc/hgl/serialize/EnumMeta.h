#pragma once

/**
 * 枚举 / 按位标记的元信息 —— 「写一次定义，名字、说明、序列化全都自动有」。
 *
 * ── 解决的问题 ────────────────────────────────────────────────────────────
 *   引擎里大量 enum / enum class 需要在四个地方各写一套：
 *     · 代码里判断（switch）；
 *     · 把值翻成名字（日志 / TOML / JSON / GUI 下拉框）；
 *     · 把名字翻回值（配置读取）；
 *     · 给 GUI 用的中文说明。
 *   这四处一旦不同步就是标准的「加了个枚举项忘了改表」事故。
 *
 *   本文件让开发者只维护一份**登记表**：枚举声明之后紧跟一个后置宏块，
 *   元信息与 `EnumTraits<T>` 特化都由宏自动生成，不存在第二份硬编码表。
 *   具体文本格式（TOML / JSON / FlatBuffers）由 Archive 后端决定，
 *   这里只产出与格式无关的「名字 ↔ 值」映射。
 *
 * ── 用法 ──────────────────────────────────────────────────────────────────
 * @code
 *   // 普通枚举：序列化为名字字符串
 *   enum class SkyMode : uint8_t { Atmosphere, CubeMapIBL, None };
 *
 *   HGL_ENUM_BEGIN(SkyMode)
 *       HGL_ENUM_ITEM(Atmosphere, "物理大气散射天光")
 *       HGL_ENUM_ITEM(CubeMapIBL, "立方体贴图 IBL 天光")
 *       HGL_ENUM_ITEM(None,        "关闭天光")
 *   HGL_ENUM_END(SkyMode)
 *
 *   // 按位标记：序列化为名字数组，读入时按位或
 *   enum class SurfaceFlags : uint32_t
 *   {
 *       None        = 0,
 *       DoubleSided = 1u << 0,
 *       AlphaTest   = 1u << 1,
 *   };
 *
 *   HGL_FLAGS_BEGIN(SurfaceFlags)
 *       HGL_FLAG_ITEM(None,        "无标记")
 *       HGL_FLAG_ITEM(DoubleSided, "关闭背面剔除")
 *       HGL_FLAG_ITEM(AlphaTest,   "Alpha 截断测试")
 *   HGL_FLAGS_END(SurfaceFlags)
 * @endcode
 *
 * ── 硬性约定 ──────────────────────────────────────────────────────────────
 *   1. 登记块必须写在**全局作用域**，宏内部会打开 `namespace hgl::serialize`。
 *      写进别的命名空间会导致登记表挂到错误的 `EnumMeta` 上；
 *      `HGL_ENUM_END(TYPE)` / `HGL_FLAGS_END(TYPE)` 末尾的 static_assert
 *      会把这种错误在编译期直接拦下。
 *   2. `TYPE` 要写**完全限定名**（如 `hgl::graph::SkyMode`），因为登记块
 *      位于全局作用域；`END` 里重复一次是为了校验登记位置。
 *   3. 说明文字（DESC）是 UTF-8 字面量，只给 GUI 显示用，编译期不参与运算；
 *      不需要说明时传 `nullptr`。
 */

#include <hgl/type/String.h>
#include <hgl/type/Str.Comp.h>
#include <hgl/type/Str.Length.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hgl::serialize
{
    // ── 元信息数据结构 ──────────────────────────────────────────────────────

    /// 单个枚举项的元信息：C++ 名（即序列化时写入的文本）、数值、GUI 说明。
    struct EnumItemMeta
    {
        const char *name;   ///< 登记名，默认就是 C++ 枚举子名
        int64_t     value;  ///< 数值
        const char *desc;   ///< UTF-8 说明，可为 nullptr
    };

    /**
     * 枚举元信息表的主模板。
     *
     * 未登记的枚举 `defined == false`，Archive 会退回「按底层整型写入」，
     * 与旧的 `EnumTraits<T>::defined == false` 行为一致。
     */
    template <typename T>
    struct EnumMeta
    {
        static constexpr bool                defined  = false;
        static constexpr bool                is_flags = false;
        static constexpr const EnumItemMeta *Items    = nullptr;
        static constexpr std::size_t         Count    = 0;
    };

    /// 「该类型是已登记的按位标记枚举」。供自由运算符做 SFINAE 约束用。
    template <typename T>
    concept FlagEnum = EnumMeta<T>::is_flags;

    namespace detail
    {
        /// 跳过首尾空白（空格 / 制表符）。
        inline void TrimRange(const char *&text, std::size_t &length) noexcept
        {
            while (length > 0 && (text[0] == ' ' || text[0] == '\t'))
            {
                ++text;
                --length;
            }

            while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t'))
                --length;
        }

        /// 解析十进制或 `0x` 前缀的十六进制整数；不允许其它字符混入。
        inline bool ParseInt64(const char *text, std::size_t length, int64_t &out) noexcept
        {
            if (!text || length == 0)
                return false;

            std::size_t index = 0;
            bool        negative = false;

            if (text[index] == '+')
                ++index;
            else if (text[index] == '-')
            {
                negative = true;
                ++index;
            }

            if (index >= length)
                return false;

            uint64_t value = 0;
            bool     any_digit = false;

            if (length - index > 2 && text[index] == '0'
                && (text[index + 1] == 'x' || text[index + 1] == 'X'))
            {
                index += 2;

                for (; index < length; ++index)
                {
                    const char ch = text[index];
                    int        digit = -1;

                    if (ch >= '0' && ch <= '9')
                        digit = ch - '0';
                    else if (ch >= 'a' && ch <= 'f')
                        digit = ch - 'a' + 10;
                    else if (ch >= 'A' && ch <= 'F')
                        digit = ch - 'A' + 10;
                    else
                        return false;

                    value = (value << 4) | static_cast<uint64_t>(digit);
                    any_digit = true;
                }
            }
            else
            {
                for (; index < length; ++index)
                {
                    const char ch = text[index];

                    if (ch < '0' || ch > '9')
                        return false;

                    value = value * 10 + static_cast<uint64_t>(ch - '0');
                    any_digit = true;
                }
            }

            if (!any_digit)
                return false;

            out = negative ? -static_cast<int64_t>(value) : static_cast<int64_t>(value);
            return true;
        }
    }// namespace detail

    // ── 查询 ────────────────────────────────────────────────────────────────

    /// 取枚举值对应的登记名；未登记返回 nullptr。
    template <typename T>
    inline const char *EnumNameOf(const T value) noexcept
    {
        if constexpr (EnumMeta<T>::defined)
        {
            const int64_t raw = static_cast<int64_t>(value);

            for (std::size_t i = 0; i < EnumMeta<T>::Count; ++i)
                if (EnumMeta<T>::Items[i].value == raw)
                    return EnumMeta<T>::Items[i].name;
        }

        return nullptr;
    }

    /// 取枚举值对应的 GUI 说明；未登记返回 nullptr。
    template <typename T>
    inline const char *EnumDescOf(const T value) noexcept
    {
        if constexpr (EnumMeta<T>::defined)
        {
            const int64_t raw = static_cast<int64_t>(value);

            for (std::size_t i = 0; i < EnumMeta<T>::Count; ++i)
                if (EnumMeta<T>::Items[i].value == raw)
                    return EnumMeta<T>::Items[i].desc;
        }

        return nullptr;
    }

    /// 登记项总数。GUI 下拉框按此遍历，不需要另写一份表。
    template <typename T>
    inline std::size_t EnumItemCount() noexcept
    {
        if constexpr (EnumMeta<T>::defined)
            return EnumMeta<T>::Count;
        else
            return 0;
    }

    /// 按序号取登记项；越界返回 nullptr。
    template <typename T>
    inline const EnumItemMeta *EnumItemAt(const std::size_t index) noexcept
    {
        if constexpr (EnumMeta<T>::defined)
        {
            if (index < EnumMeta<T>::Count)
                return &EnumMeta<T>::Items[index];
        }

        return nullptr;
    }

    // ── 名字 → 值 ──────────────────────────────────────────────────────────

    /**
     * @brief 解析单个枚举名。
     *
     * 名字比较**大小写不敏感**；同时接受十进制与 `0x` 十六进制数字字面量，
     * 便于手写配置用数字表达、也便于读到旧格式数据。
     */
    template <typename T>
    inline bool EnumParse(const char *text, std::size_t length, T &out) noexcept
    {
        if (!text)
            return false;

        const char *cursor = text;
        detail::TrimRange(cursor, length);

        if (length == 0)
            return false;

        if constexpr (EnumMeta<T>::defined)
        {
            for (std::size_t i = 0; i < EnumMeta<T>::Count; ++i)
            {
                const EnumItemMeta &item = EnumMeta<T>::Items[i];
                const std::size_t   name_length =
                    static_cast<std::size_t>(hgl::strlen(item.name));

                if (name_length != length)
                    continue;

                if (hgl::stricmp(cursor, length, item.name, name_length) == 0)
                {
                    out = static_cast<T>(item.value);
                    return true;
                }
            }
        }

        int64_t raw = 0;

        if (detail::ParseInt64(cursor, length, raw))
        {
            out = static_cast<T>(raw);
            return true;
        }

        return false;
    }

    /// 以 NUL 结尾的字符串版本。
    template <typename T>
    inline bool EnumParse(const char *text, T &out) noexcept
    {
        if (!text)
            return false;

        return EnumParse<T>(text, static_cast<std::size_t>(hgl::strlen(text)), out);
    }

    /**
     * @brief 解析按位标记文本，形如 `"DoubleSided|AlphaTest"`。
     *
     * 分隔符接受 `|` 与 `,`，各段前后空白会被忽略；空文本视为 0。
     * 任意一段解析失败则整体失败（不做部分成功）。
     */
    template <typename T>
    inline bool EnumParseFlags(const char *text, std::size_t length, T &out) noexcept
    {
        if (!text)
            return false;

        if (length == 0)
        {
            out = static_cast<T>(0);
            return true;
        }

        int64_t raw = 0;
        bool    any = false;

        std::size_t begin = 0;

        for (std::size_t i = 0; i <= length; ++i)
        {
            const bool is_end = (i == length);
            const bool is_sep = is_end || text[i] == '|' || text[i] == ',';

            if (!is_sep)
                continue;

            if (i > begin)
            {
                T part{};

                if (!EnumParse<T>(text + begin, i - begin, part))
                    return false;

                raw |= static_cast<int64_t>(part);
                any = true;
            }

            begin = i + 1;
        }

        if (!any)
        {
            out = static_cast<T>(0);
            return true;
        }

        out = static_cast<T>(raw);
        return true;
    }

    /// 以 NUL 结尾的字符串版本。
    template <typename T>
    inline bool EnumParseFlags(const char *text, T &out) noexcept
    {
        if (!text)
            return false;

        return EnumParseFlags<T>(text, static_cast<std::size_t>(hgl::strlen(text)), out);
    }

    // ── 值 → 名字集合（按位标记用）─────────────────────────────────────────

    /**
     * @brief 把标记值展开成登记项。
     *
     * 值为 0 时返回「数值为 0 的那一项」（通常是 `None`），没有则返回 0 项。
     * 非 0 时按登记顺序贪心匹配：命中一项就把它的位从剩余位里清掉，
     * 因此复合掩码（如 `RW = R|W`）不会与单一位重复命中。
     *
     * @return 写入 `out` 的项数；若存在**未登记的残留位**或 `out` 容量不足，
     *         返回 -1 —— 调用方应退回整数形式，避免静默丢信息。
     */
    template <typename T>
    inline int EnumFlagsToItems(const T value, const EnumItemMeta **out,
                                const int max_count) noexcept
    {
        if constexpr (!EnumMeta<T>::defined)
            return -1;
        else
        {
            const int64_t raw = static_cast<int64_t>(value);

            if (raw == 0)
            {
                for (std::size_t i = 0; i < EnumMeta<T>::Count; ++i)
                    if (EnumMeta<T>::Items[i].value == 0)
                    {
                        if (max_count < 1)
                            return -1;

                        out[0] = &EnumMeta<T>::Items[i];
                        return 1;
                    }

                return 0;
            }

            int   written = 0;
            int64_t remaining = raw;

            for (std::size_t i = 0; i < EnumMeta<T>::Count; ++i)
            {
                const EnumItemMeta &item = EnumMeta<T>::Items[i];

                if (item.value == 0)
                    continue;

                if ((remaining & item.value) != item.value)
                    continue;

                if (written >= max_count)
                    return -1;

                out[written] = &item;
                ++written;
                remaining &= ~item.value;
            }

            return remaining == 0 ? written : -1;
        }
    }
}// namespace hgl::serialize

// ── 按位标记的运算符 ────────────────────────────────────────────────────────
//
// `enum class` 不自带按位运算，而标记枚举又几乎必然要用 `|` 拼合。这些重载
// 用 `FlagEnum` 概念约束死了：只有显式 `HGL_FLAGS_BEGIN` 登记过的类型才会
// 参与重载，对其它任何类型（包括普通枚举与整型）完全不可见，不会造成歧义。
//
// 放在全局命名空间是刻意的：枚举可能声明在 `hgl::graph`、`hgl::mtl` 等任意
// 命名空间里，只有全局作用域的重载才能被这些命名空间里的无限定查找命中。

template <::hgl::serialize::FlagEnum T>
constexpr T operator|(const T left, const T right) noexcept
{
    using U = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<U>(left) | static_cast<U>(right));
}

template <::hgl::serialize::FlagEnum T>
constexpr T operator&(const T left, const T right) noexcept
{
    using U = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<U>(left) & static_cast<U>(right));
}

template <::hgl::serialize::FlagEnum T>
constexpr T operator^(const T left, const T right) noexcept
{
    using U = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<U>(left) ^ static_cast<U>(right));
}

template <::hgl::serialize::FlagEnum T>
constexpr T operator~(const T value) noexcept
{
    using U = std::underlying_type_t<T>;
    return static_cast<T>(~static_cast<U>(value));
}

template <::hgl::serialize::FlagEnum T>
constexpr T &operator|=(T &left, const T right) noexcept
{
    left = left | right;
    return left;
}

template <::hgl::serialize::FlagEnum T>
constexpr T &operator&=(T &left, const T right) noexcept
{
    left = left & right;
    return left;
}

template <::hgl::serialize::FlagEnum T>
constexpr T &operator^=(T &left, const T right) noexcept
{
    left = left ^ right;
    return left;
}

namespace hgl::serialize
{
    /// `value` 是否包含 `flag` 的全部位。
    template <typename T>
        requires FlagEnum<T>
    inline constexpr bool HasFlag(const T value, const T flag) noexcept
    {
        return (static_cast<int64_t>(value) & static_cast<int64_t>(flag))
               == static_cast<int64_t>(flag);
    }
}// namespace hgl::serialize

// ── 登记宏 ──────────────────────────────────────────────────────────────────

/**
 * @brief 普通枚举登记块起始。
 *
 * `TYPE` 必须写完全限定名，登记块必须位于全局作用域。
 */
#define HGL_ENUM_BEGIN(TYPE)                                                   \
    namespace hgl::serialize                                                   \
    {                                                                          \
        template <>                                                            \
        struct EnumMeta<TYPE>                                                  \
        {                                                                      \
            static_assert(::std::is_enum_v<TYPE>,                              \
                          "HGL_ENUM_BEGIN 的目标必须是枚举类型");             \
                                                                               \
            using Self = TYPE;                                                 \
                                                                               \
            static constexpr bool defined  = true;                             \
            static constexpr bool is_flags = false;                            \
                                                                               \
            static constexpr const ::hgl::serialize::EnumItemMeta Items[] = {

/// 登记一个枚举项：`NAME` 为枚举子名，`DESC` 为 UTF-8 GUI 说明（可为 nullptr）。
#define HGL_ENUM_ITEM(NAME, DESC)                                              \
    {#NAME, static_cast<int64_t>(Self::NAME), DESC},

/// 普通枚举登记块结束。`TYPE` 需与 `HGL_ENUM_BEGIN` 一致，用于校验登记位置。
#define HGL_ENUM_END(TYPE)                                                     \
    };                                                                         \
    static constexpr ::std::size_t Count =                                     \
        sizeof(Items) / sizeof(Items[0]);                                      \
    static_assert(Count > 0, "枚举登记块至少要有一个 HGL_ENUM_ITEM");           \
    }                                                                          \
    ;                                                                          \
    }                                                                          \
    static_assert(::hgl::serialize::EnumMeta<TYPE>::defined,                    \
                  "枚举登记表挂到了错误的命名空间：HGL_ENUM_BEGIN/END 必须在全局作用域，且 TYPE 用完全限定名");

/**
 * @brief 按位标记登记块起始。
 *
 * 与 `HGL_ENUM_BEGIN` 的唯一区别是 `is_flags = true`：
 * Archive 会把它序列化成名字数组，反序列化时按位或。
 */
#define HGL_FLAGS_BEGIN(TYPE)                                                  \
    namespace hgl::serialize                                                   \
    {                                                                          \
        template <>                                                            \
        struct EnumMeta<TYPE>                                                  \
        {                                                                      \
            static_assert(::std::is_enum_v<TYPE>,                              \
                          "HGL_FLAGS_BEGIN 的目标必须是枚举类型");            \
                                                                               \
            using Self = TYPE;                                                 \
                                                                               \
            static constexpr bool defined  = true;                             \
            static constexpr bool is_flags = true;                             \
                                                                               \
            static constexpr const ::hgl::serialize::EnumItemMeta Items[] = {

/// 登记一个标记位：`NAME` 为枚举子名，`DESC` 为 UTF-8 GUI 说明（可为 nullptr）。
#define HGL_FLAG_ITEM(NAME, DESC)                                              \
    {#NAME, static_cast<int64_t>(Self::NAME), DESC},

/// 按位标记登记块结束。`TYPE` 需与 `HGL_FLAGS_BEGIN` 一致。
#define HGL_FLAGS_END(TYPE)                                                    \
    };                                                                         \
    static constexpr ::std::size_t Count =                                     \
        sizeof(Items) / sizeof(Items[0]);                                      \
    static_assert(Count > 0, "标记登记块至少要有一个 HGL_FLAG_ITEM");           \
    }                                                                          \
    ;                                                                          \
    }                                                                          \
    static_assert(::hgl::serialize::EnumMeta<TYPE>::defined                    \
                      && ::hgl::serialize::EnumMeta<TYPE>::is_flags,           \
                  "按位标记登记表挂到了错误的命名空间：HGL_FLAGS_BEGIN/END 必须在全局作用域，且 TYPE 用完全限定名");
