#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include<hgl/vk/UBOTypes.h>
#include<hgl/type/String.h>
#include<vector>
#include<array>
#include<unordered_map>
#include<unordered_set>
#include<functional>

#ifndef ULRE_ECS_DEBUG_API
#define ULRE_ECS_DEBUG_API 1
#endif

namespace hgl::graph
{
    class UBOAccessorBase;
    class RenderCmdBuffer;
    class ShaderMaterialProgram;
    class DomainResourceBinding;
    class IRenderTarget;
    class IGPUBuffer;
    class Texture;
    class Sampler;
}

namespace hgl::ecs
{
    class CameraSystem;
    class EnvironmentSystem;

    using TextureBindingSlot = uint8_t;

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
        graph::UBOViewportInfo *viewport_ubo = nullptr;
        uint32_t pending_viewport_width  = 0;
        uint32_t pending_viewport_height = 0;
        std::unordered_map<const graph::ShaderMaterialProgram *, bool> contract_last_ok;
        std::unordered_map<const graph::ShaderMaterialProgram *, std::unordered_map<TextureBindingSlot, MaterialResourceBinding>> material_resource_bindings;
        bool contract_diagnostics_enabled = true;
        ContractDiagStats last_contract_stats{};
        std::unordered_set<graph::ShaderMaterialProgram *> pipeline_materials;

        // Phase 2 — DomainResourceBinding texture/sampler bindings
        std::unordered_map<const graph::DomainResourceBinding *, std::unordered_map<TextureBindingSlot, MaterialResourceBinding>> domain_resource_bindings;
        std::unordered_set<graph::DomainResourceBinding *> registered_domain_bindings;

        std::array<graph::UBOAccessorBase *, graph::mtl::UBODescriptorSemanticCount> scene_ubo_resolvers{};
        CameraSystem *camera_system = nullptr;
        EnvironmentSystem *environment_system = nullptr;

    public:
        RenderDescriptorBindingSystem(const std::string& name = "RenderDescriptorBindingSystem");
        ~RenderDescriptorBindingSystem() override;

        graph::ViewportInfo *GetViewportInfo();
        void SetViewportExtent(uint32_t w, uint32_t h);

        void RegisterSceneUBOResolver(graph::UBOAccessorBase *ubo_accessor);
        void UnregisterSceneUBOResolver(graph::mtl::UBODescriptorSemantic semantic);

        void Update(float deltaTime) override;
        void Render(graph::RenderCmdBuffer *cmd, float deltaTime) override;

        bool GetContractDiagnosticsStats(uint32_t &materials_checked,
                                        uint32_t &materials_unresolved,
                                        uint32_t &required_missing,
                                        uint32_t &optional_missing,
                                        uint32_t &fallback_hits) const;
        bool GetMaterialBindingRegistryStats(uint32_t &materials_registered,
                             uint32_t &binding_entries) const;
        bool GetMaterialBindingKeys(const graph::ShaderMaterialProgram *material,
                        std::vector<std::string> &out_keys) const;
        bool RegisterMaterialTexture(graph::ShaderMaterialProgram *material,
                                 graph::mtl::SamplerSlot slot,
                         graph::Texture *texture);
        bool RegisterMaterialTextureSampler(graph::ShaderMaterialProgram *material,
                                     graph::mtl::SamplerSlot slot,
                            graph::Texture *texture,
                            graph::Sampler *sampler);
        void RemoveMaterialBinding(graph::ShaderMaterialProgram *material, graph::mtl::SamplerSlot slot);
        void ClearMaterialBindings(graph::ShaderMaterialProgram *material);
        void RegisterPipelineMaterial(graph::ShaderMaterialProgram *material);
        void UnregisterPipelineMaterial(graph::ShaderMaterialProgram *material);
        // Phase 2 — Domain binding interface
        /**
         * 注册域绑定，使其参与每帧的 contract UBO 同步。
         * 相同 domain 可绑多个 ShaderMaterialProgram（Opaque + Masked，Phase 3）。
         */
        void RegisterDomainBinding(graph::DomainResourceBinding *binding);
        void UnregisterDomainBinding(graph::DomainResourceBinding *binding);

        /**
         * 为指定域绑定注册 Texture/Sampler，用于 MaterialTexture 语义。
         */
        bool RegisterDomainTexture(graph::DomainResourceBinding *binding,
                        graph::mtl::SamplerSlot slot, graph::Texture *tex);
        bool RegisterDomainTextureSampler(graph::DomainResourceBinding *binding,
                            graph::mtl::SamplerSlot slot,
                                          graph::Texture *tex, graph::Sampler *sampler);
        void ClearDomainBindings(graph::DomainResourceBinding *binding);

    private:

        void EnsureViewportUBO();
        void ReleaseViewportUBO();
        void InitializeResolvers();
        void RefreshSceneUBOResolvers();
        void SyncBindingsForCurrentCommand(bool run_contract_diagnostics);

        void ApplyBatchMaterialBindings(std::unordered_set<const graph::ShaderMaterialProgram *> &out_active);
        void ApplyPipelineMaterialBindings(std::unordered_set<const graph::ShaderMaterialProgram *> &out_active);
        void ApplyDomainBindings();
        void PurgeStaleBindings(const std::unordered_set<const graph::ShaderMaterialProgram *> &active);

        const MaterialResourceBinding *FindMaterialResourceBinding(const graph::ShaderMaterialProgram *material, graph::mtl::SamplerSlot slot) const;
        const MaterialResourceBinding *FindDomainResourceBinding(const graph::DomainResourceBinding *binding, graph::mtl::SamplerSlot slot) const;
        void ValidateContractsSideChannel();
        bool IsUBOSemanticResolvable(graph::mtl::UBODescriptorSemantic semantic) const;
        bool IsSSBOSemanticResolvable(graph::mtl::SSBODescriptorSemantic semantic) const;
    };
}

