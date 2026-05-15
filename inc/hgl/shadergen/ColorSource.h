#pragma once

/// ColorSource.h — 统一的"颜色/数值来源"抽象。
/// (内容详见 ColorSource_PCG_Unification_Plan.md)

#include <hgl/mtl/SamplerSlot.h>
#include <hgl/shadergen/DescriptorRequirement.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph
{

// ── ColorSource 实现来源 ──────────────────────────────────────────────────────

enum class ColorSourceKind : uint8_t
{
    None = 0,

    // ── 内置 sampler PCG（由生成器 emit 函数体）──────────────────────────────
    BuiltinSampler2D,               ///< layout(set,binding) uniform sampler2D
    BuiltinSampler2DArray,          ///< layout(set,binding) uniform sampler2DArray + per-instance layer
    BuiltinBindlessSamplerArray,    ///< bindless sampler[] via descriptor indexing（未来）
    BuiltinSubpassInput,            ///< subpass input attachment（未来）
    BuiltinStorageImage,            ///< storage image（未来）

    // ── 用户提供 ─────────────────────────────────────────────────────────────
    UserPCG,                        ///< 用户 .glsl 文件 + 入口函数（生成器 #include + 校验签名）
    PureFunction,                   ///< 无 binding 的纯程序化函数（noise / voronoi / checker）
};

// ── 函数签名规范 ──────────────────────────────────────────────────────────────

/// 规定生成器 emit 的 getter 函数原型，同一 slot 的不同实现必须遵守同一签名才可互换。
enum class ColorSourceSignature : uint8_t
{
    Unknown = 0,

    /// vec4 GetSamplerXxx(vec2 uv)
    /// 适用：普通 2D 贴图、不依赖 instance 的程序化函数
    UV2D,

    /// vec4 GetSamplerXxx(uint mi_id, vec2 uv)
    /// 适用：sampler2DArray（按 MI 选 layer）、bindless（按 MI 选 index）
    UV2DPerInstance,

    /// vec4 GetSamplerXxx(vec3 world_pos)
    /// 适用：三平面映射、体积噪声、cubemap
    WorldPos3D,

    /// vec4 GetSamplerXxx(vec2 screen_pos)
    /// 适用：subpass input、post-process
    ScreenPos2D,
};

// ── 输出格式提示 ──────────────────────────────────────────────────────────────

/// 生成器据此决定 getter 内 swizzle 策略（替代旧 SAMPLER_*_GRAYSCALE 宏分支）
enum class ColorSourceOutputFormat : uint8_t
{
    RGBA = 0,           ///< 直接返回 texture(...) 完整 vec4
    Grayscale_R,        ///< 读 .r 后 expand 成 vec4(r,r,r,r)
    RG_NormalXY,        ///< 读 .rg，zw 由 surface shader 重建（或 xy 直接 bias decode）
    RGB,                ///< 忽略 alpha，alpha 置 1.0
};

// ── 内置 sampler PCG 载荷 ─────────────────────────────────────────────────────

struct BuiltinSamplerPayload
{
    ColorSourceOutputFormat output_format = ColorSourceOutputFormat::RGBA;
    // 未来可扩展：anisotropy hint、mip bias 等，不影响签名
};

// ── 用户 PCG 载荷 ─────────────────────────────────────────────────────────────

struct UserPCGPayload
{
    std::string glsl_file;          ///< 相对于 ShaderLibrary 根目录的路径
    std::string entry_func;         ///< 必须与 ColorSourceSignature 对应的函数名
    std::vector<std::string> extra_includes; ///< 附加 #include 列表（先于 glsl_file include）

    // 约束：用户 .glsl 内部不得自己写 layout(set=,binding=)；
    // 所有 binding 必须通过 ColorSource::bindings[] 向 BindingAllocator 声明，
    // 由生成器统一 emit layout 声明后再 include 用户文件。
};

// ── ColorSource 主结构 ────────────────────────────────────────────────────────

struct ColorSource
{
    // ── 标识维度 ─────────────────────────────────────────────────────────────
    mtl::SamplerSlot         slot      = mtl::SamplerSlot::BaseColor;
    ColorSourceKind          kind      = ColorSourceKind::None;
    ColorSourceSignature     signature = ColorSourceSignature::UV2D;

    // ── 资源需求（必须显式声明，包括内置 PCG）────────────────────────────────
    std::vector<DescriptorRequirement>    bindings;      ///< descriptor 需求列表
    std::vector<PushConstantRequirement>  push_constants;///< push constant 需求列表（可选）

    // ── 实现载荷（union 语义，按 kind 取其一）────────────────────────────────
    BuiltinSamplerPayload  builtin; ///< 有效当 kind = BuiltinSampler2D / BuiltinSampler2DArray / ...
    UserPCGPayload         user;    ///< 有效当 kind = UserPCG

    // ── 便利构造：内置 Sampler2D ─────────────────────────────────────────────
    static ColorSource MakeSampler2D(mtl::SamplerSlot slot,
                                     ColorSourceOutputFormat fmt = ColorSourceOutputFormat::RGBA,
                                     const std::string &debug_name = {});

    // ── 便利构造：内置 Sampler2DArray ────────────────────────────────────────
    static ColorSource MakeSampler2DArray(mtl::SamplerSlot slot,
                                          ColorSourceOutputFormat fmt = ColorSourceOutputFormat::RGBA,
                                          const std::string &debug_name = {});
};

} // namespace hgl::graph
