#include <hgl/graph/module/ShaderGenDescriptorPolicyAdapter.h>

namespace hgl::graph
{
    ContractDescriptorDecision BuildDescriptorsByContractPolicy(const std::vector<ShaderDescriptor> &legacy_descriptors,
                                                                const mtl::contract::ShaderGenResult *contract_result,
                                                                bool mirror_spv_build_used,
                                                                bool require_mirror_valid,
                                                                std::vector<ShaderDescriptor> &out_descriptors,
                                                                ContractDescriptorFallbackPhase &out_phase,
                                                                std::string &reason)
    {
        out_descriptors.clear();
        out_phase = ContractDescriptorFallbackPhase::None;
        reason.clear();

        if (!contract_result)
        {
            out_descriptors = legacy_descriptors;
            return ContractDescriptorDecision::UseLegacy;
        }

        if (!legacy_descriptors.empty())
        {
            if (!ValidateContractDescriptorLayoutAgainstLegacy(legacy_descriptors, *contract_result, reason))
            {
                out_phase = ContractDescriptorFallbackPhase::LayoutMismatch;

                if (require_mirror_valid || mirror_spv_build_used)
                    return ContractDescriptorDecision::StrictAbort;

                out_descriptors = legacy_descriptors;
                return ContractDescriptorDecision::UseLegacy;
            }
        }

        if (!BuildShaderDescriptorsFromContractLayout(*contract_result, legacy_descriptors, out_descriptors, reason))
        {
            out_phase = ContractDescriptorFallbackPhase::BuildFailed;

            if (require_mirror_valid || mirror_spv_build_used)
                return ContractDescriptorDecision::StrictAbort;

            out_descriptors = legacy_descriptors;
            return ContractDescriptorDecision::UseLegacy;
        }

        return ContractDescriptorDecision::UseMirror;
    }
}
