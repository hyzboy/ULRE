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

#include <hgl/serialize/Archive.h>
#include <hgl/serialize/Value.h>

#include <toml/toml.hpp>

#include <cstdint>
#include <cstring>
#include <string>

namespace hgl::serialize
{
    /**
     * @brief TOML 解析失败详情。
     *
     * 后端只负责「文本 → 值树」，但解析失败时**必须**把行号带出来 —— 资产文件动辄
     * 数百行，只报「解析失败」等于没报。行号约定 **1 起**，0 表示无法定位。
     */
    struct TomlParseError
    {
        int line = 0;         ///< 出错行（1 起，0 = 未知）
        AnsiString title;     ///< 错误类别（toml11 的 title，如 `syntax error`）
        AnsiString detail;    ///< 该行的具体说明

        /// 拼成一句可直接展示的诊断文本。
        AnsiString Format() const
        {
            AnsiString result;

            if (line > 0)
            {
                result += AnsiString("第 ");
                result += AnsiString::numberOf(line);
                result += AnsiString(" 行：");
            }

            if (detail.Length() == 0)
            {
                result += (title.Length() > 0) ? title : AnsiString("TOML 语法错误");
                return result;
            }

            result += detail;

            // 具体说明与类别不同时补上类别，否则「第 3 行：bad = = 2」看不出为什么错。
            if (title.Length() > 0 && hgl::stricmp(title.c_str(), detail.c_str()) != 0)
            {
                result += AnsiString("（");
                result += title;
                result += AnsiString("）");
            }

            return result;
        }
    };

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

    /**
     * @brief 把 toml11 的语法错误折叠成一句诊断。
     *
     * toml11 的 `what()` 是一整块带竖线箭头和源码片段的格式化文本，直接塞进资产
     * 文件的错误信息里会把日志冲垮。这里只取**第一条**错误的标题、行号与该行说明。
     */
    inline void FillTomlSyntaxError(const toml::syntax_error &err, TomlParseError &out)
    {
        out.line = 0;
        out.title.Clear();
        out.detail.Clear();

        const auto &errors = err.errors();
        if (errors.empty())
        {
            out.detail = AnsiString(err.what());
            return;
        }

        const toml::error_info &info = errors.front();

        // toml11 的 `title()` 形如 `toml::parse_value: `，末尾冒号空格只是拼接痕迹。
        std::string title = info.title();
        while (!title.empty() && (title.back() == ':' || title.back() == ' '))
            title.pop_back();

        out.title = AnsiString(title.c_str());

        if (!info.locations().empty())
        {
            const auto &location = info.locations().front();
            out.line = static_cast<int>(location.first.first_line_number());

            // 第二项常常只是占位符 `here`，真正有信息量的是出错那一行的源码。
            const std::string &message = location.second;
            if (!message.empty() && message != "here")
                out.detail = AnsiString(message.c_str());
            else if (location.first.is_ok() && location.first.num_lines() > 0)
                out.detail = AnsiString(location.first.first_line().c_str());
        }

        if (out.detail.Length() == 0 && !info.suffix().empty())
            out.detail = AnsiString(info.suffix().c_str());

        if (out.detail.Length() == 0)
            out.detail = out.title;
    }

    /// 便捷入口：解析文本 → Value 树（不取诊断详情）。
    inline bool FromTomlText(const char *text, Value &out);

    /**
     * @brief 解析文本 → Value 树，失败时给出行号。
     *
     * @param text      文本起点
     * @param size      字节数（允许文本里出现 `\0`）
     * @param out       目标值树；失败时内容未定义
     * @param out_error 可选输出：解析失败详情
     */
    inline bool FromTomlText(const char *text, const size_t size, Value &out,
                             TomlParseError *out_error = nullptr)
    {
        if (out_error)
            *out_error = TomlParseError{};

        if (!text)
        {
            if (out_error)
                out_error->detail = AnsiString("TOML 文本为空");
            return false;
        }

        try
        {
            // 注意：toml11 的 `parse(const char*)` 把参数当作**文件名**；
            // 解析内容必须走 `parse_str`。
            const toml::value parsed =
                toml::parse_str(std::string(text, size));
            FromToml(parsed, out);
            return true;
        }
        catch (const toml::syntax_error &err)
        {
            if (out_error)
                FillTomlSyntaxError(err, *out_error);
            return false;
        }
        catch (...)
        {
            return false;
        }
    }

