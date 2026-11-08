#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/core/entity/Entity.h>
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
        if (primitiveAsset == asset)
            return;

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

        // P1：换资产 ⇒ 图元→几何 ID 绑定变了，主帧拓扑（成员、材质基底、包围盒行）都变。
        if (context)
            context->MarkTopologyChanged(TopologyChange::GeometryBinding);
    }

    void GeometryData::SetPrimitiveVariantPurpose(const hgl::graph::PrimitiveVariantPurpose purpose)
    {
        if (primitiveVariantPurpose == purpose)
            return;

        primitiveVariantPurpose = purpose;
        InvalidateOwnerRuntimePipeline();

        Entity *owner = GetOwner();
        ECSContext *context = owner ? owner->GetContext() : nullptr;
        if (context)
            context->MarkTopologyChanged(TopologyChange::GeometryBinding);
    }

    void GeometryData::SetPrimitiveVariantIndex(const uint32_t index)
    {
        // 2026-10 P1：原先是内联裸 setter、不触发任何失效。变体索引参与
        // `FindVariantByPurpose`（决定几何变体与默认材质配方），改它等同于换资产 ⇒
        // 必须同时让运行期管线与材质授权代失效，并记一次拓扑变更。
        if (primitiveVariantIndex == index)
            return;

        primitiveVariantIndex = index;
        InvalidateOwnerRuntimePipeline();

        Entity *owner = GetOwner();
        ECSContext *context = owner ? owner->GetContext() : nullptr;
        if (!context)
            return;

        if (MaterialData *material_data = context->GetMaterialData(owner))
            material_data->BumpAuthoredGeneration();

        context->MarkTopologyChanged(TopologyChange::GeometryBinding);
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

        // P1：卸载后该实体不再可渲染 ⇒ 主帧成员集合变化。
        // （`OnComponentRowRemoved` 也会记一次 EntityStructure，这里记的是
        //   几何绑定类别的归因，便于报表指出「是谁导致的重建」。）
        Entity *owner = GetOwner();
        ECSContext *context = owner ? owner->GetContext() : nullptr;
        if (context)
            context->MarkTopologyChanged(TopologyChange::GeometryBinding);
    }
}//namespace hgl::ecs
