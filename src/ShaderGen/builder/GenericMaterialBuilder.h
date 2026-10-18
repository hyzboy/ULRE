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

        // Phase 3.5: 声明键（emit 之前）
        //
        // stage key 的每个分量都必须能在生成 GLSL **之前**求出——运行时不该为了
        // 拿缓存键先跑一遍代码生成。这里把声明侧的全部输入先固化下来：
        //
        //   template_base_hash —— 骨架 .glsl.tmpl 正文 + 其硬编码 #include 基座
        //                        的传递内容闭包（改基座必须让键失效）
        //   module_graph_hash  —— 模板模块根 + 材质模块闭包的内容哈希
        //                        （manifest.stable_hash / resolved_template_hash）
        //   definition_hash    —— 上面两者 + 全部声明输入（ComposeInput /
        //                        vertex ABI / mesh 模式）；语义等同"生成后的文本"
        //   interface_hash     —— 片元输入 varying + 输出附件 + alpha 写输出
        //
        // FinalizeProgramLink 直接用这四个值构造 ShaderStageKey，不再读 plan.ms/plan.fs。
        // 发射器 C++ 逻辑变化不在这些哈希的覆盖范围内——由
        // SHADERGEN_CODEGEN_GENERATION 手工代次统一失效（见 ShaderGenProfileTargetVersion.h）。
        struct DeclarationKey
        {
            uint64 template_base_hash = 0;
            uint64 module_graph_hash = 0;
            uint64 definition_hash = 0;
            uint64 interface_hash = 0;
            /// 声明闭包覆盖的 include 路径（"ao/identity.glsl" 口径，与文本闭包同源可比）
            std::vector<AnsiString> include_paths;
            bool ready = false;
        };

        DeclarationKey declaration_mesh;
        DeclarationKey declaration_fragment;

        // Phase 3.5 产出、Phase 4 原样消费的发射输入（同一份对象，防止两处各自
        // 拼装导致"键覆盖的输入"与"实际发射的输入"悄悄分叉）
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
