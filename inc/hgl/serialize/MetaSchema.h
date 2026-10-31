#pragma once

/**
 * `ClassMetaInfo` 驱动的**严格键校验**。
 *
 * ── 为什么需要这一层 ──────────────────────────────────────────────────────
 * `Archive` 的读方向对「不认识的键」是宽容的：找不到对应属性就跳过，这样
 * 老资产遇到新字段、新代码读老资产都不会炸。但作者侧 TOML 必须**严格** ——
 * 把 `cull_mode` 拼成 `cull_mdoe` 却不报错，等于让一份错的资产静默跑起来。
 *
 * 因此校验单独成一层：它只看生成的 `ClassMetaInfo`（`toml_key` + 递归的
 * `struct_meta`），判断「这个键有没有被任何一个属性认领」，**不**判断取值、
 * 形态与缺键语义 —— 那些交给 `Archive`，一处一份实现。
 *
 * 与 `Archive` 一样，本头**零格式依赖**：任何后端只要能产出 `Value` 树就能用。
 */

#include <hgl/asset/AssetObject.h>
#include <hgl/serialize/Value.h>

#include <cstring>

namespace hgl::serialize
{
    /**
     * @brief 在 `meta` 里按文本键名找一个属性（**大小写敏感**）。
     *
     * @return 命中的属性；没有属性认领该键时返回 nullptr
     */
    inline const hgl::asset::PropertyMetaInfo *FindMetaProperty(
        const hgl::asset::ClassMetaInfo &meta, const char *key)
    {
        if (!key || !key[0] || !meta.properties)
            return nullptr;

        for (uint32_t i = 0; i < meta.property_count; ++i)
        {
            const hgl::asset::PropertyMetaInfo &candidate = meta.properties[i];
            const char *toml_key = hgl::asset::TomlKeyOf(candidate);
            if (toml_key && std::strcmp(toml_key, key) == 0)
                return &candidate;
        }

        return nullptr;
    }

    /**
     * @brief 严格键校验：`node` 里的每个键都必须是 `meta` 某个属性的文本键名。
     *
     * 表数组（`[[textures]]`）按同一套规则逐个元素下钻；数组不进表路径 ——
     * 与 `Archive::FailedPath()`「数组不下标」的约定一致，报错位置才能和
     * `Archive` 的失败路径直接对照。
     *
     * @param node      待校验节点（表 / 表数组 / 标量；标量直接通过）
     * @param meta      该节点对应的结构元数据
     * @param path      该节点在文档里的表路径（顶层传空串，仅诊断用）
     * @param out_path  失败时写入出错表的路径（顶层为空串）
     * @param out_key   失败时写入不被认领的键名；成功时为空串
     * @return 全部键都被属性认领时返回 true
     */
    inline bool ValidateKeysByMeta(const Value &node,
                                   const hgl::asset::ClassMetaInfo &meta,
                                   const AnsiString &path,
                                   AnsiString &out_path,
                                   AnsiString &out_key)
    {
        out_path.Clear();
        out_key.Clear();

        if (node.IsArray())
        {
            const int count = node.GetElementCount();
            for (int i = 0; i < count; ++i)
            {
                const Value *item = node.GetElement(i);
                if (!item)
                    continue;
                if (!ValidateKeysByMeta(*item, meta, path, out_path, out_key))
                    return false;
            }
            return true;
        }

        if (!node.IsTable())
            return true;

        const int field_count = node.GetFieldCount();
        for (int i = 0; i < field_count; ++i)
        {
            const char *input_key = node.GetFieldName(i);
            if (!input_key || !input_key[0])
                continue;

            const hgl::asset::PropertyMetaInfo *property = FindMetaProperty(meta, input_key);
            if (!property)
            {
                out_path = path;
                out_key = AnsiString(input_key);
                return false;
            }

            if (!property->struct_meta)
                continue;

            const Value *child = node.GetFieldValue(i);
            if (!child)
                continue;

            AnsiString child_path = path;
            if (!child_path.IsEmpty())
                child_path = child_path + ".";
            child_path = child_path + input_key;

            if (!ValidateKeysByMeta(*child, *property->struct_meta, child_path, out_path, out_key))
                return false;
        }

        return true;
    }
}//namespace hgl::serialize
