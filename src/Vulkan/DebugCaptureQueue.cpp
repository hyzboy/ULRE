#include<hgl/vk/DebugCaptureQueue.h>
#include<hgl/vk/VKTextureReadback.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/mtl/DebugViewMode.h>
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
            uint64_t frame_id=0;
            DebugCaptureQueue::Source source=DebugCaptureQueue::Source::Swapchain;
            uint32_t debug_mode=0;
            DebugCaptureQueue::CameraSnapshot camera;
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

        const char *Encoding(const DebugCaptureQueue::Result &result)
        {
            if(result.attachment==DebugCaptureQueue::Attachment::Depth)
            {
                switch(result.format)
                {
                    case VK_FORMAT_D32_SFLOAT:
                    case VK_FORMAT_D32_SFLOAT_S8_UINT: return "r32f";
                    case VK_FORMAT_D16_UNORM:
                    case VK_FORMAT_D16_UNORM_S8_UINT: return "d16_unorm";
                    case VK_FORMAT_X8_D24_UNORM_PACK32:
                    case VK_FORMAT_D24_UNORM_S8_UINT: return "d24_unorm_32";
                    default: return "unknown";
                }
            }
            switch(result.format)
            {
                case VK_FORMAT_R8G8B8A8_UNORM:
                case VK_FORMAT_R8G8B8A8_SRGB: return "rgba8";
                case VK_FORMAT_B8G8R8A8_UNORM:
                case VK_FORMAT_B8G8R8A8_SRGB: return "bgra8";
                case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return "a2bgr10";
                case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return "a2r10g10b10";
                default: return "unknown";
            }
        }

        bool Finite(float value)
        {
            uint32_t bits=0;
            ::memcpy(&bits,&value,sizeof(bits));
            return (bits&0x7f800000u)!=0x7f800000u;
        }

        bool CameraFinite(const DebugCaptureQueue::CameraSnapshot &camera)
        {
            if(!Finite(camera.znear) || !Finite(camera.zfar)) return false;
            for(float value:camera.vp) if(!Finite(value)) return false;
            for(float value:camera.inverse_vp) if(!Finite(value)) return false;
            for(float value:camera.camera_world_pos) if(!Finite(value)) return false;
            return true;
        }

        void AppendFloats(AnsiString &json,const float *values,uint32_t count)
        {
            json+="[";
            for(uint32_t i=0;i<count;++i)
            {
                if(i) json+=",";
                char text[48];
                ::snprintf(text,sizeof(text),"%.9g",static_cast<double>(values[i]));
                json+=text;
            }
            json+="]";
        }

        void AppendJSONString(AnsiString &json,const char *value)
        {
            json+="\"";
            for(const unsigned char *p=reinterpret_cast<const unsigned char *>(value);*p;++p)
            {
                if(*p=='"' || *p=='\\')
                {
                    char escaped[3]={'\\',static_cast<char>(*p),0};
                    json+=escaped;
                }
                else if(*p<0x20)
                {
                    char escaped[7];
                    ::snprintf(escaped,sizeof(escaped),"\\u%04x",static_cast<unsigned>(*p));
                    json+=escaped;
                }
                else
                {
                    char text[2]={static_cast<char>(*p),0};
                    json+=text;
                }
            }
            json+="\"";
        }

        AnsiString MakeManifest(const DebugCaptureQueue::Result &result,const char *filename)
        {
            const char *base=::strrchr(filename,'/');
            const char *backslash=::strrchr(filename,'\\');
            if(backslash && (!base || backslash>base))
                base=backslash;
            AnsiString json="{\"schema\":\"ulre.capture.v1\",\"request_id\":";
            json+=AnsiString::numberOf(result.request_id);
            json+=",\"target_id\":";
            json+=AnsiString::numberOf(result.target_id);
            json+=",\"frame_id\":";
            json+=AnsiString::numberOf(result.frame_id);
            json+=",\"source\":\"";
            json+=result.source==DebugCaptureQueue::Source::DebugView?"debug_view":"swapchain";
            json+="\",\"debug_view_mode\":\"";
            json+=mtl::GetDebugViewModeName(static_cast<mtl::DebugViewMode>(result.debug_mode));
            json+="\",\"attachment\":\"";
            json+=result.attachment==DebugCaptureQueue::Attachment::Color?"color":"depth";
            json+="\",\"capture_id\":\"";
            json+=base?base+1:filename;
            json+="\",\"data_file\":\"";
            json+=base?base+1:filename;
            if(result.grouped)
            {
                json+="\",\"capture_group_id\":\"";
                json+=result.group_id;
            }
            json+="\",\"width\":";
            json+=AnsiString::numberOf(result.width);
            json+=",\"height\":";
            json+=AnsiString::numberOf(result.height);
            json+=",\"vk_format\":";
            json+=AnsiString::numberOf(static_cast<int>(result.format));
            json+=",\"encoding\":\"";
            json+=Encoding(result);
            json+="\",\"color_space\":\"";
            json+=result.source==DebugCaptureQueue::Source::DebugView
                  && result.attachment==DebugCaptureQueue::Attachment::Color?"linear":
                  (result.format==VK_FORMAT_R8G8B8A8_SRGB || result.format==VK_FORMAT_B8G8R8A8_SRGB)
                  ?"srgb":"unknown";
            json+="\",\"byte_order\":\"little\",\"samples\":1,\"mip\":0,\"layer\":0,\"pixel_size\":";
            json+=AnsiString::numberOf(result.pixel_size);
            json+=",\"row_pitch\":";
            json+=AnsiString::numberOf(result.row_pitch);
            json+=",\"byte_count\":";
            json+=AnsiString::numberOf(result.byte_count);
            json+=",\"mask\":\"";
            json+=result.source==DebugCaptureQueue::Source::DebugView
                  && result.attachment==DebugCaptureQueue::Attachment::Color?"alpha_nonzero":"unavailable";
            json+="\",\"viewport\":{\"x\":0,\"y\":0,\"width\":";
            json+=AnsiString::numberOf(result.width);
            json+=",\"height\":";
            json+=AnsiString::numberOf(result.height);
            json+=",\"min_depth\":0,\"max_depth\":1}";
            if(result.attachment==DebugCaptureQueue::Attachment::Depth)
                json+=",\"copy_aspect\":\"depth\",\"clear_depth\":0";
            json+=",\"camera_status\":\"";
            json+=result.camera.valid?"recorded":result.camera.ambiguous?"ambiguous":"unavailable";
            json+="\",\"camera\":";
            if(result.camera.valid)
            {
                // Camera-relative view/transform offsets are not enabled in the current camera path.
                json+="{\"source\":\"primitive_collect\",\"matrix_layout\":\"column_major\","
                      "\"position_space\":\"world_absolute\",\"vp\":";
                AppendFloats(json,result.camera.vp,16);
                json+=",\"inverse_vp\":";
                AppendFloats(json,result.camera.inverse_vp,16);
                json+=",\"camera_world_pos\":";
                AppendFloats(json,result.camera.camera_world_pos,3);
                char depth_range[96];
                ::snprintf(depth_range,sizeof(depth_range),",\"znear\":%.9g,\"zfar\":%.9g",
                           static_cast<double>(result.camera.znear),static_cast<double>(result.camera.zfar));
                json+=depth_range;
                json+=",\"reversed_z\":";
                json+=result.camera.reversed_z?"true":"false";
                json+="}";
            }
            else json+="null";
            json+="}";
            return json;
        }

        bool WriteNew(const char *path,const void *data,int64 length,bool &created)
        {
            created=false;
            io::OpenFileOutputStream stream(ToOSString(path),io::FileOpenMode::Create);
            if(!stream) return false;
            created=true;
            return stream->Write(data,length)==length;
        }

        bool Save(DebugCaptureQueue::Result &result,const ValueArray<uint8_t> &pixels)
        {
            if(!filesystem::MakePath(OS_TEXT(".ulre-captures")))
                return false;
            const uint16_t endian=1;
            if(*reinterpret_cast<const uint8_t *>(&endian)!=1)
                return false;
            char base[192],raw[256],raw_tmp[256],meta[256],meta_tmp[256];
            const int length=::snprintf(base,sizeof(base),".ulre-captures%ccapture_%llu_%llu_%s",
                static_cast<char>(HGL_DIRECTORY_SEPARATOR),
                static_cast<unsigned long long>(hgl::GetTimeUs()),
                static_cast<unsigned long long>(result.request_id),
                result.attachment==DebugCaptureQueue::Attachment::Color?"color":"depth");
            if(length<0 || static_cast<size_t>(length)>=sizeof(base))
                return false;
            ::snprintf(raw,sizeof(raw),"%s.raw",base);
            ::snprintf(raw_tmp,sizeof(raw_tmp),"%s.raw.tmp",base);
            ::snprintf(meta,sizeof(meta),"%s.json",base);
            ::snprintf(meta_tmp,sizeof(meta_tmp),"%s.json.tmp",base);
            if(result.grouped && result.attachment==DebugCaptureQueue::Attachment::Color)
            {
                const char *filename=::strrchr(raw,static_cast<char>(HGL_DIRECTORY_SEPARATOR));
                filename=filename?filename+1:raw;
                if(::strlen(filename)>=sizeof(result.group_id)) return false;
                ::memcpy(result.group_id,filename,::strlen(filename)+1);
            }
            OSString cwd;
            if(!filesystem::GetCurrentPath(cwd))
                return false;
            const U8String raw_absolute=ToU8String(filesystem::JoinPathWithFilename(cwd,ToOSString(raw)));
            const U8String meta_absolute=ToU8String(filesystem::JoinPathWithFilename(cwd,ToOSString(meta)));
            if(raw_absolute.Length()>=sizeof(result.path)
               || meta_absolute.Length()>=sizeof(result.metadata_path))
                return false;
            if(filesystem::FileExist(ToOSString(raw)) || filesystem::FileExist(ToOSString(meta)))
                return false;
            bool created=false;
            if(!WriteNew(raw_tmp,pixels.GetData(),pixels.GetTotalBytes(),created))
            {
                if(created) filesystem::FileDelete(ToOSString(raw_tmp));
                return false;
            }
            const AnsiString json=MakeManifest(result,raw);
            if(!WriteNew(meta_tmp,json.c_str(),json.Length(),created))
            {
                filesystem::FileDelete(ToOSString(raw_tmp));
                if(created) filesystem::FileDelete(ToOSString(meta_tmp));
                return false;
            }
            if(!filesystem::FileRename(ToOSString(raw),ToOSString(raw_tmp)))
            {
                filesystem::FileDelete(ToOSString(raw_tmp));
                filesystem::FileDelete(ToOSString(meta_tmp));
                return false;
            }
            if(!filesystem::FileRename(ToOSString(meta),ToOSString(meta_tmp)))
            {
                filesystem::FileDelete(ToOSString(raw));
                filesystem::FileDelete(ToOSString(meta_tmp));
                return false;
            }
            ::memcpy(result.path,raw_absolute.c_str(),raw_absolute.Length()+1);
            ::memcpy(result.metadata_path,meta_absolute.c_str(),meta_absolute.Length()+1);
            return true;
        }

        bool CaptureAttachment(IRenderTarget *target,DebugCaptureQueue::Result &result)
        {
            using Queue=DebugCaptureQueue;
            TextureReadbackInfo info;
            ValueArray<uint8_t> pixels;
            result.state=Queue::State::Failed;
            result.error="readback_failed";
            Texture2D *texture=result.attachment==Queue::Attachment::Color
                ?target->GetColorTexture(0):target->GetDepthTexture();
            const VkExtent3D *extent=texture?texture->GetExtent():nullptr;
            uint32_t stride=texture?GetStrideByFormat(texture->GetFormat()):0;
            if(texture && result.attachment==Queue::Attachment::Depth)
            {
                if(texture->GetFormat()==VK_FORMAT_D16_UNORM
                   || texture->GetFormat()==VK_FORMAT_D16_UNORM_S8_UINT)
                    stride=2;
                else stride=4;
            }
            const bool too_large=extent && stride
                && uint64_t(extent->width)*extent->height>kMaxCaptureBytes/stride;
            if(too_large) result.error="capture_size_limit";
            const bool read=!too_large && (result.attachment==Queue::Attachment::Color
                ?ReadbackColorTarget(target,pixels,0,&info)
                :ReadbackDepthTarget(target,pixels,&info));
            if(!read) return false;
            result.width=info.width;
            result.height=info.height;
            result.pixel_size=info.pixel_size;
            result.row_pitch=info.row_pitch;
            result.format=info.format;
            result.byte_count=pixels.GetTotalBytes();
            if(!Save(result,pixels))
            {
                result.error="write_failed";
                return false;
            }
            result.state=Queue::State::Succeeded;
            result.error=nullptr;
            return true;
        }

        void RemoveCapture(const DebugCaptureQueue::Result &result)
        {
            if(result.path[0] && !filesystem::FileDelete(ToOSString(result.path)))
                GLogError(u8"[DebugCapture] Failed to remove partial capture data");
            if(result.metadata_path[0] && !filesystem::FileDelete(ToOSString(result.metadata_path)))
                GLogError(u8"[DebugCapture] Failed to remove partial capture manifest");
        }
    }

    void DebugCaptureQueue::BeginFrame(uint64_t target_id,uint64_t frame_id,uint32_t debug_mode)
    {
        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        for(Target &target:queue.targets)
            if(target.id==target_id && target_id)
            {
                target.frame_id=frame_id;
                target.debug_mode=debug_mode;
                target.camera=CameraSnapshot{};
                return;
            }
    }

    void DebugCaptureQueue::RecordCamera(uint64_t target_id,const CameraInfo *camera)
    {
        if(!camera || !target_id) return;
        CameraSnapshot snapshot;
        snapshot.valid=true;
        ::memcpy(snapshot.vp,&camera->vp[0][0],sizeof(snapshot.vp));
        ::memcpy(snapshot.inverse_vp,&camera->inverse_vp[0][0],sizeof(snapshot.inverse_vp));
        snapshot.camera_world_pos[0]=camera->camera_world_pos.x;
        snapshot.camera_world_pos[1]=camera->camera_world_pos.y;
        snapshot.camera_world_pos[2]=camera->camera_world_pos.z;
        snapshot.znear=camera->znear;
        snapshot.zfar=camera->zfar;
        snapshot.reversed_z=camera->use_reversed_z;

        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        for(Target &target:queue.targets)
            if(target.id==target_id && target.frame_id)
            {
                if(target.camera.ambiguous) return;
                if(!target.camera.valid)
                    target.camera=snapshot;
                else if(::memcmp(target.camera.vp,snapshot.vp,sizeof(snapshot.vp))!=0
                     || ::memcmp(target.camera.inverse_vp,snapshot.inverse_vp,sizeof(snapshot.inverse_vp))!=0
                     || ::memcmp(target.camera.camera_world_pos,snapshot.camera_world_pos,sizeof(snapshot.camera_world_pos))!=0
                     || target.camera.znear!=snapshot.znear || target.camera.zfar!=snapshot.zfar
                     || target.camera.reversed_z!=snapshot.reversed_z)
                {
                    target.camera.valid=false;
                    target.camera.ambiguous=true;
                }

                return;
            }
    }

    bool DebugCaptureQueue::CopyCamera(uint64_t from_target,uint64_t to_target,uint64_t frame_id)
    {
        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        const Target *source=FindTarget(queue,from_target);
        if(!source || source->frame_id!=frame_id || !source->camera.valid || source->camera.ambiguous)
            return false;
        for(Target &target:queue.targets)
            if(target.id==to_target && target.frame_id==frame_id)
            {
                target.camera=source->camera;
                return true;
            }
        return false;
    }

    uint64_t DebugCaptureQueue::RegisterTarget(bool color,bool depth,Source source)
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
                target.source=source;
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
                json+=",\"source\":\"";
                json+=target.source==Source::DebugView?"debug_view":"swapchain";
                json+="\",\"kind\":\"";
                json+=target.source==Source::DebugView?"offscreen":"swapchain";
                json+="\",\"attachments\":[";
                if(target.color) json+="\"color\"";
                if(target.color && target.depth) json+=",";
                if(target.depth) json+="\"depth\"";
                if(target.color && target.depth) json+=",\"color_depth\"";
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
        if(attachment!=Attachment::Color && attachment!=Attachment::Depth
           && attachment!=Attachment::ColorDepth)
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
           || (attachment==Attachment::Depth && !target->depth)
           || (attachment==Attachment::ColorDepth && (!target->color || !target->depth)))
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
        json+=result.attachment==Attachment::Color?"color"
             :result.attachment==Attachment::Depth?"depth":"color_depth";
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
        json+=",\"dump_path\":";
        AppendJSONString(json,result.path);
        json+=",\"metadata_path\":";
        AppendJSONString(json,result.metadata_path);
        if(result.attachment==Attachment::ColorDepth && result.state==State::Succeeded)
        {
            json+=",\"captures\":[{\"attachment\":\"color\",\"dump_path\":";
            AppendJSONString(json,result.path);
            json+=",\"metadata_path\":";
            AppendJSONString(json,result.metadata_path);
            json+="},{\"attachment\":\"depth\",\"dump_path\":";
            AppendJSONString(json,result.depth.path);
            json+=",\"metadata_path\":";
            AppendJSONString(json,result.depth.metadata_path);
            json+="}]";
        }
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
                const Target *registered=FindTarget(queue,target_id);
                if(!registered) return;
                for(Result &entry:queue.requests)
                    if(entry.state==State::Queued && entry.target_id==target_id)
                    {
                        entry.state=State::Capturing;
                        result=entry;
                        result.source=registered->source;
                        if(registered->frame_id==frame_id)
                        {
                            result.camera=registered->camera;
                            result.debug_mode=registered->debug_mode;
                        }
                        break;
                    }

            }
            if(!result.request_id) return;
            result.frame_id=frame_id;
            if(result.camera.valid && !CameraFinite(result.camera))
                result.camera.valid=false;
            if(result.attachment==Attachment::ColorDepth)
            {
                Result color=result;
                color.attachment=Attachment::Color;
                color.grouped=true;
                if(!CaptureAttachment(target,color))
                {
                    result.state=State::Failed;
                    result.error=color.error;
                }
                else
                {
                    Result depth=result;
                    depth.attachment=Attachment::Depth;
                    depth.grouped=true;
                    ::memcpy(depth.group_id,color.group_id,sizeof(color.group_id));
                    if(!CaptureAttachment(target,depth))
                    {
                        RemoveCapture(color);
                        result.state=State::Failed;
                        result.error=depth.error;
                    }
                    else
                    {
                        result=color;
                        result.attachment=Attachment::ColorDepth;
                        ::memcpy(result.depth.path,depth.path,sizeof(depth.path));
                        ::memcpy(result.depth.metadata_path,depth.metadata_path,sizeof(depth.metadata_path));
                    }
                }

            }
            else CaptureAttachment(target,result);
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

    void DebugCaptureQueue::FailQueued(uint64_t target_id,const char *error)
    {
        QueueState &queue=Queue();
        hgl::MutexLock lock(&queue.lock);
        for(Result &result:queue.requests)
            if(result.target_id==target_id && result.state==State::Queued)
            {
                result.state=State::Failed;
                result.error=error;
            }
    }
}
