#include<hgl/ecs/support/PrimitiveState.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/support/MaterialRuntimeTable.h>
#include<hgl/vk/buffer/DeviceBuffer.h>

namespace hgl::ecs
{
    // ── 能力判定 ──

    bool CanRender(const Entity *entity)
    {
        if (!entity)
            return false;

        ECSContext *context = entity->GetContext();
        if (!context)
            return false;

        const GeometryData *geometry = context->GetGeometryData(entity);

        return geometry && geometry->GetPrimitiveAsset() != nullptr;
    }

    bool HasAnyMaterialSource(const Entity *entity)
    {
        if (!entity)
            return false;

        ECSContext *context = entity->GetContext();
        if (!context)
            return false;

        // A7d\uff1a\u624b\u91cc\u5df2\u6709\u5b9e\u4f53 \u21d2 \u8d70 `Entity*` \u91cd\u8f7d\uff08\u4e0d\u505a\u300c\u6307\u9488 \u2192 ID \u2192 \u518d\u89e3\u6790\u56de\u6307\u9488\u300d\u7684\u5f80\u8fd4\uff09
        const MaterialData *material_data = context->GetMaterialData(entity);
        const GeometryData *geometry = context->GetGeometryData(entity);

        return (material_data && material_data->HasRecipeOverride())
            || (geometry && geometry->GetAssetMaterialRecipe() != nullptr);
    }

}//namespace hgl::ecs
