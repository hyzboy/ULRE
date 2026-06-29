#pragma once

#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/core/RuntimeTextureBinding.h>
#include<hgl/graph/module/MaterialDecoupledTypes.h>
#include<hgl/mtl/MaterialResolveRequest.h>
#include<glm/glm.hpp>

// Forward declarations to avoid heavy includes
namespace hgl
{
    namespace math
    {
        class AABB;
    }

    namespace graph
    {
        class Primitive;
        class ShaderMaterialProgram;
        class MaterialInstancePayload;
        class ProgramInstanceBinding;
        class MaterialBindingInstance;
        class ResourceDomain;
        class VertexInputLayout;
        enum class GraphicsPipelinePreset;
        class Geometry;
    }
}

namespace hgl::ecs
{
    /**
     * PrimitiveComponent - Renderable component for static mesh rendering
     *
     * Manages a single Primitive (geometry + material) for rendering.
     * Derived from RenderableComponent to provide rendering capabilities.
     *
     * Features:
     * - Holds reference to hgl::graph::Primitive
     * - Supports MaterialBindingInstanceData override
     * - Provides access to ShaderMaterialProgram, GraphicsPipeline, and AABB data
     * - Compatible with RenderCollector for batched rendering
     */
    class PrimitiveComponent : public RenderableComponent
    {
    public:

        struct ResolvedMaterialState
        {
            hgl::graph::ProgramInstanceBinding* program_binding = nullptr;
            hgl::graph::ShaderMaterialProgram* program = nullptr;
            hgl::graph::MaterialInstancePayload* payload = nullptr;
            hgl::graph::ProgramBindingID binding_id = 0;
            hgl::graph::MaterialPayloadID payload_id = 0;
            hgl::graph::MaterialBindingInstance* binding_instance = nullptr;
            hgl::graph::ShaderMaterialProgram* material = nullptr;
            hgl::graph::ResourceDomain* domain = nullptr;
            RuntimeTextureBinding runtime_texture_binding{};
            uint32_t domain_id = 0xFFFFFFFFu;
            const hgl::graph::VertexInputLayout* vil = nullptr;
            int mi_id = -1;
            hgl::graph::GraphicsPipelinePreset preset{};

            bool HasBindingInstance() const { return program_binding != nullptr || binding_instance != nullptr; }
            bool HasMaterial() const { return program != nullptr || material != nullptr; }
        };

        struct PrimitiveRenderState
        {
            hgl::graph::Primitive* primitive = nullptr;
            ResolvedMaterialState material_state{};
            uint32_t generation = 0;
            bool ready = false;

            void Reset()
            {
                primitive = nullptr;
                material_state = ResolvedMaterialState{};
                generation = 0;
                ready = false;
            }

            bool HasBindingInstance() const { return material_state.HasBindingInstance(); }
            bool HasMaterial() const { return material_state.HasMaterial(); }
        };

        using EffectiveMaterialState = ResolvedMaterialState;

    private:

        hgl::graph::Primitive* primitive;              // The primitive to render (not owned)

        hgl::graph::MaterialResolveRequest material_slot;        // Deferred MI resolution slot (Phase B)
        hgl::graph::Geometry* unresolved_geometry = nullptr; // Geometry awaiting MI (not owned)
        RuntimeTextureBinding runtime_texture_binding;
        uint32_t runtime_texture_binding_generation = 0;
        PrimitiveRenderState staging_render_state;
        PrimitiveRenderState committed_render_state;
        uint32_t render_state_generation = 0;

    public:

        explicit PrimitiveComponent(const std::string& name = "Primitive")
            : RenderableComponent(name)
            , primitive(nullptr)
        {
        }

        virtual ~PrimitiveComponent() = default;

    public:

        // Primitive management
        const char* GetSystemGroupName() const override { return "Primitive"; }

        void SetPrimitive(hgl::graph::Primitive* prim);
        hgl::graph::Primitive* GetPrimitive() const { return primitive; }

        // Deferred material resolution (Phase B)
        void SetMaterialRecipe(hgl::graph::mtl::MaterialRecipeID recipe_id,
                               const void *instance_data = nullptr,
                               uint32_t instance_data_size = 0);
        void SetUnresolvedGeometry(hgl::graph::Geometry* geom) { unresolved_geometry = geom; }
        hgl::graph::Geometry* GetUnresolvedGeometry() const { return unresolved_geometry; }
        hgl::graph::MaterialResolveRequest& GetMaterialResolveRequest() { return material_slot; }
        const hgl::graph::MaterialResolveRequest& GetMaterialResolveRequest() const { return material_slot; }
        bool NeedsMaterialBindingResolve() const { return material_slot.NeedsResolve(); }

        // ShaderMaterialProgram access (returns override if set, otherwise primitive's material)
        hgl::graph::MaterialBindingInstance* GetResolvedBindingInstance() const;
        hgl::graph::ShaderMaterialProgram* GetShaderMaterialProgram() const;
        hgl::graph::ResourceDomain* GetResolvedDomain() const;
        uint32_t GetResolvedDomainID() const;
        const hgl::graph::VertexInputLayout* GetResolvedVIL() const;
        int GetResolvedMIID() const;
        hgl::graph::GraphicsPipelinePreset GetResolvedRenderPreset() const;
        const PrimitiveRenderState& GetCommittedRenderState() const { return committed_render_state; }
        const PrimitiveRenderState& GetStagingRenderState() const { return staging_render_state; }
        bool HasCommittedRenderState() const { return committed_render_state.ready; }
        bool HasStagingRenderState() const { return staging_render_state.ready; }
        EffectiveMaterialState ResolveEffectiveMaterialState() const;
        const RuntimeTextureBinding& GetRuntimeTextureBinding() const { return runtime_texture_binding; }
        RuntimeTextureBinding& GetRuntimeTextureBinding() { return runtime_texture_binding; }
        uint32_t GetRuntimeTextureBindingGeneration() const { return runtime_texture_binding_generation; }
        void SetRuntimeTextureBinding(const RuntimeTextureBinding& binding);
        void ClearRuntimeTextureBinding();
        void SetStagingRenderState(const ResolvedMaterialState& state,
                                   hgl::graph::Primitive* resolved_primitive = nullptr);
        void ClearStagingRenderState();
        bool CommitStagingRenderState();
        void ClearCommittedRenderState();

        // Bounding volume
        bool GetLocalAABB(hgl::math::AABB& outAABB) const;

        // Rendering capability check
        bool CanRender() const;

    public:

        // Override RenderableComponent::Render
        void Render(const glm::mat4& worldMatrix) override;

        // Component lifecycle
        void OnAttach() override;
        void OnUpdate(float deltaTime) override;
        void OnDetach() override;

        static const char* GetSerializationType();
        static bool SerializeToRecord(const std::shared_ptr<Component>& component,
                                      const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                      ComponentRecord& out_record);
        static void DeserializeFromRecord(const ComponentRecord& record,
                                          Entity* entity,
                                          std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents);
    };
}//namespace hgl::ecs


