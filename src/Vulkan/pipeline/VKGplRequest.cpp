#include<hgl/vk/pipeline/VKGplRequest.h>
#include<hgl/vk/pipeline/VKRenderStateProfile.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKRenderFormat.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/type/FNV1a.h>
#include<cstring>

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
}

bool IsValidGplPipelineRequest(const GplPipelineRequest &req)
{
    return req.material
        && req.vil
        && req.render_format
        && req.pipeline_data;
}

VertexInputKey BuildVertexInputKey(const VertexInputLayout *vil)
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

PreRasterKey BuildPreRasterKey(const GplPipelineRequest &req)
{
    if (!req.material)
        return {};

    uint64_t h = hgl::hash::FNV1aInit<uint64_t>();

    HashAnsiString(h, req.material->GetName());
    HashU32(h, static_cast<uint32_t>(req.primitive));
    HashBool(h, req.primitive_restart);

    const auto &stages = req.material->GetStageList();
    HashU32(h, stages.GetCount());

    const VkPipelineShaderStageCreateInfo *list = stages.GetData();
    for (uint32_t i = 0; i < stages.GetCount(); ++i)
    {
        const auto &s = list[i];

        if (s.stage == VK_SHADER_STAGE_FRAGMENT_BIT)
            continue;

        HashU32(h, static_cast<uint32_t>(s.stage));

        const char *entry = s.pName;
        const size_t entry_len = entry ? std::strlen(entry) : 0;
        HashU64(h, static_cast<uint64_t>(entry_len));
        if (entry_len > 0)
            HashBytes(h, entry, entry_len);
    }

    return { h };
}

FragmentShaderKey BuildFragmentShaderKey(const GplPipelineRequest &req)
{
    if (!req.material)
        return {};

    uint64_t h = hgl::hash::FNV1aInit<uint64_t>();

    HashAnsiString(h, req.material->GetName());

    const auto &stages = req.material->GetStageList();
    HashU32(h, stages.GetCount());

    const VkPipelineShaderStageCreateInfo *list = stages.GetData();
    for (uint32_t i = 0; i < stages.GetCount(); ++i)
    {
        const auto &s = list[i];

        if (s.stage != VK_SHADER_STAGE_FRAGMENT_BIT)
            continue;

        HashU32(h, static_cast<uint32_t>(s.stage));

        const char *entry = s.pName;
        const size_t entry_len = entry ? std::strlen(entry) : 0;
        HashU64(h, static_cast<uint64_t>(entry_len));
        if (entry_len > 0)
            HashBytes(h, entry, entry_len);
    }

    return { h };
}

FragmentOutputKey BuildFragmentOutputKey(const RenderFormat *rf)
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

LinkedPipelineKey BuildLinkedPipelineKey(const GplPipelineRequest &req,
                                         const RenderStateProfile &state_profile)
{
    LinkedPipelineKey key{};

    key.vi = BuildVertexInputKey(req.vil);
    key.pre = BuildPreRasterKey(req);
    key.fs = BuildFragmentShaderKey(req);
    key.fo = BuildFragmentOutputKey(req.render_format);
    key.state_hash = state_profile.Hash();

    uint64_t layout_hash = hgl::hash::FNV1aInit<uint64_t>();
    if (req.material)
    {
        HashAnsiString(layout_hash, req.material->GetName());
        HashU32(layout_hash, static_cast<uint32_t>(req.material->GetPrimitiveType()));

        const auto &stages = req.material->GetStageList();
        HashU32(layout_hash, stages.GetCount());
    }

    key.layout_hash = layout_hash;
    return key;
}

}//namespace hgl::graph
