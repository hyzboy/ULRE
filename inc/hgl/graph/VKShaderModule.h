#ifndef HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE
#define HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE

#include<hgl/graph/VKShaderResource.h>
#include<hgl/type/Sets.h>

VK_NAMESPACE_BEGIN

/**
 * Shader模块<br>
 * 该模块提供的是原始的shader数据和信息，不可被修改，只能通过ShaderModuleManage创建和删除
 */
class ShaderModule
{
    VkDevice device;
    int ref_count;

private:

    VkPipelineShaderStageCreateInfo *stage_create_info;

protected:

    ShaderResource *shader_resource;

public:

    ShaderModule(VkDevice dev,VkPipelineShaderStageCreateInfo *pssci,ShaderResource *);
    virtual ~ShaderModule();

    const int IncRef(){return ++ref_count;}
    const int DecRef(){return --ref_count;}

public:

    const VkShaderStageFlagBits             GetStage        ()const{return stage_create_info->stage;}

    const bool                              IsVertex        ()const{return stage_create_info->stage==VK_SHADER_STAGE_VERTEX_BIT;}
    const bool                              IsTessControl   ()const{return stage_create_info->stage==VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;}
    const bool                              IsTessEval      ()const{return stage_create_info->stage==VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;}
    const bool                              IsGeometry      ()const{return stage_create_info->stage==VK_SHADER_STAGE_GEOMETRY_BIT;}
    const bool                              IsFragment      ()const{return stage_create_info->stage==VK_SHADER_STAGE_FRAGMENT_BIT;}
    const bool                              IsCompute       ()const{return stage_create_info->stage==VK_SHADER_STAGE_COMPUTE_BIT;}
    const bool                              IsTask          ()const{return stage_create_info->stage==VK_SHADER_STAGE_TASK_BIT_NV;}
    const bool                              IsMesh          ()const{return stage_create_info->stage==VK_SHADER_STAGE_MESH_BIT_NV;}

    const VkPipelineShaderStageCreateInfo * GetCreateInfo   ()const{return stage_create_info;}
};//class ShaderModule

class VertexAttributeBinding;

/**
 * 顶点Shader模块<br>
 * 由于顶点shader在最前方执行，所以它比其它shader多了VertexInput的数据
 */
class VertexShaderModule:public ShaderModule
{
    uint32_t attr_count;
    VkVertexInputBindingDescription *binding_list;
    VkVertexInputAttributeDescription *attribute_list;

private:

    Sets<VertexAttributeBinding *> vab_sets;

public:

    VertexShaderModule(VkDevice dev,VkPipelineShaderStageCreateInfo *pssci,ShaderResource *sr);
    virtual ~VertexShaderModule();

    /**
     * 获取输入流绑定点，需要注意的时，这里获取的binding并非是shader中的binding/location，而是绑定顺序的序列号。对应vkCmdBindVertexBuffer的缓冲区序列号
     */
    const int                                   GetStageInputBinding(const AnsiString &name)const{return shader_resource->GetStageInputBinding(name);}
    const ShaderStage *                         GetStageInput       (const AnsiString &name)const{return shader_resource->GetStageInput(name);}
    const uint                                  GetStageInputCount  ()                      const{return shader_resource->GetStageInputCount();}    
    const ShaderStageList &                     GetStageInputs      ()                      const{return shader_resource->GetStageInputs();}

    const uint32_t                              GetAttrCount()const{return attr_count;}

    const VkVertexInputBindingDescription *     GetDescList ()const{return binding_list;}
    const VkVertexInputAttributeDescription *   GetAttrList ()const{return attribute_list;}

    const VkVertexInputBindingDescription *     GetDesc     (const uint32_t index)const{return (index>=attr_count?nullptr:binding_list+index);}
    const VkVertexInputAttributeDescription *   GetAttr     (const uint32_t index)const{return (index>=attr_count?nullptr:attribute_list+index);}

public:

    VertexAttributeBinding *                    CreateVertexAttributeBinding();
    bool                                        Release(VertexAttributeBinding *);
    const uint32_t                              GetInstanceCount()const{return vab_sets.GetCount();}
};//class VertexShaderModule:public ShaderModule
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE
