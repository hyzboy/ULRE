#pragma once

#include <hgl/CoreType.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/type/ValueArray.h>
#include <hgl/util/hash/FNV1a.h>
#include <hgl/shadergen/ShaderStageKey.h>

namespace hgl::graph::mtl
{
    enum class ShaderStageValueType : uint32
    {
        Unknown = 0,
        Float,
        Vec2,
        Vec3,
        Vec4,
        Int,
        UInt,
        Bool
    };

    enum class ShaderStageInterfaceFlags : uint32
    {
        None = 0,
        Flat = 1u << 0,
        NoPerspective = 1u << 1,
        Centroid = 1u << 2,
        Sample = 1u << 3
    };

    enum class ShaderStageResourceKind : uint32
    {
        Unknown = 0,
        UniformBuffer,
        StorageBuffer,
        SampledImage,
        Sampler,
        CombinedImageSampler
    };

    struct ShaderStageInterfaceVariable
    {
        uint64 symbol_id = 0;
        ShaderStageValueType value_type = ShaderStageValueType::Unknown;
        uint32 location = 0;
        uint32 flags = 0;

        bool operator==(const ShaderStageInterfaceVariable &rhs) const noexcept
        {
            return symbol_id == rhs.symbol_id
                && value_type == rhs.value_type
                && location == rhs.location
                && flags == rhs.flags;
        }
    };

    struct ShaderStageResourceRequirement
    {
        uint64 resource_id = 0;
        ShaderStageResourceKind kind = ShaderStageResourceKind::Unknown;
        uint32 set = 0;
        uint32 binding = 0;
        uint32 value_type = 0;
        uint32 flags = 0;

        bool operator==(const ShaderStageResourceRequirement &rhs) const noexcept
        {
            return resource_id == rhs.resource_id
                && kind == rhs.kind
                && set == rhs.set
                && binding == rhs.binding
                && value_type == rhs.value_type
                && flags == rhs.flags;
        }
    };

    template<typename T>
    inline uint64 HashShaderStageValueArray(uint64 hash, const ValueArray<T> &values) noexcept
    {
        const uint32 count = static_cast<uint32>(values.GetCount());
        hash = hgl::hash::FNV1aAppendValueBytes(hash, count);

        for (uint32 i = 0; i < count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(hash, values[static_cast<int>(i)]);

        return hash;
    }

    struct ShaderStageBuildSpec
    {
        ShaderStage stage = ShaderStage::Vertex;
        uint64 definition_hash = 0;
        uint64 compiler_hash = 0;

        ValueArray<ShaderStageInterfaceVariable> inputs;
        ValueArray<ShaderStageInterfaceVariable> outputs;
        ValueArray<ShaderStageResourceRequirement> resources;

        uint64 GetInterfaceHash() const noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, stage);
            hash = HashShaderStageValueArray(hash, inputs);
            hash = HashShaderStageValueArray(hash, outputs);
            return hash;
        }

        uint64 GetResourceHash() const noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = HashShaderStageValueArray(hash, resources);
            return hash;
        }

        ShaderStageKey BuildKey() const noexcept
        {
            ShaderStageKey key;
            key.stage = stage;
            key.definition_hash = definition_hash;
            key.interface_hash = GetInterfaceHash();
            key.resource_hash = GetResourceHash();
            key.compiler_hash = compiler_hash;
            return key;
        }
    };

    inline bool HasCompatibleStageInterface(const ShaderStageBuildSpec &vertex,
                                             const ShaderStageBuildSpec &fragment) noexcept
    {
        for (int i = 0; i < fragment.inputs.GetCount(); ++i)
        {
            const ShaderStageInterfaceVariable &fragment_input = fragment.inputs[i];
            bool found = false;

            for (int j = 0; j < vertex.outputs.GetCount(); ++j)
            {
                const ShaderStageInterfaceVariable &vertex_output = vertex.outputs[j];
                if (vertex_output.location == fragment_input.location
                 && vertex_output.value_type == fragment_input.value_type
                 && vertex_output.flags == fragment_input.flags)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                return false;
        }

        return true;
    }
}
