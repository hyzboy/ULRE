#include <hgl/mtl/PipelineStateRow.h>

namespace hgl::graph::mtl
{
    const PipelineStateRow kPipelineStateRows[] = {
        { "PS_OpaqueTriangles", MaterialPreset::PureColor3D, PrimitiveType::Triangles, RenderAlphaMode::Opaque, PassType::ForwardOpaque },
        { "PS_TransparentTriangles", MaterialPreset::PureColor3D, PrimitiveType::Triangles, RenderAlphaMode::Transparent, PassType::ForwardTransparent },
        { "PS_MaskedTriangles", MaterialPreset::PureColor3D, PrimitiveType::Triangles, RenderAlphaMode::Masked, PassType::ForwardOpaque },
        { "PS_TextAlpha", MaterialPreset::Text2D, PrimitiveType::Triangles, RenderAlphaMode::Transparent, PassType::ForwardTransparent },
    };

    const size_t kPipelineStateRowsCount = sizeof(kPipelineStateRows) / sizeof(kPipelineStateRows[0]);
}
