#pragma once
#include"VK.h"

VK_NAMESPACE_BEGIN
/**
 * Shader ´´½¨Æ÷
 */
class Shader
{
    VkDevice device;

    List<VkPipelineShaderStageCreateInfo> shader_stage_list;

public:

    Shader(VkDevice dev):device(dev){}
    ~Shader();

    bool Add(const VkShaderStageFlagBits shader_stage_bit,const void *spv_data,const uint32_t spv_size);

#define ADD_SHADER_FUNC(sn,vk_name)   bool Add##sn##Shader(const void *spv_data,const uint32_t spv_size){return Add(VK_SHADER_STAGE_##vk_name##_BIT,spv_data,spv_size);}
    ADD_SHADER_FUNC(Vertex,     VERTEX)
    ADD_SHADER_FUNC(Fragment,   FRAGMENT)
    ADD_SHADER_FUNC(Geometry,   GEOMETRY)
    ADD_SHADER_FUNC(TessCtrl,   TESSELLATION_CONTROL)
    ADD_SHADER_FUNC(TessEval,   TESSELLATION_EVALUATION)
    ADD_SHADER_FUNC(Compute,    COMPUTE)
#undef ADD_SHADER_FUNC

#define ADD_NV_SHADER_FUNC(sn,vk_name)   bool Add##sn##Shader(const void *spv_data,const uint32_t spv_size) { return Add(VK_SHADER_STAGE_##vk_name##_BIT_NV,spv_data,spv_size); }
    ADD_NV_SHADER_FUNC(Raygen,      RAYGEN);
    ADD_NV_SHADER_FUNC(AnyHit,      ANY_HIT);
    ADD_NV_SHADER_FUNC(ClosestHit,  CLOSEST_HIT);
    ADD_NV_SHADER_FUNC(MissBit,     MISS);
    ADD_NV_SHADER_FUNC(Intersection,INTERSECTION);
    ADD_NV_SHADER_FUNC(Callable,    CALLABLE);
    ADD_NV_SHADER_FUNC(Task,        TASK);
    ADD_NV_SHADER_FUNC(Mesh,        MESH);
#undef ADD_NV_SHADER_FUNC

    void Clear()
    {
        shader_stage_list.Clear();
    }

    const uint32_t                          GetCount        ()const{return shader_stage_list.GetCount();}
    const VkPipelineShaderStageCreateInfo * GetShaderStages ()const{return shader_stage_list.GetData();}
};//class ShaderCreater
VK_NAMESPACE_END
