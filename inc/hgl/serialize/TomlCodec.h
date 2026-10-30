#pragma once

/**
 * TOML 后端 —— `hgl::serialize::Value` 树 ↔ `toml::value` 的机械搬运。
 *
 * 这一层不含任何资产语义：不做字段名校验、不做范围校验、不做缺省值填充。
 * 那些全部发生在 `Serialize()`（唯一一份生成代码）里。
 *
 * ── 类型映射 ──────────────────────────────────────────────────────────────
 * | Value      | TOML |
 * |---|---|
 * | Bool       | boolean |
 * | Int        | integer |
 * | UInt       | integer（超出 int64 时退化为浮点，附注释） |
 * | Float/Double | float |
 * | String     | string（UTF-8） |
 * | Array      | array |
 * | Table      | table（有序键，与声明顺序一致） |
 */

#include <hgl/serialize/Value.h>

#include <toml/toml.hpp>

#include <cstdint>

namespace hgl::serialize
{
    // ── TOML → Value ────────────────────────────────────────────────────────

    inline void FromToml(const toml::value &src, Value &out);

    inline void FromTomlArray(const toml::array &src, Value &out)
    {
        out.SetArray();
        for (const auto &item : src)
        {
            Value *slot = out.AddElement();
            if (slot)
                FromToml(item, *slot);
        }
    }

    inline void FromTomlTable(const toml::table &src, Value &out)
    {
        out.SetTable();
        for (const auto &[key, item] : src)
        {
            Value *slot = out.AddField(key.c_str(), 0);
            if (slot)
                FromToml(item, *slot);
        }
    }

    inline void FromToml(const toml::value &src, Value &out)
    {
        if (src.is_boolean())
        {
            out.SetBool(src.as_boolean());
            return;
        }
        if (src.is_integer())
        {
            out.SetInt(src.as_integer());
            return;
        }
        if (src.is_floating())
        {
            out.SetDouble(src.as_floating());
            return;
        }
        if (src.is_string())
        {
            out.SetString(src.as_string().c_str());
            return;
        }
        if (src.is_array())
        {
            FromTomlArray(src.as_array(), out);
            return;
        }
        if (src.is_table())
        {
            FromTomlTable(src.as_table(), out);
            return;
        }

        // 日期 / 时间等资产层用不到的 TOML 类型：置空，交由上层判定为类型不符。
        out.SetNull();
    }

    /// 便捷入口：解析文本 → Value 树。
    inline bool FromTomlText(const char *text, Value &out)
    {
        try
        {
            // 注意：toml11 的 `parse(const char*)` 把参数当作**文件名**；
            // 解析内容必须走 `parse_str`。
            const toml::value parsed = toml::parse_str(std::string(text ? text : ""));
            FromToml(parsed, out);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    // ── Value → TOML ────────────────────────────────────────────────────────

    inline toml::value ToToml(const Value &src);

    inline toml::value ToTomlString(const U8String &text)
    {
        toml::value::string_type value;
        if (text.Length() > 0)
            value.assign(reinterpret_cast<const char *>(text.c_str()),
                         static_cast<size_t>(text.Length()));
        return toml::value(std::move(value));
    }

    inline toml::value ToToml(const Value &src)
    {
        switch (src.Kind())
        {
        case ValueKind::Null:
            return toml::value(toml::table{});

        case ValueKind::Bool:
        {
            bool b = false;
            src.GetBool(b);
            return toml::value(b);
        }

        case ValueKind::Int:
        {
            int64_t i = 0;
            src.GetInt(i);
            return toml::value(static_cast<toml::value::integer_type>(i));
        }

        case ValueKind::UInt:
        {
            uint64_t u = 0;
            src.GetUInt(u);
            if (u <= static_cast<uint64_t>(INT64_MAX))
                return toml::value(static_cast<toml::value::integer_type>(u));
            // TOML 只有有符号 64 位整数：超出范围时退化为浮点。
            return toml::value(static_cast<toml::value::floating_type>(
                static_cast<double>(u)));
        }

        case ValueKind::Float:
        case ValueKind::Double:
        {
            double d = 0.0;
            src.GetDouble(d);
            return toml::value(static_cast<toml::value::floating_type>(d));
        }

        case ValueKind::String:
            return ToTomlString(src.GetString());

        case ValueKind::Array:
        {
            toml::array a;
            const int count = src.GetElementCount();
            a.reserve(static_cast<size_t>(count));
            for (int i = 0; i < count; ++i)
            {
                const Value *slot = src.GetElement(i);
                if (slot)
                    a.push_back(ToToml(*slot));
            }
            return toml::value(std::move(a));
        }

        case ValueKind::Table:
        {
            toml::table t;
            const int count = src.GetFieldCount();
            for (int i = 0; i < count; ++i)
            {
                const char *name = src.GetFieldName(i);
                const Value *slot = src.GetFieldValue(i);
                if (!name || !slot)
                    continue;
                t.insert_or_assign(toml::table::key_type(name, std::strlen(name)),
                                   ToToml(*slot));
            }
            return toml::value(std::move(t));
        }
        }

        return toml::value(toml::table{});
    }

    /// 便捷入口：Value 树 → TOML 文本。
    inline AnsiString ToTomlText(const Value &src)
    {
        const toml::value v = ToToml(src);
        AnsiString text;
        try
        {
            text = AnsiString(toml::format(v).c_str());
        }
        catch (...)
        {
            text.Clear();
        }
        return text;
    }
}//namespace hgl::serialize
