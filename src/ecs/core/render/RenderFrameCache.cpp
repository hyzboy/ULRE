#include <hgl/ecs/core/render/RenderFrameCache.h>
#include <hgl/ecs/core/render/RenderItem.h>
#include <hgl/ecs/core/render/MaterialBatch.h>
#include <cstring>

namespace hgl::ecs
{
    RenderFrameCache::~RenderFrameCache() = default;

    void RenderFrameCache::RecordDrop(const char *name, uint32_t id, const char *stage, const char *reason, const char *detail)
    {
        if (drop_sample_count < kMaxSampleDrops)
        {
            DropSampleRecord &sample = drop_samples[drop_sample_count++];
            sample.entity_id = id;
            if (name) { strncpy(sample.entity_name, name, sizeof(sample.entity_name) - 1); sample.entity_name[sizeof(sample.entity_name) - 1] = 0; }
            if (stage) { strncpy(sample.stage, stage, sizeof(sample.stage) - 1); sample.stage[sizeof(sample.stage) - 1] = 0; }
            if (reason) { strncpy(sample.reason, reason, sizeof(sample.reason) - 1); sample.reason[sizeof(sample.reason) - 1] = 0; }
            if (detail) { strncpy(sample.detail, detail, sizeof(sample.detail) - 1); sample.detail[sizeof(sample.detail) - 1] = 0; }
        }
        else
        {
            drop_sample_truncated = true;
        }
    }

    RenderFrameCache::EntityTraceRecord *RenderFrameCache::GetOrCreateTrace(
        const char *name,const uint32_t id,const uint32_t generation)
    {
        for(uint32_t i=0;i<trace_sample_count;++i)
        {
            EntityTraceRecord &record=trace_samples[i];
            if(record.entity_id==id && record.generation==generation)
                return &record;
        }

        if(trace_sample_count>=kMaxTraceSamples)
        {
            trace_sample_truncated=true;
            return nullptr;
        }

        EntityTraceRecord &record=trace_samples[trace_sample_count++];
        record=EntityTraceRecord{};
        record.entity_id=id;
        record.generation=generation;
        if(name)
        {
            strncpy(record.entity_name,name,sizeof(record.entity_name)-1);
            record.entity_name[sizeof(record.entity_name)-1]=0;
        }
        return &record;
    }

    void RenderFrameCache::BeginFrame()
    {
        ++frame_serial;

        renderItems.clear();
        ordered_batches.clear();
        renderableCount = 0;
        cameraInfo = nullptr;

        has_pending_resources = false;
        is_collect_steady     = false;
        fast_path_engaged     = false;

        scene_primitives  = 0;
        passed_can_render = 0;
        collect_candidates = 0;
        collected_items   = 0;

        dropped_invisible         = 0;
        dropped_no_owner          = 0;
        dropped_no_owner          = 0;
        dropped_no_transform     = 0;
        dropped_mobility         = 0;
        dropped_other            = 0;
        drop_sample_count        = 0;
        drop_sample_truncated    = false;
        pending_material_resources = 0;
        pending_geometry_resources = 0;
        trace_sample_count       = 0;
        trace_sample_truncated   = false;

        // 清空批次内容，保留对象以供重用
        for (auto& pair : materialBatches)
        {
            if (pair.second)
                pair.second->Clear();
        }
    }
}

