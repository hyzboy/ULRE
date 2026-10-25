#pragma once

#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>
#include "SPVParseFlat.h"

#include <cstring>

namespace hgl::graph::mtl
{
    #pragma pack(push, 1)
    struct ShaderReflectionAttribute
    {
        char name[SF_NAME_MAX_LENGTH]{};
        uint8_t location = 0;
        uint8_t basetype = 0;
        uint8_t vec_size = 0;
        uint8_t col_count = 0;

        bool operator==(const ShaderReflectionAttribute &rhs) const noexcept
        {
            return std::memcmp(name, rhs.name, sizeof(name)) == 0
                && location == rhs.location
                && basetype == rhs.basetype
                && vec_size == rhs.vec_size
                && col_count == rhs.col_count;
        }
    };

    struct ShaderReflectionDescriptor
    {
        char name[SF_NAME_MAX_LENGTH]{};
        uint8_t set = 0;
        uint8_t binding = 0;
        uint8_t descriptor_type = 0;
        uint8_t array_size = 0;

        bool operator==(const ShaderReflectionDescriptor &rhs) const noexcept
        {
            return std::memcmp(name, rhs.name, sizeof(name)) == 0
                && set == rhs.set
                && binding == rhs.binding
                && descriptor_type == rhs.descriptor_type
                && array_size == rhs.array_size;
        }
    };

    struct ShaderReflectionPushConstant
    {
        char name[SF_NAME_MAX_LENGTH]{};
        uint32_t offset = 0;
        uint32_t size = 0;

        bool operator==(const ShaderReflectionPushConstant &rhs) const noexcept
        {
            return std::memcmp(name, rhs.name, sizeof(name)) == 0
                && offset == rhs.offset
                && size == rhs.size;
        }
    };

    struct ShaderReflectionSubpassInput
    {
        char name[SF_NAME_MAX_LENGTH]{};
        uint8_t input_attachment_index = 0;
        uint8_t binding = 0;

        bool operator==(const ShaderReflectionSubpassInput &rhs) const noexcept
        {
            return std::memcmp(name, rhs.name, sizeof(name)) == 0
                && input_attachment_index == rhs.input_attachment_index
                && binding == rhs.binding;
        }
    };
    #pragma pack(pop)

    class ShaderReflectionData
    {
    public:
        uint32_t shader_stage = 0;
        AnsiString entrypoint;
        ValueArray<ShaderReflectionAttribute> stage_inputs;
        ValueArray<ShaderReflectionAttribute> stage_outputs;
        ValueArray<ShaderReflectionDescriptor> resources[SF_DESCRIPTOR_TYPE_COUNT];
        ValueArray<ShaderReflectionPushConstant> push_constants;
        ValueArray<ShaderReflectionSubpassInput> subpass_inputs;

        void Clear();
        bool Extract(const ShaderFlatView &view);

        const ShaderReflectionDescriptor *FindDescriptor(
            uint32_t descriptor_type,
            uint8_t set,
            uint8_t binding) const noexcept;

        bool GetPushConstantEnd(uint32_t &out_end) const noexcept;
    };
}
