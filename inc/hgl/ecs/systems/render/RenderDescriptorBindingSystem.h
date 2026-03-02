#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/shadergen/DescriptorBindingContract.h>
#include<vector>
#include<functional>
#include<unordered_map>

namespace hgl::graph
{
    class DescriptorBinding;
    class RenderCmdBuffer;
    class Material;
    class IRenderTarget;
    class IGPUBuffer;
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
    public:

        using BindingSource = std::function<void(ECSContext *, graph::RenderCmdBuffer *, graph::DescriptorBinding *)>;

    private:

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

        graph::DescriptorBinding* view_desc_binding = nullptr;
        std::vector<BindingSource> binding_sources;
        std::unordered_map<const graph::Material *, bool> contract_last_ok;
        bool contract_diagnostics_enabled = true;
        ContractDiagStats last_contract_stats{};

    public:
        RenderDescriptorBindingSystem(const std::string& name = "RenderDescriptorBindingSystem");
        ~RenderDescriptorBindingSystem() override;

        void Update(float deltaTime) override;
        void RegisterBindingSource(BindingSource source);
        bool GetContractDiagnosticsStats(uint32_t &materials_checked,
                                        uint32_t &materials_unresolved,
                                        uint32_t &required_missing,
                                        uint32_t &optional_missing,
                                        uint32_t &fallback_hits) const;

    private:

        void RegisterDefaultSources();
        void EnsureViewBinding();
        void ApplyContractBindings();
        const graph::IGPUBuffer *ResolveViewportUBO(graph::IRenderTarget *rt,const char *preferred_name) const;
        const graph::IGPUBuffer *ResolveCameraUBO() const;
        const graph::IGPUBuffer *ResolveSkyUBO();
        void ValidateContractsSideChannel();
        bool IsSemanticResolvable(graph::mtl::DescriptorSemantic semantic) const;
    };
}
