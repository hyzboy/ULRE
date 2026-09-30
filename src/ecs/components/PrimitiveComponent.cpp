#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>
#include<hgl/ecs/support/RenderResource.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    bool PrimitiveComponent::EnsureRuntimeGeometryBinding(hgl::graph::ShaderProgram *material)
    {
        if (!primitiveAsset || !material)
        {
            GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: primitiveAsset=%p material=%p",
                      primitiveAsset,
                      material);
            return false;
        }

        auto *geometry = primitiveAsset->GetGeometry();
        if (!geometry)
        {
            GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: geometry null asset=%p material=%s",
                      primitiveAsset,
                      material->GetName().c_str());
            return false;
        }

        if (!runtime_draw_range)
            runtime_draw_range = new hgl::graph::GeometryDrawRange();

        if (!runtime_draw_range)
        {
            GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: alloc GeometryDrawRange failed material=%s",
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
                GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: geometry VAB count(%u) < semantic count(%u), material=%s",
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
                GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: alloc GeometryDataBuffer failed material=%s attr_count=%u",
                          material->GetName().c_str(),
                          input_count);
                return false;
            }

            runtime_geometry = geometry;
        }

        if (!runtime_data_buffer->Update(geometry))
        {
            GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: GeometryDataBuffer::Update failed material=%s",
                      material->GetName().c_str());
            return false;
        }

        runtime_draw_range->Set(geometry);
        return true;
    }

    void PrimitiveComponent::ClearRuntimeGeometryBinding()
    {
        SAFE_CLEAR(runtime_data_buffer);
        SAFE_CLEAR(runtime_draw_range);
        runtime_geometry = nullptr;
    }

    const hgl::graph::GeometryDataBuffer *PrimitiveComponent::GetRuntimeGeometryDataBuffer() const
    {
        return runtime_data_buffer;
    }

    const hgl::graph::GeometryDrawRange *PrimitiveComponent::GetRuntimeGeometryDrawRange() const
    {
        return runtime_draw_range;
    }

    void PrimitiveComponent::SetPrimitiveAsset(const hgl::graph::PrimitiveAsset *asset)
    {
        if (primitiveAsset != asset)
        {
            InvalidateResolvedRuntimePipeline();
            ClearRuntimeGeometryBinding();
        }

        primitiveAsset = asset;

        if (primitiveAsset && primitiveAsset->GetGeometry())
        {
            const auto &bv = primitiveAsset->GetGeometry()->GetBoundingVolumes();
            auto extents = bv.aabb.GetLength();
            float radius = math::Length(extents) * 0.5f;
            SetBoundingRadius(radius);
        }
        else
        {
            SetBoundingRadius(0.0f);
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

    const hgl::graph::mtl::MaterialRecipe *PrimitiveComponent::GetAssetMaterialRecipe() const
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

    void PrimitiveComponent::InvalidateResolvedRuntimePipeline()
    {
        resolvedRuntimePipelineMap.Clear();
    }

    void PrimitiveComponent::SetResolvedRuntimePipeline(hgl::graph::RenderPass *rp,
                                                        hgl::graph::Pipeline *p,
                                                        hgl::graph::ShaderProgram *program)
    {
        if (!rp || !p)
            return;

        ResolvedRuntimePipeline entry;
        entry.pipeline = p;

        if (program)
        {
            entry.program_key     = program->GetProgramKey();
            entry.has_program_key = true;
        }

        if (ResolvedRuntimePipeline *existing = resolvedRuntimePipelineMap.GetValuePointer(rp))
            *existing = entry;
        else
            resolvedRuntimePipelineMap.Add(rp, entry);
    }

    bool PrimitiveComponent::HasResolvedRuntimePipeline(hgl::graph::RenderPass *render_pass,
                                                        hgl::graph::ShaderProgram *program) const
    {
        if (!render_pass || !program)
            return false;

        const ResolvedRuntimePipeline *entry =
            const_cast<PrimitiveComponent *>(this)->resolvedRuntimePipelineMap.GetValuePointer(render_pass);

        if (!entry || !entry->pipeline || !entry->has_program_key)
            return false;

        return entry->program_key == program->GetProgramKey();
    }

    hgl::graph::ShaderProgram* PrimitiveComponent::GetShaderProgram() const
    {
        // Recipe runtime resolves program via MaterialComponent; non-recipe items have no program.
        return nullptr;
    }

    hgl::graph::Pipeline* PrimitiveComponent::GetPipelineForRenderPass(hgl::graph::RenderPass* render_pass) const
    {
        if (overridePipeline)
            return overridePipeline;

        // Return the pipeline resolved for THIS render pass during collect/prepare phases.
        // 注意：GetValuePointer 的 const 重载返回 const V*，此处需要可变指针语义，
        // 但 map 内容并不修改，用非 const this 的映射读取即可。
        auto *entry = const_cast<PrimitiveComponent *>(this)->resolvedRuntimePipelineMap.GetValuePointer(render_pass);
        return entry ? entry->pipeline : nullptr;
    }

    bool PrimitiveComponent::GetLocalAABB(hgl::math::AABB& outAABB) const
    {
        if (!primitiveAsset || !primitiveAsset->GetGeometry())
            return false;

        const auto &bv = primitiveAsset->GetGeometry()->GetBoundingVolumes();
        outAABB = bv.aabb;
        return true;
    }

    bool PrimitiveComponent::CanRender() const
    {
        return primitiveAsset != nullptr && IsVisible();
    }

    void PrimitiveComponent::EnsureRenderItemStorageAllocated()
    {
        if (!bound_render_item_storage && owner_context)
        {
            bound_render_item_storage = owner_context->GetRenderItemStorage();
        }
        if (bound_render_item_storage && render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE)
        {
            render_item_handle = bound_render_item_storage->Allocate(render_item_descriptor);
        }
    }

    graph::RenderItemHandle PrimitiveComponent::GetRenderItemHandle() const
    {
        if (render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE)
        {
            const_cast<PrimitiveComponent*>(this)->EnsureRenderItemStorageAllocated();
        }
        return render_item_handle;
    }

    const graph::RenderItemDescriptor &PrimitiveComponent::GetRenderItemDescriptor() const
    {
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            if (const auto *desc = bound_render_item_storage->Get(render_item_handle))
                return *desc;
        }
        return render_item_descriptor;
    }

    void PrimitiveComponent::SetTransformID(uint32_t transform_id)
    {
        render_item_descriptor.transform_id = transform_id;
        EnsureRenderItemStorageAllocated();
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->SetTransformID(render_item_handle, transform_id);
        }
    }

    void PrimitiveComponent::SetGeometryID(uint32_t geometry_id)
    {
        render_item_descriptor.geometry_id = geometry_id;
        EnsureRenderItemStorageAllocated();
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->SetGeometryID(render_item_handle, geometry_id);
        }
    }

    void PrimitiveComponent::SetMaterialID(uint32_t material_id)
    {
        render_item_descriptor.material_id = material_id;
        EnsureRenderItemStorageAllocated();
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->SetMaterialID(render_item_handle, material_id);
        }
    }

    void PrimitiveComponent::SetTextureID(uint32_t texture_id)
    {
        render_item_descriptor.texture_id = texture_id;
        EnsureRenderItemStorageAllocated();
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->SetTextureID(render_item_handle, texture_id);
        }
    }

    void PrimitiveComponent::Set4ID(uint32_t transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id)
    {
        render_item_descriptor.transform_id = transform_id;
        render_item_descriptor.geometry_id = geometry_id;
        render_item_descriptor.material_id = material_id;
        render_item_descriptor.texture_id = texture_id;
        EnsureRenderItemStorageAllocated();
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->Set4ID(render_item_handle, transform_id, geometry_id, material_id, texture_id);
        }
    }


    void PrimitiveComponent::OnAttach()
    {
        RenderableComponent::OnAttach();
        EnsureRenderItemStorageAllocated();
    }

    void PrimitiveComponent::OnDetach()
    {
        RenderableComponent::OnDetach();

        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->Release(render_item_handle);
            render_item_handle = graph::INVALID_RENDER_ITEM_HANDLE;
            bound_render_item_storage = nullptr;
        }

        // Don't delete resources here; they are managed externally.
        primitiveAsset = nullptr;
        primitiveVariantIndex = 0;
        ClearRuntimeGeometryBinding();
        overridePipeline = nullptr;
        resolvedRuntimePipelineMap.Clear();
        render_item_descriptor = {};
        // 材质授权状态已迁至 MaterialData（由它自己的 OnDetach 清理）。
    }
}//namespace hgl::ecs
