#include <hgl/vk/pipeline/VKGplLibraryStatsTracker.h>

namespace hgl::graph
{
template<typename TKey>
void GplLibraryStatsTracker::TouchBucket(Bucket<TKey> &bucket, const TKey &key)
{
    auto it = bucket.entries.find(key);
    if (it != bucket.entries.end())
    {
        ++bucket.hits;
        return;
    }

    ++bucket.misses;
    bucket.entries.emplace(key, next_virtual_id++);
    ++bucket.inserts;
}

void GplLibraryStatsTracker::Touch(const GplLinkedPipelineKey &key)
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    TouchBucket(vertex_input_bucket, key.vi);
    TouchBucket(pre_raster_bucket, key.pr);
    TouchBucket(fragment_shader_bucket, key.fs);
    TouchBucket(fragment_output_bucket, key.fo);
}

GplLibraryStatsTracker::Snapshot GplLibraryStatsTracker::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    Snapshot s;

    s.vertex_input_size = vertex_input_bucket.entries.size();
    s.pre_raster_size = pre_raster_bucket.entries.size();
    s.fragment_shader_size = fragment_shader_bucket.entries.size();
    s.fragment_output_size = fragment_output_bucket.entries.size();

    s.vertex_input_hits = vertex_input_bucket.hits;
    s.pre_raster_hits = pre_raster_bucket.hits;
    s.fragment_shader_hits = fragment_shader_bucket.hits;
    s.fragment_output_hits = fragment_output_bucket.hits;

    s.vertex_input_misses = vertex_input_bucket.misses;
    s.pre_raster_misses = pre_raster_bucket.misses;
    s.fragment_shader_misses = fragment_shader_bucket.misses;
    s.fragment_output_misses = fragment_output_bucket.misses;

    s.vertex_input_inserts = vertex_input_bucket.inserts;
    s.pre_raster_inserts = pre_raster_bucket.inserts;
    s.fragment_shader_inserts = fragment_shader_bucket.inserts;
    s.fragment_output_inserts = fragment_output_bucket.inserts;

    return s;
}

void GplLibraryStatsTracker::Reset()
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    next_virtual_id = 1;

    vertex_input_bucket.entries.clear();
    pre_raster_bucket.entries.clear();
    fragment_shader_bucket.entries.clear();
    fragment_output_bucket.entries.clear();

    vertex_input_bucket.hits = 0;
    pre_raster_bucket.hits = 0;
    fragment_shader_bucket.hits = 0;
    fragment_output_bucket.hits = 0;

    vertex_input_bucket.misses = 0;
    pre_raster_bucket.misses = 0;
    fragment_shader_bucket.misses = 0;
    fragment_output_bucket.misses = 0;

    vertex_input_bucket.inserts = 0;
    pre_raster_bucket.inserts = 0;
    fragment_shader_bucket.inserts = 0;
    fragment_output_bucket.inserts = 0;
}
}
