#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/type/String.h>
#include<hgl/graph/RenderTargetOutputConfig.h>
#include<hgl/graph/VK.h>
#include<hgl/graph/mtl/SamplerName.h>

STD_MTL_NAMESPACE_BEGIN
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

public:

    const uint32 enableVertexShader     () { return shader_stage_flag_bit|=(uint32)ShaderStage::Vertex; }
    const uint32 enableGeometryShader   () { return shader_stage_flag_bit|=(uint32)ShaderStage::Geometry; }
    const uint32 enableTesslationShader () { return shader_stage_flag_bit|=(uint32)ShaderStage::Tessellation; }
    const uint32 enableFragmentShader   () { return shader_stage_flag_bit|=(uint32)ShaderStage::Fragment; }

    const uint32 enableVertexFragmentShader() { return shader_stage_flag_bit|=(uint32)ShaderStage::VertexFragment; }

    const uint32 enableComputeShader    () { return shader_stage_flag_bit|=(uint32)ShaderStage::Compute; }

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

        return shader_stage_flag_bit<=>cfg.shader_stage_flag_bit;
    }

    virtual const AnsiString ToHashString();
};//struct MaterialCreateConfig
STD_MTL_NAMESPACE_END
