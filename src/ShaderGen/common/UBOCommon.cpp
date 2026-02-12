#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/VKShaderDescriptor.h>

STD_MTL_NAMESPACE_BEGIN
UBODescriptor *CreateUBODescriptor(const ShaderBufferSource &sbs,const uint32_t flag_bits)
{
    UBODescriptor *ubo=new UBODescriptor;

    ubo->type=sbs.struct_name;

    hgl::strcpy(ubo->name,DESCRIPTOR_NAME_MAX_LENGTH,sbs.name);

    ubo->stage_flag=flag_bits;

    return ubo;
}

SSBODescriptor *CreateSSBODescriptor(const ShaderBufferSource &sbs,const uint32_t flag_bits)
{
    SSBODescriptor *ssbo=new SSBODescriptor;

    ssbo->type=sbs.struct_name;

    hgl::strcpy(ssbo->name,DESCRIPTOR_NAME_MAX_LENGTH,sbs.name);

    ssbo->stage_flag=flag_bits;

    return ssbo;
}
STD_MTL_NAMESPACE_END
