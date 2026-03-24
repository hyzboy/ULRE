#include<hgl/mtl/UBOCommon.h>
#include <hgl/common/ShaderDescriptorDef.h>
#include<cstring>

namespace hgl::graph::mtl{

static bool CStrEq(const char *lhs,const char *rhs)
{
    return lhs && rhs && std::strcmp(lhs,rhs)==0;
}

UBODescriptor *CreateUBODescriptor(const UBODescriptorSemantic semantic,const uint32_t flag_bits)
{
    const auto &meta = GetDescriptorSemanticMeta(semantic);

    UBODescriptor *ubo=new UBODescriptor;

    ubo->type=meta.struct_name;

    hgl::strcpy(ubo->name,DESCRIPTOR_NAME_MAX_LENGTH,meta.name);

    ubo->semantic=semantic;
    ubo->stage_flag=flag_bits;

    return ubo;
}

SSBODescriptor *CreateSSBODescriptor(const SSBODescriptorSemantic semantic,const uint32_t flag_bits)
{
    const auto &meta = GetDescriptorSemanticMeta(semantic);

    SSBODescriptor *ssbo=new SSBODescriptor;

    ssbo->type=meta.struct_name;

    hgl::strcpy(ssbo->name,DESCRIPTOR_NAME_MAX_LENGTH,meta.name);

    ssbo->semantic=semantic;
    ssbo->stage_flag=flag_bits;

    return ssbo;
}

const ShaderBufferSource *FindShaderBufferSourceByStructName(const char *struct_name)
{
    if(!struct_name||!*struct_name)
        return nullptr;

    static const ShaderBufferSource *const sbs_list[] =
    {
        &SBS_ViewportInfo,
        &SBS_CameraInfo,
        &SBS_SkyInfo,
        &SBS_LocalToWorld,
        &SBS_TransformID,
        &SBS_MaterialInstanceID,
        &SBS_MaterialInstance,
        &SBS_MaterialInstanceTextureID,
        &SBS_ColorPattle,
        &SBS_JointInfo,
    };

    for (const ShaderBufferSource *sbs : sbs_list)
    {
        if (sbs && CStrEq(struct_name, sbs->struct_name))
            return sbs;
    }

    return nullptr;
}
}//namespace hgl::graph::mtl
