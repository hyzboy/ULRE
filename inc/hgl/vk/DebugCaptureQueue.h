#pragma once

#include<hgl/type/String.h>
#include<hgl/vk/VK.h>

namespace hgl::graph
{
    class IRenderTarget;

    class DebugCaptureQueue
    {
    public:
        enum class Attachment { Color, Depth };
        enum class State { Empty, Queued, Capturing, Succeeded, Failed, Cancelled };

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
            char path[256]{};
            const char *error=nullptr;
        };

        static uint64_t RegisterTarget(bool color=true,bool depth=true);
        static void UnregisterTarget(uint64_t target_id);
        static AnsiString GetTargetsJSON();
        static uint64_t Request(uint64_t target_id,Attachment attachment,const char **error=nullptr);
        static bool GetResult(uint64_t request_id,Result &result);
        static bool Cancel(uint64_t request_id);
        static bool Release(uint64_t request_id);
        static AnsiString GetResultJSON(uint64_t request_id);
        static void Process(IRenderTarget *target,uint64_t target_id,uint64_t frame_id);
    };
}
