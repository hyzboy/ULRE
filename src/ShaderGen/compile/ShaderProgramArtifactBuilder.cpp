#include <hgl/mtl/ShaderProgramArtifactBuilder.h>

#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderKeyUtility.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    uint64 ComputeGeneratedSourceDigest(
        const char *mesh_glsl,
        const size_t mesh_size,
        const char *fragment_glsl,
        const size_t fragment_size) noexcept
    {
        if (!mesh_glsl || !fragment_glsl
         || mesh_size == 0 || fragment_size == 0)
            return 0;

        hgl::hash::FNV1aHasher64 source_hasher;
        source_hasher << HashFinalShaderSource(mesh_glsl, mesh_size)
                      << HashFinalShaderSource(fragment_glsl, fragment_size);
        return source_hasher;
    }

    bool MatchesGeneratedSourceDigest(
        const char *mesh_glsl,
        const size_t mesh_size,
        const char *fragment_glsl,
        const size_t fragment_size,
        const uint64 stored_source_digest) noexcept
    {
        const uint64 fresh_source_digest = ComputeGeneratedSourceDigest(
            mesh_glsl, mesh_size, fragment_glsl, fragment_size);
        return fresh_source_digest != 0
            && fresh_source_digest == stored_source_digest;
    }

    bool BuildShaderProgramArtifactMetadata(
        const contract::PhysicalDeviceProfileLite *profile,
        const ShaderBuildContext &build_spec,
        ShaderProgramArtifactMetadata &out_metadata) noexcept
    {
        out_metadata = {};
        if (!build_spec.HasProgramLink())
            return false;

        // mesh shader 材质（VS 已彻底废弃）：顶点处理 stage 是 Mesh
        const ShaderCreateInfo *vertex =
            build_spec.GetStageShader(ShaderStage::Mesh);
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

        const uint64 source_digest = ComputeGeneratedSourceDigest(
            vertex->GetFinalGLSL().data(),
            vertex->GetFinalGLSL().size(),
            fragment->GetFinalGLSL().data(),
            fragment->GetFinalGLSL().size());
        if (source_digest == 0)
            return false;

        hgl::hash::FNV1aHasher64 module_graph_hasher;
        module_graph_hasher << link.mesh_stage.glsl_module_graph_hash
                            << link.fragment_stage.glsl_module_graph_hash;
        const uint64 module_graph_hash = module_graph_hasher;

        hgl::hash::FNV1aHasher64 interface_hasher;
        interface_hasher << link.mesh_stage.interface_hash
                         << link.fragment_stage.interface_hash
                         // 2026-10-11（S1）：原资源 schema 哈希 → 其唯一活字段。
                         // 取值变化属预期（artifact / SPV 缓存一次性重建）。
                         << build_spec.RequiresRecipeRuntimeRows();
        const uint64 interface_hash = interface_hasher;

        const uint64 program_digest = link.BuildKey().GetDigest();

        out_metadata.program_key_digest = program_digest;
        out_metadata.resolved_module_graph_hash = module_graph_hash;
        out_metadata.shader_interface_hash = interface_hash;
        out_metadata.output_contract_hash = link.render_target_hash;
        out_metadata.mesh_stage_digest =
            link.mesh_stage.GetDigest();
        out_metadata.fragment_stage_digest =
            link.fragment_stage.GetDigest();
        out_metadata.compiler_profile_hash = link.compiler_hash;
        out_metadata.device_target_hash =
            contract::GetShaderCompilerProfileHash(profile);
        out_metadata.generated_source_digest = source_digest;
        return IsValidShaderProgramArtifactMetadata(out_metadata);
    }
}
