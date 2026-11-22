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
        enum class Attachment { Color, Depth };
        enum class State { Empty, Queued, Capturing, Succeeded, Failed, Cancelled };

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
            uint64_t request_id=0;
            uint64_t target_id=0;
            uint64_t frame_id=0;
            uint64_t expires_at_ms=0;
            Attachment attachment=Attachment::Color;
            State state=State::Empty;
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
        };

        static uint64_t RegisterTarget(bool color=true,bool depth=true);
        static void UnregisterTarget(uint64_t target_id);
        static AnsiString GetTargetsJSON();
        static uint64_t Request(uint64_t target_id,Attachment attachment,const char **error=nullptr,
                                uint32_t timeout_ms=30000);
        static bool GetResult(uint64_t request_id,Result &result);
        static bool Cancel(uint64_t request_id);
        static bool Release(uint64_t request_id);
        static AnsiString GetResultJSON(uint64_t request_id);
        static void BeginFrame(uint64_t target_id,uint64_t frame_id);
        static void RecordCamera(uint64_t target_id,const CameraInfo *camera);
        static void Process(IRenderTarget *target,uint64_t target_id,uint64_t frame_id);
    };
}
