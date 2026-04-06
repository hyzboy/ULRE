#include<hgl/vk/pipeline/VKGraphicsPipelineBuildRequest.h>
#include<hgl/vk/pipeline/VKGraphicsRenderState.h>
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/type/FNV1a.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include<cstring>
#include<cstddef>

namespace hgl::graph{
namespace
{
    inline void HashU32(uint64_t &h, uint32_t v)
    {
        h = hgl::hash::FNV1aAppendValueBytes(h, v);
    }

    inline void HashU64(uint64_t &h, uint64_t v)
    {
        h = hgl::hash::FNV1aAppendValueBytes(h, v);
    }

    inline void HashBool(uint64_t &h, bool v)
    {
        const uint8_t b = v ? 1 : 0;
        h = hgl::hash::FNV1aAppendValueBytes(h, b);
    }

    inline void HashBytes(uint64_t &h, const void *ptr, size_t len)
    {
        h = hgl::hash::FNV1aAppendBytes(h, ptr, len);
    }

    inline void HashAnsiString(uint64_t &h, const AnsiString &s)
    {
        const char *p = s.c_str();
        const size_t len = p ? std::strlen(p) : 0;
        HashU64(h, static_cast<uint64_t>(len));
        if (len > 0)
            HashBytes(h, p, len);
    }

    void HashSpecializationInfo(uint64_t &h, const VkSpecializationInfo *spec)
    {
        if (!spec)
        {
            HashU32(h, 0u);
            HashU32(h, 0u);
            return;
        }

        HashU32(h, spec->mapEntryCount);
        if (spec->mapEntryCount > 0 && spec->pMapEntries)
            HashBytes(h, spec->pMapEntries, sizeof(VkSpecializationMapEntry) * spec->mapEntryCount);

        HashU64(h, static_cast<uint64_t>(spec->dataSize));
        if (spec->dataSize > 0 && spec->pData)
            HashBytes(h, spec->pData, spec->dataSize);
    }

    void HashShaderStages(uint64_t &h,
                          const ShaderStageCreateInfoList &stages,
                          const bool include_fragment,
                          const bool include_non_fragment)
    {
        uint32_t filtered_count = 0;

        const VkPipelineShaderStageCreateInfo *list = stages.GetData();
        const uint32_t count = stages.GetCount();

        for (uint32_t i = 0; i < count; ++i)
        {
            const auto &s = list[i];
            const bool is_fragment = (s.stage == VK_SHADER_STAGE_FRAGMENT_BIT);

            if ((is_fragment && !include_fragment) || (!is_fragment && !include_non_fragment))
                continue;

            ++filtered_count;
            HashU32(h, static_cast<uint32_t>(s.stage));

            const char *entry = s.pName;
            const size_t entry_len = entry ? std::strlen(entry) : 0;
            HashU64(h, static_cast<uint64_t>(entry_len));
            if (entry_len > 0)
                HashBytes(h, entry, entry_len);

            // Shader module handle is part of stage identity in current runtime.
            HashU64(h, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(s.module)));

            HashSpecializationInfo(h, s.pSpecializationInfo);
        }

        HashU32(h, filtered_count);
    }
}

bool IsValidGraphicsPipelineBuildRequest(const GraphicsPipelineBuildRequest &req)
{
    return req.material
        && req.vil
        && req.render_format
        && req.pipeline_data;
}

GplVertexInputKey BuildVertexInputKey(const VertexInputLayout *vil)
{
    if (!vil)
        return {};

    uint64_t h = hgl::hash::FNV1aInit<uint64_t>();

    const uint32_t count = vil->GetVertexAttribCount();
    HashU32(h, count);

    const VertexInputFormat *vif = vil->GetVIFList();
    for (uint32_t i = 0; i < count; ++i)
    {
        HashU32(h, static_cast<uint32_t>(vif[i].format));
        HashU32(h, static_cast<uint32_t>(vif[i].vec_size));
        HashU32(h, static_cast<uint32_t>(vif[i].stride));
        HashU32(h, static_cast<uint32_t>(vif[i].attrib));
        HashU32(h, static_cast<uint32_t>(vif[i].binding));
        HashU32(h, static_cast<uint32_t>(vif[i].input_rate));
    }

    return { h };
}

