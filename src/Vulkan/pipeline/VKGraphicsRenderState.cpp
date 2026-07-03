#include<hgl/vk/pipeline/VKGraphicsRenderState.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineData.h>
#include<hgl/util/hash/FNV1a.h>
#include<cstring>

namespace hgl::graph{
namespace
{
    inline void HashBytes(uint64_t &h, const void *ptr, size_t len)
    {
        h = hgl::hash::FNV1aAppendBytes(h, ptr, len);
    }

    inline void HashU32(uint64_t &h, uint32_t v)
    {
        h = hgl::hash::FNV1aAppendValueBytes(h, v);
    }

    inline void HashFloat(uint64_t &h, float v)
    {
        h = hgl::hash::FNV1aAppendValueBytes(h, v);
    }

    static bool EqualBlendState(const VkPipelineColorBlendStateCreateInfo &a,
                                const VkPipelineColorBlendStateCreateInfo &b)
    {
        if (a.logicOpEnable != b.logicOpEnable) return false;
        if (a.logicOp != b.logicOp) return false;
        if (a.attachmentCount != b.attachmentCount) return false;

        for (int i = 0; i < 4; ++i)
            if (a.blendConstants[i] != b.blendConstants[i])
                return false;

        return true;
    }
}

GraphicsRenderState GraphicsRenderState::FromGraphicsPipelineData(const GraphicsPipelineData &pd, PrimitiveType prim, bool prim_restart)
{
    GraphicsRenderState rsp;

    rsp.topology = pd.input_assembly.topology;
    rsp.primitive_restart = prim_restart ? VK_TRUE : pd.input_assembly.primitiveRestartEnable;

    if (pd.rasterization)
        rsp.raster = *pd.rasterization;

    if (pd.depth_stencil)
        rsp.depth_stencil = *pd.depth_stencil;

    if (pd.multi_sample)
        rsp.multisample = *pd.multi_sample;

    if (pd.color_blend)
    {
        rsp.blend = *pd.color_blend;

        const uint32_t count = pd.color_blend->attachmentCount;
        rsp.blend_attachments.resize(count);

        for (uint32_t i = 0; i < count; ++i)
            rsp.blend_attachments[i] = pd.color_blend_attachments[i];

        rsp.blend.pAttachments = rsp.blend_attachments.data();
    }

    rsp.dynamic_states.resize(pd.dynamic_state.dynamicStateCount);
    for (uint32_t i = 0; i < pd.dynamic_state.dynamicStateCount; ++i)
        rsp.dynamic_states[i] = pd.dynamic_state_enables[i];

    rsp.blend.pAttachments = rsp.blend_attachments.data();
    rsp.blend.attachmentCount = (uint32_t)rsp.blend_attachments.size();

    rsp.multisample.pSampleMask = nullptr;
    rsp.raster.pNext = nullptr;
    rsp.depth_stencil.pNext = nullptr;
    rsp.multisample.pNext = nullptr;
    rsp.blend.pNext = nullptr;

    if (rsp.topology == VK_PRIMITIVE_TOPOLOGY_MAX_ENUM)
    {
        GraphicsPipelineData tmp(1);
        if (tmp.SetPrim(prim, prim_restart))
        {
            rsp.topology = tmp.input_assembly.topology;
            rsp.primitive_restart = tmp.input_assembly.primitiveRestartEnable;
        }
    }

    return rsp;
}

uint64_t GraphicsRenderState::Hash() const
{
    uint64_t h = hgl::hash::FNV1aInit<uint64_t>();

    HashU32(h, static_cast<uint32_t>(topology));
    HashU32(h, static_cast<uint32_t>(primitive_restart));

    HashU32(h, static_cast<uint32_t>(raster.depthClampEnable));
    HashU32(h, static_cast<uint32_t>(raster.rasterizerDiscardEnable));
    HashU32(h, static_cast<uint32_t>(raster.polygonMode));
    HashU32(h, static_cast<uint32_t>(raster.cullMode));
    HashU32(h, static_cast<uint32_t>(raster.frontFace));
    HashU32(h, static_cast<uint32_t>(raster.depthBiasEnable));
    HashFloat(h, raster.depthBiasConstantFactor);
    HashFloat(h, raster.depthBiasClamp);
    HashFloat(h, raster.depthBiasSlopeFactor);
    HashFloat(h, raster.lineWidth);

    HashU32(h, static_cast<uint32_t>(depth_stencil.depthTestEnable));
    HashU32(h, static_cast<uint32_t>(depth_stencil.depthWriteEnable));
    HashU32(h, static_cast<uint32_t>(depth_stencil.depthCompareOp));
    HashU32(h, static_cast<uint32_t>(depth_stencil.depthBoundsTestEnable));
    HashU32(h, static_cast<uint32_t>(depth_stencil.stencilTestEnable));
    HashBytes(h, &depth_stencil.front, sizeof(depth_stencil.front));
    HashBytes(h, &depth_stencil.back, sizeof(depth_stencil.back));
    HashFloat(h, depth_stencil.minDepthBounds);
    HashFloat(h, depth_stencil.maxDepthBounds);

    HashU32(h, static_cast<uint32_t>(multisample.rasterizationSamples));
    HashU32(h, static_cast<uint32_t>(multisample.sampleShadingEnable));
    HashFloat(h, multisample.minSampleShading);
    HashU32(h, static_cast<uint32_t>(multisample.alphaToCoverageEnable));
    HashU32(h, static_cast<uint32_t>(multisample.alphaToOneEnable));

    HashU32(h, static_cast<uint32_t>(blend.logicOpEnable));
    HashU32(h, static_cast<uint32_t>(blend.logicOp));
    for (int i = 0; i < 4; ++i)
        HashFloat(h, blend.blendConstants[i]);

    const uint32_t blend_count = (uint32_t)blend_attachments.size();
    HashU32(h, blend_count);
    for (uint32_t i = 0; i < blend_count; ++i)
        HashBytes(h, &blend_attachments[i], sizeof(VkPipelineColorBlendAttachmentState));

    const uint32_t dynamic_count = (uint32_t)dynamic_states.size();
    HashU32(h, dynamic_count);
    for (uint32_t i = 0; i < dynamic_count; ++i)
        HashU32(h, static_cast<uint32_t>(dynamic_states[i]));

    return h;
}

bool GraphicsRenderState::Equals(const GraphicsRenderState &rhs) const
{
    if (topology != rhs.topology) return false;
    if (primitive_restart != rhs.primitive_restart) return false;

    if (raster.depthClampEnable != rhs.raster.depthClampEnable) return false;
    if (raster.rasterizerDiscardEnable != rhs.raster.rasterizerDiscardEnable) return false;
    if (raster.polygonMode != rhs.raster.polygonMode) return false;
    if (raster.cullMode != rhs.raster.cullMode) return false;
    if (raster.frontFace != rhs.raster.frontFace) return false;
    if (raster.depthBiasEnable != rhs.raster.depthBiasEnable) return false;
    if (raster.depthBiasConstantFactor != rhs.raster.depthBiasConstantFactor) return false;
    if (raster.depthBiasClamp != rhs.raster.depthBiasClamp) return false;
    if (raster.depthBiasSlopeFactor != rhs.raster.depthBiasSlopeFactor) return false;
    if (raster.lineWidth != rhs.raster.lineWidth) return false;

    if (depth_stencil.depthTestEnable != rhs.depth_stencil.depthTestEnable) return false;
    if (depth_stencil.depthWriteEnable != rhs.depth_stencil.depthWriteEnable) return false;
    if (depth_stencil.depthCompareOp != rhs.depth_stencil.depthCompareOp) return false;
    if (depth_stencil.depthBoundsTestEnable != rhs.depth_stencil.depthBoundsTestEnable) return false;
    if (depth_stencil.stencilTestEnable != rhs.depth_stencil.stencilTestEnable) return false;
    if (depth_stencil.front.compareMask != rhs.depth_stencil.front.compareMask) return false;
    if (depth_stencil.front.writeMask != rhs.depth_stencil.front.writeMask) return false;
    if (depth_stencil.front.reference != rhs.depth_stencil.front.reference) return false;
    if (depth_stencil.front.compareOp != rhs.depth_stencil.front.compareOp) return false;
    if (depth_stencil.front.failOp != rhs.depth_stencil.front.failOp) return false;
    if (depth_stencil.front.passOp != rhs.depth_stencil.front.passOp) return false;
    if (depth_stencil.front.depthFailOp != rhs.depth_stencil.front.depthFailOp) return false;
    if (depth_stencil.back.compareMask != rhs.depth_stencil.back.compareMask) return false;
    if (depth_stencil.back.writeMask != rhs.depth_stencil.back.writeMask) return false;
    if (depth_stencil.back.reference != rhs.depth_stencil.back.reference) return false;
    if (depth_stencil.back.compareOp != rhs.depth_stencil.back.compareOp) return false;
    if (depth_stencil.back.failOp != rhs.depth_stencil.back.failOp) return false;
    if (depth_stencil.back.passOp != rhs.depth_stencil.back.passOp) return false;
    if (depth_stencil.back.depthFailOp != rhs.depth_stencil.back.depthFailOp) return false;
    if (depth_stencil.minDepthBounds != rhs.depth_stencil.minDepthBounds) return false;
    if (depth_stencil.maxDepthBounds != rhs.depth_stencil.maxDepthBounds) return false;

    if (multisample.rasterizationSamples != rhs.multisample.rasterizationSamples) return false;
    if (multisample.sampleShadingEnable != rhs.multisample.sampleShadingEnable) return false;
    if (multisample.minSampleShading != rhs.multisample.minSampleShading) return false;
    if (multisample.alphaToCoverageEnable != rhs.multisample.alphaToCoverageEnable) return false;
    if (multisample.alphaToOneEnable != rhs.multisample.alphaToOneEnable) return false;

    if (!EqualBlendState(blend, rhs.blend)) return false;

    if (blend_attachments.size() != rhs.blend_attachments.size()) return false;
    for (uint32_t i = 0; i < (uint32_t)blend_attachments.size(); ++i)
        if (memcmp(&blend_attachments[i], &rhs.blend_attachments[i], sizeof(VkPipelineColorBlendAttachmentState)) != 0)
            return false;

    if (dynamic_states.size() != rhs.dynamic_states.size()) return false;
    for (uint32_t i = 0; i < (uint32_t)dynamic_states.size(); ++i)
        if (dynamic_states[i] != rhs.dynamic_states[i])
            return false;

    return true;
}
}//namespace hgl::graph