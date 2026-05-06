#include <hgl/shadergen/internal/CompositorSourceDefines.h>

#include <hgl/shadergen/internal/GLSLSourceUtils.h>

#include <cstdio>

namespace hgl::graph::internal {

std::string InjectCompositorKeyDefines(const std::string &source,
                                       const mtl::MaterialVariantKey &key)
{
    std::string defines;
    {
        char buf[128] = {};
        const uint32 shadow_mode = 0u;
        std::snprintf(buf,
                      sizeof(buf),
                      "#define SURFACE_TYPE %d\n"
                      "#define SHADOW_MODE %u\n",
                      static_cast<int>(key.surface_type),
                      shadow_mode);
        defines += buf;
    }

    if (key.HasAnyTextureMode(mtl::TextureSourceMode::Array))
        defines += "#define TEXTURE_ARRAY_MODE\n";

    if (defines.empty())
        return source;

    return InjectAfterVersion(source, defines);
}

} // namespace hgl::graph::internal
