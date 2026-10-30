#pragma once

/**
 * 基础类型 traits —— 标量 / 字符串 / 枚举 / 数学向量 / 结构体分派。
 *
 * 这一层的职责是把「C++ 类型」翻译成 Archive 能识别的**类别**，并把
 * 值树节点与 C++ 值之间的搬运写成无状态的 inline 函数。它与任何具体格式无关：
 * TOML / FlatBuffers / 未来的 JSON 后端都复用同一套。
 *
 * ── 扩展点 ────────────────────────────────────────────────────────────────
 *   · `EnumTraits<T>`       —— 枚举 ↔ 字符串名；由 AssetGen 生成特化；
 *   · `StructSerializer<T>` —— 结构体字段遍历；由 AssetGen 生成特化；
 *   · `VectorTraits<T>`     —— 定长数值向量；本文件已覆盖 GLM 向量与其派生颜色类。
 *
 * 未提供特化的类型会在 `if constexpr` 分派处触发 `static_assert`，给出明确的
 * 「请先让 AssetGen 生成该结构的序列化」提示，而不是晦涩的模板报错。
 */

#include <hgl/serialize/Value.h>

#include <hgl/type/String.h>
#include <hgl/math/VectorTypes.h>
#include <hgl/color/Color3f.h>
#include <hgl/color/Color4f.h>

#include <type_traits>

namespace hgl::serialize
{
    class Archive;

    // ── 字符串 ──────────────────────────────────────────────────────────────

    template <typename T>
    struct IsStringType : std::bool_constant<
        std::is_same_v<T, AnsiString> ||
        std::is_same_v<T, U8String> ||
        std::is_same_v<T, U16String> ||
        std::is_same_v<T, U32String> ||
        std::is_same_v<T, OSString> ||
        std::is_same_v<T, WideString>> {};

    // ── 数学向量 ────────────────────────────────────────────────────────────

    template <typename T>
    struct IsGlmVector : std::false_type {};

    template <glm::length_t N, typename S, glm::qualifier Q>
    struct IsGlmVector<glm::vec<N, S, Q>> : std::true_type {};

    template <typename T>
    struct VectorTraits
    {
        static constexpr int components = 0;
        static constexpr bool is_float = false;
    };

    template <glm::length_t N, typename S, glm::qualifier Q>
    struct VectorTraits<glm::vec<N, S, Q>>
    {
        static constexpr int components = static_cast<int>(N);
        static constexpr bool is_float = std::is_floating_point_v<S>;
    };

    /// `Color3f` / `Color4f` 这类「继承自 GLM 向量」的颜色类。
    template <typename T>
        requires (std::is_base_of_v<glm::vec4, T> && !IsGlmVector<T>::value)
    struct VectorTraits<T>
    {
        static constexpr int components = 4;
        static constexpr bool is_float = true;
    };

    template <typename T>
        requires (std::is_base_of_v<glm::vec3, T>
                  && !std::is_base_of_v<glm::vec4, T>
                  && !IsGlmVector<T>::value)
    struct VectorTraits<T>
    {
        static constexpr int components = 3;
        static constexpr bool is_float = true;
    };

    // ── 枚举 ────────────────────────────────────────────────────────────────

    /// 枚举 ↔ 字符串名。AssetGen 为每个被引用的枚举生成特化。
    template <typename T>
    struct EnumTraits
    {
        static constexpr bool defined = false;

        static const char *ToName(T) { return nullptr; }

        static bool FromName(const char *, T &) { return false; }
    };

    // ── 结构体 ──────────────────────────────────────────────────────────────

    /**
     * 结构体字段遍历入口。
     *
     * 生成物形如：
     * ```cpp
     * template<> struct StructSerializer<hgl::graph::SceneSpec>
     * {
     *     static constexpr bool defined = true;
     *     static void Do(Archive &ar, hgl::graph::SceneSpec &v);
     * };
     * ```
     * 这里只前置声明 `Archive`，因此生成头可以在包含 `Archive.h` 之后再定义 `Do`，
     * 不会形成循环依赖。
     */
    template <typename T>
    struct StructSerializer
    {
        static constexpr bool defined = false;
    };

    // ── 字符串搬运 ──────────────────────────────────────────────────────────

