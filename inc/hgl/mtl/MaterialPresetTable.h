// MaterialPresetTable.h
//
// DESIGN:
//   每个 MaterialPreset 拥有一张独立的质量梯度小表。
//
//   SurfaceId  — 轻量 enum，标识 surface GLSL 文件。
//               通过 GetSurfacePath(SurfaceId) 得到 ShaderLibrary 相对路径。
//               用 enum 而非裸字符串，便于静态检查和未来拆 JSON。
//
//   PresetQualityEntry { quality_level, render_phase, SurfaceId, vs_override?, fs_override? }
//               一行 = 一个质量档 + render phase。同一 preset/phase 的多行按 quality_level 降序排列。
//
//   MaterialPresetTable::Lookup(preset, quality_level, phase)
//               遍历该 preset+phase 的表，返回 quality_level <= 请求值的最高匹配；
//               若全部条目高于请求值则返回最低质量（末尾）条目。

#pragma once

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/RenderPhase.h>
#include <cstdint>

namespace hgl::graph::mtl {

// ---------------------------------------------------------------------------
// SurfaceId — surface GLSL 文件的枚举标识
// ---------------------------------------------------------------------------
enum class SurfaceId : uint8_t {
    None = 0,

    // Error / fallback
    Checkerboard,

    // Unlit
    PureColor3D,
    VertexColor,
    VertexLuminance,
    UnlitTexture3D,
    Gizmo3D,
    VertexPaletteColor3D,

    // 2D bespoke (vs_override + fs_override 必须填写；surface fn 不使用)
    Text2D,

    // Terrain / Sky
    TerrainGrid,
    SkyMinimal,

    // Lit / PBR
    Standard,
    StandardBlinnPhong,
    StandardPBR,
    PBRColor3D,

    // Fullscreen / PCG
    FragCoord,

    COUNT
};

/// SurfaceId → ShaderLibrary 相对路径；None/Text2D 返回 nullptr
const char* GetSurfacePath(SurfaceId id) noexcept;

// ---------------------------------------------------------------------------
// PresetQualityEntry — 单个质量档条目
// ---------------------------------------------------------------------------
struct PresetQualityEntry {
    uint8_t   quality_level;  ///< 1–10，10 = 最高质量
    RenderPhase phase;        ///< 正交 render phase 维度
    SurfaceId surface;        ///< 该质量档使用的 surface
    const char* vs_override;  ///< 自定义 VS 路径；nullptr = 使用 compositor 默认
    const char* fs_override;  ///< 自定义 FS 路径；nullptr = 使用 compositor 默认
};

// ---------------------------------------------------------------------------
// MaterialPresetTable
// ---------------------------------------------------------------------------
class MaterialPresetTable {
public:
    /// 启动时调用一次，构建诊断计数
    static void Initialize();

    /// 清理
    static void Shutdown();

    /// 返回 (preset, quality_level, phase) 的最佳匹配条目。
    /// 从请求 quality 向下回退，找第一个 quality_level <= 请求值的条目。
    /// 若该 preset/phase 没有任何条目则返回 nullptr。
    static const PresetQualityEntry* Lookup(MaterialPreset preset,
                                            uint8_t quality_level,
                                            RenderPhase phase);

    /// 全部条目总数（诊断用）
    static size_t Count() noexcept;
};

} // namespace hgl::graph::mtl
