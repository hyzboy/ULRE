#pragma once

#include <hgl/vk/pipeline/VKGplRequest.h>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace hgl::graph
{
class PipelineLibraryCache
{
public:
    struct Snapshot
    {
        size_t vertex_input_size = 0;
        size_t pre_raster_size = 0;
        size_t fragment_shader_size = 0;
        size_t fragment_output_size = 0;

        uint64_t vertex_input_hits = 0;
        uint64_t pre_raster_hits = 0;
        uint64_t fragment_shader_hits = 0;
        uint64_t fragment_output_hits = 0;

        uint64_t vertex_input_misses = 0;
        uint64_t pre_raster_misses = 0;
        uint64_t fragment_shader_misses = 0;
        uint64_t fragment_output_misses = 0;

        uint64_t vertex_input_inserts = 0;
        uint64_t pre_raster_inserts = 0;
        uint64_t fragment_shader_inserts = 0;
        uint64_t fragment_output_inserts = 0;
    };

private:
    template<typename TKey>
    struct Bucket
    {
        std::unordered_map<TKey, uint64_t> entries;
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t inserts = 0;
    };

    mutable std::mutex cache_mutex;
    uint64_t next_virtual_id = 1;

    Bucket<GplVertexInputKey>   vertex_input_bucket;
    Bucket<GplPreRasterKey>     pre_raster_bucket;
    Bucket<GplFragmentShaderKey> fragment_shader_bucket;
    Bucket<GplFragmentOutputKey> fragment_output_bucket;

private:
    template<typename TKey>
    void TouchBucket(Bucket<TKey> &bucket, const TKey &key);

public:
    void Touch(const GplLinkedPipelineKey &key);
    Snapshot GetSnapshot() const;
    void Reset();
};
}