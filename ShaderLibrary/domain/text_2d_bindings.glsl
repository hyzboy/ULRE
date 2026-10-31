// text_2d_bindings.glsl — Text2D 域的数据布局，物理固定。
//
// Text2D 只采样一张字体图集（契约槽表 = `base_color`，Sampler2DArray），
// 且**不读材质私有数据行**：文字颜色来自顶点色，样式来自 `sbo_char_style`
// 样式表（基址在 `pc_root.addr_text_char_style`），都不在材质行里。
// 所以这里只有一张单元素贴图表，没有 `MTL_ROW`。
//
// ── 为什么两个采样器宏都发 ──
// `module/provider/material_source/material_source_text_gpu.glsl` 按 `TEXT_SDF_ENABLED` 二选一：
//   定义 → SDF 距离场路径，走 `LinearSampler`；
//   未定义 → 原始位图路径，走 `NearestSampler`。
// 该宏来自材质定义的 `defines`（`builtin/text_gpu` 有、`builtin/text_gpu_bitmap`
// 没有），因此**过滤方式属于定义而非领域**，物理文件里必须同时提供两个索引。
// 多出来的那个宏没有消费者，不产生任何代码。
//
// ⚠ 数值是 contract/sampler.toml 里的**全局**预设序号（Nearest=0, Linear=1），
// 不是本领域内的序号。
#ifndef HGL_DOMAIN_TEXT_2D_BINDINGS_GLSL
#define HGL_DOMAIN_TEXT_2D_BINDINGS_GLSL

// ── 采样器预设索引 ──
#define NearestSampler 0u
#define LinearSampler 1u

// ── 材质贴图表 ──
// 一个元素 = 一对 (bindless 索引, 采样器索引)，16 字节对齐；跨度 16 = 2 × 8。
layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef
{
    uvec2 tex_base_color;
};
#define MTL_TEX(i) MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(16))

#endif // HGL_DOMAIN_TEXT_2D_BINDINGS_GLSL
