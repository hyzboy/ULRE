// unlit_texture_bindings.glsl — UnlitTexture（无光单图）域的数据布局，物理固定。
//
// UnlitTexture 只有一张图，且**不读材质私有数据行**：颜色完全来自
// `module/provider/material_source/material_source_texture.glsl` 对 base_color 的采样。因此这里的贴图表只有
// 一个元素（跨度 16 = uvec2 + 对齐），也没有 `MTL_ROW`。
//
// ⚠ 槽位顺序 = `DomainContracts[]` 里 UnlitTexture 的 UnlitTextureSlots
// （只有 base_color），顺序即索引。
#ifndef HGL_DOMAIN_UNLIT_TEXTURE_BINDINGS_GLSL
#define HGL_DOMAIN_UNLIT_TEXTURE_BINDINGS_GLSL

// 采样器预设索引取自生成物，不写数字（真源 inc/hgl/shader/contract/SamplerPresetId.h）。
// Trilinear 给 2D 展开图；TrilinearClamp 给八面体展开图（需要 ClampToEdge）。
#include "contract/sampler_presets.glsl"

// ── 材质贴图表（由 ShaderLibrary/contract/material_domain_slots.toml 生成） ──
#include "domain/generated/unlit_texture_texture_bindings.glsl"

#endif // HGL_DOMAIN_UNLIT_TEXTURE_BINDINGS_GLSL
