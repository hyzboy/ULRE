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

    bool RenderDiagnosticsService::RefreshSnapshot() const
    {
#if !ULRE_ECS_DEBUG_API
        return false;
#else
        if (!world)
            return false;

        const uint32_t frame_index = world->GetFrameIndex();
        if (snapshot.valid && snapshot_frame_index == frame_index)
            return true;

        snapshot = DiagnosticsSnapshot{};
        snapshot_frame_index = ~0u;

        auto descriptor_system = world->GetSystem<RenderDescriptorBindingSystem>();
        if (!descriptor_system)
            return false;

        if (!descriptor_system->GetContractDiagnosticsStats(snapshot.materials_checked,
                                                            snapshot.materials_unresolved,
                                                            snapshot.required_missing,
                                                            snapshot.optional_missing,
                                                            snapshot.fallback_hits))
            return false;

        if (!descriptor_system->GetMaterialBindingRegistryStats(snapshot.materials_registered,
                                                                snapshot.binding_entries))
        {
            snapshot.materials_registered = 0;
            snapshot.binding_entries = 0;
        }

        std::map<std::string, uint32_t> category_histogram;
        if (world->GetShaderGenValidationCategoryHistogram(category_histogram, 128))
        {
            const auto count_of = [&category_histogram](const char *category) -> uint32_t
            {
                auto it = category_histogram.find(category);
                return it == category_histogram.end() ? 0u : it->second;
            };

            snapshot.strict_prebuild = count_of("StrictGate.Prebuild");
            snapshot.strict_spv = count_of("StrictGate.Spv");
            snapshot.strict_vertex = count_of("StrictGate.Vertex");
            snapshot.strict_descriptor = count_of("StrictGate.Descriptor");
            snapshot.strict_total = snapshot.strict_prebuild + snapshot.strict_spv + snapshot.strict_vertex + snapshot.strict_descriptor;

            if (snapshot.strict_total > 0)
            {
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
                            ++snapshot.strict_materials;
                    }
                }
            }
        }

        snapshot.valid = true;
    snapshot_frame_index = frame_index;
        return true;
#endif
    }

    bool RenderDiagnosticsService::GetDescriptorContractDiagnostics(uint32_t &materials_checked,
                                                                    uint32_t &materials_unresolved,
                                                                    uint32_t &required_missing,
                                                                    uint32_t &optional_missing,
                                                                    uint32_t &fallback_hits) const
    {
        DiagnosticsSnapshot out_snapshot;
        if (!GetDiagnosticsSnapshot(out_snapshot))
            return false;

        materials_checked = out_snapshot.materials_checked;
        materials_unresolved = out_snapshot.materials_unresolved;
        required_missing = out_snapshot.required_missing;
        optional_missing = out_snapshot.optional_missing;
        fallback_hits = out_snapshot.fallback_hits;
        return true;
    }

    bool RenderDiagnosticsService::GetDescriptorContractDiagnosticsExtended(uint32_t &materials_checked,
                                                                            uint32_t &materials_unresolved,
                                                                            uint32_t &required_missing,
                                                                            uint32_t &optional_missing,
                                                                            uint32_t &fallback_hits,
                                                                            uint32_t &materials_registered,
                                                                            uint32_t &binding_entries) const
    {
        DiagnosticsSnapshot out_snapshot;
        if (!GetDiagnosticsSnapshot(out_snapshot))
            return false;

        materials_checked = out_snapshot.materials_checked;
        materials_unresolved = out_snapshot.materials_unresolved;
        required_missing = out_snapshot.required_missing;
        optional_missing = out_snapshot.optional_missing;
        fallback_hits = out_snapshot.fallback_hits;
        materials_registered = out_snapshot.materials_registered;
        binding_entries = out_snapshot.binding_entries;
        return true;
    }

    bool RenderDiagnosticsService::GetMaterialBindingRegistryStats(uint32_t &materials_registered,
                                                                   uint32_t &binding_entries) const
    {
        DiagnosticsSnapshot out_snapshot;
        if (!GetDiagnosticsSnapshot(out_snapshot))
            return false;

        materials_registered = out_snapshot.materials_registered;
        binding_entries = out_snapshot.binding_entries;
        return true;
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

    bool RenderDiagnosticsService::GetDiagnosticsSnapshot(DiagnosticsSnapshot &out_snapshot) const
    {
        if (!RefreshSnapshot())
            return false;

        out_snapshot = snapshot;
        return out_snapshot.valid;
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

        DiagnosticsSnapshot diag_snapshot;
        if (GetDiagnosticsSnapshot(diag_snapshot))
        {
            LogInfo("[DescriptorContract][ECSContext] checked=%u unresolved=%u required_missing=%u optional_missing=%u fallback_hits=%u registered_materials=%u registered_bindings=%u",
                    diag_snapshot.materials_checked,
                    diag_snapshot.materials_unresolved,
                    diag_snapshot.required_missing,
                    diag_snapshot.optional_missing,
                    diag_snapshot.fallback_hits,
                    diag_snapshot.materials_registered,
                    diag_snapshot.binding_entries);

            if (diag_snapshot.strict_total > 0)
            {
                LogInfo("[ShaderGenValidation][ECSContext] strict_total=%u prebuild=%u spv=%u vertex=%u descriptor=%u strict_materials=%u",
                        diag_snapshot.strict_total,
                        diag_snapshot.strict_prebuild,
                        diag_snapshot.strict_spv,
                        diag_snapshot.strict_vertex,
                        diag_snapshot.strict_descriptor,
                        diag_snapshot.strict_materials);
            }
        }

        last_emit_ms = now_ms;
#endif
    }
}
