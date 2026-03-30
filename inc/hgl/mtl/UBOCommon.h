#pragma once

#include<hgl/mtl/DescriptorSemanticRegistry.h>

namespace hgl::graph{
struct UBODescriptor;
struct SSBODescriptor;
}//namespace hgl::graph

namespace hgl::graph::mtl{

UBODescriptor *CreateUBODescriptor(const UBODescriptorSemantic semantic,const uint32_t flag_bits);
SSBODescriptor *CreateSSBODescriptor(const SSBODescriptorSemantic semantic,const uint32_t flag_bits);

}//namespace hgl::graph::mtl
