#include<hgl/ecs/support/RenderResource.h>
#include<hgl/vk/VKTexture.h>

namespace hgl::ecs
{
    std::string BuildTextureResourceId(const graph::Texture *texture)
    {
        if (!texture)
            return {};

        return "texid:" + std::to_string(texture->GetID());
    }
}
