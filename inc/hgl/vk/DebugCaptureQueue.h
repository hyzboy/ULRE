#pragma once

#include<hgl/type/String.h>
#include<hgl/vk/VK.h>

namespace hgl::graph
{
    class IRenderTarget;
    struct CameraInfo;

    class DebugCaptureQueue
    {
    public:
        enum class Attachment { Color, Depth, ColorDepth };
        enum class State { Empty, Queued, Capturing, Succeeded, Failed, Cancelled };
        enum class Source { Swapchain, DebugView };

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
            struct Secondary
            {
                char path[1024]{};
                char metadata_path[1024]{};
            };
            uint64_t request_id=0;
            uint64_t target_id=0;
            uint64_t frame_id=0;
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
            char path[1024]{};
            char metadata_path[1024]{};
            const char *error=nullptr;
            CameraSnapshot camera;
            Secondary depth;
        };

        static uint64_t RegisterTarget(bool color=true,bool depth=true,Source source=Source::Swapchain);
        static void UnregisterTarget(uint64_t target_id);
        static AnsiString GetTargetsJSON();
        static uint64_t Request(uint64_t target_id,Attachment attachment,const char **error=nullptr,
                                uint32_t timeout_ms=30000);
        static bool GetResult(uint64_t request_id,Result &result);
        static bool Cancel(uint64_t request_id);
        static bool Release(uint64_t request_id);
        static AnsiString GetResultJSON(uint64_t request_id);
        static void BeginFrame(uint64_t target_id,uint64_t frame_id,uint32_t debug_mode=0);
        static void RecordCamera(uint64_t target_id,const CameraInfo *camera);
        static bool CopyCamera(uint64_t from_target,uint64_t to_target,uint64_t frame_id);
        static void Process(IRenderTarget *target,uint64_t target_id,uint64_t frame_id);
        static void FailQueued(uint64_t target_id,const char *error);
    };
}
