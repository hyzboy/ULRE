#include<hgl/vk/DebugCaptureQueue.h>
#include<hgl/vk/VKTextureReadback.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/io/FileOutputStream.h>
#include<hgl/sync/Mutex.h>
#include<hgl/time/Time.h>
#include<hgl/type/ValueArray.h>
#include<hgl/log/Log.h>
#include<cstdio>
#include<cstring>

namespace hgl::graph
{
    namespace
    {
        constexpr uint32_t kMaxTargets=16;
        constexpr uint32_t kMaxRequests=16;
        constexpr uint32_t kMaxQueueTimeoutMs=30000;
        constexpr uint64_t kMaxCaptureBytes=256ull*1024*1024;
        struct Target
        {
            uint64_t id=0;
            bool color=false;
            bool depth=false;
        };
        struct QueueState
        {
            hgl::Mutex lock;
            Target targets[kMaxTargets]{};
            DebugCaptureQueue::Result requests[kMaxRequests]{};
            uint64_t next_target=1;
            uint64_t next_request=1;
        };

        QueueState &Queue()
        {
            static QueueState state;
            return state;
        }

        const Target *FindTarget(const QueueState &queue,uint64_t id)
        {
            if(!id) return nullptr;
            for(const Target &target:queue.targets)
                if(target.id==id) return &target;
            return nullptr;
        }

        void Expire(QueueState &queue)
        {
            const uint64_t now=hgl::GetUptimeMs();
            for(DebugCaptureQueue::Result &result:queue.requests)
                if(result.state==DebugCaptureQueue::State::Queued && now>=result.expires_at_ms)
                {
                    result.state=DebugCaptureQueue::State::Failed;
                    result.error="queue_timeout";
                }
        }

        const char *StateName(DebugCaptureQueue::State state)
        {
            switch(state)
            {
                case DebugCaptureQueue::State::Queued: return "queued";
                case DebugCaptureQueue::State::Capturing: return "capturing";
                case DebugCaptureQueue::State::Succeeded: return "succeeded";
                case DebugCaptureQueue::State::Failed: return "failed";
                case DebugCaptureQueue::State::Cancelled: return "cancelled";
                default: return "empty";
            }
        }

        bool Save(const DebugCaptureQueue::Result &result,const ValueArray<uint8_t> &pixels,
                  char *path,size_t path_size)
        {
            if(!filesystem::MakePath(OS_TEXT(".ulre-captures")))
                return false;
            const int length=::snprintf(path,path_size,".ulre-captures/capture_%llu_%llu.raw",
                static_cast<unsigned long long>(hgl::GetTimeUs()),
                static_cast<unsigned long long>(result.request_id));
            if(length<0 || static_cast<size_t>(length)>=path_size)
                return false;
            io::OpenFileOutputStream stream(ToOSString(path),io::FileOpenMode::Create);
            if(!stream)
                return false;
            return stream->Write(pixels.GetData(),pixels.GetTotalBytes())==pixels.GetTotalBytes();
        }
    }

    uint64_t DebugCaptureQueue::RegisterTarget(bool color,bool depth)
    {
        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        for(Target &target:queue.targets)
            if(!target.id)
            {
                if(!queue.next_target) return 0;
                target.id=queue.next_target++;
                target.color=color;
                target.depth=depth;
                return target.id;
            }
        GLogError(u8"[DebugCapture] Target registry is full");
        return 0;
    }

    void DebugCaptureQueue::UnregisterTarget(uint64_t target_id)
    {
        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        for(Target &target:queue.targets)
            if(target.id==target_id) target=Target{};
        for(Result &result:queue.requests)
            if(result.target_id==target_id && result.state==State::Queued)
            {
                result.state=State::Cancelled;
                result.error="target_destroyed";
            }
    }

    AnsiString DebugCaptureQueue::GetTargetsJSON()
    {
        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        AnsiString json="{\"valid\":true,\"targets\":[";
        bool first=true;
        for(const Target &target:queue.targets)
            if(target.id)
            {
                if(!first) json+=",";
                first=false;
                json+="{\"target_id\":";
                json+=AnsiString::numberOf(target.id);
                json+=",\"kind\":\"swapchain\",\"attachments\":[";
                if(target.color) json+="\"color\"";
                if(target.color && target.depth) json+=",";
                if(target.depth) json+="\"depth\"";
                json+="]}";
            }
        json+="]}";
        return json;
    }

    uint64_t DebugCaptureQueue::Request(uint64_t target_id,Attachment attachment,const char **error,
                                        uint32_t timeout_ms)
    {
        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        Expire(queue);
        if(error) *error=nullptr;
        if(!timeout_ms || timeout_ms>kMaxQueueTimeoutMs)
        {
            if(error) *error="invalid_timeout";
            return 0;
        }
        if(attachment!=Attachment::Color && attachment!=Attachment::Depth)
        {
            if(error) *error="unsupported_attachment";
            return 0;
        }
        const Target *target=FindTarget(queue,target_id);
        if(!target)
        {
            if(error) *error="unknown_target";
            return 0;
        }
        if((attachment==Attachment::Color && !target->color)
           || (attachment==Attachment::Depth && !target->depth))
        {
            if(error) *error="attachment_unavailable";
            return 0;
        }
        if(!queue.next_request)
        {
            if(error) *error="request_id_exhausted";
            return 0;
        }
        for(Result &result:queue.requests)
            if(result.state==State::Empty)
            {
                result=Result{};
                result.request_id=queue.next_request++;
                result.target_id=target_id;
                result.attachment=attachment;
                result.state=State::Queued;
                result.expires_at_ms=hgl::GetUptimeMs()+timeout_ms;
                return result.request_id;
            }
        if(error) *error="queue_full";
        return 0;
    }

