#pragma once

#include <hgl/mtl/FixedMaterialDef.h>
#include <hgl/common/RenderAssignDef.h>
#include <vector>

namespace hgl::graph::mtl::vertex_builder_common
{

enum class VertexTransformIntent : uint8_t
{
    None = 0,
    LocalToWorld,
};

struct VertexSemanticDecl
{
    VertexSemantic semantic = VertexSemantic::Position;
    VkFormat fallback_format = VK_FORMAT_UNDEFINED;
    bool format_is_fixed = false;
    bool use_position_resolver = false;
};

struct VertexBuildInput
{
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    VertexTransformIntent transform_intent = VertexTransformIntent::None;
    const GeometryVertexFormat *geometry_vertex_format = nullptr;
    const VertexSemanticDecl *semantic_decls = nullptr;
    uint32 semantic_decl_count = 0;
};

inline VkFormat ResolveVertexFormat(const GeometryVertexFormat *geometry_vertex_format,
                                    const VertexSemanticDecl &decl)
{
    if (decl.format_is_fixed)
        return decl.fallback_format;

    if (decl.use_position_resolver)
        return ResolveMaterialPositionFormat(geometry_vertex_format, decl.fallback_format);

    return ResolveMaterialVertexSemanticFormat(geometry_vertex_format, decl.semantic, decl.fallback_format);
}

inline void BuildVertexEntries(std::vector<FixedVertexEntry> &out,
                               const VertexBuildInput &input)
{
    if (!input.semantic_decls || input.semantic_decl_count == 0)
        return;

    out.reserve(out.size() + input.semantic_decl_count);

    for (uint32 i = 0; i < input.semantic_decl_count; ++i)
    {
        const VertexSemanticDecl &decl = input.semantic_decls[i];
        out.push_back({ ResolveVertexFormat(input.geometry_vertex_format, decl), decl.semantic });
    }
}

inline std::vector<FixedVertexEntry> BuildVertexEntries(const VertexBuildInput &input)
{
    std::vector<FixedVertexEntry> entries;
    BuildVertexEntries(entries, input);
    return entries;
}

} // namespace hgl::graph::mtl::vertex_builder_common
