#pragma once

#include <hgl/type/DataType.h>

namespace hgl::graph
{
    // Must stay binary-compatible with src/Tools/GLSLCompiler/glsl2spv.cpp.
    enum VkDescriptorTypeLite : uint32
    {
        VK_DESCRIPTOR_TYPE_SAMPLER = 0,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER = 1,
        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE = 2,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE = 3,
        VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER = 4,
        VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER = 5,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 8,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 9,
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT = 10,
    };

    constexpr uint32 VK_DESCRIPTOR_TYPE_COUNT =
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1;

    constexpr size_t SHADER_RESOURCE_NAME_MAX_LENGTH = 32;

    struct ShaderAttribute
    {
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];
        uint8 location;
        uint8 basetype;
        uint8 vec_size;
    };

    struct ShaderAttributeArray
    {
        uint32 count;
        ShaderAttribute *items;
    };

    struct Descriptor
    {
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];
        uint8 set;
        uint8 binding;
    };

    struct PushConstant
    {
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];
        uint8 offset;
        uint8 size;
    };

    struct SubpassInput
    {
        char name[SHADER_RESOURCE_NAME_MAX_LENGTH];
        uint8 input_attachment_index;
        uint8 binding;
    };

    template<typename T>
    struct ShaderResourceData
    {
        uint32 count;
        T *items;
    };

    using ShaderDescriptorResource = ShaderResourceData<Descriptor>[VK_DESCRIPTOR_TYPE_COUNT];

    struct ShaderStageIO
    {
        ShaderAttributeArray input, output;
    };

    struct SPVParseData
    {
        ShaderStageIO stage_io;
        ShaderDescriptorResource resource;
        ShaderResourceData<PushConstant> push_constant;
        ShaderResourceData<SubpassInput> subpass_input;
    };
}
