#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    EnvironmentSystem::EnvironmentSystem(const std::string &name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);
    }

    graph::EnvProfileID EnvironmentSystem::ResolveProfileID() const
    {
        if (context)
        {
            if (auto *rt = context->GetRenderTarget())
                return rt->GetEnvironmentProfile();
        }

        return graph::kEnvProfileDefault;
    }

    graph::EnvironmentManager *EnvironmentSystem::ResolveManager()
    {
        if (render_context)
        {
            if (auto *gc = render_context->GetGraphicsContext())
                return gc->GetEnvironmentManager();
        }

        if (context)
        {
            if (auto *rc = context->GetRenderContext())
            {
                if (auto *gc = rc->GetGraphicsContext())
                    return gc->GetEnvironmentManager();
            }

            if (auto *gc = context->GetGraphicsContext())
                return gc->GetEnvironmentManager();
        }

        return nullptr;
    }

    graph::SkyInfo *EnvironmentSystem::EditSkyInfo()
    {
        auto *manager = ResolveManager();
        if (!manager)
            return nullptr;

        auto *info = manager->Edit(ResolveProfileID());
        if (!info)
            return nullptr;

        return &info->sky;
    }

    const graph::SkyInfo *EnvironmentSystem::GetSkyInfo() const
    {
        // Edit 路径非 const（manager 懒物化），GetSkyInfo 只读 CPU 数据，
        // 这里的 const_cast 不触发任何 GPU 操作。
        auto *manager = const_cast<EnvironmentSystem *>(this)->ResolveManager();
        if (!manager)
            return nullptr;

        const auto *info = manager->Get(ResolveProfileID());
        if (!info)
            return nullptr;

        return &info->sky;
    }

    void EnvironmentSystem::SetSkyInfo(const graph::SkyInfo &info, bool immediate)
    {
        auto *manager = ResolveManager();
        if (!manager)
            return;

        auto *env = manager->Edit(ResolveProfileID());
        if (!env)
            return;

        env->sky = info;
        if (immediate)
            manager->MarkDirty(ResolveProfileID());
    }

    void EnvironmentSystem::MarkSkyDirty()
    {
        if (auto *manager = ResolveManager())
            manager->MarkDirty(ResolveProfileID());
    }
}//namespace hgl::ecs
