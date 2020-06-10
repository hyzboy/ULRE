#include<hgl/graph/vulkan/VKShaderModule.h>
#include<hgl/graph/vulkan/VKVertexAttributeBinding.h>

VK_NAMESPACE_BEGIN
ShaderModule::ShaderModule(VkDevice dev,int id,VkPipelineShaderStageCreateInfo *sci,ShaderResource *sr)
{
    device=dev;
    shader_id=id;
    ref_count=0;

    stage_create_info=sci;

    shader_resource=sr;
}

ShaderModule::~ShaderModule()
{
    vkDestroyShaderModule(device,stage_create_info->module,nullptr);
    delete stage_create_info;
}

VertexShaderModule::VertexShaderModule(VkDevice dev,int id,VkPipelineShaderStageCreateInfo *pssci,ShaderResource *sr):ShaderModule(dev,id,pssci,sr)
{
    const ShaderStageList &stage_inputs=sr->GetStageInputs();

    attr_count=stage_inputs.GetCount();
    binding_list=new VkVertexInputBindingDescription[attr_count];
    attribute_list=new VkVertexInputAttributeDescription[attr_count];

    VkVertexInputBindingDescription *bind=binding_list;
    VkVertexInputAttributeDescription *attr=attribute_list;

    ShaderStage **si=stage_inputs.GetData();

    for(uint i=0;i<attr_count;i++)
    {
        bind->binding   =i;                             //binding对应在vkCmdBindVertexBuffer中设置的缓冲区的序列号，所以这个数字必须从0开始，而且紧密排列。
                                                        //在VertexInput类中，buf_list需要严格按照本此binding为序列号排列
        bind->stride    =GetStrideByFormat((*si)->format);
        bind->inputRate =VK_VERTEX_INPUT_RATE_VERTEX;

        //binding对应的是第几个数据输入流
        //实际使用一个binding可以绑定多个attrib
        //比如在一个流中传递{pos,color}这样两个数据，就需要两个attrib
        //但在我们的设计中，仅支持一个流传递一个attrib

        attr->binding   =i;
        attr->location  =(*si)->location;               //此值对应shader中的layout(location=
        attr->format    =(*si)->format;
        attr->offset    =0;

        ++attr;
        ++bind;

        ++si;
    }
}

VertexShaderModule::~VertexShaderModule()
{
    if(vab_sets.GetCount()>0)
    {
        //还有在用的，这是个错误
    }

    SAFE_CLEAR_ARRAY(binding_list);
    SAFE_CLEAR_ARRAY(attribute_list);
}

VertexAttributeBinding *VertexShaderModule::CreateVertexAttributeBinding()
{
    VertexAttributeBinding *vab=new VertexAttributeBinding(this);

    vab_sets.Add(vab);

    return(vab);
}

bool VertexShaderModule::Release(VertexAttributeBinding *vab)
{
    return vab_sets.Delete(vab);
}
VK_NAMESPACE_END