    template <typename T>
    inline void StoreString(Value &node, const T &text)
    {
        if constexpr (std::is_same_v<T, U8String>)
            node.SetString(text);
        else if constexpr (std::is_same_v<T, AnsiString>)
            node.SetString(text);
        else if constexpr (std::is_same_v<T, OSString>)
            node.SetString(text);
        else if constexpr (std::is_same_v<T, U16String>)
        {
            const U8String utf8 = to_u8(text.c_str(), text.Length());
            node.SetString(utf8);
        }
        else if constexpr (std::is_same_v<T, U32String>)
            static_assert(sizeof(T) == 0,
                          "U32String 缺少 HGL 转换器；请改用 U8String / OSString");
        else
            static_assert(sizeof(T) == 0, "不支持的字符串类型");
    }

    template <typename T>
    inline bool LoadString(const Value &node, T &text)
    {
        if (!node.IsString())
            return false;

        const U8String &utf8 = node.GetString();

        if constexpr (std::is_same_v<T, U8String>)
            text = utf8;
        else if constexpr (std::is_same_v<T, AnsiString>)
        {
            text.Clear();
            if (utf8.Length() > 0)
                text = AnsiString(reinterpret_cast<const char *>(utf8.c_str()),
                                  utf8.Length());
        }
        else if constexpr (std::is_same_v<T, OSString>)
            text = ToOSString(utf8);
        else if constexpr (std::is_same_v<T, U16String>)
            text = to_u16(utf8);
        else if constexpr (std::is_same_v<T, U32String>)
            static_assert(sizeof(T) == 0,
                          "U32String 缺少 HGL 转换器；请改用 U8String / OSString");
        else
            static_assert(sizeof(T) == 0, "不支持的字符串类型");

        return true;
    }

    /// 任意 HGL 字符串 → UTF-8（供映射键等需要文本化的场合）。
    template <typename S>
    inline U8String StringToU8(const S &text)
    {
        Value tmp;
        StoreString(tmp, text);
        return tmp.GetString();
    }

    // ── 枚举搬运 ────────────────────────────────────────────────────────────

    template <typename T>
    inline void StoreEnum(Value &node, const T &value)
    {
        if constexpr (EnumTraits<T>::defined)
        {
            const char *name = EnumTraits<T>::ToName(value);
            if (name)
            {
                node.SetString(name);
                return;
            }
        }

        if constexpr (std::is_signed_v<std::underlying_type_t<T>>)
            node.SetInt(static_cast<int64_t>(value));
        else
            node.SetUInt(static_cast<uint64_t>(value));
    }

    template <typename T>
    inline bool LoadEnum(const Value &node, T &out)
    {
        if (node.IsString())
        {
            if constexpr (EnumTraits<T>::defined)
            {
                const U8String &utf8 = node.GetString();
                const AnsiString name(reinterpret_cast<const char *>(
                                         utf8.c_str() ? utf8.c_str() : U8_TEXT("")),
                                     utf8.Length());
                return EnumTraits<T>::FromName(name.c_str(), out);
            }
            return false;
        }

        if (node.IsInteger())
        {
            int64_t raw = 0;
            if (!node.GetInt(raw))
                return false;
            out = static_cast<T>(raw);
            return true;
        }

        return false;
    }

    // ── 向量搬运 ────────────────────────────────────────────────────────────

    template <typename T>
    inline void StoreVector(Value &node, const T &value)
    {
        constexpr int count = VectorTraits<T>::components;
        using Scalar = std::remove_cv_t<decltype(value.x)>;

        node.SetArray();
        const Scalar *data = &value.x;
        for (int i = 0; i < count; ++i)
        {
            Value *slot = node.AddElement();
            if (!slot)
                continue;
            if constexpr (std::is_floating_point_v<Scalar>)
                slot->SetDouble(static_cast<double>(data[i]));
            else
                slot->SetInt(static_cast<int64_t>(data[i]));
        }
    }

    template <typename T>
    inline bool LoadVector(const Value &node, T &out)
    {
        constexpr int count = VectorTraits<T>::components;
        using Scalar = std::remove_cv_t<decltype(out.x)>;

        if (!node.IsArray() || node.GetElementCount() != count)
            return false;

        Scalar *data = &out.x;
        for (int i = 0; i < count; ++i)
        {
            const Value *slot = node.GetElement(i);
            if (!slot)
                return false;
            if constexpr (std::is_floating_point_v<Scalar>)
            {
                double parsed = 0.0;
                if (!slot->GetDouble(parsed))
                    return false;
                data[i] = static_cast<Scalar>(parsed);
            }
            else
            {
                int64_t parsed = 0;
                if (!slot->GetInt(parsed))
                    return false;
                data[i] = static_cast<Scalar>(parsed);
            }
        }
        return true;
    }
}//namespace hgl::serialize
