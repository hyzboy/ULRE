#pragma once

#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/support/PositionSourceSpec.h>
#include<hgl/ecs/support/TransformPolicySpec.h>
#include<hgl/mtl/MaterialRecipe.h>
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

        struct MaterialDataSlotAuthoringResource
        {
            uint32_t data_slot = hgl::graph::mtl::DefaultMaterialDataSlot;
            hgl::graph::mtl::SSBOType ssbo_type = hgl::graph::mtl::SSBOType::UserDefined;
            uint32_t ssbo_id = 0;
            hgl::graph::DeviceBuffer *buffer = nullptr;
            uint32_t element_capacity = 0;
            uint32_t byte_stride = 0;
            uint32_t data_index = 0;
            bool use_data_index = false;
            bool shared_across_instances = false;
            bool authored = false;
        };

        struct MaterialDataSlotNamedAuthoringResource
        {
            std::string data_slot_name;
            uint32_t ssbo_id = 0;
            uint32_t data_index = 0;
            bool use_data_index = false;
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
        std::vector<MaterialDataSlotAuthoringResource> materialDataSlotResources;
        std::vector<MaterialDataSlotNamedAuthoringResource> materialDataSlotNamedResources{};

        // Late-resolve pipeline slot:
        // Populated at render-time if primitive has no pre-baked pipeline.
        hgl::graph::Pipeline* resolvedRuntimePipeline = nullptr; // (not owned)
        hgl::graph::RenderPass* resolvedRuntimeRenderPass = nullptr; // (not owned)
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

        void SetResolvedRuntimePipeline(hgl::graph::Pipeline *p, hgl::graph::RenderPass *rp = nullptr)
        {
            resolvedRuntimePipeline = p;
            resolvedRuntimeRenderPass = rp;
        }

        hgl::graph::Pipeline* GetResolvedRuntimePipeline() const { return resolvedRuntimePipeline; }
        hgl::graph::RenderPass* GetResolvedRuntimeRenderPass() const { return resolvedRuntimeRenderPass; }
        void ClearResolvedRuntimePipeline() { resolvedRuntimePipeline = nullptr; resolvedRuntimeRenderPass = nullptr; }

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
        void SetMaterialDataSlotResource(hgl::graph::mtl::SSBOType ssbo_type,
                                       uint32_t ssbo_id,
                                       hgl::graph::DeviceBuffer *buffer,
                                       uint32_t element_capacity,
                                       uint32_t byte_stride,
                                       uint32_t data_index = 0,
                                       bool use_data_index = false,
                                       bool shared_across_instances = false);
        void SetMaterialDataSlotResource(uint32_t data_slot,
                                       hgl::graph::mtl::SSBOType ssbo_type,
                                       uint32_t ssbo_id,
                                       hgl::graph::DeviceBuffer *buffer,
                                       uint32_t element_capacity,
                                       uint32_t byte_stride,
                                       uint32_t data_index = 0,
                                       bool use_data_index = false,
                                       bool shared_across_instances = false);
        uint32_t GetMaterialDataSlotCount() const { return static_cast<uint32_t>(materialDataSlotResources.size()); }
        const MaterialDataSlotAuthoringResource *GetMaterialDataSlotResource(hgl::graph::mtl::SSBOType ssbo_type) const;
        const MaterialDataSlotAuthoringResource *GetMaterialDataSlotResourceBySlot(uint32_t data_slot) const;
        void ClearMaterialDataSlotResource(hgl::graph::mtl::SSBOType ssbo_type);
        void ClearMaterialDataSlotResourceBySlot(uint32_t data_slot);
        void SetMaterialDataSlotResource(const MaterialDataSlotNamedAuthoringResource &resource);
        const MaterialDataSlotNamedAuthoringResource *GetMaterialDataSlotResource(const std::string &data_slot_name) const;
        void ClearMaterialDataSlotResource(const std::string &data_slot_name);
        void ClearMaterialAuthoringResources();

        // ShaderProgram access (returns override if set, otherwise descriptor-bound material)
        hgl::graph::ShaderProgram* GetShaderProgram() const;

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
