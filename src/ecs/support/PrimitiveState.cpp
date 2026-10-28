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

    /// 材质侧资源就绪度：任一**必需**纹理句柄尚未加载完毕 ⇒ 本图元暂不可渲染。
    /// 没有材质数据覆盖时不做限制（由几何默认资产决定），保持既有行为。
    static bool IsMaterialResourceReady(const ECSContext *context, const Entity *entity)
    {
        const MaterialData *material_data = context->GetMaterialData(entity);

        if (!material_data)
            return true;

        return !material_data->HasPendingRequiredTexture();
    }

    bool CanRender(const Entity *entity)
    {
        if (!entity)
            return false;

        ECSContext *context = entity->GetContext();
        if (!context)
            return false;

        const GeometryData *geometry = context->GetGeometryData(entity);

        if (!geometry || geometry->GetPrimitiveAsset() == nullptr)
            return false;

        // 资源未就绪不渲染：贴图仍在流式加载时该图元不进收集（不占实例槽、不产生 DrawCall），
        // 等必需句柄全部 Resident 后 `authored_generation` 前进，本判据自动放行。
        return IsMaterialResourceReady(context, entity);
    }

    bool HasPendingMaterialResources(const Entity *entity)
    {
        if (!entity)
            return false;

        ECSContext *context = entity->GetContext();
        if (!context)
            return false;

        return !IsMaterialResourceReady(context, entity);
    }

    bool HasAnyMaterialSource(const Entity *entity)
    {
        if (!entity)
            return false;

        ECSContext *context = entity->GetContext();
        if (!context)
            return false;

        // A7d：手里已有实体 ⇒ 走 `Entity*` 重载（不做「指针 → ID → 再解析回指针」的往返）
        const MaterialData *material_data = context->GetMaterialData(entity);
        const GeometryData *geometry = context->GetGeometryData(entity);

        return (material_data && material_data->HasRecipeOverride())
            || (geometry && geometry->GetAssetMaterialRecipe() != nullptr);
    }

}//namespace hgl::ecs
