#ifndef HGL_GRAPH_MTL_CONFIG_INCLUDE
#define HGL_GRAPH_MTL_CONFIG_INCLUDE

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/type/String.h>
#include<hgl/graph/RenderTargetOutputConfig.h>
#include<hgl/graph/VK.h>

STD_MTL_NAMESPACE_BEGIN

class MaterialCreateInfo;

/**
 * 材质配置结构
 */
struct MaterialConfig
{
    AnsiString mtl_name;                                    ///<材质名称

    RenderTargetOutputConfig rt_output;                     ///<渲染目标输出配置

    uint32 shader_stage;                                    ///<需要的shader

public:

    MaterialConfig(const AnsiString &name)
    {
        mtl_name=name;

        shader_stage=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
    }
};//struct MaterialConfig
STD_MTL_NAMESPACE_END
#endif//HGL_GRAPH_MTL_CONFIG_INCLUDE
