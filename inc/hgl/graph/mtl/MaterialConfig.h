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
struct MaterialCreateConfig:public Comparator<MaterialCreateConfig>
{
    const GPUDeviceAttribute *  dev_attr;                   ///<GPU设备属性(目前仅用于获取UBO/SSBO数值，未来可以考虑干掉)

    AnsiString                  mtl_name;                   ///<材质名称

    bool                        material_instance;          ///<是否包含材质实例

    RenderTargetOutputConfig    rt_output;                  ///<渲染目标输出配置

    uint32                      shader_stage_flag_bit;      ///<需要的shader

    PrimitiveType               prim;                       ///<图元类型

public:

    MaterialCreateConfig(const GPUDeviceAttribute *da,const AnsiString &name,const PrimitiveType &p)
    {
        dev_attr=da;

        mtl_name=name;

        material_instance=false;

        shader_stage_flag_bit=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;

        prim=p;
    }

    const int compare(const MaterialCreateConfig &cfg)const override
    {
        int off;
        
        off=material_instance-cfg.material_instance;
        if(off)return(off);

        off=hgl_cmp(rt_output,cfg.rt_output);
        if(off)return(off);

        off=(int)prim-(int)cfg.prim;
        if(off)return(off);

        off=shader_stage_flag_bit-cfg.shader_stage_flag_bit;

        return off;
    }
};//struct MaterialCreateConfig
STD_MTL_NAMESPACE_END
