#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/StructuredBufferAccessor.h>

namespace hgl::ecs
{
    EnvironmentSystem::EnvironmentSystem(const std::string &name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::RenderPreBeginFrame, ExecutionPriority::First);
    }

    EnvironmentSystem::~EnvironmentSystem()
    {
        if (sky_ubo)
        {
            graph::DeviceBuffer *buf = sky_ubo->ubo();
            delete sky_ubo;
            sky_ubo = nullptr;

            if (sky_ubo_managed && buf)
            {
                graph::BufferManager *buffer_manager = nullptr;
                if (render_context)
                {
                    if (auto *gc = render_context->GetGraphicsContext())
                        buffer_manager = gc->GetBufferManager();
                }
                if (!buffer_manager && context)
                {
                    if (auto *gc = context->GetGraphicsContext())
                        buffer_manager = gc->GetBufferManager();
                }

                if (buffer_manager)
                    buffer_manager->Release(buf);
            }
            sky_ubo_managed = false;
        }
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

        auto *graphics_context = context ? context->GetGraphicsContext() : nullptr;
        if (!graphics_context && render_context)
            graphics_context = render_context->GetGraphicsContext();

        if (!render_context && !graphics_context)
            return;

        if (graphics_context)
        {
            auto *buffer_manager = graphics_context->GetBufferManager();
            if (buffer_manager)
            {
                auto *buf = buffer_manager->CreateUBO("SkyUBO", graph::StructuredBufferAccessor<graph::SkyInfo>::GetSize());
                if (buf)
                {
                    buf->SetUpdateClass(graph::BufferUpdateClass::Deferred);
                    sky_ubo = graph::StructuredBufferAccessor<graph::SkyInfo>::Create(buf, &graph::mtl::SBS_SkyInfo, false);
                }
            }
        }

        if (sky_ubo)
            sky_ubo_managed = true;
        if (sky_ubo)
        {
            sky_ubo->Data()->SetTime(10, 0, 0);
            sky_ubo->ImmediateUpdate();
        }
    }
}//namespace hgl::ecs

