#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexInputLayout.h>
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

    SAFE_CLEAR(shader_resource);
}

VertexShaderModule::VertexShaderModule(VkDevice dev,VkPipelineShaderStageCreateInfo *pssci,ShaderResource *sr):ShaderModule(dev,pssci,sr)
{
    const ShaderStageList &stage_input_list=sr->GetStageInputs();

    attr_count=stage_input_list.GetCount();
    ssi_list=stage_input_list.GetData();
    name_list=new const char *[attr_count];
    type_list=new VertexAttribType[attr_count];
    
    for(uint i=0;i<attr_count;i++)
    {
        name_list[i]            =ssi_list[i]->name;
        type_list[i].basetype   =VertexAttribType::BaseType(ssi_list[i]->basetype);
        type_list[i].vec_size   =ssi_list[i]->vec_size;
    }
}

VertexShaderModule::~VertexShaderModule()
{
    if(vil_sets.GetCount()>0)
    {
        //还有在用的，这是个错误
    }

    delete[] type_list;
    delete[] name_list;
}

const VkFormat GetVulkanFormat(const VertexAttribType::BaseType &base_type,const uint vec_size);    //VertexAttrib.cpp

VIL *VertexShaderModule::CreateVIL(const VILConfig *cfg)
{
    VkVertexInputBindingDescription *binding_list=new VkVertexInputBindingDescription[attr_count];
    VkVertexInputAttributeDescription *attribute_list=new VkVertexInputAttributeDescription[attr_count];

    VkVertexInputBindingDescription *bind=binding_list;
    VkVertexInputAttributeDescription *attr=attribute_list;

    ShaderStage **si=ssi_list;
    VAConfig vac;

    for(uint i=0;i<attr_count;i++)
    {
        //binding对应的是第几个数据输入流
        //实际使用一个binding可以绑定多个attrib
        //比如在一个流中传递{pos,color}这样两个数据，就需要两个attrib
        //但在我们的设计中，仅支持一个流传递一个attrib

        attr->binding   =i;
        attr->location  =(*si)->location;               //此值对应shader中的layout(location=
        
        attr->offset    =0;

        bind->binding   =i;                             //binding对应在vkCmdBindVertexBuffer中设置的缓冲区的序列号，所以这个数字必须从0开始，而且紧密排列。
                                                        //在Renderable类中，buffer_list必需严格按照本此binding为序列号排列

        if(!cfg||!cfg->Get((*si)->name,vac))
        {
            attr->format    =GetVulkanFormat(VertexAttribType::BaseType((*si)->basetype),(*si)->vec_size);

            //if(memcmp((*si)->name.c_str(),"Inst_",5)==0)                //不可以使用CaseComp("Inst_",5)会被认为是比较一个5字长的字符串，而不是只比较5个字符
            //    bind->inputRate =VK_VERTEX_INPUT_RATE_INSTANCE;
            //else
                bind->inputRate =VK_VERTEX_INPUT_RATE_VERTEX;
        }
        else
        {
            if(vac.format!=PF_UNDEFINED)
                attr->format    =vac.format;
            else                
                attr->format    =GetVulkanFormat(VertexAttribType::BaseType((*si)->basetype),(*si)->vec_size);

            bind->inputRate =vac.instance?VK_VERTEX_INPUT_RATE_INSTANCE:VK_VERTEX_INPUT_RATE_VERTEX;
        }

        bind->stride    =GetStrideByFormat(attr->format);

        ++attr;
        ++bind;

        ++si;
    }

    VIL *vil=new VIL(attr_count,name_list,type_list,binding_list,attribute_list);

    vil_sets.Add(vil);

    return(vil);
}

bool VertexShaderModule::Release(VIL *vil)
{
    return vil_sets.Delete(vil);
}
VK_NAMESPACE_END
