#include<hgl/vk/FrameDebugger.h>
#include<hgl/vk/VKTextureReadback.h>
#include<hgl/vk/VKBufferReadback.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/log/Log.h>
#include<cstring>
#include<cstdio>

namespace hgl::graph
{
namespace
{
FrameDebugger::CaptureRequest *Requests(){ static FrameDebugger::CaptureRequest requests[FrameDebugger::kMaxRequests]; return requests; }
uint32_t &RequestCount(){ static uint32_t count=0; return count; }

FrameDebugger::CaptureResult *Results(){ static FrameDebugger::CaptureResult results[FrameDebugger::kMaxRequests]; return results; }
uint32_t &ResultCount(){ static uint32_t count=0; return count; }

uint8_t *BufferScratch(){ static uint8_t bytes[FrameDebugger::kMaxBufferBytes]; return bytes; }
hgl::ValueArray<uint8_t> &PixelScratch(){ static hgl::ValueArray<uint8_t> bytes; return bytes; }

void CopyText(char *dst,uint32_t cap,const char *src)
{
    if(!dst || cap==0) return;
    dst[0]=0;
    if(!src) return;
    const uint32_t len=strlen(src);
    const uint32_t copy=(len<cap-1)?len:cap-1;
    memcpy(dst,src,copy);
    dst[copy]=0;
}

uint32_t AddRequest(const FrameDebugger::CaptureRequest &request)
{
    uint32_t &count=RequestCount();
    if(count>=FrameDebugger::kMaxRequests) return UINT32_MAX;
    Requests()[count]=request;
    return count++;
}
}

uint32_t FrameDebugger::RequestColor(IRenderTarget *target,const char *name,uint32_t color_index)
{
    if(!target) return UINT32_MAX;
    CaptureRequest request;
    request.kind=RequestKind::ColorTarget;
    request.render_target=target;
    request.color_index=color_index;
    CopyText(request.name,sizeof(request.name),name);
    return AddRequest(request);
}

uint32_t FrameDebugger::RequestDepth(IRenderTarget *target,const char *name)
{
    if(!target) return UINT32_MAX;
    CaptureRequest request;
    request.kind=RequestKind::DepthTarget;
    request.render_target=target;
    CopyText(request.name,sizeof(request.name),name);
    return AddRequest(request);
}

uint32_t FrameDebugger::RequestBuffer(DeviceBuffer *buffer,const char *name,uint32_t offset,uint32_t size)
{
    if(!buffer) return UINT32_MAX;
    if(offset>static_cast<uint32_t>(buffer->GetSize())) return UINT32_MAX;
    const uint32_t actual_size=(size==0)?static_cast<uint32_t>(buffer->GetSize()-offset):size;
    if(actual_size==0 || actual_size>kMaxBufferBytes) return UINT32_MAX;

    CaptureRequest request;
    request.kind=RequestKind::Buffer;
    request.buffer=buffer;
    request.buffer_offset=offset;
    request.buffer_size=actual_size;
    CopyText(request.name,sizeof(request.name),name);
    return AddRequest(request);
}

uint32_t FrameDebugger::CollectPending()
{
    uint32_t &request_count=RequestCount();
    uint32_t &result_count=ResultCount();
    uint32_t collected=0;

    for(uint32_t i=0;i<request_count;i++)
    {
        const CaptureRequest &request=Requests()[i];
        if(request.kind==RequestKind::None) continue;

        CaptureResult result;
        result.request=request;

        if(request.kind==RequestKind::ColorTarget)
        {
            TextureReadbackInfo info;
            ValueArray<uint8_t> &pixels=PixelScratch();
            if(ReadbackColorTarget(request.render_target,pixels,request.color_index,&info))
            {
                result.valid=true;
                result.width=info.width;
                result.height=info.height;
                result.format=info.format;
                result.pixel_size=info.pixel_size;
                result.byte_count=pixels.GetTotalBytes();
            }
        }
        else if(request.kind==RequestKind::DepthTarget)
        {
            TextureReadbackInfo info;
            ValueArray<uint8_t> &pixels=PixelScratch();
            if(ReadbackDepthTarget(request.render_target,pixels,&info))
            {
                result.valid=true;
                result.width=info.width;
                result.height=info.height;
                result.format=info.format;
                result.pixel_size=info.pixel_size;
                result.byte_count=pixels.GetTotalBytes();

                if(info.pixel_size>=4)
                {
                    const float *values=reinterpret_cast<const float *>(pixels.GetData());
                    const uint32_t count=pixels.GetCount()/4;
                    double sum=0.0;
                    result.min_value=1.0;
                    result.max_value=0.0;
                    for(uint32_t n=0;n<count;n++)
                    {
                        const float value=values[n];
                        if(value<result.min_value) result.min_value=value;
                        if(value>result.max_value) result.max_value=value;
                        sum+=value;
                    }
                    result.average=count?sum/count:0.0;
                }
            }
        }
        else if(request.kind==RequestKind::Buffer)
        {
            uint8_t *bytes=BufferScratch();
            memset(bytes,0,kMaxBufferBytes);
            if(ReadbackBuffer(request.buffer,request.buffer_offset,request.buffer_size,bytes))
            {
                result.valid=true;
                result.byte_count=request.buffer_size;
            }
        }

        if(result.valid)
        {
            if(result_count>=kMaxRequests)
            {
                result_count=0;
            }
            Results()[result_count++]=result;
            collected++;
        }

        Requests()[i].kind=RequestKind::None;
    }

    request_count=0;
    return collected;
}

uint32_t FrameDebugger::GetResultCount()
{
    return ResultCount();
}

const FrameDebugger::CaptureResult *FrameDebugger::GetResult(uint32_t index)
{
    if(index>=ResultCount()) return nullptr;
    return &Results()[index];
}

AnsiString FrameDebugger::GetLatestResultJSON()
{
    const uint32_t count=ResultCount();
    if(count==0) return AnsiString("{\"valid\":false}");
    const CaptureResult &result=Results()[count-1];

    AnsiString json="{\"valid\":";
    json+=result.valid?"true":"false";
    json+=",\"name\":\"";
    json+=result.request.name;
    json+="\",\"kind\":\"";
    switch(result.request.kind)
    {
        case RequestKind::ColorTarget: json+="color_rt"; break;
        case RequestKind::DepthTarget: json+="depth_rt"; break;
        case RequestKind::Buffer: json+="buffer"; break;
        default: json+="unknown"; break;
    }
    json+="\",\"width\":";
    json+=AnsiString::numberOf(result.width);
    json+=",\"height\":";
    json+=AnsiString::numberOf(result.height);
    json+=",\"format\":";
    json+=AnsiString::numberOf(static_cast<int>(result.format));
    json+=",\"bytes\":";
    json+=AnsiString::numberOf(static_cast<uint64_t>(result.byte_count));
    json+="}";
    return json;
}
}//namespace hgl::graph
