#ifndef HGL_GRAPH_MTL_CONFIG_INCLUDE
#define HGL_GRAPH_MTL_CONFIG_INCLUDE

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
    const GPUDeviceAttribute *dev_attr;

    AnsiString mtl_name;                                    ///<材质名称

    RenderTargetOutputConfig rt_output;                     ///<渲染目标输出配置

    uint32 shader_stage_flag_bit;                           ///<需要的shader

    Prim prim;                                              ///<图元类型

public:

    MaterialCreateConfig(const GPUDeviceAttribute *da,const AnsiString &name,const Prim &p)
    {
        dev_attr=da;

        mtl_name=name;

        shader_stage_flag_bit=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;

        prim=p;
    }

    int Comp(const MaterialCreateConfig &cfg)const
    {
        int off;

        off=hgl_cmp(rt_output,cfg.rt_output);

        if(off)return(off);

        off=(int)prim-(int)cfg.prim;

        if(off)return(off);

        return shader_stage_flag_bit-cfg.shader_stage_flag_bit;
    }

    CompOperator(const MaterialCreateConfig &,Comp)
};//struct MaterialCreateConfig
STD_MTL_NAMESPACE_END
#endif//HGL_GRAPH_MTL_CONFIG_INCLUDE
