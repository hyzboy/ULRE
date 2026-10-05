#include<hgl/ecs/support/PrimitiveState.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/support/MaterialRuntimeTable.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>

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

    // ── render_item 4-ID 绑定 ──

    graph::RenderItemHandle EnsureRenderItemHandle(ECSContext &world, EntityID owner)
    {
        RenderItemDataStorage *storage = world.GetRenderItemStorage();
        if (!storage)
            return graph::INVALID_RENDER_ITEM_HANDLE;

        MaterialRuntimeSlot &slot = world.GetOrCreateMaterialRuntimeSlot(owner);

        if (slot.render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE)
            slot.render_item_handle = storage->Allocate();

        return slot.render_item_handle;
    }

    graph::RenderItemHandle GetRenderItemHandle(const ECSContext &world, EntityID owner)
    {
        const MaterialRuntimeTable *runtime_table = world.GetMaterialRuntimeTable();
        const MaterialRuntimeSlot *slot = runtime_table ? runtime_table->GetSlot(owner) : nullptr;

        return slot ? slot->render_item_handle : graph::INVALID_RENDER_ITEM_HANDLE;
    }

    bool SetRenderItem4ID(ECSContext &world, EntityID owner,
                          uint32_t transform_id, uint32_t geometry_id,
                          uint32_t material_id, uint32_t texture_id)
    {
        const graph::RenderItemHandle handle = EnsureRenderItemHandle(world, owner);
        if (handle == graph::INVALID_RENDER_ITEM_HANDLE)
            return false;

        RenderItemDataStorage *storage = world.GetRenderItemStorage();
        return storage
            && storage->Set4ID(handle, transform_id, geometry_id, material_id, texture_id);
    }

    void ReleaseRenderItemHandle(ECSContext &world, EntityID owner)
    {
        MaterialRuntimeTable *runtime_table = world.GetMaterialRuntimeTable();
        MaterialRuntimeSlot *slot = runtime_table ? runtime_table->GetSlot(owner) : nullptr;
        if (!slot)
            return;

        RenderItemDataStorage *storage = world.GetRenderItemStorage();

        if (storage && slot->render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
            storage->Release(slot->render_item_handle);

        slot->render_item_handle = graph::INVALID_RENDER_ITEM_HANDLE;
    }}//namespace hgl::ecs
