#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>
#include<hgl/vk/VKShaderProgram.h>

namespace hgl::ecs
{
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
        // Recipe runtime resolves the program via the world's material variant table
        // (material runtime row); non-recipe items have no program.
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

    bool PrimitiveComponent::CanRender() const
    {
        // A5a：只表示"拥有可渲染资产"——可见性真值已收敛到实体级
        // （ECSContext::IsEntityVisible），不再混进本判据（原先这里 `&& IsVisible()`
        // 与收集链的可见性判定是同一件事的重复）。几何状态住在同实体的 GeometryData 组件。
        if (auto *owner = GetOwner())
        {
            if (auto geometry = owner->GetComponent<GeometryData>())
                return geometry->GetPrimitiveAsset() != nullptr;
        }

        return false;
    }

    void PrimitiveComponent::EnsureRenderItemStorageAllocated()
    {
        // A5a：不再在组件内缓存世界存储指针（原组件内的世界存储指针缓存已删）——
        // 每次经 owner 的 ECSContext 现取。
        RenderItemDataStorage *storage = owner_context
            ? owner_context->GetRenderItemStorage()
            : nullptr;

        if (storage && render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE)
        {
            render_item_handle = storage->Allocate(render_item_descriptor);
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
        RenderItemDataStorage *storage = owner_context
            ? owner_context->GetRenderItemStorage()
            : nullptr;

        if (storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            if (const auto *desc = storage->Get(render_item_handle))
                return *desc;
        }
        return render_item_descriptor;
    }

    void PrimitiveComponent::SetTransformID(uint32_t transform_id)
    {
        render_item_descriptor.transform_id = transform_id;
        EnsureRenderItemStorageAllocated();

        RenderItemDataStorage *storage = owner_context
            ? owner_context->GetRenderItemStorage()
            : nullptr;

        if (storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            storage->SetTransformID(render_item_handle, transform_id);
        }
    }

    void PrimitiveComponent::SetGeometryID(uint32_t geometry_id)
    {
        render_item_descriptor.geometry_id = geometry_id;
        EnsureRenderItemStorageAllocated();

        RenderItemDataStorage *storage = owner_context
            ? owner_context->GetRenderItemStorage()
            : nullptr;

        if (storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            storage->SetGeometryID(render_item_handle, geometry_id);
        }
    }

    void PrimitiveComponent::SetMaterialID(uint32_t material_id)
    {
        render_item_descriptor.material_id = material_id;
        EnsureRenderItemStorageAllocated();

        RenderItemDataStorage *storage = owner_context
            ? owner_context->GetRenderItemStorage()
            : nullptr;

        if (storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            storage->SetMaterialID(render_item_handle, material_id);
        }
    }

    void PrimitiveComponent::SetTextureID(uint32_t texture_id)
    {
        render_item_descriptor.texture_id = texture_id;
        EnsureRenderItemStorageAllocated();

        RenderItemDataStorage *storage = owner_context
            ? owner_context->GetRenderItemStorage()
            : nullptr;

        if (storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            storage->SetTextureID(render_item_handle, texture_id);
        }
    }

    void PrimitiveComponent::Set4ID(uint32_t transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id)
    {
        render_item_descriptor.transform_id = transform_id;
        render_item_descriptor.geometry_id = geometry_id;
        render_item_descriptor.material_id = material_id;
        render_item_descriptor.texture_id = texture_id;
        EnsureRenderItemStorageAllocated();

        RenderItemDataStorage *storage = owner_context
            ? owner_context->GetRenderItemStorage()
            : nullptr;

        if (storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            storage->Set4ID(render_item_handle, transform_id, geometry_id, material_id, texture_id);
        }
    }


    void PrimitiveComponent::OnAttach()
    {
        RenderableComponent::OnAttach();

        // A5a：几何/资产侧状态住在同实体的 `GeometryData` 组件里 ⇒ 挂载本组件即确保它存在，
        // 让实体始终具备 GeometryData 槽位（A5b 删本组件后由作者直接持有 GeometryData）。
        if (auto *owner = GetOwner())
        {
            if (!owner->GetComponent<GeometryData>())
                owner->AddComponent<GeometryData>();
        }

        EnsureRenderItemStorageAllocated();
    }

    void PrimitiveComponent::OnDetach()
    {
        RenderableComponent::OnDetach();

        RenderItemDataStorage *storage = owner_context
            ? owner_context->GetRenderItemStorage()
            : nullptr;

        if (storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            storage->Release(render_item_handle);
        }

        render_item_handle = graph::INVALID_RENDER_ITEM_HANDLE;

        // Don't delete resources here; they are managed externally.
        overridePipeline = nullptr;
        resolvedRuntimePipelineMap.Clear();
        render_item_descriptor = {};
        // 几何/资产侧状态已迁至 GeometryData（由它自己的 OnDetach 清理）。
        // 材质授权状态已迁至 MaterialData（由它自己的 OnDetach 清理）。
    }
}//namespace hgl::ecs
