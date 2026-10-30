#pragma once

/**
 * 资产 TOML 编解码的**叶子类型助手**。
 *
 * 只有「一个标量 / 一个字符串 / 一个定长向量」这一层的转换住在本文件；结构体与容器
 * 的组合逻辑由 `src/Tools/AssetGen/asset_gen.py` 生成，放在
 * `inc/hgl/asset/generated/<Group>Toml.gen.h`。这样切分的好处：写法（键缺失保留类内
 * 初始值、类型不符即失败）只在生成器里定义一次，helper 本身无状态、无策略。
 *
 * ── 语义约定（与 `BufferToml.gen.h` 完全一致）──────────────────────────────
 *   · **键缺失不算错误** —— 保留 C++ 结构里的类内初始值；
 *   · **键存在但类型/长度不对** —— 返回 false（调用方负责报出资产名与字段名）。
 *
 * ── 为什么不用 `toml::get<T>` ───────────────────────────────────────────────
 * `get<T>` 会抛异常且不能区分「缺失」与「类型错」。资产解析要能给出「哪个文件哪个
 * 字段」的精确诊断，因此一律走 `const toml::value *` + 显式类型判定。
 */

#include <toml/toml.hpp>
#include <hgl/type/String.h>
#include <hgl/utf.h>

#include <cstdint>
#include <float.h>

namespace hgl::asset::detail
{
    // ── 查表 ────────────────────────────────────────────────────────────────

    inline const toml::value *Find(const toml::value &table, const char *key)
    {
        if (!table.is_table())
            return nullptr;
        const auto &t = table.as_table();
        const auto it = t.find(key);
        return it == t.end() ? nullptr : &it->second;
    }

    inline const toml::value *Find(const toml::table &table, const char *key)
    {
        const auto it = table.find(key);
        return it == table.end() ? nullptr : &it->second;
    }

    // ── 标量：TOML → C++ ────────────────────────────────────────────────────

    inline bool AsBool(const toml::value &v, bool &out)
    {
        if (!v.is_boolean())
            return false;
        out = v.as_boolean();
        return true;
    }

    /// 浮点读取：允许写整数（TOML 作者写 `1` 想要 `1.0` 是最常见的偷懒）。
    inline bool AsFloat(const toml::value &v, double &out)
    {
        if (v.is_floating()) { out = v.as_floating(); return true; }
        if (v.is_integer())  { out = static_cast<double>(v.as_integer()); return true; }
        return false;
    }

    inline bool AsFloat(const toml::value &v, float &out)
    {
        double tmp = 0.0;
        if (!AsFloat(v, tmp))
            return false;
        if (tmp > FLT_MAX || tmp < -FLT_MAX)
            return false;
        out = static_cast<float>(tmp);
        return true;
    }

    template <typename T>
    inline bool AsUnsigned(const toml::value &v, T &out)
    {
        if (!v.is_integer())
            return false;
        const auto raw = v.as_integer();
        if (raw < 0)
            return false;
        if (sizeof(T) < sizeof(uint64_t))
        {
            const uint64_t max_value = (uint64_t(1) << (sizeof(T) * 8)) - 1;
            if (static_cast<uint64_t>(raw) > max_value)
                return false;
        }
        out = static_cast<T>(raw);
        return true;
    }

    template <typename T>
    inline bool AsSigned(const toml::value &v, T &out)
    {
        if (!v.is_integer())
            return false;
        const int64_t raw = v.as_integer();
        if (sizeof(T) < sizeof(int64_t))
        {
            const int bits = static_cast<int>(sizeof(T) * 8);
            const int64_t min_value = -(int64_t(1) << (bits - 1));
            const int64_t max_value = (int64_t(1) << (bits - 1)) - 1;
            if (raw < min_value || raw > max_value)
                return false;
        }
        out = static_cast<T>(raw);
        return true;
    }

    // ── 标量：C++ → TOML ────────────────────────────────────────────────────

    inline toml::value FromBool(const bool v) { return toml::value(v); }

    inline toml::value FromFloat(const double v)
    {
        return toml::value(static_cast<toml::value::floating_type>(v));
    }

    template <typename T>
    inline toml::value FromInteger(const T v)
    {
        return toml::value(static_cast<toml::value::integer_type>(v));
    }

    // ── 字符串 ──────────────────────────────────────────────────────────────
    //
    // TOML 字符串一律 UTF-8。`AnsiString` 在本工程里都当作 UTF-8 语义使用，
    // `OSString` 反过来是平台原生编码（Windows 下 UTF-16），因此走 utf.h 的转换。

    inline bool AsString(const toml::value &v, AnsiString &out)
    {
        if (!v.is_string())
            return false;
        out = v.as_string().c_str();
        return true;
    }

