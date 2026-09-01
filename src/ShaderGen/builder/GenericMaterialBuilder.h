#pragma once

/// GenericMaterialBuilder.h — phase-split generic material compilation.
///
/// BuildGenericMaterial is decomposed into five ordered phases that
/// share a GenericMaterialBuildPlan.

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/SerializedDescriptorEntry.h>
#include <hgl/mtl/ModuleResourceManifest.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/mtl/DescriptorContract.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/ShaderLinkSpec.h>

#include <string>
#include <vector>

namespace hgl::graph::mtl
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


    /// 中间产物：BuildGenericMaterial 各相位之间共享的全部状态。
    /// 字段对应原单函数中的局部变量；相位按原执行顺序逐行搬移。
    struct GenericMaterialBuildPlan
    {
        // Phase 1: purpose / coverage / varying / stage interface
        mtl::ShaderProgramPurpose purpose =
            mtl::ShaderProgramPurpose::ForwardColor;
        bool depth_purpose = false;
        mtl::MaterialCoverageContract coverage;
        MaterialVertexVaryingConfig effective_vertex_varying;
        MaterialDefinition vertex_definition;
        MaterialVertexVaryingConfig varying;
        hgl::ValueArray<mtl::InterStageSemanticContractEntry>
            stage_interface;

        // Phase 2: vertex ABI
        VkFormat position_format = VK_FORMAT_UNDEFINED;
        VertexShaderNodeConfig vertex_node_config;
        std::string resolved_vertex_input_glsl;
        std::string resolved_provider_glsl;
        uint64 resolved_provider_graph_hash = 0;

        hgl::graph::PrimitiveType primitive_type = hgl::graph::PrimitiveType::Triangles;

        // Phase 3: resource contract
        ModuleResourceManifest manifest;
        MaterialDefinition manifest_definition;
        std::vector<SerializedDescriptorEntry> descriptors;
        mtl::DescriptorContract descriptor_contract;

        // Phase 4: stage sources
        std::string ms;   // mesh stage（唯一顶点路径——VS 已废弃）
        std::string fs;
        mtl::OutputContract output_contract;

        // Phase 5: program link + compile config
        MaterialDefinition contract_definition;
        mtl::ShaderLinkSpec program_link;
    };

    /// 统一 generic 材质编译入口（原 MaterialDefinitionRegistry.cpp 中
    /// BuildGenericMaterial 的相位化版本）。返回编译好的 ShaderBuildContext*。
    mtl::ShaderBuildContext *BuildGenericMaterial(
        const mtl::contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinitionBuildRequest &request,
        const MaterialDefinition &definition);
}
