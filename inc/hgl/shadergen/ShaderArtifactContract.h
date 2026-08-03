#pragma once

#include <hgl/CoreType.h>
#include <hgl/common/ShaderStageDef.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    constexpr uint32 ShaderArtifactFileMagic = 0x554C5350u; // "ULSP"
    constexpr uint16 ShaderArtifactFileVersion = 1;

    constexpr const char ShaderArtifactCacheDirectory[] = "shader-cache";
    constexpr const char ShaderArtifactStageDirectory[] = "stage";
    constexpr const char ShaderArtifactProgramDirectory[] = "program";
    constexpr const char ShaderArtifactSPVExtension[] = ".spv";
    constexpr const char ShaderArtifactMetadataExtension[] = ".meta";
    constexpr uint32 ShaderArtifactFileHeaderSize = 40;

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
        uint16 version = ShaderArtifactFileVersion;
        ShaderArtifactKind kind = ShaderArtifactKind::StageSPV;
        uint8 reserved = 0;
        uint32 header_size = ShaderArtifactFileHeaderSize;
        uint32 stage = static_cast<uint32>(ShaderStage::Vertex);
        uint64 key_digest = 0;
        uint64 payload_size = 0;
        uint64 payload_hash = 0;
    };

    static_assert(sizeof(ShaderArtifactFileHeader) == ShaderArtifactFileHeaderSize,
                  "Shader artifact header layout must remain stable");

    inline bool IsValidShaderArtifactFileHeader(const ShaderArtifactFileHeader &header) noexcept
    {
        return header.magic == ShaderArtifactFileMagic
            && header.version == ShaderArtifactFileVersion
            && header.header_size == ShaderArtifactFileHeaderSize;
    }
}
