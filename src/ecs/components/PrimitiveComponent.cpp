#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/ComponentRecords.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/log/Log.h>
#include<cassert>

namespace hgl::ecs
{
    namespace
    {
        struct RenderableRecord
        {
            bool visible = true;
            float boundingRadius = 1.0f;
        };

        struct PrimitiveRecord
        {
            RenderableRecord renderable;
            bool hasPrimitive = false;
        };

        static PrimitiveComponent::ResolvedMaterialState BuildMaterialStateFromResolveRequest(
            const graph::MaterialResolveRequest &request,
            const RuntimeTextureBinding &runtime_binding)
        {
            PrimitiveComponent::ResolvedMaterialState state{};
            state.preset = hgl::graph::GraphicsPipelinePreset::Solid3D;
            state.runtime_texture_binding = runtime_binding;

            state.program_binding = request.resolved_program_binding;
            state.program = request.resolved_program ? request.resolved_program : request.resolved_material;
            state.payload = request.resolved_payload;
            state.binding_id = request.resolved_binding_id;
            state.payload_id = request.resolved_payload_id;

            state.binding_instance = request.resolved_binding_instance;
            state.material = state.program ? state.program : request.resolved_material;
            state.domain = request.resolved_domain;
            state.domain_id = request.resolved_domain_id;
            state.vil = request.resolved_vil;
            state.mi_id = request.resolved_mi_id;
            state.preset = request.resolved_preset;

            if (state.runtime_texture_binding.IsReady() && state.runtime_texture_binding.domain)
                state.domain = state.runtime_texture_binding.domain;

            return state;
        }
    }

    const char* PrimitiveComponent::GetSerializationType()
    {
        return "Primitive";
    }

