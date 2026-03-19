#include<hgl/mtl/UBOCommon.h>
#include <hgl/common/ShaderDescriptorDef.h>
#include<cstring>

namespace hgl::graph::mtl{

static bool CStrEq(const char *lhs,const char *rhs)
{
    return lhs && rhs && std::strcmp(lhs,rhs)==0;
}

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

    if(CStrEq(struct_name,SBS_ViewportInfo.struct_name))     return &SBS_ViewportInfo;
    if(CStrEq(struct_name,SBS_CameraInfo.struct_name))       return &SBS_CameraInfo;
    if(CStrEq(struct_name,SBS_LocalToWorld.struct_name))     return &SBS_LocalToWorld;
    if(CStrEq(struct_name,SBS_TransformID.struct_name))      return &SBS_TransformID;
    if(CStrEq(struct_name,SBS_MaterialInstance.struct_name)) return &SBS_MaterialInstance;
        if(CStrEq(struct_name,SBS_MaterialInstanceID.struct_name)) return &SBS_MaterialInstanceID;
    if(CStrEq(struct_name,SBS_JointInfo.struct_name))        return &SBS_JointInfo;
    if(CStrEq(struct_name,SBS_SkyInfo.struct_name))          return &SBS_SkyInfo;

    return nullptr;
}
}//namespace hgl::graph::mtl
