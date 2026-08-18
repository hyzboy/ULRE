#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/common/ShaderStageDef.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    constexpr uint32 ShaderArtifactFileMagic = 0x554C5350u; // "ULSP"
    constexpr uint32 ShaderArtifactSPVMagic = 0x07230203u;

    constexpr const char ShaderArtifactCacheDirectory[] = "shader-cache";
    constexpr const char ShaderArtifactStageDirectory[] = "stage";
    constexpr const char ShaderArtifactProgramDirectory[] = "program";
    constexpr const char ShaderArtifactSPVExtension[] = ".spv";
    constexpr const char ShaderArtifactMetadataExtension[] = ".meta";
    constexpr uint32 ShaderArtifactFileHeaderSize = 40;
    constexpr uint32 ShaderProgramMetadataPayloadSize =
        sizeof(uint64) * 9;

    enum class ShaderArtifactKind : uint8
    {
        StageSPV = 0,
        ProgramMetadata
    };

    enum class ShaderCacheMode : uint8
    {
        BuildIfMissing = 0,
        ReadOnly
    };

    struct ShaderArtifactFileHeader
    {
        uint32 magic = ShaderArtifactFileMagic;
        ShaderArtifactKind kind = ShaderArtifactKind::StageSPV;
        uint8 reserved = 0;
        uint16 reserved_pad = 0;
        uint32 header_size = ShaderArtifactFileHeaderSize;
        uint32 stage = static_cast<uint32>(ShaderStage::Vertex);
        uint64 key_digest = 0;
        uint64 payload_size = 0;
        uint64 payload_hash = 0;
    };

    struct ShaderProgramArtifactMetadata
    {
        uint64 program_key_digest = 0;
        uint64 resolved_module_graph_hash = 0;
        uint64 shader_interface_hash = 0;
        uint64 output_contract_hash = 0;
        uint64 vertex_stage_digest = 0;
        uint64 fragment_stage_digest = 0;
        uint64 compiler_profile_hash = 0;
        uint64 device_target_hash = 0;
        uint64 generated_source_digest = 0;
    };

    inline bool operator==(
        const ShaderProgramArtifactMetadata &lhs,
        const ShaderProgramArtifactMetadata &rhs) noexcept
    {
        return lhs.program_key_digest == rhs.program_key_digest
            && lhs.resolved_module_graph_hash
                == rhs.resolved_module_graph_hash
            && lhs.shader_interface_hash == rhs.shader_interface_hash
            && lhs.output_contract_hash == rhs.output_contract_hash
            && lhs.vertex_stage_digest == rhs.vertex_stage_digest
            && lhs.fragment_stage_digest == rhs.fragment_stage_digest
            && lhs.compiler_profile_hash == rhs.compiler_profile_hash
            && lhs.device_target_hash == rhs.device_target_hash
            && lhs.generated_source_digest
                == rhs.generated_source_digest;
    }

    inline bool IsValidShaderProgramArtifactMetadata(
        const ShaderProgramArtifactMetadata &metadata) noexcept
    {
        return metadata.program_key_digest != 0
            && metadata.resolved_module_graph_hash != 0
            && metadata.shader_interface_hash != 0
            && metadata.output_contract_hash != 0
            && metadata.vertex_stage_digest != 0
            && metadata.fragment_stage_digest != 0
            && metadata.compiler_profile_hash != 0
            && metadata.device_target_hash != 0
            && metadata.generated_source_digest != 0;
    }

    static_assert(sizeof(ShaderArtifactFileHeader) == ShaderArtifactFileHeaderSize,
                  "Shader artifact header layout must remain stable");

    inline bool IsValidShaderArtifactFileHeader(const ShaderArtifactFileHeader &header) noexcept
    {
        return header.magic == ShaderArtifactFileMagic
            && header.header_size == ShaderArtifactFileHeaderSize;
    }
}
