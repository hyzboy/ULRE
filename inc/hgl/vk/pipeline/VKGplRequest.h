#pragma once

#include<hgl/type/String.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<cstdint>
#include<functional>

namespace hgl::graph{
class Material;
class RenderFormat;
class VertexInputLayout;
struct PipelineData;

struct VertexInputKey
{
    uint64_t hash = 0;
    bool operator==(const VertexInputKey &rhs) const { return hash == rhs.hash; }
};

struct PreRasterKey
{
    uint64_t hash = 0;
    bool operator==(const PreRasterKey &rhs) const { return hash == rhs.hash; }
};

struct FragmentShaderKey
{
    uint64_t hash = 0;
    bool operator==(const FragmentShaderKey &rhs) const { return hash == rhs.hash; }
};

struct FragmentOutputKey
{
    uint64_t hash = 0;
    bool operator==(const FragmentOutputKey &rhs) const { return hash == rhs.hash; }
};

struct LinkedPipelineKey
{
    VertexInputKey vi;
    PreRasterKey pre;
    FragmentShaderKey fs;
    FragmentOutputKey fo;
    uint64_t state_hash = 0;
    uint64_t layout_hash = 0;
    bool operator==(const LinkedPipelineKey &rhs) const
    {
        return vi == rhs.vi
            && pre == rhs.pre
            && fs == rhs.fs
            && fo == rhs.fo
            && state_hash == rhs.state_hash
            && layout_hash == rhs.layout_hash;
    }
};

struct GplPipelineRequest
{
    const Material *material = nullptr;
    const VertexInputLayout *vil = nullptr;
    const RenderFormat *render_format = nullptr;
    const PipelineData *pipeline_data = nullptr;
    PrimitiveType primitive = PrimitiveType::Triangles;
    bool primitive_restart = false;
    AnsiString debug_name;
};
}//namespace hgl::graph

namespace std
{
template<>
struct hash<hgl::graph::VertexInputKey>
{
    size_t operator()(const hgl::graph::VertexInputKey &key) const noexcept
    {
        return static_cast<size_t>(key.hash);
    }
};

template<>
struct hash<hgl::graph::PreRasterKey>
{
    size_t operator()(const hgl::graph::PreRasterKey &key) const noexcept
    {
        return static_cast<size_t>(key.hash);
    }
};

template<>
struct hash<hgl::graph::FragmentShaderKey>
{
    size_t operator()(const hgl::graph::FragmentShaderKey &key) const noexcept
    {
        return static_cast<size_t>(key.hash);
    }
};

template<>
struct hash<hgl::graph::FragmentOutputKey>
{
    size_t operator()(const hgl::graph::FragmentOutputKey &key) const noexcept
    {
        return static_cast<size_t>(key.hash);
    }
};

template<>
struct hash<hgl::graph::LinkedPipelineKey>
{
    size_t operator()(const hgl::graph::LinkedPipelineKey &key) const noexcept
    {
        size_t h = std::hash<hgl::graph::VertexInputKey>{}(key.vi);
        h ^= (std::hash<hgl::graph::PreRasterKey>{}(key.pre) << 1);
        h ^= (std::hash<hgl::graph::FragmentShaderKey>{}(key.fs) << 2);
        h ^= (std::hash<hgl::graph::FragmentOutputKey>{}(key.fo) << 3);
        h ^= (std::hash<uint64_t>{}(key.state_hash) << 4);
        h ^= (std::hash<uint64_t>{}(key.layout_hash) << 5);
        return h;
    }
};
}
