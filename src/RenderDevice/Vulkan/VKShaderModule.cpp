#include<hgl/graph/vulkan/VKShaderModule.h>
#include<hgl/graph/vulkan/VKVertexAttributeBinding.h>
#include<hgl/graph/vulkan/VKShaderParse.h>

VK_NAMESPACE_BEGIN
ShaderModule::ShaderModule(int id,VkPipelineShaderStageCreateInfo *sci,const ShaderParse *sp)
{
    shader_id=id;
    ref_count=0;

    stage_create_info=sci;

    ParseUBO(sp);
}

ShaderModule::~ShaderModule()
{
    delete stage_create_info;
}

void ShaderModule::ParseUBO(const ShaderParse *parse)
{
    for(const auto &ubo:parse->GetUBO())
    {
        const UTF8String &  name    =parse->GetName(ubo);
        const uint          binding =parse->GetBinding(ubo);

        ubo_map.Add(name,binding);
        ubo_list.Add(binding);
    }
}

void VertexShaderModule::ParseVertexInput(const ShaderParse *parse)
{
    const auto &stage_inputs=parse->GetStageInput();

    attr_count=stage_inputs.size();
    binding_list=new VkVertexInputBindingDescription[attr_count];
    attribute_list=new VkVertexInputAttributeDescription[attr_count];

    VkVertexInputBindingDescription *bind=binding_list;
    VkVertexInputAttributeDescription *attr=attribute_list;

    uint32_t binding_index=0;

    for(const auto &si:stage_inputs)
    {
        const VkFormat                  format  =parse->GetFormat(si);                //注意这个格式有可能会解析不出来(比如各种压缩格式)
        const UTF8String &              name    =parse->GetName(si);

        bind->binding   =binding_index;                 //binding对应在vkCmdBindVertexBuffer中设置的缓冲区的序列号，所以这个数字必须从0开始，而且紧密排列。
                                                        //在VertexInput类中，buf_list需要严格按照本此binding为序列号排列
        bind->stride    =GetStrideByFormat(format);
        bind->inputRate =VK_VERTEX_INPUT_RATE_VERTEX;

        //binding对应的是第几个数据输入流
        //实际使用一个binding可以绑定多个attrib
        //比如在一个流中传递{pos,color}这样两个数据，就需要两个attrib
        //但在我们的设计中，仅支持一个流传递一个attrib

        attr->binding   =binding_index;
        attr->location  =parse->GetLocation(si);                                        //此值对应shader中的layout(location=
        attr->format    =format;
        attr->offset    =0;

        stage_input_locations.Add(name,attr);

        ++attr;
        ++bind;
        ++binding_index;
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

const int VertexShaderModule::GetLocation(const UTF8String &name)const
{
    if(name.IsEmpty())return -1;

    VkVertexInputAttributeDescription *attr;

    if(!stage_input_locations.Get(name,attr))
        return -1;

    return attr->location;
}

const int VertexShaderModule::GetBinding(const UTF8String &name)const
{
    if(name.IsEmpty())return -1;

    VkVertexInputAttributeDescription *attr;

    if(!stage_input_locations.Get(name,attr))
        return -1;

    return attr->binding;
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
