#pragma once

#include <hgl/shadergen/MaterialCompiler.h>
#include <hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl
{
    inline CompositorMaterialBuildConfig ToCompositorBuildConfigLegacy3D(const Material3DCreateConfig *config,
                                                                          const PrimitiveType default_primitive_type = PrimitiveType::Triangles)
    {
        CompositorMaterialBuildConfig out;
        out.primitive_type = config ? config->prim : default_primitive_type;
        out.shader_stage_flag_bits = config ? config->shader_stage_flag_bit : uint32_t(ShaderStage::VertexFragment);
        out.material_instance = config ? config->material_instance : false;
        out.with_local_to_world = config ? config->local_to_world : false;
        out.with_camera = config ? config->camera : false;
        out.with_sky = config ? config->sky : false;
        out.sky_ambient_model = config ? config->sky_ambient_model : SkyLightAmbientModel::Simple;
        out.private_shader_buffer_sources = config ? config->private_shader_buffer_sources : nullptr;
        out.private_shader_buffer_source_count = config ? config->private_shader_buffer_source_count : 0;
        out.geometry_vertex_format = config ? config->geometry_vertex_format : nullptr;
        return out;
    }
}
