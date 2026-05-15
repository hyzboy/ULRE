#pragma once

#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/PassType.h>
#include <hgl/mtl/RenderAlphaMode.h>

namespace hgl::graph::mtl
{
    struct PipelineStateRow
    {
        const char *name = "";
        MaterialPreset preset = MaterialPreset::PureColor3D;
        PrimitiveType primitive = PrimitiveType::Triangles;
        RenderAlphaMode blend = RenderAlphaMode::Opaque;
        PassType pass = PassType::ForwardOpaque;
    };
}
