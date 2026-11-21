#include<hgl/vk/RenderHealthLedger.h>
#include<cstring>
#include<cstdio>

namespace hgl::graph
{
namespace
{
RenderHealthAlert *Alerts()
{
    static RenderHealthAlert alerts[RenderHealthLedger::kMaxAlerts];
    return alerts;
}

uint32_t &AlertCount()
{
    static uint32_t count=0;
    return count;
}

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

const char *SeverityName(RenderHealthSeverity severity)
{
    switch(severity)
    {
        case RenderHealthSeverity::Error:   return "error";
        case RenderHealthSeverity::Warning: return "warning";
        default:                            return "info";
    }
}
}

void RenderHealthLedger::Clear()
{
    for(uint32_t i=0;i<kMaxAlerts;i++) Alerts()[i]=RenderHealthAlert{};
    AlertCount()=0;
}

void RenderHealthLedger::Add(RenderHealthSeverity severity,const char *subsystem,const char *message)
{
    uint32_t &count=AlertCount();
    if(count>=kMaxAlerts) return;
    RenderHealthAlert &alert=Alerts()[count++];
    alert.severity=severity;
    CopyText(alert.subsystem,sizeof(alert.subsystem),subsystem);
    CopyText(alert.message,sizeof(alert.message),message);
}

void RenderHealthLedger::ReportProjection(uint32_t camera_id,float m00,float m11,float near_plane,float far_plane)
{
    const bool bad=((m00<=0.0f)||(m11>=0.0f)||(near_plane>=far_plane));
    if(!bad) return;

    char text[256];
    snprintf(text,sizeof(text),
             "Projection contract violation: camera=%u m00=%.6f m11=%.6f near=%.6f far=%.6f",
             camera_id,m00,m11,near_plane,far_plane);
    Add(RenderHealthSeverity::Error,"Camera",text);
}

void RenderHealthLedger::ReportTextureFallback(const char *material,uint32_t slot,const char *missing_resource)
{
    char text[256];
    snprintf(text,sizeof(text),"Material \"%s\" slot %u missing \"%s\"; bound to fallback",
             material?material:"<unknown>",slot,missing_resource?missing_resource:"<unknown>");
    Add(RenderHealthSeverity::Warning,"TextureManager",text);
}

void RenderHealthLedger::ReportPipelineFailure(const char *pipeline,const char *detail)
{
    char text[256];
    snprintf(text,sizeof(text),"Pipeline \"%s\" failed: %s",
             pipeline?pipeline:"<unknown>",detail?detail:"<unspecified>");
    Add(RenderHealthSeverity::Error,"Pipeline",text);
}

uint32_t RenderHealthLedger::GetCount()
{
    return AlertCount();
}

const RenderHealthAlert *RenderHealthLedger::Get(uint32_t index)
{
    if(index>=AlertCount()) return nullptr;
    return &Alerts()[index];
}

AnsiString RenderHealthLedger::GetStatusJSON()
{
    const uint32_t count=AlertCount();
    RenderHealthSeverity worst=RenderHealthSeverity::Info;
    for(uint32_t i=0;i<count;i++)
    {
        if(Get(i)->severity==RenderHealthSeverity::Error)
            worst=RenderHealthSeverity::Error;
        else if(Get(i)->severity==RenderHealthSeverity::Warning
                && worst!=RenderHealthSeverity::Error)
            worst=RenderHealthSeverity::Warning;
    }

    AnsiString json="{\"status\":\"";
    json+=SeverityName(worst);
    json+="\",\"alerts\":[";

    for(uint32_t i=0;i<count;i++)
    {
        const RenderHealthAlert *a=Get(i);
        if(i) json+=",";
        json+="{\"severity\":\"";
        json+=SeverityName(a->severity);
        json+="\",\"subsystem\":\"";
        json+=a->subsystem;
        json+="\",\"message\":\"";
        json+=a->message;
        json+="\"}";
    }
    json+="]}";
    return json;
}
}//namespace hgl::graph
