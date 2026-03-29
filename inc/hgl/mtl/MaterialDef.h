#pragma once

#include <hgl/mtl/FixedMaterialDef.h>
#include <hgl/mtl/new/MaterialVariantKey.h>
#include <hgl/mtl/new/SurfaceType.h>
#include <hgl/mtl/new/BlendMode.h>
#include <hgl/mtl/new/PassType.h>
#include <hgl/common/PrimitiveTypeDef.h>
#include <string>
#include <vector>
#include <set>
#include <map>

namespace hgl::graph::mtl
{
    /// MaterialDef — data-driven material definition loaded from a .mat JSON file.
    /// Replaces the FixedMaterialDef + M_*.cpp pattern.
    struct MaterialDef
    {
        std::string name;               // e.g. "PureColor3D"

        SurfaceType  surface_type  = SurfaceType::Unlit;
        GeometryMode geometry_mode = GeometryMode::Mesh3D;
        BlendMode    blend_mode    = BlendMode::Opaque;
        PassType     pass_type     = PassType::ForwardOpaque;

        PrimitiveType primitive_type = PrimitiveType::Triangles;

        // --- Vertex attributes (ordered) ---
        std::vector<FixedVertexEntry> vertex_entries;

        // --- Descriptor resources ---
        std::set<UBODescriptorSemantic>                      ubo_descriptors;
        std::set<SSBODescriptorSemantic>                     ssbo_descriptors;
        std::map<SamplerSlot, FixedTextureSamplerDescriptor> texture_samplers;

        // --- Material Instance ---
        std::string  mi_glsl_struct;        // GLSL field declarations inside the MI struct
        uint32_t     mi_struct_bytes = 0;

        // --- Shader templates (inja, relative to shader root) ---
        std::string vs_template;
        std::string fs_template;

        // --- Feature flags passed to the inja template ---
        std::map<std::string, bool>        bool_features;
        std::map<std::string, int>         int_features;
        std::map<std::string, std::string> string_features;

        bool IsValid() const
        {
            return !name.empty() && !vertex_entries.empty();
        }
    };

} // namespace hgl::graph::mtl
