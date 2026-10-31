#include <hgl/graph/asset/MaterialRecipeAsset.h>

#include <hgl/asset/generated/MaterialRecipeMeta.gen.h>
#include <hgl/graph/asset/TextureAsset.h>
#include <hgl/filesystem/Path.h>
#include <hgl/utf.h>

namespace hgl::graph::mtl
{
    const char *GetMaterialRecipeAssetStateName(MaterialRecipeAssetState state) noexcept
    {
        switch (state)
        {
            case MaterialRecipeAssetState::Parsed: return "Parsed";
            case MaterialRecipeAssetState::Ready:  return "Ready";
            case MaterialRecipeAssetState::Failed: return "Failed";
            default:
                break;
        }
        return "Unknown";
    }

    MaterialRecipeAsset::MaterialRecipeAsset(const MaterialRecipeFileData &data,
                                             MaterialRecipeSpec            &&parsed_spec)
        : spec(std::move(parsed_spec)),
          recipe(data.recipe),
          parse_error(data.parse_error)
    {
        if (!recipe.recipe_name.empty())
            asset_name = recipe.recipe_name;
    }

    const hgl::asset::ClassMetaInfo *MaterialRecipeAsset::GetClassMetaInfo() const
    {
        return &hgl::asset::generated::Class_MaterialRecipeSpec;
    }

    void MaterialRecipeAsset::SetSource(const os_char *label)
    {
        if (!label || !label[0])
        {
            source_path.Clear();
            resource_id = U8String(u8"rcp:@");
            resource_id += ToU8String(asset_name);
            return;
        }

        source_path = label;

        if (!asset_name.IsEmpty())
        {
            resource_id = U8String(u8"rcp:@");
            resource_id += ToU8String(asset_name);
            return;
        }

        // 无名配方（解析器已拒绝，这里只做兜底）用路径做身份，保证身份唯一。
        resource_id = U8String(u8"rcpfile:");
        resource_id += ToU8String(source_path);
    }

    void MaterialRecipeAsset::MarkReady() noexcept
    {
        if (state == MaterialRecipeAssetState::Ready)
            return;

        state = MaterialRecipeAssetState::Ready;
        failure_reason.Clear();
    }

    void MaterialRecipeAsset::MarkFailed(const char *reason)
    {
        state = MaterialRecipeAssetState::Failed;

        if (reason && reason[0])
            failure_reason = reason;
        else
            failure_reason.Clear();
    }

    int MaterialRecipeAsset::GetPendingTextureCount() const
    {
        int pending = 0;

        for (const RecipeTextureBinding &binding : recipe.textures)
        {
            if (!binding.asset)
            {
                // 没给句柄：必需槽 ⇒ 永远等不到就绪；可选槽 ⇒ 不参与门禁。
                if (binding.required)
                    ++pending;
                continue;
            }

            if (!binding.asset->IsPayloadResident())
                ++pending;
        }

        return pending;
    }

    bool MaterialRecipeAsset::AreTexturesResident() const
    {
        return GetPendingTextureCount() == 0;
    }
}//namespace hgl::graph::mtl
