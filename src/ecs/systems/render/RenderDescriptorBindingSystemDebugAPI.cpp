#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<algorithm>
#include<limits>

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
        materials_registered = static_cast<uint32_t>(material_resource_bindings.size());

        for (const auto &pair : material_resource_bindings)
            binding_entries += static_cast<uint32_t>(pair.second.size());

        return true;
    #endif
    }

    bool RenderDescriptorBindingSystem::GetCompatibilityId0FallbackStats(uint32_t &current_frame_hits,
                                                                          uint32_t &last_summary_hits,
                                                                          uint32_t &last_summary_frame) const
    {
        current_frame_hits = 0;
        last_summary_hits = 0;
        last_summary_frame = std::numeric_limits<uint32_t>::max();

    #if !ULRE_ECS_DEBUG_API
        return false;
    #else
        current_frame_hits = compat_id0_fallback_hits_current_frame;
        last_summary_hits = compat_id0_fallback_hits_last_summary;
        last_summary_frame = compat_id0_fallback_summary_frame;
        return true;
    #endif
    }

    bool RenderDescriptorBindingSystem::GetMaterialBindingKeys(const graph::Material *material,
                                                               std::vector<std::string> &out_keys) const
    {
        out_keys.clear();

        if (!material)
            return false;

    #if !ULRE_ECS_DEBUG_API
        return false;
    #else
        auto material_it = material_resource_bindings.find(material);
        if (material_it == material_resource_bindings.end())
            return false;

        out_keys.reserve(material_it->second.size());
        for (const auto &entry : material_it->second)
            out_keys.push_back(entry.first);

        std::sort(out_keys.begin(), out_keys.end());
        return true;
    #endif
    }
}
