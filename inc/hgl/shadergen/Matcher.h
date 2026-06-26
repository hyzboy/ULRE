#pragma once

#include <hgl/mtl/MaterialPresetTable.h>
#include <hgl/shadergen/MatchedShaderSet.h>
#include <hgl/mtl/SurfaceType.h>

#include <set>
#include <string>

namespace hgl::graph::mtl
{
    struct MatcherCapabilities
    {
        std::set<std::string> vertex_attribs;
        std::set<std::string> textures;
        std::set<std::string> ubos;
        std::set<std::string> ssbos;
    };

    struct MatcherResolveRequest
    {
        const MaterialPresetTable *preset_table = nullptr;
        const char *shader_library_path = nullptr;

        MaterialPreset preset = MaterialPreset::Checkerboard3D;
        MaterialLOD requested_quality = MaterialLOD::Base;
        RenderPhase phase = RenderPhase::Forward;
        SurfaceType surface_type = SurfaceType::Unlit;

        MatcherCapabilities capabilities{};
    };

    class Matcher
    {
    public:
        static MatchedShaderSet Resolve(const MatcherResolveRequest &request);
    };
}
