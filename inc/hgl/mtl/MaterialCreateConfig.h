#pragma once

#include<hgl/mtl/StdMaterial.h>
#include<hgl/type/String.h>
#include<hgl/common/PrimitiveTypeDef.h>
#include<hgl/common/ShaderStageDef.h>
#include<hgl/common/RenderTargetOutputConfig.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/mtl/ShaderBufferSource.h>
#include<cstring>

namespace hgl::graph
{
    class GeometryVertexFormat;
}

namespace hgl::graph::mtl{
class ShaderProgramBuildSpec;

/**
 * 材质配置结构
 */
struct MaterialCreateConfig
{
    bool                        material_instance;          ///<是否包含材质实例

    RenderTargetOutputConfig    rt_output;                  ///<渲染目标输出配置

    uint32                      shader_stage_flag_bit;      ///<需要的shader

    PrimitiveType               prim;                       ///<图元类型

    const GeometryVertexFormat *geometry_vertex_format;     ///<可选：用于推导 shader 顶点输入格式的 GeometryVertexFormat

    bool                        local_to_world;             ///<包含LocalToWorld矩阵

    const ShaderBufferSource *const * private_shader_buffer_sources;       ///<私有ShaderBufferSource列表（材质独占）
    uint32                              private_shader_buffer_source_count; ///<私有ShaderBufferSource数量

public:

    void SetPrivateShaderBufferSources(const ShaderBufferSource *const *list,const uint32 count)
    {
        private_shader_buffer_sources=list;
        private_shader_buffer_source_count=count;
    }

    void SetGeometryVertexFormat(const GeometryVertexFormat *gvf)
    {
        geometry_vertex_format=gvf;
    }

    const ShaderBufferSource *FindPrivateShaderBufferSourceByStructName(const char *struct_name)const
    {
        if(!struct_name||!*struct_name)
            return nullptr;

        if(!private_shader_buffer_sources||private_shader_buffer_source_count==0)
            return nullptr;

        for(uint32 i=0;i<private_shader_buffer_source_count;++i)
        {
            const ShaderBufferSource *sbs=private_shader_buffer_sources[i];

            if(!sbs||!sbs->struct_name)
                continue;

            if(std::strcmp(sbs->struct_name,struct_name)==0)
                return sbs;
        }

        return nullptr;
    }

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

        geometry_vertex_format=nullptr;

        local_to_world=l2w;

        private_shader_buffer_sources=nullptr;
        private_shader_buffer_source_count=0;
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

        if(auto cmp=private_shader_buffer_source_count<=>cfg.private_shader_buffer_source_count;cmp!=0)
            return cmp;

        for(uint32 i=0;i<private_shader_buffer_source_count;++i)
        {
            const ShaderBufferSource *lhs=private_shader_buffer_sources?private_shader_buffer_sources[i]:nullptr;
            const ShaderBufferSource *rhs=cfg.private_shader_buffer_sources?cfg.private_shader_buffer_sources[i]:nullptr;

            const char *lhs_name=(lhs&&lhs->struct_name)?lhs->struct_name:"";
            const char *rhs_name=(rhs&&rhs->struct_name)?rhs->struct_name:"";

            const int cmp=std::strcmp(lhs_name,rhs_name);
            if(cmp<0)
                return std::strong_ordering::less;
            if(cmp>0)
                return std::strong_ordering::greater;
        }

        return shader_stage_flag_bit<=>cfg.shader_stage_flag_bit;
    }

    virtual std::string ToHashStdString();
};//struct MaterialCreateConfig

}//namespace hgl::graph::mtl

// ── Legacy resolve helpers — only visible when MaterialCreateConfig is fully defined ──
// They forward to the GeometryVertexFormat* overloads declared in MaterialLibrary.h
// Include MaterialLibrary.h after MaterialCreateConfig.h to get all overloads.
#include<hgl/mtl/MaterialLibrary.h>
namespace hgl::graph::mtl{
inline VkFormat ResolveMaterialVertexSemanticFormat(const MaterialCreateConfig *cfg, VertexSemantic semantic, VkFormat fallback_format)
{
    return ResolveMaterialVertexSemanticFormat(cfg ? cfg->geometry_vertex_format : nullptr, semantic, fallback_format);
}
inline VkFormat ResolveMaterialPositionFormat(const MaterialCreateConfig *cfg, VkFormat fallback_format)
{
    return ResolveMaterialPositionFormat(cfg ? cfg->geometry_vertex_format : nullptr, fallback_format);
}
}//namespace hgl::graph::mtl
