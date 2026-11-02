#pragma once

/**
 * 测试专用的「TOML 快照」小工具。
 *
 * 生产代码里已经没有 `toml::value` 这一层了：`hgl/serialize/TomlCodec.h` 只暴露
 * **文本 ⇄ 值树**，第三方 TOML 库是 `CMAssetsManage` 的实现细节。但测试仍想做
 * 两件带点 DOM 味道的事：
 *
 *   1. 把结构写成一份快照，再读回来做往返断言；
 *   2. 对快照做「某张表里只有 N 个键」这类结构断言 —— 这是「未 present 的键
 *      不能出现在输出里」的唯一可观测方式。
 *
 * 这里用公共 API（`SaveToTomlText` / `FromTomlText` + `Value::FindByPath`）实现，
 * 因此测试 TU 不必包含任何第三方头，断言的对象也正是**真正落盘的那份文本**。
 */

#include <hgl/serialize/TomlCodec.h>
#include <hgl/serialize/Value.h>
#include <hgl/type/String.h>

namespace serialize_test
{
    /// `T` → TOML 文本快照（序列化失败时返回空串）
    template <typename T>
    inline hgl::AnsiString WriteToml(const T &src, hgl::serialize::Archive *out_archive = nullptr)
    {
        return hgl::serialize::SaveToTomlText<T>(src, out_archive);
    }

    /// TOML 文本 → 值树（解析失败返回 false）
    inline bool ParseToml(const hgl::AnsiString &text, hgl::serialize::Value &out)
    {
        // 空的 `AnsiString` 可能连 `c_str()` 都是 nullptr，而入口把 nullptr 当作
        // 「没给文本」的失败；这里补一个空串字面量，语义仍是「一张空表」。
        const char *chars = text.c_str();

        return hgl::serialize::FromTomlText(chars ? chars : "",
                                            static_cast<size_t>(text.Length()), out);
    }

    /// `dot_path` 指向的表有多少个键；路径不存在或不是表时返回 -1
    inline int FieldCount(const hgl::serialize::Value &root, const char *dot_path)
    {
        const hgl::serialize::Value *node =
            (dot_path && dot_path[0]) ? root.FindByPath(dot_path) : &root;

        return (node && node->IsTable()) ? node->GetFieldCount() : -1;
    }

    /// `dot_path` 指向的数组有多少个元素；路径不存在或不是数组时返回 -1
    inline int ElementCount(const hgl::serialize::Value &root, const char *dot_path)
    {
        const hgl::serialize::Value *node =
            (dot_path && dot_path[0]) ? root.FindByPath(dot_path) : &root;

        return (node && node->IsArray()) ? node->GetElementCount() : -1;
    }

    /// `dot_path` 指向的表里有没有 `key` 这个键
    inline bool HasField(const hgl::serialize::Value &root,
                         const char *dot_path, const char *key)
    {
        const hgl::serialize::Value *node =
            (dot_path && dot_path[0]) ? root.FindByPath(dot_path) : &root;

        return node && node->IsTable() && node->FindField(key) != nullptr;
    }
}//namespace serialize_test
