#ifndef HGL_GRAPH_VULKAN_SHADER_MODULE_MANAGE_INCLUDE
#define HGL_GRAPH_VULKAN_SHADER_MODULE_MANAGE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
VK_NAMESPACE_BEGIN

class ShaderResource;

/**
 * Shader模块管理器<br>
 * 所有的shader模块均由它创建和释放
 * 该管理器在一个设备下仅有一个，它负责管理所有的shader(仅vs/fs/gs等单个，非组合体)
 */
class ShaderModuleManage
{
    Device *device;

    int shader_count;
    Map<int,ShaderModule *> shader_list;

public:

    ShaderModuleManage(Device *dev)
    {
        device=dev;
        shader_count=0;
    }

    ~ShaderModuleManage();

    const ShaderModule *CreateShader(ShaderResource *);
    const ShaderModule *CreateShader(const VkShaderStageFlagBits shader_stage_bit,const OSString &filename);
/*
#define ADD_SHADER_FUNC(sn,vk_name)   const ShaderModule *Create##sn##Shader(const void *spv_data,const uint32_t spv_size){return CreateShader(VK_SHADER_STAGE_##vk_name##_BIT,spv_data,spv_size);}
    ADD_SHADER_FUNC(Vertex,     VERTEX)
    ADD_SHADER_FUNC(Fragment,   FRAGMENT)
    ADD_SHADER_FUNC(Geometry,   GEOMETRY)
    ADD_SHADER_FUNC(TessCtrl,   TESSELLATION_CONTROL)
    ADD_SHADER_FUNC(TessEval,   TESSELLATION_EVALUATION)
    ADD_SHADER_FUNC(Compute,    COMPUTE)
#undef ADD_SHADER_FUNC

#define ADD_NV_SHADER_FUNC(sn,vk_name)   const ShaderModule *Create##sn##Shader(const void *spv_data,const uint32_t spv_size){return CreateShader(VK_SHADER_STAGE_##vk_name##_BIT_NV,spv_data,spv_size);}
    ADD_NV_SHADER_FUNC(Raygen,      RAYGEN);
    ADD_NV_SHADER_FUNC(AnyHit,      ANY_HIT);
    ADD_NV_SHADER_FUNC(ClosestHit,  CLOSEST_HIT);
    ADD_NV_SHADER_FUNC(MissBit,     MISS);
    ADD_NV_SHADER_FUNC(Intersection,INTERSECTION);
    ADD_NV_SHADER_FUNC(Callable,    CALLABLE);
    ADD_NV_SHADER_FUNC(Task,        TASK);
    ADD_NV_SHADER_FUNC(Mesh,        MESH);
#undef ADD_NV_SHADER_FUNC
*/
    const ShaderModule *GetShader       (int);
    bool                ReleaseShader   (const ShaderModule *);

    Material *    CreateMaterial(const VertexShaderModule *vertex_shader_module,const ShaderModule *fragment_shader_module)const;
    Material *    CreateMaterial(const VertexShaderModule *vertex_shader_module,const ShaderModule *geometry_shader_module,const ShaderModule *fragment_shader_module)const;

    Material *    CreateMaterial(const OSString &vertex_shader_filename,const OSString &fragment_shader_filename)
    {
        const ShaderModule *vs=CreateShader(VK_SHADER_STAGE_VERTEX_BIT,vertex_shader_filename);

        if(!vs)
            return(nullptr);

        const ShaderModule *fs=CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT,fragment_shader_filename);

        if(!fs)
        {
            ReleaseShader(vs);
            return(nullptr);
        }

        return(CreateMaterial((VertexShaderModule *)vs,fs));
    }

    Material *    CreateMaterial(const OSString &vertex_shader_filename,const OSString &geometry_shader_filename,const OSString &fragment_shader_filename)
    {
        const ShaderModule *vs=CreateShader(VK_SHADER_STAGE_VERTEX_BIT,vertex_shader_filename);

        if(!vs)
            return(nullptr);

        const ShaderModule *gs=CreateShader(VK_SHADER_STAGE_GEOMETRY_BIT,geometry_shader_filename);

        if(!gs)
        {
            ReleaseShader(vs);
            return(nullptr);
        }

        const ShaderModule *fs=CreateShader(VK_SHADER_STAGE_FRAGMENT_BIT,fragment_shader_filename);

        if(!fs)
        {
            ReleaseShader(gs);
            ReleaseShader(vs);
            return(nullptr);
        }

        return(CreateMaterial((VertexShaderModule *)vs,gs,fs));
    }
};//class ShaderModuleManage
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SHADER_MODULE_MANAGE_INCLUDE
