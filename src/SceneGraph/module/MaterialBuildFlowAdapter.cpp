#include <hgl/graph/module/MaterialBuildFlowAdapter.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/ShaderGenSPVModuleAdapter.h>
#include <hgl/graph/module/ShaderGenVertexPolicyAdapter.h>
#include <hgl/graph/module/ShaderGenDescriptorPolicyAdapter.h>
#include <hgl/graph/module/ShaderGenContractGateReporter.h>
#include <hgl/graph/module/ShaderGenValidationStorageService.h>
#include <hgl/vk/VKVertexInput.h>
#include <hgl/vk/VKMaterialDescriptorManager.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>

namespace hgl::graph
{
    namespace
    {
        bool ReportStrictAbortDecision(const AnsiString &material_name,
                                       const char *decision_key,
                                       const char *strict_gate_category,
                                       const char *reason)
        {
            RecordShaderGenContractPathDecision(decision_key);
            ReportMirrorPreferredStrictAbort(material_name.c_str(),
                                             strict_gate_category,
                                             reason);
            return false;
        }

        void RecordBinaryDecision(const bool use_legacy,
                                  const char *legacy_decision_key,
                                  const char *mirror_decision_key)
        {
            RecordShaderGenContractPathDecision(use_legacy ? legacy_decision_key : mirror_decision_key);
        }

        void AppendShaderModules(ShaderModuleMap *shader_maps,
                                 const std::vector<const ShaderModule *> &modules)
        {
            if (!shader_maps)
                return;

            for (const auto *module : modules)
                shader_maps->Add(module);
        }

        std::vector<ShaderDescriptor> CollectLegacyDescriptors(const mtl::MaterialCreateInfo *mci)
        {
            std::vector<ShaderDescriptor> legacy_descriptors;
            if (!mci)
                return legacy_descriptors;

            const auto &mdi = mci->GetMDI();
            if (mdi.GetCount() == 0)
                return legacy_descriptors;

            const auto &sds_array = mdi.Get();
            legacy_descriptors.reserve(mdi.GetCount());

            for (size_t i = 0; i < DESCRIPTOR_SET_TYPE_COUNT; i++)
            {
                std::vector<ShaderDescriptor *> values;
                sds_array[i].descriptor_map.GetValueArray(values);

                for (auto *sd : values)
                {
                    if (sd)
                        legacy_descriptors.emplace_back(*sd);
                }
            }

            return legacy_descriptors;
        }

        bool ResolveVertexInputByContractPolicy(const AnsiString &material_name,
                                                ShaderCreateInfoVertex *vert,
                                                const mtl::contract::ShaderGenResult *mirror_result,
                                                const bool mirror_spv_build_used,
                                                const bool require_mirror_valid,
                                                VertexInput *&out_vertex_input)
        {
            VIAArray mirror_input;
            std::string reason;
            const ContractVertexInputDecision vertex_decision = BuildVertexInputByContractPolicy(
                vert,
                mirror_result,
                mirror_spv_build_used,
                require_mirror_valid,
                mirror_input,
                reason);

            if (vertex_decision == ContractVertexInputDecision::StrictAbort)
            {
                return ReportStrictAbortDecision(material_name,
                                                 kShaderGenPathDecisionVertexStrictAbort,
                                                 kShaderGenStrictGateVertexCategory,
                                                 reason.c_str());
            }

            if (vertex_decision == ContractVertexInputDecision::UseLegacy && !reason.empty())
                ReportMirrorVertexFallback(material_name.c_str(), reason.c_str());

            RecordBinaryDecision(vertex_decision == ContractVertexInputDecision::UseLegacy,
                                 kShaderGenPathDecisionVertexUseLegacy,
                                 kShaderGenPathDecisionVertexUseMirror);

            if (vertex_decision == ContractVertexInputDecision::UseMirror)
                out_vertex_input = GetVertexInput(mirror_input);

            if (!out_vertex_input && vert)
                out_vertex_input = GetVertexInput(vert->GetInput());

            return true;
        }

        bool ResolveDescriptorsByContractPolicy(const AnsiString &material_name,
                                                const std::vector<ShaderDescriptor> &legacy_descriptors,
                                                const mtl::contract::ShaderGenResult *mirror_result,
                                                const bool mirror_spv_build_used,
                                                const bool require_mirror_valid,
                                                std::vector<ShaderDescriptor> &out_descriptors)
        {
            ContractDescriptorFallbackPhase descriptor_phase = ContractDescriptorFallbackPhase::None;
            std::string descriptor_reason;
            const ContractDescriptorDecision descriptor_decision = BuildDescriptorsByContractPolicy(
                legacy_descriptors,
                mirror_result,
                mirror_spv_build_used,
                require_mirror_valid,
                out_descriptors,
                descriptor_phase,
                descriptor_reason);

            if (descriptor_decision == ContractDescriptorDecision::StrictAbort)
            {
                return ReportStrictAbortDecision(material_name,
                                                 kShaderGenPathDecisionDescriptorStrictAbort,
                                                 kShaderGenStrictGateDescriptorCategory,
                                                 descriptor_reason.c_str());
            }

            if (descriptor_decision == ContractDescriptorDecision::UseLegacy && !descriptor_reason.empty())
            {
                const char *fallback_phase_text = (descriptor_phase == ContractDescriptorFallbackPhase::LayoutMismatch)
                                                    ? kShaderGenDescriptorFallbackPhaseLayoutMismatch
                                                    : kShaderGenDescriptorFallbackPhaseBuildFailed;

                ReportMirrorDescriptorFallback(material_name.c_str(), fallback_phase_text, descriptor_reason.c_str());
            }

            RecordBinaryDecision(descriptor_decision == ContractDescriptorDecision::UseLegacy,
                                 kShaderGenPathDecisionDescriptorUseLegacy,
                                 kShaderGenPathDecisionDescriptorUseMirror);

            return true;
        }

