#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/UBOCommon.h>
#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/common/MeshShaderStreamContract.h>
#include <cstdio>
#include <vector>

#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/SamplerSlot.h>

#include "MaterialFactory3DCommon.h"
#include "StandardDescriptorBuilder.h"
#include "StandardVariantRouter.h"

namespace hgl::graph::mtl{
namespace
{
#if defined(ULRE_SHADERGEN_VERBOSE)
    constexpr bool kStandardVerbose = true;
#else
    constexpr bool kStandardVerbose = false;
#endif

    static void PrintStandardRouteKey(const char *label, const MaterialVariantKey &key, const bool any_array)
    {
        if (!kStandardVerbose)
            return;

        std::fprintf(stderr,
            "[Standard] %s hash=%llu surface=%u geom=%u sky=%u light=%u tex_bits=0x%08X sampler_bits=0x%08X va_bits=0x%08X extra_bits=0x%08X any_array=%d\n",
            label ? label : "route",
            static_cast<unsigned long long>(key.Hash()),
            static_cast<unsigned>(key.surface_type),
            static_cast<unsigned>(key.geometry_mode),
            static_cast<unsigned>(key.sky_ambient_model),
            static_cast<unsigned>(key.lighting_model),
            key.texture_source_bits,
            key.sampler_feature_bits,
            key.vertex_attribute_feature_bits,
            key.extra_feature_bits,
            any_array ? 1 : 0);
    }

