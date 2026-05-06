#include<hgl/mtl/UBOCommon.h>
#include <hgl/common/ShaderDescriptorDef.h>
#include<cstring>
#include<memory>

namespace hgl::graph::mtl{

static bool CStrEq(const char *lhs,const char *rhs)
{
    return lhs && rhs && std::strcmp(lhs,rhs)==0;
}

std::unique_ptr<UBODescriptor> CreateUBODescriptorOwned(const UBODescriptorSemantic semantic,const uint32_t flag_bits)
{
    const auto &meta = GetDescriptorSemanticMeta(semantic);

    auto ubo=std::make_unique<UBODescriptor>();

    ubo->type=meta.struct_name;

    hgl::strcpy(ubo->name,DESCRIPTOR_NAME_MAX_LENGTH,meta.name);

    ubo->semantic=semantic;
    ubo->stage_flag=flag_bits;

    return ubo;
}

std::unique_ptr<SSBODescriptor> CreateSSBODescriptorOwned(const SSBODescriptorSemantic semantic,const uint32_t flag_bits)
{
    const auto &meta = GetDescriptorSemanticMeta(semantic);

    auto ssbo=std::make_unique<SSBODescriptor>();

    ssbo->type=meta.struct_name;

    hgl::strcpy(ssbo->name,DESCRIPTOR_NAME_MAX_LENGTH,meta.name);

    ssbo->semantic=semantic;
    ssbo->stage_flag=flag_bits;

    return ssbo;
}
}//namespace hgl::graph::mtl
