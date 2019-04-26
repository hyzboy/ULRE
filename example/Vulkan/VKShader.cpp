#include"VKShader.h"
#include"VKVertexInput.h"
#include"spirv_cross.hpp"

VK_NAMESPACE_BEGIN
const VkFormat GetVecFormat(const spirv_cross::SPIRType &type)
{
    constexpr VkFormat format[][4]=
    {
        {FMT_R8I,FMT_RG8I,FMT_RGB8I,FMT_RGBA8I},    //sbyte
        {FMT_R8U,FMT_RG8U,FMT_RGB8U,FMT_RGBA8U},    //ubyte
        {FMT_R16I,FMT_RG16I,FMT_RGB16I,FMT_RGBA16I},//short
        {FMT_R16U,FMT_RG16U,FMT_RGB16U,FMT_RGBA16U},//ushort
        {FMT_R32I,FMT_RG32I,FMT_RGB32I,FMT_RGBA32I},//int
        {FMT_R32U,FMT_RG32U,FMT_RGB32U,FMT_RGBA32U},//uint
        {FMT_R64I,FMT_RG64I,FMT_RGB64I,FMT_RGBA64I},//int64
        {FMT_R64U,FMT_RG64U,FMT_RGB64U,FMT_RGBA64U},//uint64
        {}, //atomic
        {FMT_R16F,FMT_RG16F,FMT_RGB16F,FMT_RGBA16F},//half
        {FMT_R32F,FMT_RG32F,FMT_RGB32F,FMT_RGBA32F},//float
        {FMT_R64F,FMT_RG64F,FMT_RGB64F,FMT_RGBA64F} //double
    };

    if(type.basetype<spirv_cross::SPIRType::SByte
     ||type.basetype>spirv_cross::SPIRType::Double
     ||type.basetype==spirv_cross::SPIRType::AtomicCounter
     ||type.vecsize<1
     ||type.vecsize>4)
        return VK_FORMAT_UNDEFINED;

    return format[type.basetype-spirv_cross::SPIRType::SByte][type.vecsize-1];
}

bool Shader::CreateVIS(const void *spv_data,const uint32_t spv_size)
{
    spirv_cross::Compiler comp((const uint32_t *)spv_data,spv_size/sizeof(uint32_t));

    spirv_cross::ShaderResources res=comp.get_shader_resources();

    for(auto &si:res.stage_inputs)
    {
        const spirv_cross::SPIRType &   type    =comp.get_type(si.type_id);
        const VkFormat                  format  =GetVecFormat(type);

        if(format==VK_FORMAT_UNDEFINED)
            return(false);

        const uint32_t                  location=comp.get_decoration(si.id,spv::DecorationLocation);
        const UTF8String &              name    =comp.get_name(si.id).c_str();

        vertex_input_state->Add(name,location,format);        
    }

    return(true);
}

Shader::Shader(VkDevice dev)
{
    device=dev;

    vertex_input_state=new VertexInputState();
}

Shader::~Shader()
{
    delete vertex_input_state;

    const int count=shader_stage_list.GetCount();

    if(count>0)
    {
        VkPipelineShaderStageCreateInfo *ss=shader_stage_list.GetData();
        for(int i=0;i<count;i++)
        {
            vkDestroyShaderModule(device,ss->module,nullptr);
            ++ss;
        }
    }
}

bool Shader::Add(const VkShaderStageFlagBits shader_stage_bit,const void *spv_data,const uint32_t spv_size)
{   
    if(shader_stage_bit==VK_SHADER_STAGE_VERTEX_BIT)
        CreateVIS(spv_data,spv_size);

    VkPipelineShaderStageCreateInfo shader_stage;
    shader_stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage.pNext=nullptr;
    shader_stage.pSpecializationInfo=nullptr;
    shader_stage.flags=0;
    shader_stage.stage=shader_stage_bit;
    shader_stage.pName="main";

    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext=nullptr;
    moduleCreateInfo.flags=0;
    moduleCreateInfo.codeSize=spv_size;
    moduleCreateInfo.pCode=(const uint32_t *)spv_data;

    if(vkCreateShaderModule(device,&moduleCreateInfo,nullptr,&shader_stage.module)!=VK_SUCCESS)
        return(false);

    shader_stage_list.Add(shader_stage);

    return(true);
}

void Shader::Clear()
{
    shader_stage_list.Clear();
    vertex_input_state->Clear();
}
VK_NAMESPACE_END
