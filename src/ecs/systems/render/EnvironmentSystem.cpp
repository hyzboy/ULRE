#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    EnvironmentSystem::EnvironmentSystem(const std::string &name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);
    }

    EnvironmentSystem::~EnvironmentSystem() = default;

    graph::EnvProfileID EnvironmentSystem::ResolveProfileID() const
    {
        // Env 随世界（C2）：profile 由**世界**持有（世界初始化时取本世界 RT 的 env_profile，
        // 也可由世界显式 SetEnvProfileID / CreateEnvProfile）——不再每次去问 RT。
        if (context)
            return context->GetEnvProfileID();

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

    // ── 环境光 provider 槽（`ambient_light_provider`）──
    //  ⚠ 原 `sky` 三件套（EditSkyInfo/GetSkyInfo/SetSkyInfo/MarkSkyDirty）已于 2026-10-07
    //  随 sky 段整删（两个字段全仓无写者 ⇒ 取值搬成 shader 常量；`addr_sky` 退出世界表）。

    graph::AmbientEnvInfo *EnvironmentSystem::EditAmbientEnvInfo()
    {
        auto *manager = ResolveManager();
        if (!manager)
            return nullptr;

        auto *info = manager->Edit(ResolveProfileID());
        if (!info)
            return nullptr;

        return &info->ambient_env;
    }

    void EnvironmentSystem::MarkAmbientEnvDirty()
    {
        if (auto *manager = ResolveManager())
            manager->MarkDirty(ResolveProfileID());
    }

    // ── ambient provider 模块选择（算法轴）──
    //  与 `EditAmbientEnvInfo()`（数据轴）配对：环境决定"用哪个算法 + 喂什么数据"。
    bool EnvironmentSystem::SetAmbientProvider(const AnsiString &module_name)
    {
        auto *manager = ResolveManager();
        if (!manager)
            return false;

        return manager->SetAmbientProvider(ResolveProfileID(), module_name);
    }

    const AnsiString &EnvironmentSystem::GetAmbientProvider() const
    {
        static const AnsiString empty;

        auto *manager = const_cast<EnvironmentSystem *>(this)->ResolveManager();
        if (!manager)
            return empty;

        return manager->GetAmbientProvider(ResolveProfileID());
    }
}//namespace hgl::ecs