    bool PrimitiveComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                               const hgl::UnorderedMap<EntityID, int32_t>&,
                                               ComponentRecord& out_record)
    {
        auto primitive = std::dynamic_pointer_cast<PrimitiveComponent>(component);
        if (!primitive)
            return false;

        PrimitiveRecord data{};
        data.renderable.visible = primitive->IsVisible();
        data.renderable.boundingRadius = primitive->GetBoundingRadius();
        data.hasPrimitive = primitive->GetPrimitive() != nullptr;

        out_record.type = GetSerializationType();
        out_record.payload = data;
        return true;
    }

    void PrimitiveComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                   Entity* entity,
                                                   std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
    {
        const auto& data = std::any_cast<const PrimitiveRecord&>(record.payload);
        auto primitive = std::make_shared<PrimitiveComponent>();
        primitive->SetVisible(data.renderable.visible);
        primitive->SetBoundingRadius(data.renderable.boundingRadius);
        entity->AddComponentInstance(primitive);
    }

    void PrimitiveComponent::SetPrimitive(hgl::graph::Primitive* prim)
    {
        primitive = prim;

        // Update bounding radius based on primitive's bounding volume
        if (primitive)
        {
            const auto& bv = primitive->GetBoundingVolumes();

            // Calculate bounding radius from AABB for frustum culling
            // Use the length (diagonal) of the AABB as the bounding radius
            auto extents = bv.aabb.GetLength();
            float radius = math::Length(extents) * 0.5f; // Half diagonal

            SetBoundingRadius(radius);

            // Auto-stage render state from the primitive's MI when the caller
            // has already created the Primitive externally (bypassing MaterialResolveSystem).
            auto* mi = primitive->GetResolvedBindingInstance();
            if (mi)
            {
                auto* material = graph::MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(mi);
                auto* domain   = graph::MaterialBindingInstanceInternalAccess::GetDomain(mi);
                uint32_t domain_id = graph::MaterialBindingInstanceInternalAccess::GetDomainID(mi);

                if (material)
                {
                    ResolvedMaterialState staged{};
                    staged.program_binding = nullptr;
                    staged.program          = material;
                    staged.payload          = nullptr;
                    staged.binding_id       = 0;
                    staged.payload_id       = 0;
                    staged.binding_instance = mi;
                    staged.material         = material;
                    staged.domain           = domain;
                    staged.domain_id        = domain_id;
                    staged.vil              = primitive->GetVIL();
                    staged.mi_id            = mi->GetMIID();
                    staged.preset           = mi->GetRenderPreset();

                    SetStagingRenderState(staged, primitive);
                    CommitStagingRenderState();
                }
            }
        }
        else
        {
            SetBoundingRadius(0.0f);
        }
    }

    void PrimitiveComponent::SetMaterialRecipe(hgl::graph::mtl::MaterialRecipeID recipe_id,
                                               const void *instance_data,
                                               uint32_t instance_data_size)
    {
        material_slot.SetRecipeID(recipe_id);
        if (instance_data && instance_data_size > 0)
            material_slot.SetInstanceData(instance_data, instance_data_size);
    }

    void PrimitiveComponent::SetRuntimeTextureBinding(const RuntimeTextureBinding& binding)
    {
        runtime_texture_binding = binding;
        runtime_texture_binding_generation++;
        runtime_texture_binding.generation = runtime_texture_binding_generation;
        if (runtime_texture_binding.ready)
            runtime_texture_binding.status = RuntimeTextureBinding::Status::Ready;
    }

    void PrimitiveComponent::ClearRuntimeTextureBinding()
    {
        runtime_texture_binding_generation++;
        runtime_texture_binding.Reset();
        runtime_texture_binding.generation = runtime_texture_binding_generation;
    }

    void PrimitiveComponent::SetStagingRenderState(const ResolvedMaterialState& state,
                                                   hgl::graph::Primitive* resolved_primitive)
    {
        staging_render_state.primitive = resolved_primitive ? resolved_primitive : primitive;
        staging_render_state.material_state = state;
        staging_render_state.generation = render_state_generation + 1;
        staging_render_state.ready = state.binding_instance != nullptr
                                  && state.material != nullptr
                                  && state.vil != nullptr;
    }

    void PrimitiveComponent::ClearStagingRenderState()
    {
        staging_render_state.Reset();
    }

    bool PrimitiveComponent::CommitStagingRenderState()
    {
        if (!staging_render_state.ready)
            return false;

#ifdef _DEBUG
        const auto &staged = staging_render_state.material_state;
        if (staged.program_binding)
        {
            if (!staged.program)
            {
                GLogWarning("[ECS::PrimitiveComponent] R3 consistency: program_binding=%p but program is null",
                           static_cast<const void *>(staged.program_binding));
            }

            if (!staged.payload && staged.payload_id != 0)
            {
                GLogWarning("[ECS::PrimitiveComponent] R3 consistency: payload pointer is null but payload_id=%llu",
                           static_cast<unsigned long long>(staged.payload_id));
            }

            if (staged.payload && staged.payload_id != staged.payload->id)
            {
                GLogWarning("[ECS::PrimitiveComponent] R3 consistency: payload pointer/id mismatch payload=%p payload_id=%llu actual_id=%llu",
                           static_cast<const void *>(staged.payload),
                           static_cast<unsigned long long>(staged.payload_id),
                           static_cast<unsigned long long>(staged.payload->id));
            }

            if (staged.binding_id != staged.program_binding->id)
            {
                GLogWarning("[ECS::PrimitiveComponent] R3 consistency: binding pointer/id mismatch binding=%p binding_id=%llu actual_id=%llu",
                           static_cast<const void *>(staged.program_binding),
                           static_cast<unsigned long long>(staged.binding_id),
                           static_cast<unsigned long long>(staged.program_binding->id));
            }
        }
#endif

        committed_render_state = staging_render_state;
        render_state_generation++;
        committed_render_state.generation = render_state_generation;
        committed_render_state.ready = true;
        staging_render_state.Reset();
        return true;
    }

    void PrimitiveComponent::ClearCommittedRenderState()
    {
        committed_render_state.Reset();
    }

    hgl::graph::MaterialBindingInstance* PrimitiveComponent::GetResolvedBindingInstance() const
    {
        return ResolveEffectiveMaterialState().binding_instance;
    }

    hgl::graph::ShaderMaterialProgram* PrimitiveComponent::GetShaderMaterialProgram() const
    {
        return ResolveEffectiveMaterialState().material;
    }

    hgl::graph::ResourceDomain* PrimitiveComponent::GetResolvedDomain() const
    {
        return ResolveEffectiveMaterialState().domain;
    }

    uint32_t PrimitiveComponent::GetResolvedDomainID() const
    {
        return ResolveEffectiveMaterialState().domain_id;
    }

    const hgl::graph::VertexInputLayout* PrimitiveComponent::GetResolvedVIL() const
    {
        return ResolveEffectiveMaterialState().vil;
    }

    int PrimitiveComponent::GetResolvedMIID() const
    {
        return ResolveEffectiveMaterialState().mi_id;
    }

    hgl::graph::GraphicsPipelinePreset PrimitiveComponent::GetResolvedRenderPreset() const
    {
        return ResolveEffectiveMaterialState().preset;
    }

    PrimitiveComponent::EffectiveMaterialState PrimitiveComponent::ResolveEffectiveMaterialState() const
    {
        if (committed_render_state.ready)
            return committed_render_state.material_state;

        return EffectiveMaterialState{};
    }

    bool PrimitiveComponent::GetLocalAABB(hgl::math::AABB& outAABB) const
    {
        if (!primitive)
            return false;

        const auto& bv = primitive->GetBoundingVolumes();
        outAABB = bv.aabb;
        return true;
    }

    bool PrimitiveComponent::CanRender() const
    {
        return primitive != nullptr && IsVisible();
    }

    void PrimitiveComponent::Render(const glm::mat4& worldMatrix)
    {
        // This is called by RenderCollector or rendering systems
        // The actual rendering would be done through the graphics API
        // Here we just verify we can render
        if (!CanRender())
            return;

        // In a real implementation, this would submit draw commands
        // to a command buffer or render queue using the primitive,
        // material instance, and world matrix
    }

    void PrimitiveComponent::OnAttach()
    {
        RenderableComponent::OnAttach();
        // Additional attachment logic if needed
    }

    void PrimitiveComponent::OnUpdate(float deltaTime)
    {
        RenderableComponent::OnUpdate(deltaTime);
        // Update logic if needed (e.g., animation updates)
    }

    void PrimitiveComponent::OnDetach()
    {
        RenderableComponent::OnDetach();

        // Don't delete primitive - it's managed externally
        // Just clear our reference
        primitive = nullptr;
        runtime_texture_binding.Reset();
        staging_render_state.Reset();
        committed_render_state.Reset();
    }
}//namespace hgl::ecs


