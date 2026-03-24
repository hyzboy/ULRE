#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<cstdio>
#include<string>
#include<hgl/mtl/MaterialVariantDesc.h>

namespace hgl::graph::mtl
{
namespace
{
    // ─────────────────────────────────────────────────────────────────────────────
    // 顶点输入和描述符定义
    // ─────────────────────────────────────────────────────────────────────────────

    constexpr FixedVertexEntry GIZMO_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Normal },
    };

    const FixedUBODescriptors GIZMO_3D_UBOS = {
        {UBODescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::CameraInfo,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    const FixedSSBODescriptors GIZMO_3D_SSBOS = {
        {SSBODescriptorSemantic::LocalToWorld,       uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {SSBODescriptorSemantic::TransformID,        uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstanceID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstance,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // 材质实例定义
    // ─────────────────────────────────────────────────────────────────────────────

    constexpr const char GIZMO_3D_MI_GLSL[] = "vec4 Color;";
    constexpr uint32_t GIZMO_3D_MI_BYTES = sizeof(math::Vector4f);

    const FixedMaterialDef GIZMO_3D_DEF {
        "Gizmo3D",
        PrimitiveType::Triangles,
        GIZMO_3D_VERTEX,
        uint32_t(sizeof(GIZMO_3D_VERTEX) / sizeof(GIZMO_3D_VERTEX[0])),
        &GIZMO_3D_UBOS,
        &GIZMO_3D_SSBOS,
        nullptr,
        GIZMO_3D_MI_GLSL,
        GIZMO_3D_MI_BYTES,
    };
}

MaterialCreateInfo *CreateGizmo3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    MaterialVariantKey var_key;
    var_key.SetDebugShading(true);
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[Gizmo3D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler("ShaderLibrary");

    auto result = assembler.Assemble(var_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[Gizmo3D] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    if(cfg)
        cfg->material_instance=true;

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        GIZMO_3D_DEF,
        result.vertex_glsl,
        result.fragment_glsl,
        cfg);

    if (!mci)
        std::fprintf(stderr, "[Gizmo3D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