    inline bool FromTomlText(const char *text, Value &out)
    {
        return FromTomlText(text, text ? std::strlen(text) : 0u, out, nullptr);
    }

    /**
     * @brief 文本 → `T`：解析成值树后立刻整体读成结构。
     *
     * 这是资产文件读取的**唯一**入口。文本层只产出值树；字段名、类型、缺键语义、
     * 枚举拼写表全部来自生成的 `StructSerializer<T>`，本函数不认任何具体资产类型。
     *
     * @param out_archive 可选输出：序列化阶段（而非解析阶段）的失败详情
     * @param out_error   可选输出：TOML 解析阶段的失败详情
     */
    template <typename T>
    inline bool LoadFromTomlText(const char *text, const size_t size, T &out,
                                 Archive *out_archive = nullptr,
                                 TomlParseError *out_error = nullptr)
    {
        Value root;
        if (!FromTomlText(text, size, root, out_error))
            return false;

        return ReadFromValue<T>(root, out, out_archive);
    }

    /// 便捷重载：以 NUL 结尾的文本。
    template <typename T>
    inline bool LoadFromTomlText(const char *text, T &out,
                                 Archive *out_archive = nullptr,
                                 TomlParseError *out_error = nullptr)
    {
        return LoadFromTomlText<T>(text, text ? std::strlen(text) : 0u, out,
                                   out_archive, out_error);
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

    /**
     * @brief `T` → TOML 文本：整体写成值树后一次格式化。
     *
     * @param out_archive 可选输出：序列化阶段的失败详情
     * @return 成功时返回文本；失败返回空串（详情见 `out_archive`）
     */
    template <typename T>
    inline AnsiString SaveToTomlText(const T &src, Archive *out_archive = nullptr)
    {
        Value root;

        Archive archive = Archive::ForWrite(root);
        archive.WriteTo(root, src);

        const bool ok = archive.Ok();

        if (out_archive)
            *out_archive = std::move(archive);

        if (!ok)
            return AnsiString();

        return ToTomlText(root);
    }

    // ── toml::value 表 ⇄ T ──────────────────────────────────────────────────
    //
    // 调用方已经自己解析出 TOML DOM（例如要先看顶层键、或同一份 DOM 要喂给
    // 多个结构）时用这两个入口。它们与 `LoadFromTomlText<T>()` /
    // `SaveToTomlText<T>()` 完全等价，只是省掉/补回文本这一层。
    //
    // 有了它们，生成器就不必再为每个资产结构各生成一份 `Read<X>` / `Write<X>`
    // 门面 —— 泛型模板已经把「TOML 表 → 值树 → 结构」这条链包住了。

    /**
     * @brief `toml::value` 表 → `T`。
     *
     * @param out_archive 可选输出：序列化阶段的失败详情
     */
    template <typename T>
    inline bool ReadTomlTable(const toml::value &table, T &out,
                              Archive *out_archive = nullptr)
    {
        Value root;
        FromToml(table, root);
        return ReadFromValue<T>(root, out, out_archive);
    }

    /**
     * @brief `T` → `toml::value` 表。
     *
     * @param out_archive 可选输出：序列化阶段的失败详情
     * @return 成功时返回表；失败返回空表（详情见 `out_archive`）
     */
    template <typename T>
    inline toml::value WriteTomlTable(const T &src, Archive *out_archive = nullptr)
    {
        Value root;

        Archive archive = Archive::ForWrite(root);
        archive.WriteTo(root, src);

        const bool ok = archive.Ok();

        if (out_archive)
            *out_archive = std::move(archive);

        if (!ok)
            return toml::value(toml::table{});

        return ToToml(root);
    }
}//namespace hgl::serialize
