#pragma once

/// MaterialProgramAssembler.h — phase-split generic material compilation.
///
/// AssembleMaterialProgram is decomposed into five ordered phases that
/// share a MaterialProgramAssemblyPlan.

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/mtl/MaterialCoverageContract.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/ShaderLinkSpec.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/type/String.h>

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

    /// 物理固定阶段的编译输入。
    ///
    /// 这两个阶段不是"生成出来的"：片段永远由领域主着色器固定，网格通常由
    /// 构建器按形状加载固定源。把它们的输入从 `MaterialDefinitionBuildRequest`
    /// 里分出来，是为了让构建请求只描述"这次构建的上下文"，而固定阶段的
    /// 字节由后端显式传入——请求里少四个只有领域后端认识的字段。
    struct FixedStageInputs
    {
        /// 最终片段源（已含 options 溯源注释）。缺失即硬失败：片段拼装链已删除，
        /// 没有"退回去生成"这回事。
        const char *fragment_source = nullptr;

        /// 片段接口形状哈希（领域 + purpose + 覆盖判定）。与吃源文本的
        /// definition_hash 互补，两者在键里不互相掩盖。
        uint64 fragment_interface_hash = 0;

        /// 非空 = 覆盖网格阶段源（离线/回归通道）；调用方保证文本与已解析的
        /// mode / ABI / stage interface 相符。
        const char *mesh_source = nullptr;
    };

    struct MaterialProgramBuildCapture
    {
        uint64 mesh_source_shape_key = 0;
        bool mesh_source_was_fixed = false;
        /// 本次解析出的 `generated/mesh_options.glsl` 文本。装配器只发射文本、
        /// 不碰文件系统：落盘位置由后端决定（必须与片段 options 同一个根）。
        AnsiString mesh_options;

        void Clear() noexcept
        {
            mesh_source_shape_key = 0;
            mesh_source_was_fixed = false;
            mesh_options.Clear();
        }
    };

    /// 中间产物：AssembleMaterialProgram 各相位之间共享的全部状态。
    /// 字段对应原单函数中的局部变量；相位按原执行顺序逐行搬移。
    struct MaterialProgramAssemblyPlan
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
        // 求解出的顶点输入声明文本。生成器删除后它不再进任何 shader：只作为
        // 网格形状键（ComputeMeshSourceShapeKey）的哈希输入保留——19 份已落盘
        // 的 .mesh 文件名就是按它算出来的，改动会全体失配。
        AnsiString resolved_vertex_input_glsl;

        // Phase 2.5: mesh stage 发射模式（只依赖 profile + definition + primitive）
        MeshShaderMode mesh_shader_mode = MeshShaderMode::VertexPassthrough;
        uint32 mesh_max_invocations = 0;

        hgl::graph::PrimitiveType primitive_type = hgl::graph::PrimitiveType::Triangles;

        // Phase 3: resource manifest
        ShaderCodeResourceManifest manifest;
        MaterialDefinition manifest_definition;

        // Phase 4 产出、Phase 5 原样消费的发射输入
        mtl::OutputContract output_contract;

        // Phase 4: stage sources
        // 片段源**不在**这里生成：它由领域主着色器物理固定，构建请求带进来。
        AnsiString ms;   // mesh stage（唯一顶点路径——VS 已废弃）
        AnsiString fs;
        uint64 mesh_source_shape_key = 0;
        bool mesh_source_was_fixed = false;

        // Phase 5: program link + compile config
        MaterialDefinition contract_definition;
        mtl::ShaderLinkSpec program_link;
    };

    /// 统一 generic 材质编译入口（原 MaterialDefinitionRegistry.cpp 中
    /// AssembleMaterialProgram 的相位化版本）。返回编译好的 ShaderBuildContext*。
    mtl::ShaderBuildContext *AssembleMaterialProgram(
        const mtl::contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinitionBuildRequest &request,
        const MaterialDefinition &definition,
        MaterialProgramBuildCapture *build_capture,
        const FixedStageInputs &fixed);

    const AnsiString &GetLastMaterialProgramAssemblyError();
}
