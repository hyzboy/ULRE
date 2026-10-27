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

    uint64 ComputeGeneratedStageSourceDigest(
        const char *glsl,
        const size_t glsl_size) noexcept
    {
        if (!glsl || glsl_size == 0)
            return 0;

        return HashFinalShaderSource(glsl, glsl_size);
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
        // 编译配置不属于键的身份，但仍是元数据可写的前提：拿不到本 profile 的
        // 编译器摘要就说明 stage 键不是为这台设备算的。
        if (link.mesh_stage.compiler_hash
            != contract::GetShaderCompilerProfileHash(profile))
            return false;

        const uint64 source_digest = ComputeGeneratedSourceDigest(
            vertex->GetFinalGLSL().data(),
            vertex->GetFinalGLSL().size(),
            fragment->GetFinalGLSL().data(),
            fragment->GetFinalGLSL().size());
        if (source_digest == 0)
            return false;

        // 只剩两项：程序键摘要 + 生成源摘要。
        //
        // 原来这里还落 7 个派生字段（模块图/接口/输出契约/stage 摘要/编译器/设备
        // 目标），全部是由 `link` 与 `profile` 现算出来的。落盘它们没有增加任何
        // 判据强度——事后校验只能比对程序键摘要，其余 6 项要么恒等于输入的
        // 纯函数值、要么（如 output_contract_hash）已被程序键传递性覆盖。
        out_metadata.program_key_digest = link.BuildKey().GetDigest();
        out_metadata.generated_source_digest = source_digest;
        return IsValidShaderProgramArtifactMetadata(out_metadata);
    }
}
