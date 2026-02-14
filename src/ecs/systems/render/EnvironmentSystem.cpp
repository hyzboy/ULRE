#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/render/RenderContext.h>
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
    }

    void EnvironmentSystem::EnsureResources()
    {
        if (sky_ubo)
            return;

        if (!render_context && context)
            render_context = context->GetRenderContext();

        auto *device = render_context ? render_context->GetDevice() : nullptr;
        if (!device && context)
            device = context->GetGPUDevice();

        if (!device)
            return;

        sky_ubo = device->CreateUBO<UBOSkyInfo>(&graph::mtl::SBS_SkyInfo,
                                                graph::BufferUpdateClass::Deferred);
        if (sky_ubo)
        {
            sky_ubo->Data()->SetTime(10, 0, 0);
            sky_ubo->ImmediateUpdate();
        }
    }
}//namespace hgl::ecs

