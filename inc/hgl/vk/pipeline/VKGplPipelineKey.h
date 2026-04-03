#pragma once

#include<cstdint>
#include<functional>

namespace hgl::graph{

/**
 * GPL 四段管线库的缓存键类型。
 *
 * 每个 Key 封装对应阶段所有影响编译的状态的 64 位哈希值。
 * 仅供 GPL 内部缓存层（GplLibraryHandleCache / GplLibraryStatsTracker）使用，
 * 不暴露给上层公共 API。
 */

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
    GplVertexInputKey    vi;
    GplPreRasterKey      pr;
    GplFragmentShaderKey fs;
    GplFragmentOutputKey fo;
    uint64_t state_hash  = 0;
    uint64_t layout_hash = 0;

    bool operator==(const GplLinkedPipelineKey &rhs) const
    {
        return vi          == rhs.vi
            && pr          == rhs.pr
            && fs          == rhs.fs
            && fo          == rhs.fo
            && state_hash  == rhs.state_hash
            && layout_hash == rhs.layout_hash;
    }
};

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
        h ^= (std::hash<hgl::graph::GplPreRasterKey>{}(key.pr)          << 1);
        h ^= (std::hash<hgl::graph::GplFragmentShaderKey>{}(key.fs)     << 2);
        h ^= (std::hash<hgl::graph::GplFragmentOutputKey>{}(key.fo)     << 3);
        h ^= (std::hash<uint64_t>{}(key.state_hash)                     << 4);
        h ^= (std::hash<uint64_t>{}(key.layout_hash)                    << 5);
        return h;
    }
};
}//namespace std
