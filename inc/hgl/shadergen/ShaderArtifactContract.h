#pragma once

#include <hgl/CoreType.h>
#include <hgl/common/ShaderStageDef.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    constexpr uint32 ShaderArtifactFileMagic = 0x554C5350u; // "ULSP"
    constexpr uint16 ShaderArtifactFileVersion = 1;

    constexpr const char ShaderArtifactCacheDirectory[] = "shader-cache";
    constexpr const char ShaderArtifactShadowCacheDirectory[] = "shadow-v1";
    constexpr const char ShaderArtifactContractCacheDirectory[] = "contract-v1";
    constexpr const char ShaderArtifactStageDirectory[] = "stage";
    constexpr const char ShaderArtifactProgramDirectory[] = "program";
    constexpr const char ShaderArtifactSPVExtension[] = ".spv";
    constexpr const char ShaderArtifactMetadataExtension[] = ".meta";
    constexpr uint32 ShaderArtifactFileHeaderSize = 40;
    constexpr uint32 ShaderProgramMetadataSchemaVersion = 1;

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

    enum class ShaderArtifactCacheNamespace : uint8
    {
        Legacy = 0,
        ShadowV1,
        ContractV1
    };

    inline bool IsValidShaderArtifactCacheNamespace(
        const ShaderArtifactCacheNamespace cache_namespace) noexcept
    {
        return cache_namespace == ShaderArtifactCacheNamespace::Legacy
            || cache_namespace == ShaderArtifactCacheNamespace::ShadowV1
            || cache_namespace == ShaderArtifactCacheNamespace::ContractV1;
    }

    inline const char *GetShaderArtifactCacheNamespaceDirectory(
        const ShaderArtifactCacheNamespace cache_namespace) noexcept
    {
        switch (cache_namespace)
        {
        case ShaderArtifactCacheNamespace::Legacy:
            return nullptr;
        case ShaderArtifactCacheNamespace::ShadowV1:
            return ShaderArtifactShadowCacheDirectory;
        case ShaderArtifactCacheNamespace::ContractV1:
            return ShaderArtifactContractCacheDirectory;
        }

        return nullptr;
    }

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

    struct ShaderProgramArtifactMetadata
    {
        uint32 schema_version = ShaderProgramMetadataSchemaVersion;
        uint64 program_key_digest = 0;
        uint64 effective_material_program_digest = 0;
        uint64 shader_variant_digest = 0;
        uint64 resolved_module_graph_hash = 0;
        uint64 shader_interface_hash = 0;
        uint64 output_contract_hash = 0;
        uint64 vertex_stage_digest = 0;
        uint64 fragment_stage_digest = 0;
        uint64 compiler_profile_hash = 0;
        uint64 device_target_hash = 0;
        uint64 generated_source_digest = 0;
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
