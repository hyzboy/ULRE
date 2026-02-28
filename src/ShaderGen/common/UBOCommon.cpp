#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/vk/VKShaderDescriptor.h>
#include<cstring>

namespace hgl::graph::mtl{
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

const ShaderBufferSource *FindShaderBufferSourceByStructName(const char *struct_name)
{
    if(!struct_name||!*struct_name)
        return nullptr;

    if(std::strcmp(struct_name,SBS_ViewportInfo.struct_name)==0)     return &SBS_ViewportInfo;
    if(std::strcmp(struct_name,SBS_CameraInfo.struct_name)==0)       return &SBS_CameraInfo;
    if(std::strcmp(struct_name,SBS_LocalToWorld.struct_name)==0)     return &SBS_LocalToWorld;
    if(std::strcmp(struct_name,SBS_MaterialInstance.struct_name)==0) return &SBS_MaterialInstance;
    if(std::strcmp(struct_name,SBS_JointInfo.struct_name)==0)        return &SBS_JointInfo;
    if(std::strcmp(struct_name,SBS_SkyInfo.struct_name)==0)          return &SBS_SkyInfo;

    return nullptr;
}
}//namespace hgl::graph::mtl
