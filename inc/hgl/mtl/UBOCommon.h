#pragma once

#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include <hgl/common/ShaderDescriptorDef.h>
#include <memory>

namespace hgl::graph::mtl{

std::unique_ptr<UBODescriptor> CreateUBODescriptorOwned(const UBODescriptorSemantic semantic,const uint32_t flag_bits);
std::unique_ptr<SSBODescriptor> CreateSSBODescriptorOwned(const SSBODescriptorSemantic semantic,const uint32_t flag_bits);

UBODescriptor *CreateUBODescriptor(const UBODescriptorSemantic semantic,const uint32_t flag_bits);
SSBODescriptor *CreateSSBODescriptor(const SSBODescriptorSemantic semantic,const uint32_t flag_bits);

}//namespace hgl::graph::mtl
