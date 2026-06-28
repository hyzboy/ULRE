#include <hgl/ecs/systems/render/RenderDiagnosticsService.h>
#include <hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/time/Time.h>

#include <map>
#include <string>

namespace hgl::ecs
{
    RenderDiagnosticsService::RenderDiagnosticsService(const std::string &name)
        : System(name)
    {
        SetSystemType(SystemType::ShaderMaterialProgram);
        SetExecutionOrder(ExecutionPhase::RenderFrameSync);
        AddDependency<RenderDescriptorBindingSystem>();
    }

    bool RenderDiagnosticsService::GetDescriptorContractDiagnostics(uint32_t &materials_checked,
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
        if (!world)
            return false;

        auto descriptor_system = world->GetSystem<RenderDescriptorBindingSystem>();
        if (!descriptor_system)
            return false;

        return descriptor_system->GetContractDiagnosticsStats(materials_checked,
                                                              materials_unresolved,
                                                              required_missing,
                                                              optional_missing,
                                                              fallback_hits);
#endif
    }

    bool RenderDiagnosticsService::GetDescriptorContractDiagnosticsExtended(uint32_t &materials_checked,
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
        if (!GetDescriptorContractDiagnostics(materials_checked,
                                              materials_unresolved,
                                              required_missing,
                                              optional_missing,
                                              fallback_hits))
            return false;

        if (!GetMaterialBindingRegistryStats(materials_registered, binding_entries))
        {
            materials_registered = 0;
            binding_entries = 0;
        }

        return true;
#endif
    }

    bool RenderDiagnosticsService::GetMaterialBindingRegistryStats(uint32_t &materials_registered,
                                                                   uint32_t &binding_entries) const
    {
        materials_registered = 0;
        binding_entries = 0;

#if !ULRE_ECS_DEBUG_API
        return false;
#else
        if (!world)
            return false;

        auto descriptor_system = world->GetSystem<RenderDescriptorBindingSystem>();
        if (!descriptor_system)
            return false;

        return descriptor_system->GetMaterialBindingRegistryStats(materials_registered,
                                                                  binding_entries);
#endif
    }

    bool RenderDiagnosticsService::GetMaterialBindingKeys(const graph::ShaderMaterialProgram *material,
                                                          std::vector<std::string> &out_keys) const
    {
        out_keys.clear();

        if (!material)
            return false;

#if !ULRE_ECS_DEBUG_API
        return false;
#else
        if (!world)
            return false;

        auto descriptor_system = world->GetSystem<RenderDescriptorBindingSystem>();
        if (!descriptor_system)
            return false;

        return descriptor_system->GetMaterialBindingKeys(material, out_keys);
#endif
    }

    void RenderDiagnosticsService::Update(float)
    {
#if ULRE_ECS_DEBUG_API
        if (!world || !world->IsDescriptorContractDiagnosticsLogEnabled())
            return;

        const uint64_t now_ms = ::hgl::GetTimeMs();
        if (last_emit_ms == 0)
        {
            last_emit_ms = now_ms;
            return;
        }

        if (now_ms - last_emit_ms < 1000)
            return;

        uint32_t materials_checked = 0;
        uint32_t materials_unresolved = 0;
        uint32_t required_missing = 0;
        uint32_t optional_missing = 0;
        uint32_t fallback_hits = 0;
        uint32_t materials_registered = 0;
        uint32_t binding_entries = 0;

        if (GetDescriptorContractDiagnosticsExtended(materials_checked,
                                 materials_unresolved,
                                 required_missing,
                                 optional_missing,
                                 fallback_hits,
                                 materials_registered,
                                 binding_entries))
        {
            LogInfo("[DescriptorContract][ECSContext] checked=%u unresolved=%u required_missing=%u optional_missing=%u fallback_hits=%u registered_materials=%u registered_bindings=%u",
                    materials_checked,
                    materials_unresolved,
                    required_missing,
                    optional_missing,
                    fallback_hits,
                    materials_registered,
                    binding_entries);

            std::map<std::string, uint32_t> category_histogram;
            if (world->GetShaderGenValidationCategoryHistogram(category_histogram, 128))
            {
                const auto count_of = [&category_histogram](const char *category) -> uint32_t
                {
                    auto it = category_histogram.find(category);
                    return it == category_histogram.end() ? 0u : it->second;
                };

                const uint32_t strict_prebuild = count_of("StrictGate.Prebuild");
                const uint32_t strict_spv = count_of("StrictGate.Spv");
                const uint32_t strict_vertex = count_of("StrictGate.Vertex");
                const uint32_t strict_descriptor = count_of("StrictGate.Descriptor");
                const uint32_t strict_total = strict_prebuild + strict_spv + strict_vertex + strict_descriptor;

                if (strict_total > 0)
                {
                    uint32_t strict_materials = 0;
                    std::map<std::string, std::map<std::string, uint32_t>> material_category_matrix;
                    if (world->GetShaderGenValidationMaterialCategoryMatrix(material_category_matrix, 128))
                    {
                        for (const auto &mat_pair : material_category_matrix)
                        {
                            bool has_strict = false;
                            for (const auto &cat_pair : mat_pair.second)
                            {
                                if (cat_pair.second > 0 && cat_pair.first.rfind("StrictGate.", 0) == 0)
                                {
                                    has_strict = true;
                                    break;
                                }
                            }

                            if (has_strict)
                                ++strict_materials;
                        }
                    }

                    LogInfo("[ShaderGenValidation][ECSContext] strict_total=%u prebuild=%u spv=%u vertex=%u descriptor=%u strict_materials=%u",
                            strict_total,
                            strict_prebuild,
                            strict_spv,
                            strict_vertex,
                            strict_descriptor,
                            strict_materials);
                }
            }
        }

        last_emit_ms = now_ms;
#endif
    }
}
