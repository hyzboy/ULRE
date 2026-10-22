#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/common/ShaderStageDef.h>
#include <cstdint>

namespace hgl::graph::mtl
{
    constexpr uint32 ShaderArtifactFileMagic = 0x554C5350u; // "ULSP"
    constexpr uint32 ShaderArtifactSPVMagic = 0x07230203u;

    constexpr const char ShaderArtifactCacheDirectory[] = "shader-cache";
    constexpr const char ShaderArtifactStageDirectory[] = "stage";
    constexpr const char ShaderArtifactProgramDirectory[] = "program";
    constexpr const char ShaderArtifactSPVExtension[] = ".spv";
    constexpr const char ShaderArtifactMetadataExtension[] = ".meta";

    // ── Shader program manifest（发行形态的查表入口）──
    // 发行形态不带生成器 ⇒ 算不出 ShaderProgramKey（它由 GLSL 声明闭包派生）。
    // Manifest 记录「轻量语义键 → program link」，让运行时用纯数据维度查表拿到
    // 键与各 stage SPV，全程不碰 GLSL。文件直接放在缓存根目录下、与 stage/
    // program 目录平级。
    constexpr const char ShaderArtifactManifestFilename[] = "program_manifest.pmf";

    // ── 生成源（最终 GLSL）落盘扩展名按 shader stage 定（GLSL 命名惯例）──
    // mesh shader → .mesh；fragment shader → .frag；未来的 compute shader → .comp。
    // 源文件与对应 stage SPV 缓存文件共用同一主文件名（同目录，仅扩展名不同）。
    struct ShaderArtifactStageSourceExtensionEntry
    {
        ShaderStage stage;
        const char *extension;
    };

    constexpr ShaderArtifactStageSourceExtensionEntry kShaderArtifactStageSourceExtensionTable[] =
    {
        { ShaderStage::Mesh,     ".mesh" },
        { ShaderStage::Fragment, ".frag" },
        { ShaderStage::Compute,  ".comp" },    ///< 引擎暂无 compute shader；行先备好，加入即生效
    };

    inline const char *GetShaderArtifactStageSourceExtension(const ShaderStage stage) noexcept
    {
        for (const auto &row : kShaderArtifactStageSourceExtensionTable)
            if (row.stage == stage)
                return row.extension;
        return "";
    }
    constexpr uint32 ShaderArtifactFileHeaderSize = 48;
    constexpr uint32 ShaderProgramMetadataPayloadSize =
        sizeof(uint64) * 9;

    enum class ShaderArtifactKind : uint8
    {
        StageSPV = 0,
        ProgramMetadata,
        /// 语义键 → program link 的查表清单（整个文件一个 artifact）。
        /// header.key_digest = compiler_profile_hash，header.source_digest =
        /// codegen generation —— 用既有 header 字段承载这两枚章，避免 payload 里重复。
        ProgramManifest
    };

    /// manifest payload 布局（小端，逐字段手写序列化，不 memcpy 结构体）：
    ///   uint32 format_version
    ///   uint32 entry_count
    ///   entry[entry_count]
    /// entry 布局（132 字节）：
    ///   uint64 lookup_key
    ///   stage  mesh     : uint32 stage + uint64[5]
    ///   stage  fragment : uint32 stage + uint64[5]
    ///   uint64 resource_layout_hash
    ///   uint64 vertex_input_hash
    ///   uint64 render_target_hash
    ///   uint64 compiler_hash
    ///   uint32 flags（bit0 = requires_recipe_runtime_rows）
    constexpr uint32 ShaderProgramManifestFormatVersion = 1;
    constexpr uint32 ShaderProgramManifestEntryPayloadSize =
        sizeof(uint64)
      + (sizeof(uint32) + sizeof(uint64) * 5)
      + (sizeof(uint32) + sizeof(uint64) * 5)
      + sizeof(uint64) * 4
      + sizeof(uint32);
    constexpr uint32 ShaderProgramManifestFlagRequiresRecipeRows = 0x1u;

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
        uint32 stage = static_cast<uint32>(ShaderStage::Mesh);
        uint64 key_digest = 0;
        uint64 payload_size = 0;
        uint64 payload_hash = 0;
        // StageSPV source identity is recorded per artifact; ProgramMetadata leaves this zero.
        uint64 source_digest = 0;
    };

    struct ShaderProgramArtifactMetadata
    {
        uint64 program_key_digest = 0;
        uint64 resolved_module_graph_hash = 0;
        uint64 shader_interface_hash = 0;
        uint64 output_contract_hash = 0;
        uint64 mesh_stage_digest = 0;
        uint64 fragment_stage_digest = 0;
        uint64 compiler_profile_hash = 0;
        uint64 device_target_hash = 0;
        uint64 generated_source_digest = 0;
    };

    /// 缓存命中判定：只比较"能由声明源算出"的字段。
    ///
    /// generated_source_digest 刻意不参与 program metadata 判定；stage
    /// source identity is checked against the independent stage artifact header.
    inline bool operator==(
        const ShaderProgramArtifactMetadata &lhs,
        const ShaderProgramArtifactMetadata &rhs) noexcept
    {
        return lhs.program_key_digest == rhs.program_key_digest
            && lhs.resolved_module_graph_hash
                == rhs.resolved_module_graph_hash
            && lhs.shader_interface_hash == rhs.shader_interface_hash
            && lhs.output_contract_hash == rhs.output_contract_hash
            && lhs.mesh_stage_digest == rhs.mesh_stage_digest
            && lhs.fragment_stage_digest == rhs.fragment_stage_digest
            && lhs.compiler_profile_hash == rhs.compiler_profile_hash
            && lhs.device_target_hash == rhs.device_target_hash;
    }

    inline bool IsValidShaderProgramArtifactMetadata(
        const ShaderProgramArtifactMetadata &metadata) noexcept
    {
        return metadata.program_key_digest != 0
            && metadata.resolved_module_graph_hash != 0
            && metadata.shader_interface_hash != 0
            && metadata.output_contract_hash != 0
            && metadata.mesh_stage_digest != 0
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
