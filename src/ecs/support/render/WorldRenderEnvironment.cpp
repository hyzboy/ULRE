#include <hgl/ecs/support/render/WorldRenderEnvironment.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/VKQueue.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/EnvironmentManager.h>

namespace hgl::ecs
{
    hgl::graph::GraphicsContext *WorldRenderEnvironment::GetGraphicsContext()
    {
        if (!graphics_context && render_context)
            graphics_context = render_context->GetGraphicsContext();

        return graphics_context;
    }

    const hgl::graph::GraphicsContext *WorldRenderEnvironment::GetGraphicsContext() const
    {
        if (graphics_context)
            return graphics_context;

        return render_context ? render_context->GetGraphicsContext() : nullptr;
    }

    hgl::graph::EnvironmentManager *WorldRenderEnvironment::ResolveEnvManager() const
    {
        hgl::graph::GraphicsContext *graphics = nullptr;
        if (render_context)
            graphics = render_context->GetGraphicsContext();
        if (!graphics)
            graphics = const_cast<WorldRenderEnvironment *>(this)->GetGraphicsContext();
        return graphics ? graphics->GetEnvironmentManager() : nullptr;
    }

    bool WorldRenderEnvironment::IsGPUQuiescent() const
    {
        if (!GetGraphicsContext())
            return true;

        auto *queue = render_target ? render_target->GetQueue() : nullptr;
        return queue && queue->IsLastSubmitComplete();
    }

    hgl::graph::EnvProfileID WorldRenderEnvironment::GetEnvProfileID() const
    {
        if (env_profile_explicit)
            return env_profile;

        if (render_target)
            return render_target->GetEnvironmentProfile();

        return hgl::graph::kEnvProfileDefault;
    }

    void WorldRenderEnvironment::ResetEnvProfile()
    {
        env_profile = hgl::graph::kEnvProfileDefault;
        env_profile_explicit = false;
        env_profile_owned = false;
    }

    hgl::graph::EnvProfileID WorldRenderEnvironment::CreateEnvProfile(
        const AnsiString &name, const hgl::graph::EnvironmentInfo &init)
    {
        auto *manager = ResolveEnvManager();
        if (!manager)
            return hgl::graph::kEnvProfileInvalid;

        const hgl::graph::EnvProfileID id = manager->Create(name, init);
        if (id != hgl::graph::kEnvProfileInvalid)
            SetEnvProfileID(id, true);

        return id;
    }

    void WorldRenderEnvironment::SetEnvProfileID(hgl::graph::EnvProfileID id, bool take_ownership)
    {
        if (env_profile_owned && env_profile != id)
            if (auto *manager = ResolveEnvManager())
                manager->Release(env_profile);

        env_profile = id;
        env_profile_explicit = true;
        env_profile_owned = take_ownership;
    }

    void WorldRenderEnvironment::ReleaseOwnedEnvProfile()
    {
        if (env_profile_owned)
            if (auto *manager = ResolveEnvManager())
                manager->Release(env_profile);

        ResetEnvProfile();
    }

    const AnsiString &WorldRenderEnvironment::GetAmbientProviderName() const
    {
        static const AnsiString empty;

        auto *manager = ResolveEnvManager();
        return manager ? manager->GetAmbientProvider(GetEnvProfileID()) : empty;
    }
}
