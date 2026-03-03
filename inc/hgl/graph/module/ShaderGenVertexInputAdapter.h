#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <hgl/vk/VKVertexInputAttribute.h>
#include <string>
#include <string_view>

namespace hgl::graph
{
    VertexInputGroup ResolveVertexInputGroupBySemantic(std::string_view semantic);

    bool BuildVertexInputFromContractLayout(const mtl::contract::VertexInputLayout &layout,
                                            VIAArray &out_input,
                                            std::string &reason);
}
