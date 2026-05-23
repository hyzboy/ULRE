// MaterialPresetTable.cpp
//
// 每个 preset 一张独立质量梯度表，surface 用 SurfaceId enum 引用。
// 未来拆 JSON：每张 kTable_Xxx 对应一个 "Xxx.preset.json"。

#include <hgl/mtl/MaterialPresetTable.h>
#include <cstdio>

namespace hgl::graph::mtl {

namespace {

constexpr RenderPhase kForwardOpaque = RenderPhase::ForwardOpaque;

} // namespace

// ---------------------------------------------------------------------------
// SurfaceId → glsl path
// ---------------------------------------------------------------------------
const char* GetSurfacePath(SurfaceId id) noexcept
{
    switch (id)
    {
        case SurfaceId::Checkerboard:           return "surface/checkerboard_surface.glsl";
        case SurfaceId::PureColor3D:            return "surface/purecolor3d_surface.glsl";
        case SurfaceId::VertexColor:            return "surface/unlit_vertexcolor_surface.glsl";
        case SurfaceId::VertexLuminance:        return "surface/unlit_luminance_surface.glsl";
        case SurfaceId::UnlitTexture3D:         return "surface/unlit_texture3d_surface.glsl";
        case SurfaceId::Gizmo3D:                return "surface/gizmo3d_surface.glsl";
        case SurfaceId::VertexPaletteColor3D:   return "surface/unlit_vertexcolor_surface.glsl";
        case SurfaceId::Text2D:                return nullptr; // bespoke VS+FS, no surface fn
        case SurfaceId::TerrainGrid:           return "surface/terrain_grid_surface.glsl";
        case SurfaceId::SkyMinimal:            return "surface/sky_minimal_surface.glsl";
        case SurfaceId::Standard:              return "surface/standard_surface.glsl";
        case SurfaceId::StandardBlinnPhong:    return "surface/textureblinnphong_surface.glsl";
        case SurfaceId::StandardPBR:           return "surface/standard_surface.glsl";
        case SurfaceId::PBRColor3D:            return "surface/pbrcolor3d_surface.glsl";
        case SurfaceId::FragCoord:             return "surface/fragcoord_surface.glsl";
        default:                               return nullptr;
    }
}

// ---------------------------------------------------------------------------
// 每个 preset 独立质量梯度表
// 格式：{ quality_level, RenderPhase, SurfaceId, vs_override, fs_override }
// 同一 preset/phase 内按 quality_level 降序排列（Lookup 从上往下找第一个 <= 请求值的）
// ---------------------------------------------------------------------------

static constexpr PresetQualityEntry kTable_Checkerboard3D[] = {
    { 10, kForwardOpaque, SurfaceId::Checkerboard, nullptr, nullptr },
};

static constexpr PresetQualityEntry kTable_VertexColor[] = {
    { 10, kForwardOpaque, SurfaceId::VertexColor, nullptr, nullptr },
};

static constexpr PresetQualityEntry kTable_PureColor[] = {
    { 10, kForwardOpaque, SurfaceId::PureColor3D, nullptr, nullptr },
};

static constexpr PresetQualityEntry kTable_UnlitTexture[] = {
    { 10, kForwardOpaque, SurfaceId::UnlitTexture3D, nullptr, nullptr },
};

static constexpr PresetQualityEntry kTable_VertexLuminance[] = {
    { 10, kForwardOpaque, SurfaceId::VertexLuminance, nullptr, nullptr },
};

// Text2D 使用 bespoke VS+FS；无 surface fn（SurfaceId::Text2D → nullptr path）
static constexpr PresetQualityEntry kTable_Text2D[] = {
    { 10, kForwardOpaque, SurfaceId::Text2D, "2d/text2d.vert.glsl", "2d/text2d.frag.glsl" },
};

static constexpr PresetQualityEntry kTable_VertexPaletteColor3D[] = {
    { 10, kForwardOpaque, SurfaceId::VertexPaletteColor3D,
      "compositor/main_forward_unlit_palette.vert.glsl", nullptr },
};

static constexpr PresetQualityEntry kTable_Gizmo3D[] = {
    { 10, kForwardOpaque, SurfaceId::Gizmo3D, nullptr, nullptr },
};

static constexpr PresetQualityEntry kTable_TerrainGrid[] = {
    { 10, kForwardOpaque, SurfaceId::TerrainGrid,
      "compositor/main_terrain_grid.vert.glsl", nullptr },
};

static constexpr PresetQualityEntry kTable_SkyMinimal[] = {
    { 10, kForwardOpaque, SurfaceId::SkyMinimal, nullptr, nullptr },
};

// Standard: Q10=PBR, Q6=BlinnPhong, Q1=Lambert/Standard
static constexpr PresetQualityEntry kTable_Standard[] = {
    { 10, kForwardOpaque, SurfaceId::StandardPBR,        nullptr, nullptr },
    {  6, kForwardOpaque, SurfaceId::StandardBlinnPhong, nullptr, nullptr },
    {  1, kForwardOpaque, SurfaceId::Standard,           nullptr, nullptr },
};

static constexpr PresetQualityEntry kTable_PBRColor3D[] = {
    { 10, kForwardOpaque, SurfaceId::PBRColor3D, nullptr, nullptr },
};

// Semantic presets：当前 fallback 到 StandardPBR；各自 surface 独立实现后逐步拆开
static constexpr PresetQualityEntry kTable_HumanSkin[]      = {{ 10, kForwardOpaque, SurfaceId::StandardPBR, nullptr, nullptr }};
static constexpr PresetQualityEntry kTable_AmphibiansSkin[] = {{ 10, kForwardOpaque, SurfaceId::StandardPBR, nullptr, nullptr }};
static constexpr PresetQualityEntry kTable_Wood[]           = {{ 10, kForwardOpaque, SurfaceId::StandardPBR, nullptr, nullptr }};
static constexpr PresetQualityEntry kTable_TreeBark[]       = {{ 10, kForwardOpaque, SurfaceId::StandardPBR, nullptr, nullptr }};
static constexpr PresetQualityEntry kTable_Stone[]          = {{ 10, kForwardOpaque, SurfaceId::StandardPBR, nullptr, nullptr }};
static constexpr PresetQualityEntry kTable_Leaf[]           = {{ 10, kForwardOpaque, SurfaceId::StandardPBR, nullptr, nullptr }};
static constexpr PresetQualityEntry kTable_Metal[]          = {{ 10, kForwardOpaque, SurfaceId::StandardPBR, nullptr, nullptr }};
static constexpr PresetQualityEntry kTable_BirdFeathers[]   = {{ 10, kForwardOpaque, SurfaceId::StandardPBR, nullptr, nullptr }};
static constexpr PresetQualityEntry kTable_Scales[]         = {{ 10, kForwardOpaque, SurfaceId::StandardPBR, nullptr, nullptr }};

static constexpr PresetQualityEntry kTable_FullscreenTriangle[] = {
    { 10, kForwardOpaque, SurfaceId::FragCoord, nullptr, nullptr },
};

// Custom：无内置条目；调用方必须自行提供 surface
// 用一个哨兵占位避免零长数组（MSVC 不允许 constexpr T[] = {}）
static constexpr PresetQualityEntry kTable_Custom[] = {
    { 0, kForwardOpaque, SurfaceId::None, nullptr, nullptr },
};

// ---------------------------------------------------------------------------
// Index: MaterialPreset → { ptr, count }
// 顺序必须与 MaterialPreset enum 声明顺序完全一致。
// ---------------------------------------------------------------------------
struct TableSpan {
    const PresetQualityEntry* data;
    size_t count;
};

#define SPAN(name) TableSpan{ kTable_##name, sizeof(kTable_##name) / sizeof(kTable_##name[0]) }

static constexpr TableSpan kPresetIndex[] = {
    SPAN(Checkerboard3D),       // MaterialPreset::Checkerboard3D
    SPAN(VertexColor),
    SPAN(PureColor),
    SPAN(UnlitTexture),
    SPAN(VertexLuminance),
    SPAN(Text2D),
    SPAN(VertexPaletteColor3D),
    SPAN(Gizmo3D),
    SPAN(TerrainGrid),
    SPAN(SkyMinimal),
    SPAN(Standard),
    SPAN(PBRColor3D),
    SPAN(HumanSkin),
    SPAN(AmphibiansSkin),
    SPAN(Wood),
    SPAN(TreeBark),
    SPAN(Stone),
    SPAN(Leaf),
    SPAN(Metal),
    SPAN(BirdFeathers),
    SPAN(Scales),
    SPAN(FullscreenTriangle),
    SPAN(Custom),
};

#undef SPAN

static constexpr size_t kPresetIndexCount =
    sizeof(kPresetIndex) / sizeof(kPresetIndex[0]);

// 确保 kPresetIndex 条目数与 enum 中 preset 数量一致。
// MaterialPreset 使用 ENUM_CLASS_RANGE(Checkerboard3D, Custom)，
// 所以枚举个数 = static_cast<size_t>(Custom) - static_cast<size_t>(Checkerboard3D) + 1。
static_assert(
    kPresetIndexCount ==
    static_cast<size_t>(MaterialPreset::Custom) -
    static_cast<size_t>(MaterialPreset::Checkerboard3D) + 1,
    "kPresetIndex size must match MaterialPreset enum count"
);

// ---------------------------------------------------------------------------
// MaterialPresetTable implementation
// ---------------------------------------------------------------------------

void MaterialPresetTable::Initialize() {
    std::fprintf(stderr,
        "[MaterialPresetTable] %zu presets, %zu total entries\n",
        kPresetIndexCount, Count());
}

void MaterialPresetTable::Shutdown() {}

const PresetQualityEntry* MaterialPresetTable::Lookup(MaterialPreset preset,
                                                       uint8_t quality_level,
                                                       RenderPhase phase) {
    const size_t idx = static_cast<size_t>(preset)
                     - static_cast<size_t>(MaterialPreset::Checkerboard3D);
    if (idx >= kPresetIndexCount) return nullptr;

    const TableSpan& span = kPresetIndex[idx];
    if (span.count == 0) return nullptr;

    if (quality_level < 1)  quality_level = 1;
    if (quality_level > 10) quality_level = 10;

    const PresetQualityEntry *lowest_phase_entry = nullptr;

    // 表内按 quality_level 降序排列；在相同 phase 中找第一个 quality_level <= 请求值的条目
    for (size_t i = 0; i < span.count; ++i) {
        if (span.data[i].phase != phase)
            continue;

        lowest_phase_entry = &span.data[i];

        if (span.data[i].quality_level <= quality_level)
            return &span.data[i];
    }

    // 该 phase 的所有条目都高于请求值，返回该 phase 的最低质量条目；
    // 若 preset 没有该 phase 条目则返回 nullptr，禁止跨 phase 回退。
    return lowest_phase_entry;
}

size_t MaterialPresetTable::Count() noexcept {
    size_t total = 0;
    for (size_t i = 0; i < kPresetIndexCount; ++i)
        total += kPresetIndex[i].count;
    return total;
}

} // namespace hgl::graph::mtl
