#pragma once

#include<hgl/type/ValueArray.h>
#include<hgl/vk/VK.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<cstdint>

namespace hgl::graph{
struct PipelineData;

struct RenderStateProfile
{
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkBool32 primitive_restart = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo raster{};
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    VkPipelineMultisampleStateCreateInfo multisample{};

    ValueArray<VkPipelineColorBlendAttachmentState> blend_attachments;
    VkPipelineColorBlendStateCreateInfo blend{};

    ValueArray<VkDynamicState> dynamic_states;

    uint64_t Hash() const;
    bool Equals(const RenderStateProfile &) const;
    bool operator==(const RenderStateProfile &rhs) const { return Equals(rhs); }

    static RenderStateProfile FromPipelineData(const PipelineData &, PrimitiveType, bool prim_restart);
};
}//namespace hgl::graph