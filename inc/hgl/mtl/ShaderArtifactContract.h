#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/asset/AssetAnnotation.h>
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

    // ── Compute program manifest ──
    // compute 程序（如 GPU cull）没有材质定义，也没有 mesh/fragment 两段 link，
    // 塞不进上面那份清单的定长记录里。它只有「程序名 → 一段 compute SPV」一条关系，
    // 因此单独一份文件，格式机制完全相同（同一套 header / stamp / 校验和 +
    // AssetGen 反射表驱动的 FlatBuffers 载荷），且与材质清单互不影响 ——
    // 一份损坏不会污染另一份。
    constexpr const char ShaderArtifactComputeManifestFilename[] = "compute_manifest.pmf";

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
        { ShaderStage::Compute,  ".comp" },    ///< compute 程序（GPU cull）走 compute_manifest 预烘焙 SPV，生成源暂不落盘；行先备好
    };

    inline const char *GetShaderArtifactStageSourceExtension(const ShaderStage stage) noexcept
    {
        for (const auto &row : kShaderArtifactStageSourceExtensionTable)
            if (row.stage == stage)
                return row.extension;
        return "";
    }
    constexpr uint32 ShaderArtifactFileHeaderSize = 48;
    // program metadata 载荷不再是「2 × uint64 裸字节」：v2 起走 AssetGen 反射表
    // 驱动的 FlatBuffers（ShaderProgramArtifactMetadata 本身即载荷结构），
    // 尺寸随序列化后端变化，不再有公开定长常量。

    enum class ShaderArtifactKind : uint8
    {
        StageSPV = 0,
        ProgramMetadata,
        /// 语义键 → program link 的查表清单（整个文件一个 artifact）。
        /// header.key_digest = compiler_profile_hash，header.source_digest =
        /// codegen generation —— 用既有 header 字段承载这两枚章，避免 payload 里重复。
        ProgramManifest,
        /// 程序名 → compute stage 的查表清单（整个文件一个 artifact）。
        /// stamp 语义与 ProgramManifest 相同。
        ComputeProgramManifest
    };

    /// manifest payload = AssetGen `[group.ShaderCache]` 生成的 FlatBuffers 载荷
    /// （`ShaderProgramManifestPayload`：`format_version` + 条目表；后端在
    /// `hgl/serialize/FlatCodec.h`）。外层帧不变：`ShaderArtifactFileHeader` 的
    /// magic/kind/尺寸/`payload_hash`（FNV1a 覆盖整段 FlatBuffers 缓冲）。
    ///
    /// v2：stage 键 5 → 4 个哈希（`resource_hash` 折进 `compile_inputs_hash`）、
    ///     4 个 link 哈希收成 1 个 `link_contract_hash`。
    /// v3：载荷从「小端定长记录手写编解码」换成生成反射表驱动的 FlatBuffers ——
    ///     字段真源回到结构体本身，不再有第二份布局描述。旧版整份拒收、重烘焙。
    constexpr uint32 ShaderProgramManifestFormatVersion = 3;

    /// compute manifest payload：同 v3，FlatBuffers 生成的
    /// `ShaderComputeProgramManifestPayload`（真源见
    /// `hgl/mtl/ShaderComputeProgramManifest.h`）。
    constexpr uint32 ShaderComputeProgramManifestFormatVersion = 3;

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

    /// program artifact 的落盘元数据。
    ///
    /// 缓存命中判定要的是「这份 SPV 是不是我这个 link 的产物」，而 link 的全部
    /// 身份就是 `link.BuildKey()`。所以这里只留两项：program 键摘要，和事后校验
    /// 用的 `generated_source_digest`。
    ///
    /// 早期版本把 program 键的每个分量（module graph / interface / output contract /
    /// 两个 stage digest / compiler profile / device target）逐一平铺成 9 个字段，
    /// 逐个序列化、逐个比对。其中 `device_target_hash` 与 `compiler_profile_hash`
    /// 是同一个值（都来自 `GetShaderCompilerProfileHash`），其余 6 个都是 stage 键
    /// 的重复投影——一致性检查的真正来源只有一个：键本身。平铺只会制造「两个字段
    /// 该不该同步」的维护负担。
    struct HGL_ASSET_STRUCT ShaderProgramArtifactMetadata
    {
        uint64 program_key_digest = 0;
        /// 命中后的事后校验：把落盘 SPV 反查回源文本摘要（不参与命中判定）。
        uint64 generated_source_digest = 0;
    };

    /// 缓存命中判定：只比较 program 键摘要。
    ///
    /// generated_source_digest 刻意不参与判定，它由 stage artifact 头独立承担
    /// 「这份 SPV 是不是这段源编出来的」的证明。
    inline bool operator==(
        const ShaderProgramArtifactMetadata &lhs,
        const ShaderProgramArtifactMetadata &rhs) noexcept
    {
        return lhs.program_key_digest == rhs.program_key_digest;
    }

    inline bool IsValidShaderProgramArtifactMetadata(
        const ShaderProgramArtifactMetadata &metadata) noexcept
    {
        return metadata.program_key_digest != 0
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
