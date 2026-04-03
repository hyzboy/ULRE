#pragma once

#include<hgl/type/String.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<cstdint>
#include<functional>

namespace hgl::graph{
class Material;
class RenderFormat;
class VertexInputLayout;
struct GraphicsPipelineData;
struct RenderStateProfile;

struct GplVertexInputKey
{
    uint64_t hash = 0;
    bool operator==(const GplVertexInputKey &rhs) const { return hash == rhs.hash; }
};

struct GplPreRasterKey
{
    uint64_t hash = 0;
    bool operator==(const GplPreRasterKey &rhs) const { return hash == rhs.hash; }
};

struct GplFragmentShaderKey
{
    uint64_t hash = 0;
    bool operator==(const GplFragmentShaderKey &rhs) const { return hash == rhs.hash; }
};

struct GplFragmentOutputKey
{
    uint64_t hash = 0;
    bool operator==(const GplFragmentOutputKey &rhs) const { return hash == rhs.hash; }
};

struct GplLinkedPipelineKey
{
    GplVertexInputKey  vi;
    GplPreRasterKey    pr;
    GplFragmentShaderKey fs;
    GplFragmentOutputKey fo;
    uint64_t state_hash = 0;
    uint64_t layout_hash = 0;
    bool operator==(const GplLinkedPipelineKey &rhs) const
    {
        return vi == rhs.vi
            && pr == rhs.pr
            && fs == rhs.fs
            && fo == rhs.fo
            && state_hash == rhs.state_hash
            && layout_hash == rhs.layout_hash;
    }
};

struct GraphicsPipelineBuildRequest
{
    const Material *material = nullptr;
    const VertexInputLayout *vil = nullptr;
    const RenderFormat *render_format = nullptr;
    const GraphicsPipelineData *pipeline_data = nullptr;
    PrimitiveType primitive = PrimitiveType::Triangles;
    bool primitive_restart = false;
    AnsiString debug_name;
};

bool IsValidGraphicsPipelineBuildRequest(const GraphicsPipelineBuildRequest &req);

GplVertexInputKey  BuildVertexInputKey(const VertexInputLayout *vil);
GplPreRasterKey    BuildPreRasterKey(const GraphicsPipelineBuildRequest &req);
GplFragmentShaderKey BuildFragmentShaderKey(const GraphicsPipelineBuildRequest &req);
GplFragmentOutputKey BuildFragmentOutputKey(const RenderFormat *rf);
GplLinkedPipelineKey BuildLinkedPipelineKey(const GraphicsPipelineBuildRequest &req,
                                            const RenderStateProfile &state_profile);
}//namespace hgl::graph

namespace std
{
template<>
struct hash<hgl::graph::GplVertexInputKey>
{
    size_t operator()(const hgl::graph::GplVertexInputKey &key) const noexcept
    {
        return static_cast<size_t>(key.hash);
    }
};

template<>
struct hash<hgl::graph::GplPreRasterKey>
{
    size_t operator()(const hgl::graph::GplPreRasterKey &key) const noexcept
    {
        return static_cast<size_t>(key.hash);
    }
};

template<>
struct hash<hgl::graph::GplFragmentShaderKey>
{
    size_t operator()(const hgl::graph::GplFragmentShaderKey &key) const noexcept
    {
        return static_cast<size_t>(key.hash);
    }
};

template<>
struct hash<hgl::graph::GplFragmentOutputKey>
{
    size_t operator()(const hgl::graph::GplFragmentOutputKey &key) const noexcept
    {
        return static_cast<size_t>(key.hash);
    }
};

template<>
struct hash<hgl::graph::GplLinkedPipelineKey>
{
    size_t operator()(const hgl::graph::GplLinkedPipelineKey &key) const noexcept
    {
        size_t h = std::hash<hgl::graph::GplVertexInputKey>{}(key.vi);
        h ^= (std::hash<hgl::graph::GplPreRasterKey>{}(key.pr) << 1);
        h ^= (std::hash<hgl::graph::GplFragmentShaderKey>{}(key.fs) << 2);
        h ^= (std::hash<hgl::graph::GplFragmentOutputKey>{}(key.fo) << 3);
        h ^= (std::hash<uint64_t>{}(key.state_hash) << 4);
        h ^= (std::hash<uint64_t>{}(key.layout_hash) << 5);
        return h;
    }
};
}
