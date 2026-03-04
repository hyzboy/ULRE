#include <hgl/graph/module/ShaderGenVertexPolicyAdapter.h>

namespace hgl::graph
{
    ContractVertexInputDecision BuildVertexInputByContractPolicy(ShaderCreateInfoVertex *legacy_vertex,
                                                                 const mtl::contract::ShaderGenResult *contract_result,
                                                                 bool mirror_spv_build_used,
                                                                 bool require_mirror_valid,
                                                                 VIAArray &out_mirror_input,
                                                                 std::string &reason)
    {
        reason.clear();
        out_mirror_input.Clear();

        if (!contract_result)
            return ContractVertexInputDecision::UseLegacy;

        if (legacy_vertex && !mirror_spv_build_used)
        {
            if (!ValidateContractVertexLayoutAgainstLegacy(legacy_vertex, contract_result->vertex_layout, reason))
            {
                if (require_mirror_valid)
                    return ContractVertexInputDecision::StrictAbort;

                return ContractVertexInputDecision::UseLegacy;
            }
        }

        if (mirror_spv_build_used)
        {
            if (!ValidateContractVertexLayoutAgainstLegacy(legacy_vertex, contract_result->vertex_layout, reason))
                return ContractVertexInputDecision::StrictAbort;
        }

        if (!BuildVertexInputFromContractLayout(contract_result->vertex_layout, out_mirror_input, reason))
        {
            if (require_mirror_valid || mirror_spv_build_used)
                return ContractVertexInputDecision::StrictAbort;

            return ContractVertexInputDecision::UseLegacy;
        }

        return ContractVertexInputDecision::UseMirror;
    }
}
