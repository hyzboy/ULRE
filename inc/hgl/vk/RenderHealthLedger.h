#pragma once

#include<hgl/type/String.h>

namespace hgl::graph
{
enum class RenderHealthSeverity : uint32_t
{
    Info,
    Warning,
    Error
};

struct RenderHealthAlert
{
    RenderHealthSeverity severity = RenderHealthSeverity::Info;
    char subsystem[32] = {};
    char message[256] = {};
};

class RenderHealthLedger
{
public:
    static constexpr uint32_t kMaxAlerts = 64;

    static void Clear();
    static void Add(RenderHealthSeverity severity,const char *subsystem,const char *message);
    static void ReportProjection(uint32_t camera_id,float m00,float m11,float near_plane,float far_plane);
    static void ReportTextureFallback(const char *material,uint32_t slot,const char *missing_resource);
    static void ReportPipelineFailure(const char *pipeline,const char *detail);

    static uint32_t GetCount();
    static const RenderHealthAlert *Get(uint32_t index);
    static AnsiString GetStatusJSON();
};
}//namespace hgl::graph