    constexpr FixedVertexEntry STANDARD_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC2, VAN::TexCoord },
        { VAT_VEC3, VAN::Normal },
    };

    // Non-texture descriptors only texture entries are built dynamically in CreateStandardVariant().
    const UBOSemanticSet STANDARD_BASE_UBOS = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
        UBODescriptorSemantic::SkyInfo,
    };

    const SSBOSemanticSet STANDARD_BASE_SSBOS = {
        SSBODescriptorSemantic::TransformData,
        SSBODescriptorSemantic::TransformID,
        SSBODescriptorSemantic::MaterialBindingInstanceID,
        SSBODescriptorSemantic::MaterialBindingInstanceData,
    };

    // Ordered list of texture slots used by the Standard material.
    // Standard is a schema-fixed material: extending slots (e.g. Emissive/ORM) means a new material type,
    // not an in-place quality/feature variant inside Standard.
    constexpr SamplerSlot STANDARD_TEX_SLOTS[] = {
        SamplerSlot::BaseColor,
        SamplerSlot::Normal,
    };
    constexpr uint32_t STANDARD_TEX_SLOT_COUNT = uint32_t(sizeof(STANDARD_TEX_SLOTS) / sizeof(STANDARD_TEX_SLOTS[0]));
    static_assert(STANDARD_TEX_SLOT_COUNT == 2, "Standard material slot schema is fixed (BaseColor + Normal).");

    static constexpr const char *kAttribFetchMacroTags[] = {
        "NORMAL",            // 0
        "TANGENT",           // 1
        "COLOR",             // 2
        "TEXCOORD0",         // 3
        "TEXCOORD1",         // 4
        "JOINTS",            // 5
        "WEIGHTS",           // 6
        "INSTANCETRANSFORM", // 7
    };
    static_assert(
        sizeof(kAttribFetchMacroTags) / sizeof(*kAttribFetchMacroTags)
            == size_t(AttributeSemantic::BuiltinCount),
        "kAttribFetchMacroTags size mismatch");

    const StaticMaterialDef STANDARD_DEF_TEMPLATE {
        "Standard_v1",
        PrimitiveType::Triangles,
        STANDARD_VERTEX,
        uint32_t(sizeof(STANDARD_VERTEX) / sizeof(STANDARD_VERTEX[0])),
        &STANDARD_BASE_UBOS,
        &STANDARD_BASE_SSBOS,
        nullptr,
        ShaderDataSchema::StandardParams,
    };

    static bool UsesMeshPipeline(const Material3DCreateConfig &cfg)
    {
        const uint32_t bits = cfg.shader_stage_flag_bit;
        const bool has_mesh = (bits & uint32_t(ShaderStage::Mesh)) != 0;
        const bool has_vertex = (bits & uint32_t(ShaderStage::Vertex)) != 0;
        return has_mesh && !has_vertex;
    }

    static void AppendMeshFetchDefines(std::string &glsl,const MaterialVariantKey &key)
    {
        glsl += "#define GEOMETRY_FETCH_SSBO 1\n";

        if (key.position_provider == PositionProviderId::SSBO_PackedVec3)
        {
            glsl += "#define POSITION_SSBO_SET VERTEXSTREAMS_SET\n";
            glsl += "#define POSITION_SSBO_BINDING ";
            glsl += std::to_string(size_t(AttributeSemantic::BuiltinCount));
            glsl += "\n";
        }

        for (size_t i = 0; i < key.attribute_providers.size(); ++i)
        {
            const AttributeProviderId provider = key.attribute_providers[i];
            if (provider == AttributeProviderId::None || provider == AttributeProviderId::Constant)
                continue;

            glsl += "#define FETCH_";
            glsl += kAttribFetchMacroTags[i];
            glsl += "_SSBO_BINDING ";
            glsl += std::to_string(i);
            glsl += "\n";
        }
    }

    static std::string BuildStandardMeshShader(const MaterialVariantKey &key)
    {
        std::string glsl;
        glsl.reserve(4096);

        glsl += "#version 460\n";
        glsl += "#extension GL_EXT_mesh_shader : require\n\n";
        glsl += "#define ULRE_MESH_SHADER_STAGE 1\n";
        glsl += "#define HAS_POSITION\n";

        if (key.HasVertexAttrib(VertexAttrib::Normal))
            glsl += "#define HAS_NORMAL\n";

        if (key.HasVertexAttrib(VertexAttrib::Tangent))
            glsl += "#define HAS_TANGENT\n";

        if (key.HasVertexAttrib(VertexAttrib::TexCoord))
            glsl += "#define HAS_TEXCOORD\n";

        if (key.HasVertexAttrib(VertexAttrib::Color))
            glsl += "#define HAS_COLOR\n";

        if (key.HasVertexAttrib(VertexAttrib::Luminance))
            glsl += "#define HAS_LUMINANCE\n";

        AppendMeshFetchDefines(glsl, key);

        glsl += "\nlayout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;\n";
        glsl += "layout(triangles, max_vertices = 3, max_primitives = 1) out;\n\n";

        glsl += "layout(set=VERTEXSTREAMS_SET, binding=";
        glsl += std::to_string(kMeshShaderIndexStreamBinding);
        glsl += ", std430) readonly buffer MeshIndexData\n";
        glsl += "{\n";
        glsl += "    uint indices[];\n";
        glsl += "} mesh_index_stream;\n\n";

        glsl += "#include \"compositor/vert_forward_ubo.glsl\"\n";
        glsl += "#include \"common/vertex_fetch_ssbo.glsl\"\n\n";

        glsl += "layout(location=0) flat out uint fragMaterialInstanceID[];\n";
        glsl += "layout(location=1) out vec3 fragWorldPos[];\n";

        if (key.HasVertexAttrib(VertexAttrib::Normal))
            glsl += "layout(location=2) out vec3 fragWorldNormal[];\n";

        if (key.HasVertexAttrib(VertexAttrib::TexCoord))
            glsl += "layout(location=3) out vec2 fragUV0[];\n";

        if (key.HasVertexAttrib(VertexAttrib::Color))
            glsl += "layout(location=4) out vec4 fragVertexColor[];\n";

        if (key.HasVertexAttrib(VertexAttrib::Luminance))
            glsl += "layout(location=7) out float fragLuminance[];\n";

        if (key.HasVertexAttrib(VertexAttrib::Tangent))
            glsl += "layout(location=9) out vec4 fragWorldTangent[];\n";

        glsl += "\nvoid EmitVertexData(uint out_index, uint src_index, mat4 transform_mat, uint mi)\n";
        glsl += "{\n";
        glsl += "    vec3 pos3 = FetchPosition(src_index);\n";
        glsl += "    vec4 worldPos = transform_mat * vec4(pos3, 1.0);\n";
        glsl += "    gl_MeshVerticesEXT[out_index].gl_Position = camera.vp * worldPos;\n";
        glsl += "    fragMaterialInstanceID[out_index] = mi;\n";
        glsl += "    fragWorldPos[out_index] = worldPos.xyz;\n";

        if (key.HasVertexAttrib(VertexAttrib::Normal))
        {
            glsl += "    vec3 n = FetchNormal(src_index);\n";
            glsl += "    fragWorldNormal[out_index] = normalize(mat3(transform_mat) * n);\n";
        }

        if (key.HasVertexAttrib(VertexAttrib::TexCoord))
            glsl += "    fragUV0[out_index] = FetchUV0(src_index);\n";

        if (key.HasVertexAttrib(VertexAttrib::Color))
            glsl += "    fragVertexColor[out_index] = vec4(1.0);\n";

        if (key.HasVertexAttrib(VertexAttrib::Luminance))
            glsl += "    fragLuminance[out_index] = 1.0;\n";

        if (key.HasVertexAttrib(VertexAttrib::Tangent))
        {
            glsl += "    vec3 t = FetchTangent(src_index);\n";
            glsl += "    fragWorldTangent[out_index] = vec4(normalize(mat3(transform_mat) * t), 1.0);\n";
        }

        glsl += "}\n\n";

        glsl += "void main()\n";
        glsl += "{\n";
        glsl += "    SetMeshOutputsEXT(3u, 1u);\n";
        glsl += "\n";
        glsl += "    const uint tri = gl_WorkGroupID.x;\n";
        glsl += "    const uint index_base = tri * 3u;\n";
        glsl += "\n";
        glsl += "    const uint i0 = mesh_index_stream.indices[index_base + 0u];\n";
        glsl += "    const uint i1 = mesh_index_stream.indices[index_base + 1u];\n";
        glsl += "    const uint i2 = mesh_index_stream.indices[index_base + 2u];\n";
        glsl += "\n";
        glsl += "    const mat4 transform_mat = GetTransform();\n";
        glsl += "    const uint mi = GetMaterialInstanceID();\n";
        glsl += "\n";
        glsl += "    EmitVertexData(0u, i0, transform_mat, mi);\n";
        glsl += "    EmitVertexData(1u, i1, transform_mat, mi);\n";
        glsl += "    EmitVertexData(2u, i2, transform_mat, mi);\n";
        glsl += "\n";
        glsl += "    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);\n";
        glsl += "}\n";

        return glsl;
    }

} // anonymous namespace

