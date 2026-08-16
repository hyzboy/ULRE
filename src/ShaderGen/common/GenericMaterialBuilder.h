#pragma once

/// GenericMaterialBuilder.h — phase-split generic material compilation.
///
/// BuildGenericMaterial (formerly a ~390-line function in
/// MaterialDefinitionRegistry.cpp) is decomposed into five ordered phases that
/// share a GenericMaterialBuildPlan. The phases are behavior-preserving: they
/// execute exactly the same statements in the same order as the original
/// function, so every produced hash / stage key is identical (zero drift).

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/SerializedVertexEntry.h>
#include <hgl/mtl/SerializedDescriptorEntry.h>
#include <hgl/graph/glsl/ShaderResourceManifest.h>
#include <hgl/shadergen/CanonicalShaderContract.h>
#include <hgl/shadergen/DescriptorContract.h>
#include <hgl/shadergen/MaterialCoverageContract.h>
#include <hgl/shadergen/MaterialStageInterface.h>
#include <hgl/shadergen/ShaderLinkSpec.h>
#include "VertexShaderAssembler.h"      // shadergen::VertexVaryingConfig

#include <string>
#include <vector>

namespace hgl::graph::shadergen
{
    class ShaderBuildContext;
    namespace contract
    {
        struct PhysicalDeviceProfileLite;
    }
}

namespace hgl::graph::mtl
{
    struct MaterialDefinition;
    struct MaterialDefinitionBuildRequest;

    namespace shadergen = hgl::graph::shadergen;

    /// 中间产物：BuildGenericMaterial 各相位之间共享的全部状态。
    /// 字段对应原单函数中的局部变量；相位按原执行顺序逐行搬移。
    struct GenericMaterialBuildPlan
    {
        // Phase 1: purpose / coverage / varying / stage interface
        ResolvedMaterialRenderState render_state;
        shadergen::ShaderProgramPurpose purpose =
            shadergen::ShaderProgramPurpose::ForwardColor;
        bool depth_purpose = false;
        shadergen::MaterialCoverageContract coverage;
        MaterialVertexVaryingConfig effective_vertex_varying;
        MaterialDefinition vertex_definition;
        shadergen::VertexVaryingConfig varying;
        hgl::ValueArray<shadergen::InterStageSemanticContractEntry>
            stage_interface;

        // Phase 2: vertex ABI
        std::vector<SerializedVertexEntry> vertices;
        VkFormat position_format = VK_FORMAT_UNDEFINED;
        VertexShaderNodeConfig vertex_node_config;
        std::string resolved_vertex_input_glsl;
        std::string resolved_provider_glsl;
        uint64 resolved_provider_graph_hash = 0;

        // Phase 3: resource contract
        ShaderResourceManifest manifest;
        MaterialDefinition manifest_definition;
        std::vector<SerializedDescriptorEntry> descriptors;
        shadergen::DescriptorContract descriptor_contract;

        // Phase 4: stage sources
        std::string vs;
        std::string fs;
        shadergen::OutputContract output_contract;

        // Phase 5: program link + compile config
        MaterialDefinition contract_definition;
        shadergen::ShaderLinkSpec program_link;
    };

    /// 统一 generic 材质编译入口（原 MaterialDefinitionRegistry.cpp 中
    /// BuildGenericMaterial 的相位化版本）。返回编译好的 ShaderBuildContext*。
    shadergen::ShaderBuildContext *BuildGenericMaterial(
        const shadergen::contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinitionBuildRequest &request,
        const MaterialDefinition &definition);
}