    bool DebugCaptureQueue::GetResult(uint64_t request_id,Result &out)
    {
        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        Expire(queue);
        for(const Result &result:queue.requests)
            if(result.state!=State::Empty && result.request_id==request_id)
            {
                out=result;
                return true;
            }
        return false;
    }

    bool DebugCaptureQueue::Cancel(uint64_t request_id)
    {
        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        Expire(queue);
        for(Result &result:queue.requests)
            if(result.request_id==request_id && result.state==State::Queued)
            {
                result.state=State::Cancelled;
                result.error="cancelled_by_client";
                return true;
            }
        return false;
    }

    bool DebugCaptureQueue::Release(uint64_t request_id)
    {
        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        Expire(queue);
        for(Result &result:queue.requests)
            if(result.request_id==request_id && (result.state==State::Succeeded
               || result.state==State::Failed || result.state==State::Cancelled))
            {
                result=Result{};
                return true;
            }
        return false;
    }

    AnsiString DebugCaptureQueue::GetResultJSON(uint64_t request_id)
    {
        Result result;
        if(!GetResult(request_id,result))
            return AnsiString("{\"valid\":false,\"error\":\"unknown_request\"}");
        AnsiString json="{\"valid\":";
        json+=(result.state==State::Failed || result.state==State::Cancelled)?"false":"true";
        json+=",\"request_id\":";
        json+=AnsiString::numberOf(result.request_id);
        json+=",\"target_id\":";
        json+=AnsiString::numberOf(result.target_id);
        json+=",\"state\":\"";
        json+=StateName(result.state);
        json+="\",\"attachment\":\"";
        json+=result.attachment==Attachment::Color?"color":"depth";
        json+="\",\"frame_id\":";
        json+=AnsiString::numberOf(result.frame_id);
        json+=",\"width\":";
        json+=AnsiString::numberOf(result.width);
        json+=",\"height\":";
        json+=AnsiString::numberOf(result.height);
        json+=",\"pixel_size\":";
        json+=AnsiString::numberOf(result.pixel_size);
        json+=",\"row_pitch\":";
        json+=AnsiString::numberOf(result.row_pitch);
        json+=",\"format\":";
        json+=AnsiString::numberOf(static_cast<int>(result.format));
        json+=",\"bytes\":";
        json+=AnsiString::numberOf(result.byte_count);
        json+=",\"dump_path\":\"";
        json+=result.path;
        json+="\"";
        if(result.error)
        {
            json+=",\"error\":\"";
            json+=result.error;
            json+="\"";
        }
        json+="}";
        return json;
    }

    void DebugCaptureQueue::Process(IRenderTarget *target,uint64_t target_id,uint64_t frame_id)
    {
        if(!target || !target_id) return;
        for(;;)
        {
            Result result;
            {
                QueueState &queue=Queue();
                hgl::MutexLock lock(&queue.lock);
                Expire(queue);
                if(!FindTarget(queue,target_id)) return;
                for(Result &entry:queue.requests)
                    if(entry.state==State::Queued && entry.target_id==target_id)
                    {
                        entry.state=State::Capturing;
                        result=entry;
                        break;
                    }
            }
            if(!result.request_id) return;
            result.frame_id=frame_id;
            TextureReadbackInfo info;
            ValueArray<uint8_t> pixels;
            result.state=State::Failed;
            result.error="readback_failed";
            Texture2D *texture=result.attachment==Attachment::Color
                ?target->GetColorTexture(0):target->GetDepthTexture();
            const VkExtent3D *extent=texture?texture->GetExtent():nullptr;
            uint32_t stride=texture?GetStrideByFormat(texture->GetFormat()):0;
            if(texture && result.attachment==Attachment::Depth)
            {
                if(texture->GetFormat()==VK_FORMAT_D16_UNORM
                   || texture->GetFormat()==VK_FORMAT_D16_UNORM_S8_UINT)
                    stride=2;
                else stride=4;
            }
            const bool too_large=extent && stride
                && uint64_t(extent->width)*extent->height>kMaxCaptureBytes/stride;
            if(too_large) result.error="capture_size_limit";
            const bool read=!too_large && (result.attachment==Attachment::Color
                ?ReadbackColorTarget(target,pixels,0,&info)
                :ReadbackDepthTarget(target,pixels,&info));
            if(read)
            {
                result.width=info.width;
                result.height=info.height;
                result.pixel_size=info.pixel_size;
                result.row_pitch=info.row_pitch;
                result.format=info.format;
                result.byte_count=pixels.GetTotalBytes();
                if(Save(result,pixels,result.path,sizeof(result.path)))
                {
                    result.state=State::Succeeded;
                    result.error=nullptr;
                }
                else result.error="write_failed";
            }
            if(result.error)
                GLogError(u8"[DebugCapture] Request %llu failed: %s",
                          static_cast<unsigned long long>(result.request_id),result.error);
            QueueState &queue=Queue();
            hgl::MutexLock lock(&queue.lock);
            for(Result &entry:queue.requests)
                if(entry.request_id==result.request_id && entry.state==State::Capturing)
                {
                    entry=result;
                    break;
                }
        }
    }
}