MaterialCreateInfo *CreateStandardVariant(const contract::PhysicalDeviceProfileLite *profile,
                                          const MaterialVariantDesc &desc,
                                          const MaterialVariantKey &input_key,
                                          const Material3DCreateConfig *cfg)
{
    if (!cfg)
    {
        std::fprintf(stderr, "[Standard] CreateStandardVariant failed: cfg is null\n");
        return nullptr;
    }

    if (!profile)
    {
        std::fprintf(stderr, "[Standard] CreateStandardVariant warning: profile is null\n");
    }

    const StandardVariantPolicyResult policy = BuildStandardVariantPolicy(input_key);

    const TextureSourceMode standard_tex_slot_modes[] = {
        policy.resolved_base,
        policy.resolved_normal,
    };

    Material3DCreateConfig cfg_with_mi;
    SkyLightAmbientModel ambient = SkyLightAmbientModel::Simple;
    LightingModel lighting = LightingModel::Lambert;

    // Start with stable non-texture descriptors, then append texture entries.
    SSBOSemanticSet dynamic_ssbos;
    StaticTextureSamplerDescriptors dynamic_samplers;
    std::vector<const char *> unused_resources;
    bool any_array = false;

    BuildStandardDescriptorState(
        cfg,
        STANDARD_TEX_SLOTS,
        standard_tex_slot_modes,
        STANDARD_TEX_SLOT_COUNT,
        policy.any_array,
        STANDARD_BASE_SSBOS,
        cfg_with_mi,
        ambient,
        lighting,
        dynamic_ssbos,
        dynamic_samplers,
        unused_resources,
        any_array);

    StaticMaterialDef dynamic_def = BuildStandardDynamicDef(
        STANDARD_DEF_TEMPLATE,
        dynamic_ssbos,
        dynamic_samplers,
        ShaderDataSchema::StandardParams,
        any_array);

    const bool use_mesh_pipeline = UsesMeshPipeline(cfg_with_mi);
    MeshShaderStreamContract mesh_stream_contract = MakeDefaultMeshShaderStreamContract();

    MaterialVariantKey route_key = policy.route_key;
    route_key.lighting_model = lighting;
    // Registry descriptors are not split by sky model; keep lookup key on canonical sky.
    route_key.sky_ambient_model = SkyLightAmbientModel::Simple;

    const MaterialVariantDesc *var_desc = &desc;
    PrintStandardRouteKey("VariantRegistry resolved route-request", route_key, any_array);
    PrintStandardRouteKey("VariantRegistry resolved route-final", route_key, any_array);
    if (kStandardVerbose)
    {
        std::fprintf(stderr,
            "[Standard] VariantRegistry resolved variant=%s\n",
            var_desc->variant_name.c_str());
    }

    // Populate vertex attribute feature bits from the actual vertex layout.
    // policy is const, so take a mutable copy of assemble_key.
    MaterialVariantKey assemble_key = policy.assemble_key;
    assemble_key.lighting_model = lighting;
    assemble_key.sky_ambient_model = ambient;
    PopulateVariantKeyVertexAttribBits(assemble_key, dynamic_def);

    // If the assembled key uses any SSBO-backed vertex streams, register them
    // in the VertexStreams descriptor set so the pipeline layout includes set=4.
    dynamic_def.vertex_stream_key = &assemble_key;

    if (use_mesh_pipeline)
    {
        if (assemble_key.position_provider != PositionProviderId::SSBO_PackedVec3)
        {
            std::fprintf(stderr,
                "[Standard] CreateStandardVariant failed: mesh pipeline requires SSBO_PackedVec3 position provider\n");
            return nullptr;
        }

        dynamic_def.mesh_stream_contract = &mesh_stream_contract;
    }

    CompositorAssembler assembler;

    auto result = assembler.Assemble(assemble_key, *var_desc);

    if (!result.success)
    {
        std::fprintf(stderr, "[Standard] CompositorAssembler failed: %s\n",
            result.error_message.c_str());
        return nullptr;
    }

    std::string stage0_glsl = result.vertex_glsl;
    if (use_mesh_pipeline)
        stage0_glsl = BuildStandardMeshShader(assemble_key);

    MaterialCreateInfo *mci = CompileCompositorMaterial(
        profile,
        dynamic_def,
        stage0_glsl,
        result.fragment_glsl,
        &cfg_with_mi);

    if (!mci)
        std::fprintf(stderr, "[Standard] CompileCompositorMaterial failed\n");
    return mci;
}

static MaterialCreateInfo *Standard_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreateStandardVariant(profile, *desc, key, static_cast<const Material3DCreateConfig *>(cfg)); }

}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(Standard, "Standard", hgl::graph::mtl::Standard_Adapter)

