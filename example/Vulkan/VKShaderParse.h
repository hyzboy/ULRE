#pragma once
#include"spirv_cross.hpp"
#include<vulkan/vulkan.h>
#include<hgl/type/BaseString.h>

using namespace hgl;

VK_NAMESPACE_BEGIN
class ShaderParse
{
    spirv_cross::Compiler *compiler;
    spirv_cross::ShaderResources resource;

public:

    ShaderParse(const void *spv_data,const uint32_t spv_size)
    {
        compiler=new spirv_cross::Compiler((const uint32_t *)spv_data,spv_size/sizeof(uint32_t));

        resource=compiler->get_shader_resources();
    }

    ~ShaderParse()
    {
        delete compiler;
    }

    const spirv_cross::SmallVector<spirv_cross::Resource> &GetUBO()const{return resource.uniform_buffers;}
    const spirv_cross::SmallVector<spirv_cross::Resource> &GetStageInput()const{return resource.stage_inputs;}

public:

    const UTF8String GetName(const spirv_cross::Resource &res)const
    {
        return UTF8String(compiler->get_name(res.id).c_str());
    }

    const uint32_t GetBinding(const spirv_cross::Resource &res)const
    {
        return compiler->get_decoration(res.id,spv::DecorationBinding);
    }

    const uint32_t GetLocation(const spirv_cross::Resource &res)const
    {
        return compiler->get_decoration(res.id,spv::DecorationLocation);
    }

    const VkFormat GetFormat(const spirv_cross::Resource &res)const
    {
        const spirv_cross::SPIRType &type=compiler->get_type(res.type_id);
        
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
};//class ShaderParse
VK_NAMESPACE_END