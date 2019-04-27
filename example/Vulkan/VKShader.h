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



    Set<VertexAttributeBinding *> vab_sets;

private:

    bool ParseVertexShader(const void *,const uint32_t);

public:

    Shader(VkDevice);
    ~Shader();


public: //shader部分

public: //Vertex Input部分

    VertexAttributeBinding *                    CreateVertexAttributeBinding();
    bool                                        Release(VertexAttributeBinding *);
    const uint32_t                              GetInstanceCount()const{return vab_sets.GetCount();}

};//class ShaderCreater
VK_NAMESPACE_END
