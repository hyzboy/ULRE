#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/ComponentRecords.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/math/geometry/BoundingVolumes.h>

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
            bool hasOverrideMaterial = false;
        };
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
        data.hasOverrideMaterial = primitive->GetOverrideMaterial() != nullptr;

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
        }
        else
        {
            SetBoundingRadius(0.0f);
        }
    }

    void PrimitiveComponent::SetOverrideMaterial(hgl::graph::MaterialBindingInstance* mi)
    {
        overrideMaterial = mi;
    }

    void PrimitiveComponent::SetMaterialRecipe(const hgl::graph::mtl::MaterialRecipe *rec,
                                               const void *instance_data,
                                               uint32_t instance_data_size)
    {
        material_slot.SetRecord(rec);
        if (instance_data && instance_data_size > 0)
            material_slot.SetInstanceData(instance_data, instance_data_size);
    }

    hgl::graph::MaterialBindingInstance* PrimitiveComponent::GetMaterialInstance() const
    {
        // Phase B: resolved MI from MaterialResolveRequest takes priority
        if (material_slot.resolved_mi)
            return material_slot.resolved_mi;

        // Override material is second priority
        if (overrideMaterial)
            return overrideMaterial;

        if (!primitive)
            return nullptr;

        return primitive->GetMaterialInstance();
    }

    hgl::graph::ShaderMaterialProgram* PrimitiveComponent::GetShaderMaterialProgram() const
    {
        // Return override material's base if set
        if (overrideMaterial)
            return overrideMaterial->GetShaderMaterialProgram();

        if (!primitive)
            return nullptr;

        return primitive->GetShaderMaterialProgram();
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

        // Don't delete primitive or material - they're managed externally
        // Just clear our references
        primitive = nullptr;
        overrideMaterial = nullptr;
    }
}//namespace hgl::ecs


