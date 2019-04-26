#pragma once
#include"VK.h"
#include<hgl/type/BaseString.h>
#include<hgl/type/Map.h>
#include<hgl/type/Set.h>
VK_NAMESPACE_BEGIN
class VertexAttributeBinding;

/**
 * Shader 创建器
 */
class Shader
{
    VkDevice device;

    List<VkPipelineShaderStageCreateInfo> shader_stage_list;

private:

    uint32_t attr_count;
    VkVertexInputBindingDescription *binding_list;
    VkVertexInputAttributeDescription *attribute_list;

    Map<UTF8String,VkVertexInputAttributeDescription *> stage_input_locations;

    Set<VertexAttributeBinding *> instance_set;

private:

    bool CreateVIS(const void *,const uint32_t);

public:

    Shader(VkDevice);
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

public: //shader部分

    const uint32_t                          GetStageCount    ()const{return shader_stage_list.GetCount();}
    const VkPipelineShaderStageCreateInfo * GetShaderStages   ()const{return shader_stage_list.GetData();}

public: //Vertex Input部分

    VertexAttributeBinding *                    CreateVertexAttributeBinding();
    bool                                        Release(VertexAttributeBinding *);
    const uint32_t                              GetInstanceCount()const{return instance_set.GetCount();}

    const uint32_t                              GetAttrCount()const{return attr_count;}

    const int                                   GetLocation (const UTF8String &)const;
    const int                                   GetBinding  (const UTF8String &)const;

    const VkVertexInputBindingDescription *     GetDescList ()const{return binding_list;}
    const VkVertexInputAttributeDescription *   GetAttrList ()const{return attribute_list;}

    const VkVertexInputBindingDescription *     GetDesc     (const uint32_t index)const{return (index>=attr_count?nullptr:binding_list+index);}
    const VkVertexInputAttributeDescription *   GetAttr     (const uint32_t index)const{return (index>=attr_count?nullptr:attribute_list+index);}
};//class ShaderCreater
VK_NAMESPACE_END
