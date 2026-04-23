#pragma once
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/RenderAlphaMode.h>
#include<hgl/common/TextureSamplerTypeDef.h>

namespace hgl::graph::mtl
{
    struct Sprite2DMaterialCreateConfig : public Material3DCreateConfig
    {
        bool               axis_locked        = false;   ///< true = Sprite2DAxisLocked (pixel-fixed size)
        bool               fixed_size         = false;   ///< alias for axis_locked; kept for API clarity
        bool               use_texture_array  = false;   ///< use sampler2DArray instead of sampler2D
        TextureChannelHint base_color_channel = TextureChannelHint::RGBA;
        RenderAlphaMode    blend_mode         = RenderAlphaMode::Opaque;

    public:

        Sprite2DMaterialCreateConfig()
            : Material3DCreateConfig(PrimitiveType::Triangles, IncludeCamera::With, IncludeL2W::With, IncludeSky::Without)
        {
            kind = ConfigKind::Sprite2D;
        }
    };

} // namespace hgl::graph::mtl