        bool TryBuildMirrorShaderModules(const AnsiString &material_name,
                                         MaterialManager *manager,
                                         const mtl::contract::ShaderGenResult &mirror_result,
                                         const bool require_mirror_valid,
                                         ShaderModuleMap *shader_maps,
                                         bool &mirror_spv_build_used)
        {
            std::vector<const ShaderModule *> mirror_modules;
            std::string mirror_spv_fail_reason;

            const bool mirror_spv_build_ok = BuildShaderModulesFromContractSPV(
                mirror_result,
                [&](VkShaderStageFlagBits stage, const uint32_t *spv_data, size_t spv_size) -> const ShaderModule *
                {
                    return manager->CreateShaderModuleFromSPV(material_name, stage, spv_data, spv_size);
                },
                mirror_modules,
                mirror_spv_fail_reason);

            if (mirror_spv_build_ok)
            {
                AppendShaderModules(shader_maps, mirror_modules);

                mirror_spv_build_used = true;
                RecordShaderGenContractPathDecision(kShaderGenPathDecisionSpvUseMirror);
                return true;
            }

            if (require_mirror_valid)
            {
                return ReportStrictAbortDecision(material_name,
                                                 kShaderGenPathDecisionSpvStrictAbort,
                                                 kShaderGenStrictGateSpvCategory,
                                                 mirror_spv_fail_reason.c_str());
            }

            RecordShaderGenContractPathDecision(kShaderGenPathDecisionSpvUseLegacyFallback);
            ReportMirrorSPVFallback(material_name.c_str(), mirror_spv_fail_reason.c_str());
            return true;
        }

        bool BuildLegacyShaderModules(const AnsiString &material_name,
                                      MaterialManager *manager,
                                      const ShaderCreateInfoMap &sci_map,
                                      ShaderModuleMap *shader_maps)
        {
            std::vector<const ShaderModule *> legacy_modules;
            std::string legacy_spv_fail_reason;

            if (!BuildShaderModulesFromLegacySCIMap(
                    sci_map,
                    [&](ShaderCreateInfo *sci_ptr) -> const ShaderModule *
                    {
                        return manager->CreateShaderModule(material_name, sci_ptr);
                    },
                    legacy_modules,
                    legacy_spv_fail_reason))
            {
                return false;
            }

            AppendShaderModules(shader_maps, legacy_modules);
            return true;
        }
    }//namespace

    bool BuildShaderModulesFlow(MaterialManager *manager,
                                const AnsiString &mtl_name,
                                const ShaderCreateInfoMap &sci_map,
                                const mtl::contract::ShaderGenResult *mirror_result,
                                const bool require_mirror_valid,
                                ShaderModuleMap *shader_maps,
                                bool &mirror_spv_build_used)
    {
        mirror_spv_build_used = false;
        const bool prefer_mirror_spv_build = mirror_result != nullptr;

        if (prefer_mirror_spv_build)
        {
            if (!TryBuildMirrorShaderModules(mtl_name,
                                             manager,
                                             *mirror_result,
                                             require_mirror_valid,
                                             shader_maps,
                                             mirror_spv_build_used))
            {
                return false;
            }
        }

        if (!mirror_spv_build_used)
        {
            if (!BuildLegacyShaderModules(mtl_name,
                                          manager,
                                          sci_map,
                                          shader_maps))
            {
                return false;
            }

            if (!prefer_mirror_spv_build)
                RecordShaderGenContractPathDecision(kShaderGenPathDecisionSpvUseLegacyDirect);
        }

        return true;
    }

    bool BuildMaterialBindingsFlow(const AnsiString &mtl_name,
                                   const mtl::MaterialCreateInfo *mci,
                                   const mtl::contract::ShaderGenResult *mirror_result,
                                   const bool mirror_spv_build_used,
                                   const bool require_mirror_valid,
                                   VertexInput *&out_vertex_input,
                                   MaterialDescriptorManager *&out_desc_manager)
    {
        out_vertex_input = nullptr;
        out_desc_manager = nullptr;

        ShaderCreateInfoVertex *vert = mci->GetVS();
        if (!ResolveVertexInputByContractPolicy(mtl_name,
                                                vert,
                                                mirror_result,
                                                mirror_spv_build_used,
                                                require_mirror_valid,
                                                out_vertex_input))
        {
            return false;
        }

        std::vector<ShaderDescriptor> legacy_descriptors = CollectLegacyDescriptors(mci);

        std::vector<ShaderDescriptor> descriptors;
        if (!ResolveDescriptorsByContractPolicy(mtl_name,
                                                legacy_descriptors,
                                                mirror_result,
                                                mirror_spv_build_used,
                                                require_mirror_valid,
                                                descriptors))
        {
            return false;
        }

        if (!descriptors.empty())
            out_desc_manager = new MaterialDescriptorManager(mtl_name, descriptors.data(), static_cast<uint>(descriptors.size()));

        return true;
    }
}
