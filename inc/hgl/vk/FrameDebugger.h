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
};
}//namespace hgl::graph
