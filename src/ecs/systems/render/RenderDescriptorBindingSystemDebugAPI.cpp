#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

namespace hgl::ecs
{
    bool RenderDescriptorBindingSystem::GetContractDiagnosticsStats(uint32_t &materials_checked,
                                                                    uint32_t &materials_unresolved,
                                                                    uint32_t &required_missing,
                                                                    uint32_t &optional_missing,
                                                                    uint32_t &fallback_hits) const
    {
        materials_checked = 0;
        materials_unresolved = 0;
        required_missing = 0;
        optional_missing = 0;
        fallback_hits = 0;

    #if !ULRE_ECS_DEBUG_API
        return false;
    #else
        materials_checked = last_contract_stats.materials_checked;
        materials_unresolved = last_contract_stats.materials_unresolved;
        required_missing = last_contract_stats.required_missing;
        optional_missing = last_contract_stats.optional_missing;
        fallback_hits = last_contract_stats.fallback_hits;
        return true;
    #endif
    }

    bool RenderDescriptorBindingSystem::GetMaterialBindingRegistryStats(uint32_t &materials_registered,
                                                                        uint32_t &binding_entries) const
    {
        materials_registered = 0;
        binding_entries = 0;

    #if !ULRE_ECS_DEBUG_API
        return false;
    #else
        materials_registered = static_cast<uint32_t>(materialization_resource_handles.GetCount());
        binding_entries = materials_registered;

        return true;
    #endif
    }
}
