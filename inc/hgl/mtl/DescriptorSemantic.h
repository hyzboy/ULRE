#pragma once

#include <hgl/CoreType.h>

namespace hgl::graph::mtl
{
    enum class DescriptorSemantic : uint8
    {
        Unknown = 0,

        ViewportInfo,
        CameraInfo,
        SkyInfo,

        LocalToWorld,
        LocalToWorldIndexTable,
        MaterialInstance,

        MaterialTexture,
        MaterialSampler,
        MaterialTextureLayerTable,
        MaterialDataIndexTable,

        Custom,
    };
}
