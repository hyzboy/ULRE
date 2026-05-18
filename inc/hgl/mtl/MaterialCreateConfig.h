#pragma once

#include<hgl/mtl/MaterialBuildFlags.h>
#include<hgl/type/String.h>
#include<hgl/common/PrimitiveTypeDef.h>
#include<hgl/common/ShaderStageDef.h>
#include<hgl/common/RenderTargetOutputConfig.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/MaterialVariantKey.h>
#include<cstring>

namespace hgl::graph::mtl{
class MaterialCreateInfo;

/// Discriminator that replaces dynamic_cast<> in the MaterialCreateConfig hierarchy.
/// Each subclass sets this in its constructor.
enum class ConfigKind : uint8_t
{
    D3        = 0,   ///< Material3DCreateConfig (default — most common)
    D2        = 1,   ///< Material2DCreateConfig
    Text2D    = 2,   ///< Text2DMaterialCreateConfig
};

/**
 * 材质配置结构
 */
struct MaterialCreateConfig
{
    const char *                preset_name=nullptr;        ///<原始材质预设名称(如"PBRColor3D")，用于日志输出

    /// Subclass identity — set by each derived class constructor.
    /// Never modify after construction.  Use As3D() helper.
    ConfigKind                  kind = ConfigKind::D3;

    bool                        material_instance;          ///<是否包含材质实例

    RenderTargetOutputConfig    rt_output;                  ///<渲染目标输出配置

    uint32                      shader_stage_flag_bit;      ///<需要的shader

    PrimitiveType               prim;                       ///<图元类型

    bool                        local_to_world;             ///<包含LocalToWorld矩阵

    // Optional variant overrides (default disabled).
    uint32                      texture_source_bits_override  = 0;
    uint32                      sampler_feature_bits_override = 0;       ///< slot enable mask paired with texture_source_bits_override

public:

    const uint32 enableVertexFragmentShader() { return shader_stage_flag_bit|=(uint32)ShaderStage::VertexFragment; }

    void SetTextureSourceModeOverride(const SamplerSlot slot, const TextureSourceMode mode)
    {
        const uint32 shift = uint32(slot) * MaterialVariantKey::TextureSourceBitsPerSlot;
        texture_source_bits_override &= ~(MaterialVariantKey::TextureSourceMask << shift);
        texture_source_bits_override |= (uint32(mode) & MaterialVariantKey::TextureSourceMask) << shift;

        const uint32 bit = SamplerFeatureBit(slot);
        if (mode == TextureSourceMode::None)
            sampler_feature_bits_override &= ~bit;
        else
            sampler_feature_bits_override |= bit;
    }

    void SetTextureSourceSlotEnabledOverride(const SamplerSlot slot, const bool enabled = true)
    {
        const uint32 bit = SamplerFeatureBit(slot);
        if (enabled)
            sampler_feature_bits_override |= bit;
        else
            sampler_feature_bits_override &= ~bit;
    }

    bool HasTextureSourceBitsOverride() const
    {
        return texture_source_bits_override != 0;
    }

public:

    MaterialCreateConfig(const PrimitiveType &p,const bool l2w)
    {
        material_instance=false;

        mem_zero(rt_output);

        shader_stage_flag_bit=(uint32_t)ShaderStage::VertexFragment;

        prim=p;

        local_to_world=l2w;
    }

    std::strong_ordering operator<=>(const MaterialCreateConfig &cfg)const
    {
        if(auto cmp=material_instance<=>cfg.material_instance;cmp!=0)
            return cmp;

        if(auto cmp=mem_compare(rt_output,cfg.rt_output);cmp!=0)
            return cmp<0?std::strong_ordering::less:std::strong_ordering::greater;

        if(auto cmp=prim<=>cfg.prim;cmp!=0)
            return cmp;

        if(auto cmp=local_to_world<=>cfg.local_to_world;cmp!=0)
            return cmp;

        if(auto cmp=shader_stage_flag_bit<=>cfg.shader_stage_flag_bit;cmp!=0)
            return cmp;

        if(auto cmp=texture_source_bits_override<=>cfg.texture_source_bits_override;cmp!=0)
            return cmp;

        return sampler_feature_bits_override<=>cfg.sampler_feature_bits_override;
    }

    virtual std::string ToHashStdString();
};//struct MaterialCreateConfig

}//namespace hgl::graph::mtl
