#pragma once

#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/support/PositionSourceSpec.h>
#include<hgl/ecs/support/TransformPolicySpec.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/vk/pipeline/VKInlinePipeline.h>
#include<array>
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
        class DescriptorBindingSet;
        class DeviceBuffer;
        class Primitive;
        class PrimitiveAsset;
        class MaterialProgram;
        class MaterialInstance;
        class Pipeline;
        class RenderPass;
        class Sampler;
        class Texture;
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
     * - Supports MaterialInstance override
     * - Provides access to MaterialProgram, Pipeline, and AABB data
     * - Compatible with RenderCollector for batched rendering
     */
    class PrimitiveComponent : public RenderableComponent
    {
    public:
        enum class MaterialTextureResourceKind : uint8_t
        {
            Texture2D = 0,
            Texture2DArray
        };

        struct MaterialTextureAuthoringResource
        {
            std::string resource_id;
            hgl::graph::Texture *texture = nullptr;
            hgl::graph::Sampler *sampler = nullptr;
            MaterialTextureResourceKind kind = MaterialTextureResourceKind::Texture2D;
            uint32_t direct_value = 0;
            bool use_direct_value = false;
            bool required = false;
        };

        struct MaterialStructAuthoringResource
        {
            hgl::graph::mtl::SSBOType ssbo_type = hgl::graph::mtl::SSBOType::UserDefined;
            uint32_t ssbo_id = 0;
            hgl::graph::DeviceBuffer *buffer = nullptr;
            uint32_t element_capacity = 0;
            uint32_t byte_stride = 0;
            uint32_t struct_index = 0;
            bool use_struct_index = false;
            bool shared_across_instances = false;
        };

    private:

        hgl::graph::Primitive* primitive;                // The primitive to render (not owned)
        const hgl::graph::PrimitiveAsset* primitiveAsset = nullptr;  // Asset-level geometry+recipe pairing (not owned)
        hgl::graph::MaterialInstance* overrideMaterial;   // Optional material override (not owned)
        hgl::graph::DescriptorBindingSet* descriptorBindingSet; // Optional explicit binding set (not owned)
        hgl::graph::Pipeline* overridePipeline;           // Optional pipeline override (not owned)
        bool hasMaterialRecipe = false;
        hgl::graph::mtl::MaterialRecipe materialRecipe;
        std::array<MaterialTextureAuthoringResource, static_cast<size_t>(hgl::graph::mtl::TextureSlot::RANGE_SIZE)> materialTextureResources{};
        std::array<MaterialStructAuthoringResource, static_cast<size_t>(hgl::graph::mtl::DataSlot::RANGE_SIZE)> materialStructResources{};

        // Late-resolve pipeline slot:
        // Populated at render-time if primitive has no pre-baked pipeline.
        hgl::graph::Pipeline* resolvedRuntimePipeline = nullptr; // (not owned)
        hgl::graph::RenderPass* resolvedRuntimeRenderPass = nullptr; // (not owned)
        bool hasPendingPipelinePreset = false;
        hgl::graph::InlinePipeline pendingPipelinePreset = hgl::graph::InlinePipeline::Solid3D;
        void InvalidateResolvedRuntimePipeline();

        PositionSourceSpec positionSourceSpec;            // Unified position source ingress policy
        TransformPolicySpec transformPolicySpec;           // Unified transform policy ingress

    public:

        explicit PrimitiveComponent(const std::string& name = "Primitive")
            : RenderableComponent(name)
            , primitive(nullptr)
            , overrideMaterial(nullptr)
            , descriptorBindingSet(nullptr)
            , overridePipeline(nullptr)
            , positionSourceSpec(PositionSourceSpec::MeshVertex)
            , transformPolicySpec{}
        {
        }

        virtual ~PrimitiveComponent() = default;

    public:

        // Primitive management
        const char* GetSystemGroupName() const override { return "Primitive"; }

        void SetPrimitive(hgl::graph::Primitive* prim);
        hgl::graph::Primitive* GetPrimitive() const { return primitive; }
        void SetPrimitiveAsset(const hgl::graph::PrimitiveAsset *asset) { primitiveAsset = asset; }
        const hgl::graph::PrimitiveAsset *GetPrimitiveAsset() const { return primitiveAsset; }
        void ClearPrimitiveAsset() { primitiveAsset = nullptr; }

        // Legacy runtime bridge — internal engine use only during Phase 7 migration.
        void SetInternalOverrideMaterial(hgl::graph::MaterialInstance* mi);
        hgl::graph::MaterialInstance* GetInternalOverrideMaterial() const { return overrideMaterial; }
        void ClearInternalOverrideMaterial() { SetInternalOverrideMaterial(nullptr); }

        void SetInternalDescriptorBindingSet(hgl::graph::DescriptorBindingSet* set);
        hgl::graph::DescriptorBindingSet* GetInternalDescriptorBindingSet() const;
        void ClearInternalDescriptorBindingSet() { SetInternalDescriptorBindingSet(nullptr); }

        void SetOverridePipeline(hgl::graph::Pipeline* p) { overridePipeline = p; }
        hgl::graph::Pipeline* GetOverridePipeline() const { return overridePipeline; }
        void ClearOverridePipeline() { overridePipeline = nullptr; }

        // Late-resolve pipeline request:
        // Call this when creating a Primitive without a pre-baked pipeline.
        // The render path will call RenderPass::CreatePipeline(material, vil, preset) on first batch.
        void RequestPipeline(const hgl::graph::InlinePipeline preset)
        {
            pendingPipelinePreset = preset;
            hasPendingPipelinePreset = true;
        }

        bool HasPendingPipelinePreset() const { return hasPendingPipelinePreset; }
        hgl::graph::InlinePipeline GetPendingPipelinePreset() const { return pendingPipelinePreset; }

        void SetResolvedRuntimePipeline(hgl::graph::Pipeline *p, hgl::graph::RenderPass *rp = nullptr)
        {
            resolvedRuntimePipeline = p;
            resolvedRuntimeRenderPass = rp;
            if(p) hasPendingPipelinePreset = false;
        }

        hgl::graph::Pipeline* GetResolvedRuntimePipeline() const { return resolvedRuntimePipeline; }
        hgl::graph::RenderPass* GetResolvedRuntimeRenderPass() const { return resolvedRuntimeRenderPass; }
        void ClearResolvedRuntimePipeline() { resolvedRuntimePipeline = nullptr; resolvedRuntimeRenderPass = nullptr; hasPendingPipelinePreset = true; }

        void SetTransformPolicySpec(const TransformPolicySpec& spec) { transformPolicySpec = spec; }
        const TransformPolicySpec& GetTransformPolicySpec() const { return transformPolicySpec; }
        void SetPositionSourceSpec(PositionSourceSpec spec) { positionSourceSpec = spec; }
        PositionSourceSpec GetPositionSourceSpec() const { return positionSourceSpec; }

        // Authoring entry (Phase 2): recipe stores intent only.
        // Runtime resolve/materialize is handled by ECS in later phases.
        void SetMaterialRecipe(const hgl::graph::mtl::MaterialRecipe &recipe);
        const hgl::graph::mtl::MaterialRecipe *GetMaterialRecipe() const;
        bool HasMaterialRecipe() const { return hasMaterialRecipe; }
        void ClearMaterialRecipe();
        void SetMaterialTextureResource(hgl::graph::mtl::TextureSlot slot,
                                        hgl::graph::Texture *texture,
                                        hgl::graph::Sampler *sampler,
                                        MaterialTextureResourceKind kind = MaterialTextureResourceKind::Texture2D,
                                        const std::string &resource_id = std::string(),
                                        bool required = false);
        void SetMaterialTextureValue(hgl::graph::mtl::TextureSlot slot, uint32_t value);
        const MaterialTextureAuthoringResource *GetMaterialTextureResource(hgl::graph::mtl::TextureSlot slot) const;
        void ClearMaterialTextureResource(hgl::graph::mtl::TextureSlot slot);
        void SetMaterialStructResource(hgl::graph::mtl::DataSlot slot,
                                       hgl::graph::mtl::SSBOType ssbo_type,
                                       uint32_t ssbo_id,
                                       hgl::graph::DeviceBuffer *buffer,
                                       uint32_t element_capacity,
                                       uint32_t byte_stride,
                                       uint32_t struct_index = 0,
                                       bool use_struct_index = false,
                                       bool shared_across_instances = false);
        const MaterialStructAuthoringResource *GetMaterialStructResource(hgl::graph::mtl::DataSlot slot) const;
        void ClearMaterialStructResource(hgl::graph::mtl::DataSlot slot);
        void ClearMaterialAuthoringResources();

        // MaterialProgram access (returns override if set, otherwise primitive's material)
        hgl::graph::MaterialInstance* GetMaterialInstance() const;
        hgl::graph::MaterialProgram* GetMaterialProgram() const;

        // Pipeline access: override → primitive's pre-baked → runtime resolved
        hgl::graph::Pipeline* GetPipeline() const;

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
    };
}//namespace hgl::ecs
