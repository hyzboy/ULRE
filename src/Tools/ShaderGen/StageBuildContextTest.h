#pragma once

// StageBuildContextTest.h — 回归门专用的 stage key/接口一致性测试辅助。
// 原生产头 ShaderStageBuildContext.h（2026-09 清扫迁出）：生产路径构建
// ShaderStageKey 的唯一位置是 ShaderKeyUtility/GenericMaterialBuilder，
// 本结构仅供门用例构造键与做 mesh↔fragment 接口一致性断言。
// 已相对原生产版删减：resources 集合与 GetResourceHash（全库零使用）、
// ShaderStageInterfaceFlags / ShaderStageResourceKind /
// ShaderStageResourceRequirement（零使用）。

#include <hgl/CoreType.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/type/ValueArray.h>
#include <hgl/util/hash/FNV1a.h>
#include <hgl/mtl/ShaderStageKey.h>
#include <hgl/mtl/ShaderStageValueType.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;

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

    template<typename T>
    inline void HashShaderStageValueArray(hgl::hash::FNV1aHasher64 &h,
                                          const ValueArray<T> &values) noexcept
    {
        const uint32 count = static_cast<uint32>(values.GetCount());
        h << count;

        for (uint32 i = 0; i < count; ++i)
            h << values[static_cast<int>(i)];
    }

    struct ShaderStageBuildContext
    {
        ShaderStage stage = ShaderStage::Mesh;
        uint64 definition_hash = 0;
        uint64 glsl_module_graph_hash = 0;
        uint64 compiler_hash = 0;

        ValueArray<ShaderStageInterfaceVariable> inputs;
        ValueArray<ShaderStageInterfaceVariable> outputs;

        uint64 GetInterfaceHash() const noexcept
        {
            hgl::hash::FNV1aHasher64 h;

            h << stage;
            HashShaderStageValueArray(h, inputs);
            HashShaderStageValueArray(h, outputs);
            return h;
        }

        ShaderStageKey BuildKey() const noexcept
        {
            ShaderStageKey key;
            key.stage = stage;
            key.definition_hash = definition_hash;
            key.glsl_module_graph_hash = glsl_module_graph_hash;
            key.interface_hash = GetInterfaceHash();
            key.resource_hash = 0;
            key.compiler_hash = compiler_hash;
            return key;
        }

        ShaderStageKey BuildKeyWithProviderGraphHash(
            const uint64 provider_graph_hash) const noexcept
        {
            ShaderStageKey key = BuildKey();
            key.glsl_module_graph_hash = provider_graph_hash;
            return key;
        }
    };
}
