#include <hgl/shadergen/ShaderProgramArtifactBuilder.h>

#include <hgl/mtl/ShaderResourceSchema.h>
#include <hgl/shadergen/ShaderCreateInfo.h>
#include <hgl/shadergen/ShaderKeyUtility.h>
#include <hgl/shadergen/ShaderBuildContext.h>
#include <hgl/shadergen/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    bool BuildShaderProgramArtifactMetadata(
        const contract::PhysicalDeviceProfileLite *profile,
        const ShaderBuildContext &build_spec,
        ShaderProgramArtifactMetadata &out_metadata) noexcept
    {
        out_metadata = {};
        if (!build_spec.HasProgramLink())
            return false;

        const ShaderCreateInfo *vertex =
            build_spec.GetStageShader(ShaderStage::Vertex);
        const ShaderCreateInfo *fragment =
            build_spec.GetStageShader(ShaderStage::Fragment);
        if (!vertex || !fragment
         || vertex->GetFinalGLSL().empty()
         || fragment->GetFinalGLSL().empty())
            return false;

        const ShaderLinkSpec &link =
            build_spec.GetProgramLink();
        if (link.compiler_hash
            != contract::GetShaderCompilerProfileHash(profile))
            return false;

        const uint64 vertex_source_hash = HashFinalShaderSource(
            vertex->GetFinalGLSL().data(),
            vertex->GetFinalGLSL().size());
        const uint64 fragment_source_hash = HashFinalShaderSource(
            fragment->GetFinalGLSL().data(),
            fragment->GetFinalGLSL().size());

        hgl::hash::FNV1aHasher64 module_graph_hasher;
        module_graph_hasher << link.vertex_stage.glsl_module_graph_hash
                            << link.fragment_stage.glsl_module_graph_hash;
        const uint64 module_graph_hash = module_graph_hasher;

        hgl::hash::FNV1aHasher64 interface_hasher;
        interface_hasher << link.vertex_stage.interface_hash
                         << link.fragment_stage.interface_hash
                         << HashShaderResourceSchema(
                             build_spec.GetShaderResourceSchema());
        const uint64 interface_hash = interface_hasher;

        const uint64 program_digest = link.BuildKey().GetDigest();
        hgl::hash::FNV1aHasher64 source_hasher;
        source_hasher << vertex_source_hash
                      << fragment_source_hash;
        const uint64 source_digest = source_hasher;

        out_metadata.program_key_digest = program_digest;
        out_metadata.resolved_module_graph_hash = module_graph_hash;
        out_metadata.shader_interface_hash = interface_hash;
        out_metadata.output_contract_hash = link.render_target_hash;
        out_metadata.vertex_stage_digest =
            link.vertex_stage.GetDigest();
        out_metadata.fragment_stage_digest =
            link.fragment_stage.GetDigest();
        out_metadata.compiler_profile_hash = link.compiler_hash;
        out_metadata.device_target_hash =
            contract::GetShaderCompilerProfileHash(profile);
        out_metadata.generated_source_digest = source_digest;
        return IsValidShaderProgramArtifactMetadata(out_metadata);
    }
}
