#include "ShaderReflectionData.h"

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        template<typename Source, typename T>
        bool CopyItems(
            const Source *source,
            const uint32_t count,
            ValueArray<T> &destination)
        {
            static_assert(sizeof(Source) == sizeof(T));

            if (count > 0x7fffffffu
             || (count > 0 && !source)
             || static_cast<size_t>(count)
                    > static_cast<size_t>(-1) / sizeof(T))
                return false;

            if (!destination.Resize(static_cast<int>(count)))
                return false;

            if (count > 0)
                std::memcpy(destination.GetData(), source, size_t(count) * sizeof(T));

            return true;
        }
    }

    void ShaderReflectionData::Clear()
    {
        shader_stage = 0;
        entrypoint.Clear();
        stage_inputs.Clear();
        stage_outputs.Clear();
        for (ValueArray<ShaderReflectionDescriptor> &items : resources)
            items.Clear();
        push_constants.Clear();
        subpass_inputs.Clear();
    }

    bool ShaderReflectionData::Extract(const ShaderFlatView &view)
    {
        Clear();

        if (view.shader_stage == 0
         || !view.entrypoint
         || !CopyItems(view.inputs, view.input_count, stage_inputs)
         || !CopyItems(view.outputs, view.output_count, stage_outputs)
         || !CopyItems(view.push_constants, view.push_constant_count, push_constants)
         || !CopyItems(view.subpass_inputs, view.subpass_input_count, subpass_inputs))
        {
            Clear();
            return false;
        }

        for (uint32_t type = 0; type < SF_DESCRIPTOR_TYPE_COUNT; ++type)
        {
            if (!CopyItems(
                    view.resources[type],
                    view.resource_count[type],
                    resources[type]))
            {
                Clear();
                return false;
            }
        }

        shader_stage = view.shader_stage;
        entrypoint = view.entrypoint;
        return true;
    }

    const ShaderReflectionDescriptor *ShaderReflectionData::FindDescriptor(
        const uint32_t descriptor_type,
        const uint8_t set,
        const uint8_t binding) const noexcept
    {
        if (descriptor_type >= SF_DESCRIPTOR_TYPE_COUNT)
            return nullptr;

        const ValueArray<ShaderReflectionDescriptor> &items = resources[descriptor_type];
        for (int i = 0; i < items.GetCount(); ++i)
        {
            const ShaderReflectionDescriptor &item = items[i];
            if (item.set == set && item.binding == binding)
                return &item;
        }

        return nullptr;
    }

    bool ShaderReflectionData::GetPushConstantEnd(uint32_t &out_end) const noexcept
    {
        out_end = 0;
        for (int i = 0; i < push_constants.GetCount(); ++i)
        {
            const ShaderReflectionPushConstant &range = push_constants[i];
            if (range.size > uint32_t(-1) - range.offset)
            {
                out_end = 0;
                return false;
            }

            const uint32_t range_end = range.offset + range.size;
            if (range_end > out_end)
                out_end = range_end;
        }

        return true;
    }
}
