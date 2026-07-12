#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/mtl/DescriptorBindingContract.h>
#include<hgl/mtl/MaterializationPools.h>
#include<hgl/type/String.h>
#include<limits>
#include<vector>
#include<unordered_map>
#include<unordered_set>

namespace hgl::graph {
    template<typename> class StructuredBufferAccessor;
    struct ViewportInfo;
}

#ifndef ULRE_ECS_DEBUG_API
#define ULRE_ECS_DEBUG_API 1
#endif

namespace hgl::graph
{
    class RenderCmdBuffer;
    class Material;
    class IRenderTarget;
    class IGPUBuffer;
    class Texture;
    class Sampler;
}

namespace hgl::ecs
{
    /**
     * RenderDescriptorBindingSystem
     *
     * Centralized descriptor-binding submission point in ECS render pipeline.
     * It gathers descriptor bindings from render target and camera-related systems
     * and pushes them into current RenderCmdBuffer at RenderFrameSync phase.
     */
    class RenderDescriptorBindingSystem : public System
    {
    private:

        struct MaterialResourceBinding
        {
            graph::Texture *texture = nullptr;
            graph::Sampler *sampler = nullptr;
        };

        struct ContractDiagStats
        {
            uint32_t materials_checked = 0;
            uint32_t materials_unresolved = 0;
            uint32_t required_missing = 0;
            uint32_t optional_missing = 0;
            uint32_t fallback_hits = 0;

            bool operator==(const ContractDiagStats &rhs) const
            {
                return materials_checked == rhs.materials_checked
                    && materials_unresolved == rhs.materials_unresolved
                    && required_missing == rhs.required_missing
                    && optional_missing == rhs.optional_missing
                    && fallback_hits == rhs.fallback_hits;
            }

            bool operator!=(const ContractDiagStats &rhs) const
            {
                return !(*this == rhs);
            }
        };

        // Viewport UBO — owned here, stable across swapchain resize.
        graph::StructuredBufferAccessor<graph::ViewportInfo> *viewport_ubo = nullptr;
        uint32_t pending_viewport_width  = 0;
        uint32_t pending_viewport_height = 0;
        std::unordered_map<const graph::Material *, bool> contract_last_ok;
        std::unordered_map<const graph::Material *, std::unordered_map<std::string, MaterialResourceBinding>> material_resource_bindings;
        bool contract_diagnostics_enabled = true;
        bool enable_legacy_material_binding_fallback = true;
        ContractDiagStats last_contract_stats{};
        std::unordered_set<graph::Material *> pipeline_materials;
        graph::mtl::BindlessTexturePool materialization_texture_pool;
        graph::mtl::StructDataPool materialization_struct_pool;
        graph::mtl::MaterializationIndexTables materialization_index_tables;
        graph::mtl::MaterializationResolveCallbacks materialization_callbacks;
        uint32_t materialization_last_reset_frame = std::numeric_limits<uint32_t>::max();

    public:
        RenderDescriptorBindingSystem(const std::string& name = "RenderDescriptorBindingSystem");
        ~RenderDescriptorBindingSystem() override;

        graph::ViewportInfo *GetViewportInfo();
        void SetViewportExtent(uint32_t w, uint32_t h);

        void Update(float deltaTime) override;
        void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;

        bool GetContractDiagnosticsStats(uint32_t &materials_checked,
                                        uint32_t &materials_unresolved,
                                        uint32_t &required_missing,
                                        uint32_t &optional_missing,
                                        uint32_t &fallback_hits) const;
        bool GetMaterialBindingRegistryStats(uint32_t &materials_registered,
                             uint32_t &binding_entries) const;
        bool GetMaterialBindingKeys(const graph::Material *material,
                        std::vector<std::string> &out_keys) const;
        bool RegisterMaterialTexture(graph::Material *material,
                         const AnsiString &name,
                         graph::Texture *texture);
        bool RegisterMaterialTextureSampler(graph::Material *material,
                            const AnsiString &name,
                            graph::Texture *texture,
                            graph::Sampler *sampler);
        void RemoveMaterialBinding(graph::Material *material, const AnsiString &name);
        void ClearMaterialBindings(graph::Material *material);
        void RegisterPipelineMaterial(graph::Material *material);
        void UnregisterPipelineMaterial(graph::Material *material);
        void SetLegacyMaterialBindingFallbackEnabled(bool enabled) { enable_legacy_material_binding_fallback = enabled; }
        bool IsLegacyMaterialBindingFallbackEnabled() const { return enable_legacy_material_binding_fallback; }
        bool RegisterMaterialStructLayout(const std::string &struct_name,
                                          graph::mtl::SSBOCategory category,
                                          uint32_t byte_stride);
        void ResetMaterializationFrameData();
        bool ResolveMaterialRecipe(const graph::mtl::MaterialRecipe &recipe,
                                   graph::mtl::MaterializationSpec &out_spec,
                                   uint32_t *out_texture_layer_row = nullptr,
                                   uint32_t *out_data_index_row = nullptr);
        bool GetMaterializationPoolStats(uint32_t &texture_count,
                                         uint32_t &struct_layout_count,
                                         uint32_t &texture_layer_rows,
                                         uint32_t &data_index_rows) const;

    private:

        void EnsureViewportUBO();
        void ReleaseViewportUBO();
        void SyncBindingsForCurrentCommand(bool run_contract_diagnostics);
        void ApplyContractBindings();
        const graph::IGPUBuffer *ResolveViewportUBO() const;
        const graph::IGPUBuffer *ResolveCameraUBO() const;
        const graph::IGPUBuffer *ResolveSkyUBO();
        void EnsureMaterializationCallbacks();
        const MaterialResourceBinding *FindMaterialResourceBinding(const graph::Material *material, const char *name) const;
        void ValidateContractsSideChannel();
        bool IsSemanticResolvable(graph::mtl::DescriptorSemantic semantic) const;
    };
}
