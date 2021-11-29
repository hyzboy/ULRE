#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttributeBinding.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
ShaderModule *GPUDevice::CreateShaderModule(ShaderResource *sr)
{
    if(!sr)return(nullptr);

    PipelineShaderStageCreateInfo *shader_stage=new PipelineShaderStageCreateInfo(sr->GetStage());

    ShaderModuleCreateInfo moduleCreateInfo(sr);

    if(vkCreateShaderModule(attr->device,&moduleCreateInfo,nullptr,&(shader_stage->module))!=VK_SUCCESS)
        return(nullptr);

    ShaderModule *sm;

    if(sr->GetStage()==VK_SHADER_STAGE_VERTEX_BIT)
        sm=new VertexShaderModule(attr->device,shader_stage,sr);
    else
        sm=new ShaderModule(attr->device,shader_stage,sr);

    return sm;
}

ShaderModule::ShaderModule(VkDevice dev,VkPipelineShaderStageCreateInfo *sci,ShaderResource *sr)
{
    device=dev;
    ref_count=0;

    stage_create_info=sci;

    shader_resource=sr;
}

ShaderModule::~ShaderModule()
{
    vkDestroyShaderModule(device,stage_create_info->module,nullptr);
    //这里不用删除stage_create_info，材质中会删除的
}

VertexShaderModule::VertexShaderModule(VkDevice dev,VkPipelineShaderStageCreateInfo *pssci,ShaderResource *sr):ShaderModule(dev,pssci,sr)
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

VAB *VertexShaderModule::CreateVAB()
{
    VAB *vab=new VAB(attr_count,binding_list,attribute_list);

    vab_sets.Add(vab);

    return(vab);
}

bool VertexShaderModule::Release(VAB *vab)
{
    return vab_sets.Delete(vab);
}
VK_NAMESPACE_END
