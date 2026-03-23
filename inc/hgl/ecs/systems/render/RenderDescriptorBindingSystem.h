#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/mtl/DescriptorBindingContract.h>
#include<hgl/type/String.h>
#include<vector>
#include<unordered_map>
#include<unordered_set>
#include<functional>

namespace hgl::graph {
    template<typename> class UBOAccessor;
    struct ViewportInfo;
}

#ifndef ULRE_ECS_DEBUG_API
#define ULRE_ECS_DEBUG_API 1
#endif

namespace hgl::graph
{
    class RenderCmdBuffer;
    class Material;
    class DomainMaterialBinding;
    class IRenderTarget;
    class IGPUBuffer;
    class Texture;
    class Sampler;
}

namespace hgl::ecs
{
    using TextureBindingSlot = uint8_t;
    using SceneUBOResolver = std::function<const graph::IGPUBuffer*()>;

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
        graph::UBOAccessor<graph::ViewportInfo> *viewport_ubo = nullptr;
        uint32_t pending_viewport_width  = 0;
        uint32_t pending_viewport_height = 0;
        std::unordered_map<const graph::Material *, bool> contract_last_ok;
        std::unordered_map<const graph::Material *, std::unordered_map<TextureBindingSlot, MaterialResourceBinding>> material_resource_bindings;
        bool contract_diagnostics_enabled = true;
        ContractDiagStats last_contract_stats{};
        std::unordered_set<graph::Material *> pipeline_materials;

        // Phase 2 — DomainMaterialBinding texture/sampler bindings
        std::unordered_map<const graph::DomainMaterialBinding *,
                           std::unordered_map<TextureBindingSlot, MaterialResourceBinding>>
            domain_resource_bindings;
        std::unordered_set<graph::DomainMaterialBinding *> registered_domain_bindings;

        std::unordered_map<graph::mtl::DescriptorSemantic, SceneUBOResolver> scene_ubo_resolvers;

    public:
        RenderDescriptorBindingSystem(const std::string& name = "RenderDescriptorBindingSystem");
        ~RenderDescriptorBindingSystem() override;

        graph::ViewportInfo *GetViewportInfo();
        void SetViewportExtent(uint32_t w, uint32_t h);

        void RegisterSceneUBOResolver(graph::mtl::DescriptorSemantic semantic, SceneUBOResolver resolver);
        void UnregisterSceneUBOResolver(graph::mtl::DescriptorSemantic semantic);

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
                                 graph::mtl::SamplerSlot slot,
                         graph::Texture *texture);
        bool RegisterMaterialTextureSampler(graph::Material *material,
                                     graph::mtl::SamplerSlot slot,
                            graph::Texture *texture,
                            graph::Sampler *sampler);
        void RemoveMaterialBinding(graph::Material *material, graph::mtl::SamplerSlot slot);
        void ClearMaterialBindings(graph::Material *material);
        void RegisterPipelineMaterial(graph::Material *material);
        void UnregisterPipelineMaterial(graph::Material *material);
        // Phase 2 — Domain binding interface
        /**
         * 注册域绑定，使其参与每帧的 contract UBO 同步。
         * 相同 domain 可绑多个 Material（Opaque + Masked，Phase 3）。
         */
        void RegisterDomainBinding(graph::DomainMaterialBinding *binding);
        void UnregisterDomainBinding(graph::DomainMaterialBinding *binding);

        /**
         * 为指定域绑定注册 Texture/Sampler，用于 MaterialTexture 语义。
         */
        bool RegisterDomainTexture(graph::DomainMaterialBinding *binding,
                        graph::mtl::SamplerSlot slot, graph::Texture *tex);
        bool RegisterDomainTextureSampler(graph::DomainMaterialBinding *binding,
                            graph::mtl::SamplerSlot slot,
                                          graph::Texture *tex, graph::Sampler *sampler);
        void ClearDomainBindings(graph::DomainMaterialBinding *binding);

    private:

        void EnsureViewportUBO();
        void ReleaseViewportUBO();
        void InitializeResolvers();
        void SyncBindingsForCurrentCommand(bool run_contract_diagnostics);

        void ApplyBatchMaterialBindings(std::unordered_set<const graph::Material *> &out_active);
        void ApplyPipelineMaterialBindings(std::unordered_set<const graph::Material *> &out_active);
        void ApplyDomainBindings();
        void PurgeStaleBindings(const std::unordered_set<const graph::Material *> &active);

        const MaterialResourceBinding *FindMaterialResourceBinding(const graph::Material *material, graph::mtl::SamplerSlot slot) const;
        const MaterialResourceBinding *FindDomainResourceBinding(const graph::DomainMaterialBinding *binding, graph::mtl::SamplerSlot slot) const;
        void ValidateContractsSideChannel();
        bool IsSemanticResolvable(graph::mtl::DescriptorSemantic semantic) const;
    };
}

