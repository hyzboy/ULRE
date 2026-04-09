#pragma once

#include<hgl/mtl/MaterialBuildFlags.h>
#include<hgl/common/PrimitiveTypeDef.h>
#include<hgl/common/ShaderStageDef.h>
#include<hgl/common/RenderTargetOutputConfig.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/mtl/MaterialVariantKey.h>
#include<cstring>

namespace hgl::graph::mtl{
class MaterialCreateInfo;

/**
 * 材质配置结构
 */
struct MaterialCreateConfig
{
    bool                        material_instance;          ///<是否包含材质实例

    RenderTargetOutputConfig    rt_output;                  ///<渲染目标输出配置

    uint32                      shader_stage_flag_bit;      ///<需要的shader

    PrimitiveType               prim;                       ///<图元类型

    bool                        local_to_world;             ///<包含LocalToWorld矩阵

    // Optional variant overrides (default disabled).
    bool                        override_geometry_mode      = false;
    GeometryMode                geometry_mode_override      = GeometryMode::Mesh3D;

    uint32                      texture_source_bits_override  = 0;       ///< 2 bits per slot, same packing as MaterialVariantKey
    uint32                      sampler_feature_bits_override = 0;       ///< slot enable mask paired with texture_source_bits_override

public:

    const uint32 enableVertexShader     () { return shader_stage_flag_bit|=(uint32)ShaderStage::Vertex; }
    const uint32 enableGeometryShader   () { return shader_stage_flag_bit|=(uint32)ShaderStage::Geometry; }
    const uint32 enableTesslationShader () { return shader_stage_flag_bit|=(uint32)ShaderStage::Tessellation; }
    const uint32 enableFragmentShader   () { return shader_stage_flag_bit|=(uint32)ShaderStage::Fragment; }

    const uint32 enableVertexFragmentShader() { return shader_stage_flag_bit|=(uint32)ShaderStage::VertexFragment; }

    const uint32 enableComputeShader    () { return shader_stage_flag_bit|=(uint32)ShaderStage::Compute; }

    void SetGeometryModeOverride(const GeometryMode gm)
    {
        override_geometry_mode = true;
        geometry_mode_override = gm;
    }

    void ClearGeometryModeOverride()
    {
        override_geometry_mode = false;
    }

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

        if(auto cmp=override_geometry_mode<=>cfg.override_geometry_mode;cmp!=0)
            return cmp;

        if(auto cmp=geometry_mode_override<=>cfg.geometry_mode_override;cmp!=0)
            return cmp;

        if(auto cmp=texture_source_bits_override<=>cfg.texture_source_bits_override;cmp!=0)
            return cmp;

        return sampler_feature_bits_override<=>cfg.sampler_feature_bits_override;
    }

    virtual std::string ToHashStdString();
};//struct MaterialCreateConfig
}//namespace hgl::graph::mtl
