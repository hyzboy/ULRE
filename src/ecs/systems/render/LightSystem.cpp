#include<hgl/ecs/systems/render/LightSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/LightRowTable.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    LightSystem::LightSystem(const std::string &name)
        : System(name)
    {
        // 与 EnvironmentSystem / MainLightShadowSystem 同阶段：本帧渲染设置之前（光源行在
        // prepass 的阴影解算与主帧绘制之前写妥）。
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);
    }

    graph::LightRow *LightSystem::EditMainLight()
    {
        if (!context)
            return nullptr;

        return context->GetLightRowTable().Row(graph::kMainLightRowIndex);
    }

    const graph::LightRow *LightSystem::GetMainLight() const
    {
        if (!context)
            return nullptr;

        return context->GetLightRowTable().Row(graph::kMainLightRowIndex);
    }

    void LightSystem::SetMainLight(const graph::LightRow &row)
    {
        if (!context)
            return;

        context->GetLightRowTable().SetRow(graph::kMainLightRowIndex, row);
    }

    void LightSystem::SetMainLightDirection(const math::Vector3f &to_light)
    {
        const graph::LightRow *row = EditMainLight();
        if (!row)
            return;

        // 逐字段改写（不是整体替换）：调用方只想改方向时，颜色/强度/角径/阴影开关必须原样保留 ——
        // 「先读行、改一项、再整行写回」是这里唯一的写姿势（值变才标脏由 SetRow 判定）。
        graph::LightRow next = *row;
        next.direction_intensity.x = to_light.x;
        next.direction_intensity.y = to_light.y;
        next.direction_intensity.z = to_light.z;
        SetMainLight(next);
    }

    void LightSystem::SetMainLightColor(const Color4f &color)
    {
        const graph::LightRow *row = EditMainLight();
        if (!row)
            return;

        graph::LightRow next = *row;
        next.color_angle.x = color.r;
        next.color_angle.y = color.g;
        next.color_angle.z = color.b;
        SetMainLight(next);
    }

    void LightSystem::SetMainLightIntensity(float intensity)
    {
        const graph::LightRow *row = EditMainLight();
        if (!row)
            return;

        graph::LightRow next = *row;
        next.direction_intensity.w = intensity;
        SetMainLight(next);
    }

    void LightSystem::SetMainLightAngleDeg(float angle_deg)
    {
        const graph::LightRow *row = EditMainLight();
        if (!row)
            return;

        graph::LightRow next = *row;
        next.color_angle.w = angle_deg;
        SetMainLight(next);
    }

    void LightSystem::SetMainLightTime(float hour, float minute, float second)
    {
        const graph::LightRow *row = EditMainLight();
        if (!row)
            return;

        const graph::SunLightSample sun = graph::EvaluateSunLight(hour, minute, second);

        graph::LightRow next = *row;
        next.direction_intensity = math::Vector4f(sun.direction.x, sun.direction.y, sun.direction.z, sun.intensity);
        next.color_angle.x = sun.color.r;
        next.color_angle.y = sun.color.g;
        next.color_angle.z = sun.color.b;
        // 角径（color_angle.w）不动：旧 SkyInfo::SetTime 同样不碰 sun_ang_deg
        SetMainLight(next);
    }
}//namespace hgl::ecs
