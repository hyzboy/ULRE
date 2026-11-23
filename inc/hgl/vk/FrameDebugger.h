#pragma once

#include<hgl/type/String.h>
#include<hgl/type/ValueArray.h>
#include<hgl/vk/VK.h>

namespace hgl
{
namespace graph
{
class IRenderTarget;
class DeviceBuffer;
}
}

namespace hgl::graph
{
struct CameraInfo;

class FrameDebugger
{
public:
    static constexpr uint32_t kMaxRequests = 8;
    static constexpr uint32_t kMaxBufferBytes = 4096;

    enum class RequestKind : uint32_t
    {
        None,
        ColorTarget,
        DepthTarget,
        Buffer
    };

    struct CaptureRequest
    {
        RequestKind kind = RequestKind::None;
        char name[128] = {};
        IRenderTarget *render_target = nullptr;
        DeviceBuffer *buffer = nullptr;
        uint32_t color_index = 0;
        uint32_t buffer_offset = 0;
        uint32_t buffer_size = 0;
    };

    struct CaptureResult
    {
        bool valid = false;
        CaptureRequest request;
        uint32_t width = 0;
        uint32_t height = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t pixel_size = 0;
        uint64_t byte_count = 0;
        double min_value = 0.0;
        double max_value = 0.0;
        double average = 0.0;
        uint64_t nan_count = 0;
        uint64_t inf_count = 0;
        char dump_path[256] = {};
        const char *error = nullptr;
        uint64_t capture_id = 0;
    };

    struct CameraSnapshot
    {
        bool valid = false;
        float inverse_vp[16] = {};
        float vp[16] = {};
        float inverse_view[16] = {};
        float inverse_projection[16] = {};
        float znear = 0.0f;
        float zfar = 0.0f;
        uint32_t use_reversed_z = 0;
    };

    struct PipelineInspectionSnapshot
    {
        bool valid = false;
        uint32_t schema_version = 2;
        uint64_t world_id = 0;
        uint32_t frame_serial = 0;
        uint64_t snapshot_sequence = 0;
        uint64_t published_at_ms = 0;
        bool collect_available = false;
        bool collect_complete = false;
        bool batch_available = false;
        bool gpu_cull_available = false;
        bool gpu_execution_available = false;
        uint32_t scene_primitives = 0;
        uint32_t passed_can_render = 0;
        uint32_t collect_candidates = 0;
        uint32_t collected_items = 0;
        uint32_t batched_items = 0;
        uint32_t batch_count = 0;
        uint32_t gpu_draw_count = 0;
        bool gpu_draw_count_available = false;

        bool fast_path_engaged = false;
        bool last_collect_steady = false;
        bool has_pending_resources = false;
        bool snapshot_ready = false;
        uint64_t snapshot_topology_rev = 0;
        uint64_t world_topology_rev = 0;
        uint32_t snapshot_item_count = 0;

        uint32_t dropped_invisible = 0;
        uint32_t dropped_no_owner = 0;
        uint32_t dropped_no_transform = 0;
        uint32_t dropped_mobility = 0;
        uint32_t dropped_other = 0;
        uint32_t pending_material_resources = 0;
        uint32_t pending_geometry_resources = 0;

        static constexpr uint32_t kMaxSampleDrops = 16;
        struct DropSample
        {
            char entity_name[64] = {};
            uint32_t entity_id = 0;
            char stage[32] = {};
            char reason[32] = {};
            char detail[64] = {};
        };
        uint32_t drop_sample_count = 0;
        bool drop_sample_truncated = false;
        DropSample drop_samples[kMaxSampleDrops];

        static constexpr uint32_t kMaxTraceSamples = 64;
        struct TraceSample
        {
            uint32_t entity_id = UINT32_MAX;
            uint16_t generation = 0;
            bool pending_geometry = false;
            bool pending_material = false;
            uint32_t batch_index = UINT32_MAX;
            char entity_name[64] = {};
            char collect_status[16] = "unknown";
            char collect_reason[32] = {};
            char batch_status[16] = "unknown";
            char batch_reason[32] = {};
        };
        uint32_t trace_sample_count = 0;
        bool trace_sample_truncated = false;
        TraceSample trace_samples[kMaxTraceSamples];
    };

    static uint32_t RequestColor(IRenderTarget *target,const char *name,uint32_t color_index=0);
    static uint32_t RequestDepth(IRenderTarget *target,const char *name);
    static uint32_t RequestBuffer(DeviceBuffer *buffer,const char *name,uint32_t offset=0,uint32_t size=0);

    static uint32_t CollectPending();
    static uint32_t GetResultCount();
    /// Only call on the collecting thread when no collection is in progress.
    /// The returned pointer is invalidated by subsequent collection.
    static const CaptureResult *GetResult(uint32_t index);
    static AnsiString GetLatestResultJSON();
    static void SetCameraSnapshot(const CameraInfo *camera);
    static CameraSnapshot GetCameraSnapshot();
    static AnsiString GetCameraSnapshotJSON();
    static void SetPipelineInspection(const PipelineInspectionSnapshot &snapshot);
    static PipelineInspectionSnapshot GetPipelineInspection();
    static bool GetPipelineInspection(uint64_t world_id,uint32_t frame_serial,
                                      PipelineInspectionSnapshot &snapshot);
    static AnsiString GetPipelineInspectionJSON(uint64_t world_id=0,uint32_t frame_serial=0);
    static AnsiString TraceEntityPipelineJSON(const char *name_or_id,
                                               uint64_t world_id=0,uint32_t frame_serial=0);
};
}//namespace hgl::graph
