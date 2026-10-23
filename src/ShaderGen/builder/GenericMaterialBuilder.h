#pragma once

/// GenericMaterialBuilder.h — phase-split generic material compilation.
///
/// BuildGenericMaterial is decomposed into five ordered phases that
/// share a GenericMaterialBuildPlan.

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/mtl/ShaderLinkSpec.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/type/String.h>

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
    struct MaterialShaderDocumentCapture;


    /// 中间产物：BuildGenericMaterial 各相位之间共享的全部状态。
    /// 字段对应原单函数中的局部变量；相位按原执行顺序逐行搬移。
    struct GenericMaterialBuildPlan
    {
        // Phase 1: purpose / coverage / varying / stage interface
        mtl::ShaderProgramPurpose purpose =
            mtl::ShaderProgramPurpose::ForwardColor;
        bool depth_purpose = false;

        // 曲面 alpha 写进片元输出（alpha_blend 需要；见 ComposeInput）
        bool alpha_to_output = false;
        RenderTemplateRequest render_template_request_storage;
        const RenderTemplateRequest *render_template_request = nullptr;
        ResolvedRenderTemplate resolved_render_template;
        uint64 resolved_template_hash = 0;
        mtl::MaterialCoverageContract coverage;
        MaterialVertexVaryingConfig effective_vertex_varying;
        MaterialDefinition vertex_definition;
        MaterialVertexVaryingConfig varying;
        hgl::ValueArray<mtl::InterStageSemanticContractEntry>
            stage_interface;

        // Phase 2: vertex ABI
        VkFormat position_format = VK_FORMAT_UNDEFINED;
        VertexShaderNodeConfig vertex_node_config;
        ShaderDocument resolved_vertex_input_document;

        // Phase 2.5: mesh stage 发射模式（只依赖 profile + definition + primitive）
        MeshShaderMode mesh_shader_mode = MeshShaderMode::VertexPassthrough;
        uint32 mesh_max_invocations = 0;

        hgl::graph::PrimitiveType primitive_type = hgl::graph::PrimitiveType::Triangles;

        // Phase 3: resource manifest
        ShaderCodeResourceManifest manifest;
        MaterialDefinition manifest_definition;

        // Phase 4 产出、Phase 5 原样消费的发射输入（同一份对象，防止两处各自
        // 拼装导致"声明侧看到的"与"实际发射的"悄悄分叉）
        FragmentTemplateComposer::ComposeInput compose_input;
        ShaderDocument code_module_document;
        mtl::OutputContract output_contract;

        // Phase 4: stage sources
        std::string ms;   // mesh stage（唯一顶点路径——VS 已废弃）
        std::string fs;
        ShaderDocument mesh_source_document;
        ShaderDocument fragment_source_document;

        // Phase 5: program link + compile config
        MaterialDefinition contract_definition;
        mtl::ShaderLinkSpec program_link;
    };

    /// 统一 generic 材质编译入口（原 MaterialDefinitionRegistry.cpp 中
    /// BuildGenericMaterial 的相位化版本）。返回编译好的 ShaderBuildContext*。
    mtl::ShaderBuildContext *BuildGenericMaterial(
        const mtl::contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinitionBuildRequest &request,
        const MaterialDefinition &definition,
        MaterialShaderDocumentCapture *document_capture);

    const AnsiString &GetLastBuildGenericMaterialError();
}
