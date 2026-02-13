#include<hgl/ecs/EnvironmentSystem.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/UBOCommon.h>

namespace hgl::ecs
{
    EnvironmentSystem::EnvironmentSystem(const std::string &name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderPreBeginFrame, ExecutionPriority::First);
    }

    EnvironmentSystem::~EnvironmentSystem()
    {
        delete sky_ubo;
    }

    void EnvironmentSystem::SetRenderFramework(graph::RenderFramework *rf)
    {
        if (render_framework == rf)
            return;

        render_framework = rf;
        EnsureResources();
    }

    graph::SkyInfo *EnvironmentSystem::EditSkyInfo()
    {
        EnsureResources();
        return sky_ubo ? sky_ubo->Data() : nullptr;
    }

    const graph::SkyInfo *EnvironmentSystem::GetSkyInfo() const
    {
        return sky_ubo ? sky_ubo->Data() : nullptr;
    }

    void EnvironmentSystem::SetSkyInfo(const graph::SkyInfo &info, bool immediate)
    {
        EnsureResources();
        if (!sky_ubo)
            return;

        sky_ubo->Update(info);
        if (immediate)
            sky_ubo->Commit();
    }

    void EnvironmentSystem::MarkSkyDirty()
    {
        if (sky_ubo)
            sky_ubo->MarkDirty();
    }

    void EnvironmentSystem::SyncSkyUBO()
    {
        if (sky_ubo && sky_ubo->IsDirty())
            sky_ubo->Commit();
    }

    void EnvironmentSystem::Update(float /*deltaTime*/)
    {
        SyncSkyUBO();
    }

    void EnvironmentSystem::EnsureResources()
    {
        if (!render_framework || sky_ubo)
            return;

        sky_ubo = render_framework->CreateUBO<UBOSkyInfo>(&graph::mtl::SBS_SkyInfo,
                                                          graph::BufferUpdateClass::Deferred);
        if (sky_ubo)
        {
            sky_ubo->Data()->SetTime(10, 0, 0);
            sky_ubo->ImmediateUpdate();
        }
    }
}//namespace hgl::ecs