    inline bool AsString(const toml::value &v, U8String &out)
    {
        if (!v.is_string())
            return false;
        const auto &s = v.as_string();
        out = U8String(reinterpret_cast<const u8char *>(s.c_str()), static_cast<int>(s.size()));
        return true;
    }

    inline bool AsString(const toml::value &v, OSString &out)
    {
        if (!v.is_string())
            return false;
        out = ToOSString(v.as_string().c_str());
        return true;
    }

    inline toml::value FromString(const AnsiString &in)
    {
        return toml::value(static_cast<toml::value::string_type>(in.c_str()));
    }

    inline toml::value FromString(const U8String &in)
    {
        return toml::value(static_cast<toml::value::string_type>(
            reinterpret_cast<const char *>(in.c_str())));
    }

    inline toml::value FromString(const OSString &in)
    {
        return FromString(ToU8String(in));
    }

    // ── 定长浮点数组（向量 / 矩阵走这里）────────────────────────────────────

    inline bool AsFloatArray(const toml::value &v, float *out, const int count)
    {
        if (!v.is_array())
            return false;
        const auto &a = v.as_array();
        if (a.size() != static_cast<size_t>(count))
            return false;
        for (int i = 0; i < count; ++i)
        {
            float parsed = 0.0f;
            if (!AsFloat(a[static_cast<size_t>(i)], parsed))
                return false;
        }
        for (int i = 0; i < count; ++i)
        {
            AsFloat(a[static_cast<size_t>(i)], out[i]);
        }
        return true;
    }

    inline bool AsBoolArray(const toml::value &v, bool *out, const int count)
    {
        if (!v.is_array())
            return false;
        const auto &a = v.as_array();
        if (a.size() != static_cast<size_t>(count))
            return false;
        for (int i = 0; i < count; ++i)
        {
            bool parsed = false;
            if (!AsBool(a[static_cast<size_t>(i)], parsed))
                return false;
        }
        for (int i = 0; i < count; ++i)
            AsBool(a[static_cast<size_t>(i)], out[i]);
        return true;
    }

    template <typename T>
    inline bool AsUnsignedArray(const toml::value &v, T *out, const int count)
    {
        if (!v.is_array())
            return false;
        const auto &a = v.as_array();
        if (a.size() != static_cast<size_t>(count))
            return false;
        for (int i = 0; i < count; ++i)
        {
            T parsed = 0;
            if (!AsUnsigned(a[static_cast<size_t>(i)], parsed))
                return false;
        }
        for (int i = 0; i < count; ++i)
            AsUnsigned(a[static_cast<size_t>(i)], out[i]);
        return true;
    }

    template <typename T>
    inline bool AsSignedArray(const toml::value &v, T *out, const int count)
    {
        if (!v.is_array())
            return false;
        const auto &a = v.as_array();
        if (a.size() != static_cast<size_t>(count))
            return false;
        for (int i = 0; i < count; ++i)
        {
            T parsed = 0;
            if (!AsSigned(a[static_cast<size_t>(i)], parsed))
                return false;
        }
        for (int i = 0; i < count; ++i)
            AsSigned(a[static_cast<size_t>(i)], out[i]);
        return true;
    }

    inline toml::value FromFloatArray(const float *src, const int count)
    {
        toml::array a;
        a.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
            a.push_back(FromFloat(static_cast<double>(src[i])));
        return toml::value(std::move(a));
    }

    inline toml::value FromBoolArray(const bool *src, const int count)
    {
        toml::array a;
        a.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
            a.push_back(FromBool(src[i]));
        return toml::value(std::move(a));
    }

    template <typename T>
    inline toml::value FromIntegerArray(const T *src, const int count)
    {
        toml::array a;
        a.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i)
            a.push_back(FromInteger(src[i]));
        return toml::value(std::move(a));
    }

    // ── 动态数组（ValueArray / ManagedArray 走这里）─────────────────────────

    inline void AppendFloat(toml::array &a, const double v) { a.push_back(FromFloat(v)); }

    template <typename T>
    inline void AppendInteger(toml::array &a, const T v) { a.push_back(FromInteger(v)); }

    // ── 行结构默认值语义 ────────────────────────────────────────────────────
    //
    // 生成的 Read 以 `T{}` 的引用开始；键缺失即保留类内初始值。这里给一个显式名字，
    // 让生成代码的意图在阅读时立刻可见（而不是靠读者自己推断 `Find` 返回 nullptr 的后果）。
    template <typename T>
    inline void ResetToDefaults(T &out) { out = T{}; }
}//namespace hgl::asset::detail
