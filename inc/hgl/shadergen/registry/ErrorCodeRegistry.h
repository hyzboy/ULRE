#pragma once

#include <cstdint>
#include <string>

namespace hgl::graph::mtl
{
    /// FSErrorReason — FS 路由失败原因分类（bit 0..7 of error_code）
    /// 编码规则：
    ///   error_code bit  0..7  = FSErrorReason (reason category)
    ///   error_code bit  8..15 = surface_model id (SurfaceType cast to uint8)
    ///   error_code bit 16..23 = missing tex_bits low 8 bits
    ///   error_code bit 24..31 = missing sampler_bits low 8 bits
    ///
    /// 反推路径：截屏取 ErrorIndicator 棋盘"黑"格像素 (R,G,B) →
    ///   error_code = (uint32)R | ((uint32)G << 8) | ((uint32)B << 16)
    ///   → DecodeFSError(error_code) 得到人读文本
    enum class FSErrorReason : uint8_t
    {
        Unknown             = 0,    ///< 未分类错误
        NoVariantRegistered = 1,    ///< VariantRegistry 完全 miss（无任何候选）
        NoSurfaceVariant    = 2,    ///< 找到了 VS 候选，但 FS/surface 路由失败（tex_bits 不兼容）
        NoVSTemplate        = 3,    ///< VS 模板文件缺失
        NoFSTemplate        = 4,    ///< FS/surface 模板文件缺失
        AssemblyFailed      = 5,    ///< CompositorAssembler 装配失败
        FactoryTypeMissing  = 6,    ///< variant_desc->factory_type 未设置
        FactoryDispatchFail = 7,    ///< MaterialFactory3D::Create 返回 null
    };

    /// 将 FS 错误各字段编码为 uint32 error_code
    inline uint32_t EncodeFSError(FSErrorReason reason,
                                   uint8_t surface_model_id,
                                   uint8_t tex_bits_lo,
                                   uint8_t sampler_bits_lo) noexcept
    {
        return static_cast<uint32_t>(reason)
             | (static_cast<uint32_t>(surface_model_id) << 8)
             | (static_cast<uint32_t>(tex_bits_lo)      << 16)
             | (static_cast<uint32_t>(sampler_bits_lo)  << 24);
    }

    /// 从 uint32 error_code 反推各字段
    struct DecodedFSError
    {
        FSErrorReason reason;
        uint8_t       surface_model_id;
        uint8_t       tex_bits_lo;
        uint8_t       sampler_bits_lo;
    };

    inline DecodedFSError DecodeFSError(uint32_t code) noexcept
    {
        return {
            static_cast<FSErrorReason>(code & 0xFF),
            static_cast<uint8_t>((code >>  8) & 0xFF),
            static_cast<uint8_t>((code >> 16) & 0xFF),
            static_cast<uint8_t>((code >> 24) & 0xFF),
        };
    }

    /// 返回 FSErrorReason 的可读名称
    const char *GetFSErrorReasonName(FSErrorReason r) noexcept;

    /// 将 error_code 格式化为可读诊断文本
    /// 例："reason=NoSurfaceVariant surface_model=3 tex_bits_lo=0x02 sampler_bits_lo=0x00"
    std::string FormatFSError(uint32_t error_code);

    /// 将 error_code 的三字节颜色部分提取为 RGB（用于与截屏像素对比）
    ///   R = error_code & 0xFF
    ///   G = (error_code >> 8) & 0xFF
    ///   B = (error_code >> 16) & 0xFF
    /// 注意：第 24..31 位（sampler_bits_lo）未编入颜色，仅存在于 error_code 数值中。
    inline void FSErrorToRGB(uint32_t error_code,
                              uint8_t &out_r, uint8_t &out_g, uint8_t &out_b) noexcept
    {
        out_r = static_cast<uint8_t>( error_code        & 0xFF);
        out_g = static_cast<uint8_t>((error_code >>  8) & 0xFF);
        out_b = static_cast<uint8_t>((error_code >> 16) & 0xFF);
    }

} // namespace hgl::graph::mtl
