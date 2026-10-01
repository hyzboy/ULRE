#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<vector>
#include<memory>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
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

    bool GeometryData::EnsureRuntimeGeometryBinding(hgl::graph::ShaderProgram *material)
    {
        if (!primitiveAsset || !material)
        {
            GLogError("[GeometryData] EnsureRuntimeGeometryBinding failed: primitiveAsset=%p material=%p",
                      primitiveAsset,
                      material);
            return false;
        }

        auto *geometry = primitiveAsset->GetGeometry();
        if (!geometry)
        {
            GLogError("[GeometryData] EnsureRuntimeGeometryBinding failed: geometry null asset=%p material=%s",
                      primitiveAsset,
                      material->GetName().c_str());
            return false;
        }

        if (!runtime_draw_range)
            runtime_draw_range = new hgl::graph::GeometryDrawRange();

        if (!runtime_draw_range)
        {
            GLogError("[GeometryData] EnsureRuntimeGeometryBinding failed: alloc GeometryDrawRange failed material=%s",
                      material->GetName().c_str());
            return false;
        }

        // SSBO 顶点方案无 VIL：GeometryDataBuffer 的内容与布局只由 geometry
        // 决定，program 按语义索引读同一缓冲，与缓冲构建无关。哨兵只看
        // geometry 身份——若按 program 指针比较，Forward↔Shadow 双槽交替或
        // program 重解析都会让几何缓冲每帧销毁重建（顶点数据整段重传）。
        const bool needs_rebuild =
            (!runtime_data_buffer)
         || (runtime_geometry != geometry);

        if (needs_rebuild)
        {
            // 顶点输入统一为 SSBO：无 VIL attribute 布局，顶点数据槽位
            // 直接按 Geometry 语义列表填充（GeometryDataBuffer::Update）
            const uint32_t input_count = geometry->GetGeometryVertexFormat().GetCount();

            if (geometry->GetVABCount() < input_count)
            {
                GLogError("[GeometryData] EnsureRuntimeGeometryBinding failed: geometry VAB count(%u) < semantic count(%u), material=%s",
                          geometry->GetVABCount(),
                          input_count,
                          material->GetName().c_str());
                return false;
            }

            SAFE_CLEAR(runtime_data_buffer);

            runtime_data_buffer = new hgl::graph::GeometryDataBuffer(input_count,
                                                                     geometry->GetIBO(),
                                                                     geometry->GetVDM());
            if (!runtime_data_buffer)
            {
                GLogError("[GeometryData] EnsureRuntimeGeometryBinding failed: alloc GeometryDataBuffer failed material=%s attr_count=%u",
                          material->GetName().c_str(),
                          input_count);
                return false;
            }

            runtime_geometry = geometry;
        }

        if (!runtime_data_buffer->Update(geometry))
        {
            GLogError("[GeometryData] EnsureRuntimeGeometryBinding failed: GeometryDataBuffer::Update failed material=%s",
                      material->GetName().c_str());
            return false;
        }

        runtime_draw_range->Set(geometry);
        return true;
    }

    void GeometryData::ClearRuntimeGeometryBinding()
    {
        SAFE_CLEAR(runtime_data_buffer);
        SAFE_CLEAR(runtime_draw_range);
        runtime_geometry = nullptr;
    }

    void GeometryData::SetPrimitiveAsset(const hgl::graph::PrimitiveAsset *asset)
    {
        if (primitiveAsset != asset)
        {
            InvalidateOwnerRuntimePipeline();
            ClearRuntimeGeometryBinding();
        }

        primitiveAsset = asset;

        if (primitiveAsset && primitiveAsset->GetGeometry())
        {
            const auto &bv = primitiveAsset->GetGeometry()->GetBoundingVolumes();
            auto extents = bv.aabb.GetLength();
            bounding_radius = math::Length(extents) * 0.5f;
        }
        else
        {
            bounding_radius = 0.0f;
        }

        // asset 默认配方是材质解析的基底来源：换 asset ⇒ 让本实体材质数据层的
        // 授权代数前进，渲染侧的重解析快路径据此失配（原先由组件级 generation
        // 承担的失效点，A2 随授权状态一并迁入 MaterialData）。
        Entity *owner = GetOwner();
        ECSContext *context = owner ? owner->GetContext() : nullptr;
        MaterialData *material_data = context
            ? context->GetMaterialData(owner->GetEntityID())
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

    void GeometryData::OnDetach()
    {
        Component::OnDetach();

        // 不删几何资源：它们归资源管理器所有。
        primitiveAsset = nullptr;
        primitiveVariantIndex = 0;
        ClearRuntimeGeometryBinding();
        bounding_radius = 0.0f;
    }
}//namespace hgl::ecs
