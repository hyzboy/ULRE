#ifndef HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE
#define HGL_GRAPH_VULKAN_SHADER_MODULE_INCLUDE

#include"VK.h"
#include<hgl/type/BaseString.h>
#include<hgl/type/Map.h>

VK_NAMESPACE_BEGIN

using ShaderBindingList=List<uint32_t>;                     ///<shader绑定点列表

class ShaderParse;

/**
 * Shader模块<br>
 * 该模块提供的是原始的shader数据和信息，不可被修改，只能通过ShaderModuleManage创建和删除
 */
class ShaderModule
{
    VkDevice device;
    int shader_id;
    int ref_count;

private:

    VkPipelineShaderStageCreateInfo *stage_create_info;

    Map<UTF8String,int> ubo_map;
    ShaderBindingList ubo_list;

protected:

    void ParseUBO(const ShaderParse *);

public:

    ShaderModule(VkDevice dev,int id,VkPipelineShaderStageCreateInfo *pssci,const ShaderParse *);
    virtual ~ShaderModule();

    const int GetID()const{return shader_id;}

    const int IncRef(){return ++ref_count;}
    const int DecRef(){return --ref_count;}

public:

    const VkShaderStageFlagBits             GetStage        ()const{return stage_create_info->stage;}
    const VkPipelineShaderStageCreateInfo * GetCreateInfo   ()const{return stage_create_info;}

    const int                               GetUBO          (const UTF8String &name)const
    {
        int binding;

        if(ubo_map.Get(name,binding))
            return binding;
        else
            return -1;
    }

    const ShaderBindingList &               GetUBOBindingList()const{return ubo_list;}    ///<取得UBO绑定点列表
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

    Map<UTF8String,VkVertexInputAttributeDescription *> stage_input_locations;

    Set<VertexAttributeBinding *> vab_sets;

public:

    VertexShaderModule(VkDevice dev,int id,VkPipelineShaderStageCreateInfo *pssci,const ShaderParse *parse);
    virtual ~VertexShaderModule();

    /**
     * 获取输入流绑定点，需要注意的时，这里获取的binding并非是shader中的binding/location，而是绑定顺序的序列号。对应vkCmdBindVertexBuffer的缓冲区序列号
     */
    const int                                   GetStageInputBinding(const UTF8String &)const;

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
