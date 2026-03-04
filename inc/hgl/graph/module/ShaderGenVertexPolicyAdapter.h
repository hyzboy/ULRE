#pragma once

#include <hgl/graph/module/ShaderGenVertexInputAdapter.h>

namespace hgl::graph
{
    enum class ContractVertexInputDecision
    {
        UseLegacy,
        UseMirror,
        StrictAbort
    };

    ContractVertexInputDecision BuildVertexInputByContractPolicy(ShaderCreateInfoVertex *legacy_vertex,
                                                                 const mtl::contract::ShaderGenResult *contract_result,
                                                                 bool mirror_spv_build_used,
                                                                 bool require_mirror_valid,
                                                                 VIAArray &out_mirror_input,
                                                                 std::string &reason);
}