GplPreRasterKey BuildPreRasterKey(const GraphicsPipelineBuildRequest &req)
{
    if (!req.material)
        return {};

    uint64_t h = hgl::hash::FNV1aInit<uint64_t>();

    HashAnsiString(h, req.material->GetName());
    HashU32(h, static_cast<uint32_t>(req.primitive));
    HashBool(h, req.primitive_restart);

    const auto &stages = req.material->GetStageList();
    HashShaderStages(h, stages, false, true);

    return { h };
}

GplFragmentShaderKey BuildFragmentShaderKey(const GraphicsPipelineBuildRequest &req)
{
    if (!req.material)
        return {};

    uint64_t h = hgl::hash::FNV1aInit<uint64_t>();

    HashAnsiString(h, req.material->GetName());

    const auto &stages = req.material->GetStageList();
    HashShaderStages(h, stages, true, false);

    return { h };
}

GplFragmentOutputKey BuildFragmentOutputKey(const RenderTargetFormat *rf)
{
    if (!rf)
        return {};

    uint64_t h = hgl::hash::FNV1aInit<uint64_t>();

    HashU32(h, rf->GetColorCount());
    for (uint32_t i = 0; i < rf->GetColorCount(); ++i)
        HashU32(h, static_cast<uint32_t>(rf->GetColorFormat(static_cast<int>(i))));

    HashU32(h, static_cast<uint32_t>(rf->GetDepthFormat()));

    return { h };
}

GplLinkedPipelineKey BuildLinkedPipelineKey(const GraphicsPipelineBuildRequest &req,
                                            const GraphicsRenderState &state_profile)
{
    GplLinkedPipelineKey key{};

    key.vi = BuildVertexInputKey(req.vil);
    key.pr = BuildPreRasterKey(req);
    key.fs = BuildFragmentShaderKey(req);
    key.fo = BuildFragmentOutputKey(req.render_format);
    key.state_hash = state_profile.Hash();

    uint64_t layout_hash = hgl::hash::FNV1aInit<uint64_t>();
    if (req.material)
    {
        HashAnsiString(layout_hash, req.material->GetName());
        HashU32(layout_hash, static_cast<uint32_t>(req.material->GetPrimitiveType()));
        HashBool(layout_hash, req.material->hasLocalToWorld());
        HashBool(layout_hash, req.material->hasMI());
        HashU32(layout_hash, req.material->GetMIDataBytes());
        HashU32(layout_hash, req.material->GetMIMaxCount());
        HashU32(layout_hash, req.material->GetTextureArraySlotFlags());

        for (uint32_t i = 0; i < static_cast<uint32_t>(DESCRIPTOR_SET_TYPE_COUNT); ++i)
            HashBool(layout_hash, req.material->hasSet(static_cast<DescriptorSetType>(i)));

        const auto &binding_contract = req.material->GetBindingContract();
        for (size_t i = 0; i < size_t(mtl::UBODescriptorSemantic::RANGE_SIZE); ++i)
            HashU32(layout_hash, binding_contract.ubos[i]);

        for (size_t i = 0; i < size_t(mtl::SSBODescriptorSemantic::RANGE_SIZE); ++i)
            HashU32(layout_hash, binding_contract.ssbos[i]);

        const auto &stages = req.material->GetStageList();
        HashShaderStages(layout_hash, stages, true, true);

        HashU64(layout_hash,
                static_cast<uint64_t>(reinterpret_cast<uintptr_t>(req.material->GetPipelineLayout())));
    }

    key.layout_hash = layout_hash;
    return key;
}

}//namespace hgl::graph
