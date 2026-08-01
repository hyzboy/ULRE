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
        class DeviceBuffer;
        struct GeometryDataBuffer;
        struct GeometryDrawRange;
        class Geometry;
        class PrimitiveAsset;
        class ShaderProgram;
        class Pipeline;
        class RenderPass;
        class Sampler;
        class Texture;
        class VertexInputLayout;
    }
}

namespace hgl::ecs
{
    /**
     * PrimitiveComponent - Renderable component for static mesh rendering
     *
     * Manages a single PrimitiveAsset (geometry + recipe) for rendering.
     * Derived from RenderableComponent to provide rendering capabilities.
     *
     * Features:
     * - Holds reference to hgl::graph::PrimitiveAsset
     * - Provides access to ShaderProgram, Pipeline, and AABB data
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

        struct MaterialSSBOAuthoringResource
        {
            uint32_t ssbo_slot = hgl::graph::mtl::DefaultMaterialSSBOSlot;
            hgl::graph::mtl::SSBOType ssbo_type = hgl::graph::mtl::SSBOType::UserDefined;
            uint32_t ssbo_id = 0;
            hgl::graph::DeviceBuffer *buffer = nullptr;
            uint32_t element_capacity = 0;
            uint32_t byte_stride = 0;
            uint32_t ssbo_element_index = 0;
            bool use_ssbo_element_index = false;
            bool shared_across_instances = false;
            bool authored = false;
        };

        struct MaterialSSBONamedAuthoringResource
        {
            std::string ssbo_name;
            uint32_t ssbo_id = 0;
            uint32_t ssbo_element_index = 0;
            bool use_ssbo_element_index = false;
            bool shared_across_instances = false;
            bool authored = false;
        };

    private:

        const hgl::graph::PrimitiveAsset* primitiveAsset = nullptr;  // Asset-level geometry+recipe pairing (not owned)
        uint32_t primitiveVariantIndex = 0;
        hgl::graph::GeometryDataBuffer *runtime_data_buffer = nullptr;
        hgl::graph::GeometryDrawRange *runtime_draw_range = nullptr;
        hgl::graph::Geometry *runtime_geometry = nullptr;
        hgl::graph::ShaderProgram *runtime_material = nullptr;
        const hgl::graph::VertexInputLayout *runtime_vil = nullptr;
        bool runtime_vil_owned = false;
        hgl::graph::Pipeline* overridePipeline = nullptr;  // Optional pipeline override (not owned)
        bool hasMaterialRecipeOverride = false;
        hgl::graph::mtl::MaterialRecipe materialRecipeOverride;
        std::array<MaterialTextureAuthoringResource, static_cast<size_t>(hgl::graph::mtl::TextureSlot::RANGE_SIZE)> materialTextureResources{};
        std::vector<MaterialSSBOAuthoringResource> materialSSBOResources;
        std::vector<MaterialSSBONamedAuthoringResource> materialSSBONamedResources{};

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
            , overridePipeline(nullptr)
            , positionSourceSpec(PositionSourceSpec::MeshVertex)
            , transformPolicySpec{}
        {
        }

        virtual ~PrimitiveComponent() = default;

    public:

        // Primitive management
        const char* GetSystemGroupName() const override { return "Primitive"; }

        void SetPrimitiveAsset(const hgl::graph::PrimitiveAsset *asset);
        const hgl::graph::PrimitiveAsset *GetPrimitiveAsset() const { return primitiveAsset; }
        void ClearPrimitiveAsset() { SetPrimitiveAsset(nullptr); }
        void SetPrimitiveVariantIndex(const uint32_t index) { primitiveVariantIndex = index; }
        uint32_t GetPrimitiveVariantIndex() const { return primitiveVariantIndex; }
        bool EnsureRuntimeGeometryBinding(hgl::graph::ShaderProgram *material);
        void ClearRuntimeGeometryBinding();
        const hgl::graph::GeometryDataBuffer *GetRuntimeGeometryDataBuffer() const;
        const hgl::graph::GeometryDrawRange *GetRuntimeGeometryDrawRange() const;
        const hgl::graph::VertexInputLayout *GetRuntimeVIL() const;

        void SetOverridePipeline(hgl::graph::Pipeline* p) { overridePipeline = p; }
        hgl::graph::Pipeline* GetOverridePipeline() const { return overridePipeline; }
        void ClearOverridePipeline() { overridePipeline = nullptr; }

        // Runtime pipeline request:
        // Call this when creating a Primitive without a pre-baked pipeline.
        // Collect/resolve stages will materialize the pipeline before batching.
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

        // Authoring entry: asset recipe is the default source, component recipe is the override source.
        // Runtime resolve/materialize is handled by ECS in later phases.
        void SetMaterialRecipe(const hgl::graph::mtl::MaterialRecipe &recipe);
        const hgl::graph::mtl::MaterialRecipe *GetMaterialRecipe() const;
        const hgl::graph::mtl::MaterialRecipe *GetMaterialRecipeOverride() const;
        const hgl::graph::mtl::MaterialRecipe *GetAssetMaterialRecipe() const;
        bool BuildResolvedAuthoringMaterialRecipe(hgl::graph::mtl::MaterialRecipe &out_recipe,
                                                  const hgl::graph::ShaderProgram *material_program = nullptr) const;
        bool HasMaterialRecipe() const { return GetMaterialRecipeOverride() != nullptr; }
        bool HasMaterialRecipeOverride() const { return GetMaterialRecipeOverride() != nullptr; }
        bool HasAnyMaterialRecipeSource() const { return GetMaterialRecipeOverride() != nullptr || GetAssetMaterialRecipe() != nullptr; }
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
        void SetMaterialSSBOResource(hgl::graph::mtl::SSBOType ssbo_type,
                                       uint32_t ssbo_id,
                                       hgl::graph::DeviceBuffer *buffer,
                                       uint32_t element_capacity,
                                       uint32_t byte_stride,
                                       uint32_t ssbo_element_index = 0,
                                       bool use_ssbo_element_index = false,
                                       bool shared_across_instances = false);
        void SetMaterialSSBOResource(uint32_t ssbo_slot,
                                       hgl::graph::mtl::SSBOType ssbo_type,
                                       uint32_t ssbo_id,
                                       hgl::graph::DeviceBuffer *buffer,
                                       uint32_t element_capacity,
                                       uint32_t byte_stride,
                                       uint32_t ssbo_element_index = 0,
                                       bool use_ssbo_element_index = false,
                                       bool shared_across_instances = false);
        uint32_t GetMaterialSSBOSlotCount() const { return static_cast<uint32_t>(materialSSBOResources.size()); }
        const MaterialSSBOAuthoringResource *GetMaterialSSBOResource(hgl::graph::mtl::SSBOType ssbo_type) const;
        const MaterialSSBOAuthoringResource *GetMaterialSSBOResourceBySlot(uint32_t ssbo_slot) const;
        void ClearMaterialSSBOResource(hgl::graph::mtl::SSBOType ssbo_type);
        void ClearMaterialSSBOResourceBySlot(uint32_t ssbo_slot);
        void SetMaterialSSBOResource(const MaterialSSBONamedAuthoringResource &resource);
        const MaterialSSBONamedAuthoringResource *GetMaterialSSBOResource(const std::string &ssbo_name) const;
        void ClearMaterialSSBOResource(const std::string &ssbo_name);
        void ClearMaterialAuthoringResources();

        // ShaderProgram access (returns override if set, otherwise descriptor-bound material)
        hgl::graph::ShaderProgram* GetMaterialProgram() const;

        // Pipeline access: override → runtime resolved
        hgl::graph::Pipeline* GetPipeline() const;

        // Bounding volume
        bool GetLocalAABB(hgl::math::AABB& outAABB) const;

        // Rendering capability check
        bool CanRender() const;

    public:

        // Kept as a no-op compatibility override while some call sites/vtables still
        // expect a concrete PrimitiveComponent::Render symbol. ECS render path does
        // not use this entry for actual draw submission.
        void Render(const glm::mat4& worldMatrix) override;

        // Component lifecycle
        void OnAttach() override;
        void OnUpdate(float deltaTime) override;
        void OnDetach() override;
    };
}//namespace hgl::ecs
