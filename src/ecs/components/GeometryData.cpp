#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<vector>
#include<memory>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/Macro.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    void GeometryData::InvalidateOwnerRuntimePipeline()
    {
        // 几何/变体变化只让**同实体**的运行期管线缓存失效：A5b 起管线缓存归
        // 世界的每实例 slot（经 Context::InvalidateEntityRuntimePipeline）。
        Entity *owner = GetOwner();
        if (!owner)
            return;

        ECSContext *context = owner->GetContext();
        if (!context)
            return;

        context->InvalidateEntityRuntimePipeline(owner->GetEntityID());
    }

    void GeometryData::SetPrimitiveAsset(const hgl::graph::PrimitiveAsset *asset)
    {
        if (primitiveAsset != asset)
            InvalidateOwnerRuntimePipeline();

        primitiveAsset = asset;

        // asset 默认配方是材质解析的基底来源：换 asset ⇒ 让本实体材质数据层的
        // 授权代数前进，渲染侧的重解析快路径据此失配（原先由组件级 generation
        // 承担的失效点，A2 随授权状态一并迁入 MaterialData）。
        Entity *owner = GetOwner();
        ECSContext *context = owner ? owner->GetContext() : nullptr;
        MaterialData *material_data = context
            ? context->GetMaterialData(owner)
            : nullptr;
        if (material_data)
            material_data->BumpAuthoredGeneration();
    }

    void GeometryData::SetPrimitiveVariantPurpose(const hgl::graph::PrimitiveVariantPurpose purpose)
    {
        if (primitiveVariantPurpose == purpose)
            return;

        primitiveVariantPurpose = purpose;
        InvalidateOwnerRuntimePipeline();
    }

    const hgl::graph::mtl::MaterialRecipe *GeometryData::GetAssetMaterialRecipe() const
    {
        if (!primitiveAsset)
            return nullptr;

        if (const auto *variant =
                primitiveAsset->FindVariantByPurpose(
                    primitiveVariantPurpose,
                    primitiveVariantIndex))
            return variant->material_recipe;

        return primitiveAsset->GetMaterialRecipe();
    }

    bool GeometryData::GetLocalAABB(hgl::math::AABB& outAABB) const
    {
        if (!primitiveAsset || !primitiveAsset->GetGeometry())
            return false;

        const auto &bv = primitiveAsset->GetGeometry()->GetBoundingVolumes();
        outAABB = bv.aabb;
        return true;
    }

    uint32_t GeometryData::GetGeometryID() const
    {
        if (!primitiveAsset || !primitiveAsset->GetGeometry())
            return 0;

        return primitiveAsset->GetGeometry()->GetGeometryID();
    }

    float GeometryData::GetBoundingRadius() const
    {
        if (!primitiveAsset || !primitiveAsset->GetGeometry())
            return 0.0f;

        // 与 GeometryAABB 行的写入同式：本地 AABB 的半对角线长
        return primitiveAsset->GetGeometry()->GetBoundingVolumes().aabb.GetDiagonal() * 0.5f;
    }

    void GeometryData::OnDetach()
    {
        // 不删几何资源：它们归资源管理器所有。
        primitiveAsset = nullptr;
        primitiveVariantIndex = 0;
    }
}//namespace hgl::ecs
