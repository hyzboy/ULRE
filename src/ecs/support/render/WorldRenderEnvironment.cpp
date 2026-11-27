#include <hgl/ecs/support/render/WorldRenderEnvironment.h>
#include <hgl/vk/VKRenderTarget.h>

namespace hgl::ecs
{
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

    void WorldRenderEnvironment::SetEnvProfile(hgl::graph::EnvProfileID id, bool take_ownership)
    {
        env_profile = id;
        env_profile_explicit = true;
        env_profile_owned = take_ownership;
    }
}
