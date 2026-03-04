#include <hgl/graph/module/MaterialBuildFlowAdapter.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/ShaderGenSPVModuleAdapter.h>
#include <hgl/graph/module/ShaderGenVertexPolicyAdapter.h>
#include <hgl/graph/module/ShaderGenDescriptorPolicyAdapter.h>
#include <hgl/graph/module/ShaderGenContractGateReporter.h>
#include <hgl/vk/VKVertexInput.h>
#include <hgl/vk/VKMaterialDescriptorManager.h>

namespace hgl::graph
{
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
            std::vector<const ShaderModule *> mirror_modules;
            std::string mirror_spv_fail_reason;

            const bool mirror_spv_build_ok = BuildShaderModulesFromContractSPV(
                *mirror_result,
                [&](VkShaderStageFlagBits stage, const uint32_t *spv_data, size_t spv_size) -> const ShaderModule *
                {
                    return manager->CreateShaderModuleFromSPV(mtl_name, stage, spv_data, spv_size);
                },
                mirror_modules,
                mirror_spv_fail_reason);

            if (mirror_spv_build_ok)
            {
                for (const auto *module : mirror_modules)
                    shader_maps->Add(module);

                mirror_spv_build_used = true;
            }
            else
            {
                if (require_mirror_valid)
                {
                    ReportMirrorPreferredStrictAbort(mtl_name.c_str(),
                                                     "StrictGate.Spv",
                                                     (std::string("mirror-preferred build aborted: ") + mirror_spv_fail_reason).c_str());
                    return false;
                }

                ReportMirrorSPVFallback(mtl_name.c_str(), mirror_spv_fail_reason.c_str());
            }
        }

        if (!mirror_spv_build_used)
        {
            std::vector<const ShaderModule *> legacy_modules;
            std::string legacy_spv_fail_reason;

            if (!BuildShaderModulesFromLegacySCIMap(
                    sci_map,
                    [&](ShaderCreateInfo *sci_ptr) -> const ShaderModule *
                    {
                        return manager->CreateShaderModule(mtl_name, sci_ptr);
                    },
                    legacy_modules,
                    legacy_spv_fail_reason))
            {
                return false;
            }

            for (const auto *module : legacy_modules)
                shader_maps->Add(module);
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
            ReportMirrorPreferredStrictAbort(mtl_name.c_str(),
                                             "StrictGate.Vertex",
                                             (std::string("mirror-preferred build aborted: ") + reason).c_str());
            return false;
        }

        if (vertex_decision == ContractVertexInputDecision::UseLegacy && !reason.empty())
            ReportMirrorVertexFallback(mtl_name.c_str(), reason.c_str());

        if (vertex_decision == ContractVertexInputDecision::UseMirror)
            out_vertex_input = GetVertexInput(mirror_input);

        if (!out_vertex_input && vert)
            out_vertex_input = GetVertexInput(vert->GetInput());

        const auto &mdi = mci->GetMDI();

        std::vector<ShaderDescriptor> legacy_descriptors;
        if (mdi.GetCount() > 0)
        {
            const auto &sds_array = mdi.Get();

            legacy_descriptors.reserve(mdi.GetCount());

            for (size_t i = 0; i < DESCRIPTOR_SET_TYPE_COUNT; i++)
            {
                std::vector<ShaderDescriptor *> values;
                sds_array[i].descriptor_map.GetValueArray(values);

                for (auto *sd : values)
                    if (sd)
                        legacy_descriptors.emplace_back(*sd);
            }
        }

        std::vector<ShaderDescriptor> descriptors;
        ContractDescriptorFallbackPhase descriptor_phase = ContractDescriptorFallbackPhase::None;
        std::string descriptor_reason;
        const ContractDescriptorDecision descriptor_decision = BuildDescriptorsByContractPolicy(
            legacy_descriptors,
            mirror_result,
            mirror_spv_build_used,
            require_mirror_valid,
            descriptors,
            descriptor_phase,
            descriptor_reason);

        if (descriptor_decision == ContractDescriptorDecision::StrictAbort)
        {
            ReportMirrorPreferredStrictAbort(mtl_name.c_str(),
                                             "StrictGate.Descriptor",
                                             (std::string("mirror-preferred build aborted: ") + descriptor_reason).c_str());
            return false;
        }

        if (descriptor_decision == ContractDescriptorDecision::UseLegacy && !descriptor_reason.empty())
        {
            const char *fallback_phase_text = (descriptor_phase == ContractDescriptorFallbackPhase::LayoutMismatch)
                                                ? "layout mismatch"
                                                : "layout build failed";

            ReportMirrorDescriptorFallback(mtl_name.c_str(), fallback_phase_text, descriptor_reason.c_str());
        }

        if (!descriptors.empty())
            out_desc_manager = new MaterialDescriptorManager(mtl_name, descriptors.data(), static_cast<uint>(descriptors.size()));

        return true;
    }
}
