#pragma once

#include <hgl/mtl/MaterializationSpec.h>
#include <functional>

namespace hgl::graph::mtl
{
    // Recipe -> Spec 的外部解析回调集合。
    // 说明：
    // 1) 本层只定义契约，不关心资源来自哪（Bindless池/资产系统/缓存都可接入）；
    // 2) 解析失败策略由调用方控制（例如可选纹理失败是否忽略）。
    struct MaterializationResolveCallbacks
    {
        // 输入：Recipe 纹理声明；输出：解析后的 bindless handle + layer 信息。
        std::function<bool(const RecipeTextureBinding &, ResolvedResource &)> resolve_texture;

        // 输入：Recipe SSBO 绑定声明；输出：解析后的 SSBO 引用信息。
        std::function<bool(const RecipeSSBOAssetBinding &, ResolvedStructRef &)> resolve_struct;

        // 若为 true，非 required 纹理解析失败时可忽略；required 纹理始终必须成功。
        bool allow_optional_texture_unresolved = true;
    };

    // 将 MaterialRecipe 解析为完整 MaterializationSpec。
    // 约定：
    // - 成功时 out_spec.spec_hash 已刷新；
    // - 失败时返回 false，out_spec 内容不保证完整。
    inline bool ResolveMaterializationSpec(const MaterialRecipe &recipe,
                                           const MaterializationResolveCallbacks &callbacks,
                                           MaterializationSpec &out_spec)
    {
        out_spec = MakeMaterializationSpecSkeleton(recipe);
        out_spec.resources.clear();
        out_spec.struct_refs.clear();

        for (const auto &texture : recipe.textures)
        {
            ResolvedResource resolved{};
            resolved.slot = texture.slot;

            const bool ok = callbacks.resolve_texture && callbacks.resolve_texture(texture, resolved);
            if (!ok)
            {
                if (texture.required || !callbacks.allow_optional_texture_unresolved)
                    return false;

                continue;
            }

            // 语义槽位由 Recipe 决定，解析器只负责填资源定位信息。
            resolved.slot = texture.slot;
            out_spec.resources.emplace_back(std::move(resolved));
        }

        for (const auto &asset : recipe.ssbo_assets)
        {
            if (!callbacks.resolve_struct)
                return false;

            ResolvedStructRef resolved{};
            resolved.data_slot = asset.data_slot;

            if (!callbacks.resolve_struct(asset, resolved))
                return false;

            resolved.data_slot = asset.data_slot;
            out_spec.struct_refs.emplace_back(std::move(resolved));
        }

        RefreshMaterializationSpecHash(out_spec);
        return true;
    }

    // 解析只保留可由多个实例共享的资源定位结果；实例 data_index 在后续
    // MaterializeMaterializationInstance 阶段从当前 Recipe 写回。
    inline bool ResolveMaterializationSharedSpec(const MaterialRecipe &recipe,
                                                 const MaterializationResolveCallbacks &callbacks,
                                                 MaterializationSharedSpec &out_shared)
    {
        MaterializationSpec resolved;
        if (!ResolveMaterializationSpec(recipe, callbacks, resolved))
            return false;

        out_shared = MakeMaterializationSharedSpec(resolved);
        return true;
    }
}
