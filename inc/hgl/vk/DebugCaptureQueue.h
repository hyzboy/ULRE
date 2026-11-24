#pragma once

#include<hgl/type/String.h>
#include<hgl/vk/VK.h>

namespace hgl::graph
{
    class IRenderTarget;
    class DeviceBuffer;
    struct CameraInfo;

    class DebugCaptureQueue
    {
    public:
        enum class Attachment { Color, Depth, ColorDepth, Buffer };
        enum class State { Empty, Queued, Capturing, Succeeded, Failed, Cancelled };
        enum class Source { Swapchain, DebugView, Offscreen, Buffer };

        struct CameraSnapshot
        {
            bool valid=false;
            bool ambiguous=false;
            float vp[16]{};
            float inverse_vp[16]{};
            float camera_world_pos[3]{};
            float znear=0;
            float zfar=0;
            uint32_t reversed_z=0;
        };

        struct Result
        {
            struct DeviceIdentity
            {
                bool available=false;
                bool driver_properties_available=false;
                uint32_t vendor_id=0;
                uint32_t device_id=0;
                uint32_t device_type=0;
                uint32_t api_version=0;
                uint32_t driver_version=0;
                uint32_t driver_id=0;
                char device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE]{};
                char driver_name[VK_MAX_DRIVER_NAME_SIZE]{};
                char driver_info[VK_MAX_DRIVER_INFO_SIZE]{};
            };
            struct Secondary
            {
                char path[1024]{};
                char metadata_path[1024]{};
            };
            uint64_t request_id=0;
            uint64_t target_id=0;
            uint64_t frame_id=0;
            uint64_t source_world_id=0;
            uint64_t source_frame_serial=0;
            uint64_t expires_at_ms=0;
            Attachment attachment=Attachment::Color;
            State state=State::Empty;
            Source source=Source::Swapchain;
            uint32_t debug_mode=0;
            bool grouped=false;
            char group_id[192]{};
            uint32_t width=0;
            uint32_t height=0;
            uint32_t pixel_size=0;
            uint32_t row_pitch=0;
            VkFormat format=VK_FORMAT_UNDEFINED;
            uint64_t byte_count=0;
            uint64_t buffer_offset=0;
            VkBufferUsageFlags buffer_usage=0;
            char path[1024]{};
            char metadata_path[1024]{};
            const char *error=nullptr;
            CameraSnapshot camera;
            DeviceIdentity device;
            bool clear_depth_valid=false;
            float clear_depth=0;
            Secondary depth;
        };

        static uint64_t RegisterTarget(bool color=true,bool depth=true,Source source=Source::Swapchain,
                                       IRenderTarget *bound_target=nullptr,const char *name=nullptr);
        // Registration, processing and unregistration must run on the resource owner/render thread.
        // The owner guarantees that all GPU access is ordered on the graphics queue and that
        // the buffer remains alive and unchanged until ProcessBuffer returns.
        static uint64_t RegisterBuffer(DeviceBuffer *buffer,const char *name);
        static AnsiString GetBuffersJSON();
        static uint64_t RequestBuffer(uint64_t resource_id,uint64_t offset,uint64_t size,
                                      const char **error=nullptr,uint32_t timeout_ms=30000);
        static void ProcessBuffer(uint64_t resource_id,DeviceBuffer *buffer,uint64_t frame_id,
                                  VkQueue submitted_queue,bool submission_succeeded,
                                  uint64_t source_world_id=0,uint64_t source_frame_serial=0);
        static bool SupportsFormat(VkFormat format,Attachment attachment);
        static void SetOffscreenDetails(uint64_t target_id,const VkFormat *colors,uint32_t count,VkFormat depth,
                                        bool can_color,bool can_depth);
        static void UnregisterTarget(uint64_t target_id);
        static AnsiString GetTargetsJSON();
        static uint64_t Request(uint64_t target_id,Attachment attachment,const char **error=nullptr,
                                uint32_t timeout_ms=30000);
        static bool GetResult(uint64_t request_id,Result &result);
        static bool Cancel(uint64_t request_id);
        static bool Release(uint64_t request_id);
        static bool HasQueued(uint64_t target_id);
        static AnsiString GetResultJSON(uint64_t request_id);
        static void BeginFrame(uint64_t target_id,uint64_t frame_id,uint32_t debug_mode=0);
        static void RecordDepthClear(uint64_t target_id,bool full_clear,float value);
        static void RecordCamera(uint64_t target_id,const CameraInfo *camera);
        static bool CopyCamera(uint64_t from_target,uint64_t to_target,uint64_t frame_id);
        static void Process(IRenderTarget *target,uint64_t target_id,uint64_t frame_id);
        static void FailQueued(uint64_t target_id,const char *error);
        static void ResetAllTargets();
    };
}
