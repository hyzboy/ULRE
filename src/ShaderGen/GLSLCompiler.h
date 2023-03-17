﻿#ifndef HGL_GLSL_COMPILER_INCLUDE
#define HGL_GLSL_COMPILER_INCLUDE

#include<hgl/type/String.h>
#include<hgl/type/Map.h>
#include<stdint.h>

namespace hgl
{
    namespace io
    {
        class MemoryOutputStream;
        class DataOutputStream;
    }
}

namespace glsl_compiler
{
    using namespace hgl;

    enum class DescriptorType         //等同VkDescriptorType
    {
        SAMPLER = 0,
        COMBINED_IMAGE_SAMPLER,
        SAMPLED_IMAGE,
        STORAGE_IMAGE,
        UNIFORM_TEXEL_BUFFER,
        STORAGE_TEXEL_BUFFER,
        UNIFORM_BUFFER,
        STORAGE_BUFFER,
        UNIFORM_BUFFER_DYNAMIC,
        STORAGE_BUFFER_DYNAMIC,
        INPUT_ATTACHMENT,

        ENUM_CLASS_RANGE(SAMPLER,INPUT_ATTACHMENT)
    };
    
    constexpr size_t SHADER_RESOURCE_NAME_MAX_LENGTH=32;

    struct ShaderAttribute
    {
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];
        uint8_t location;
        uint8_t basetype;      //现在改对应hgl/graph/VertexAttrib.h中的enum class VertexAttribType::BaseType
        uint8_t vec_size;
    };//

    struct ShaderStageData
    {
        uint32_t count;
        ShaderAttribute *items;
    };

    struct Descriptor
    {
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];
        uint8_t set;
        uint8_t binding;
    };

    struct PushConstant
    {
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];
        uint8_t offset;
        uint8_t size;
    };

    struct SubpassInput
    {
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];
        uint8_t input_attachment_index;
        uint8_t binding;
    };

    template<typename T>
    struct ShaderResourceData
    {
        uint32_t count;
        T *items;
    };

    using ShaderDescriptorResource=ShaderResourceData<Descriptor>[size_t(DescriptorType::RANGE_SIZE)];

    struct SPVData
    {
        bool result;
        char *log;
        char *debug_log;

        uint32_t *spv_data;
        uint32_t spv_length;

        ShaderStageData                     input,
                                            output;
        ShaderDescriptorResource            resource;
        ShaderResourceData<PushConstant>    push_constant;
        ShaderResourceData<SubpassInput>    subpass_input;
    };

    bool Init();
    void Close();

    void        AddGLSLIncludePath(const char *);    
    void        RebuildGLSLIncludePath();

    uint32_t    GetType (const char *ext_name);
    
    SPVData *   Compile (const uint32_t type,const char *source);
    void        Free    (SPVData *spv_data);
}//namespace glsl_compiler
#endif//HGL_GLSL_COMPILER_INCLUDE