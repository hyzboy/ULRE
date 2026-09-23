#pragma once

#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/support/PositionSourceSpec.h>
#include<hgl/ecs/support/TransformPolicySpec.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/render/RenderItemDescriptor.h>
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>
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
        class ShaderProgram;
        class Pipeline;
        class RenderPass;
        class Sampler;
        class Texture;
    }
}

namespace hgl::ecs
{
    class RenderItemDataStorage;

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
            uint32_t array_layer = 0;
            bool required = false;
        };

        struct MaterialDataAuthoringResource
            : hgl::graph::GlobalSSBOBinding
        {
            hgl::graph::DeviceBuffer *buffer = nullptr;
            uint32_t element_capacity = 0;
            uint32_t byte_stride = 0;
            bool authored = false;

            MaterialDataAuthoringResource() = default;
            MaterialDataAuthoringResource(
                const MaterialDataAuthoringResource &) = default;
            MaterialDataAuthoringResource(
                const hgl::graph::GlobalSSBOBinding &binding) noexcept
                : hgl::graph::GlobalSSBOBinding(binding)
            {
            }

            MaterialDataAuthoringResource &operator=(
                const MaterialDataAuthoringResource &) = default;

            MaterialDataAuthoringResource &operator=(
                const hgl::graph::GlobalSSBOBinding &binding) noexcept
            {
                hgl::graph::GlobalSSBOBinding::operator=(binding);
                return *this;
            }

            hgl::graph::GlobalSSBOBinding
                GetGlobalSSBOBinding() const noexcept
            {
                return {ssbo_type, ssbo_id, data_index};
            }
        };

    private:

        const hgl::graph::PrimitiveAsset* primitiveAsset = nullptr;  // Asset-level geometry+recipe pairing (not owned)
        uint32_t primitiveVariantIndex = 0;
        hgl::graph::PrimitiveVariantPurpose primitiveVariantPurpose =
            hgl::graph::PrimitiveVariantPurpose::Surface;
        hgl::graph::GeometryDataBuffer *runtime_data_buffer = nullptr;
        hgl::graph::GeometryDrawRange *runtime_draw_range = nullptr;
        hgl::graph::Geometry *runtime_geometry = nullptr;
        hgl::graph::ShaderProgram *runtime_material = nullptr;
        hgl::graph::Pipeline* overridePipeline = nullptr;  // Optional pipeline override (not owned)
        bool hasMaterialRecipeOverride = false;
        hgl::graph::mtl::MaterialRecipe materialRecipeOverride;
        hgl::UnorderedMap<hgl::AnsiString, MaterialTextureAuthoringResource>
            namedMaterialTextureResources;
        MaterialDataAuthoringResource materialDataResource{};

        // Monotonic counter incremented every time authored material resources change
        // (textures, SSBOs, recipe). Compared against MaterialComponent to skip
        // BuildResolvedRecipe when nothing has changed.
        uint32_t material_authored_generation = 0;

        // Late-resolve pipeline slot:
        // Populated at render-time if primitive has no pre-baked pipeline.
        // 每个 RenderPass（≈每个 RenderTarget）各自持有解析出的管线——同一世界
        // 被 RenderTo 到多个 RT（如 ShadowMap 的 depth-only 离屏 Pass）时，
        // 各 RT 使用各自格式匹配的管线，互不驱逐。Pipeline 归 RenderPass 所有。
        hgl::UnorderedMap<hgl::graph::RenderPass *, hgl::graph::Pipeline *> resolvedRuntimePipelineMap;
        void InvalidateResolvedRuntimePipeline();

        PositionSourceSpec positionSourceSpec;            // Unified position source ingress policy
        TransformPolicySpec transformPolicySpec;           // Unified transform policy ingress

    protected:
        // RenderItem 4-ID descriptor handle and storage binding
        graph::RenderItemHandle render_item_handle = graph::INVALID_RENDER_ITEM_HANDLE;
        RenderItemDataStorage *bound_render_item_storage = nullptr;
        graph::RenderItemDescriptor render_item_descriptor{};

        virtual void EnsureRenderItemStorageAllocated();

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
        void SetPrimitiveVariantPurpose(
            const hgl::graph::PrimitiveVariantPurpose purpose)
        {
            if (primitiveVariantPurpose == purpose)
                return;
            primitiveVariantPurpose = purpose;
            InvalidateResolvedRuntimePipeline();
        }
        hgl::graph::PrimitiveVariantPurpose GetPrimitiveVariantPurpose()
            const
        {
            return primitiveVariantPurpose;
        }
        bool EnsureRuntimeGeometryBinding(hgl::graph::ShaderProgram *material);
        void ClearRuntimeGeometryBinding();
        const hgl::graph::GeometryDataBuffer *GetRuntimeGeometryDataBuffer() const;
        const hgl::graph::GeometryDrawRange *GetRuntimeGeometryDrawRange() const;

        void SetOverridePipeline(hgl::graph::Pipeline* p) { overridePipeline = p; }
        hgl::graph::Pipeline* GetOverridePipeline() const { return overridePipeline; }
        void ClearOverridePipeline() { overridePipeline = nullptr; }

        void SetResolvedRuntimePipeline(hgl::graph::RenderPass *rp, hgl::graph::Pipeline *p)
        {
            if (!rp || !p)
                return;

            if (hgl::graph::Pipeline **existing = resolvedRuntimePipelineMap.GetValuePointer(rp))
                *existing = p;
            else
                resolvedRuntimePipelineMap.Add(rp, p);
        }

        bool HasResolvedRuntimePipeline(hgl::graph::RenderPass *rp) const
        {
            return rp && resolvedRuntimePipelineMap.ContainsKey(rp);
        }

        void SetTransformPolicySpec(const TransformPolicySpec& spec) { transformPolicySpec = spec; }
        const TransformPolicySpec& GetTransformPolicySpec() const { return transformPolicySpec; }
        void SetPositionSourceSpec(PositionSourceSpec spec) { positionSourceSpec = spec; }
        PositionSourceSpec GetPositionSourceSpec() const { return positionSourceSpec; }

        // Authoring entry: asset recipe is the default source, component recipe is the override source.
        // Runtime resolve/materialize is handled by ECS in later phases.
        void SetMaterialRecipe(const hgl::graph::mtl::MaterialRecipe &recipe);
        const hgl::graph::mtl::MaterialRecipe *GetMaterialRecipeOverride() const;
        const hgl::graph::mtl::MaterialRecipe *GetAssetMaterialRecipe() const;
        bool BuildResolvedAuthoringMaterialRecipe(hgl::graph::mtl::MaterialRecipe &out_recipe,
                                                  const hgl::graph::ShaderProgram *material_program = nullptr) const;
        bool HasMaterialRecipeOverride() const { return GetMaterialRecipeOverride() != nullptr; }
        bool HasAnyMaterialRecipeSource() const { return GetMaterialRecipeOverride() != nullptr || GetAssetMaterialRecipe() != nullptr; }
        bool SetMaterialTextureResource(const std::string &name,
                                        hgl::graph::Texture *texture,
                                        hgl::graph::Sampler *sampler,
                                        MaterialTextureResourceKind kind = MaterialTextureResourceKind::Texture2D,
                                        const std::string &resource_id = std::string(),
                                        uint32_t array_layer = 0,
                                        bool required = false);
        bool SetMaterialTextureArrayLayer(const std::string &name, uint32_t array_layer);
        const MaterialTextureAuthoringResource *GetMaterialTextureResource(const std::string &name) const;
        void SetMaterialDataResource(
            const MaterialDataAuthoringResource &resource);
        const MaterialDataAuthoringResource *GetMaterialDataResource() const;
        void ClearMaterialDataResource();
        void ClearMaterialAuthoringResources();

        // Generation counter for authored material resources.
        // Compared against MaterialComponent to detect changes.
        uint32_t GetMaterialAuthoredGeneration() const { return material_authored_generation; }

        // ShaderProgram access (returns override if set, otherwise descriptor-bound material)
        hgl::graph::ShaderProgram* GetShaderProgram() const;

        // Pipeline access: override → runtime resolved (per render pass)
        hgl::graph::Pipeline* GetPipelineForRenderPass(hgl::graph::RenderPass *render_pass) const;

        // Bounding volume
        bool GetLocalAABB(hgl::math::AABB& outAABB) const;

        // Rendering capability check
        bool CanRender() const;

        // RenderItem 4-ID Descriptor & Handle
        graph::RenderItemHandle GetRenderItemHandle() const;
        const graph::RenderItemDescriptor &GetRenderItemDescriptor() const;

        void SetTransformID(uint32_t transform_id);
        void SetGeometryID(uint32_t geometry_id);
        void SetMaterialID(uint32_t material_id);
        void SetTextureID(uint32_t texture_id);
        void Set4ID(uint32_t transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id);

        uint32_t GetTransformID() const { return render_item_descriptor.transform_id; }
        uint32_t GetGeometryID() const { return render_item_descriptor.geometry_id; }
        uint32_t GetMaterialID() const { return render_item_descriptor.material_id; }
        uint32_t GetTextureID() const { return render_item_descriptor.texture_id; }

    public:

        // Kept as a no-op compatibility override while some call sites/vtables still
        // expect a concrete PrimitiveComponent::Render symbol. ECS render path does
        // not use this entry for actual draw submission.

        // Component lifecycle
        void OnAttach() override;
        void OnDetach() override;
    };
}//namespace hgl::ecs
