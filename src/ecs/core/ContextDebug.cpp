#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

namespace hgl
{
    namespace ecs
    {
        bool ECSContext::GetDescriptorContractDiagnosticsExtended(uint32_t &materials_checked,
                                                                  uint32_t &materials_unresolved,
                                                                  uint32_t &required_missing,
                                                                  uint32_t &optional_missing,
                                                                  uint32_t &fallback_hits,
                                                                  uint32_t &materials_registered,
                                                                  uint32_t &binding_entries) const
        {
            materials_checked = 0;
            materials_unresolved = 0;
            required_missing = 0;
            optional_missing = 0;
            fallback_hits = 0;
            materials_registered = 0;
            binding_entries = 0;

        #if !ULRE_ECS_DEBUG_API
            return false;
        #else
            auto descriptor_system = GetSystem<RenderDescriptorBindingSystem>();
            if (!descriptor_system)
                return false;

            if (!descriptor_system->GetContractDiagnosticsStats(materials_checked,
                                                                materials_unresolved,
                                                                required_missing,
                                                                optional_missing,
                                                                fallback_hits))
                return false;

            descriptor_system->GetMaterialBindingRegistryStats(materials_registered,
                                                               binding_entries);
            return true;
        #endif
        }

    }
}
