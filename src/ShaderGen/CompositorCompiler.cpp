/// CompositorCompiler.cpp — StaticMaterialDef → MaterialCreateInfo 编译器实现
///
/// 流程：
///   1. 从 StaticMaterialDef 的 UBO/SSBO/TextureSampler 组构建 MaterialDescriptorDB
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 SetFinalGLSL + CreateShaderDirect 直接编译

#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialBuilder.h>
#include "BuiltinVariantEntry.h"
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/mtl/MaterialVariantRegistry.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/ShaderLayoutResolver.h>
#include <hgl/shadergen/ShaderLayoutEmitter.h>
#include <hgl/shadergen/PositionProviderRegistry.h>
#include <hgl/shadergen/SamplerGLSLEmitter.h>
#include <hgl/shadergen/ShaderBuildPipeline.h>
#include <hgl/shadergen/CompositorAssembler.h>
#include "3d/StandardDescriptorBuilder.h"
#include "3d/Build3DCommon.h"
#include "3d/StandardVariantRouter.h"
#include "3d/MaterialFactory3DCommon.h"
#include "2d/Build2DCommon.h"
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <string>

namespace hgl::graph::mtl {

static bool HasUBOSemantic(const StaticMaterialDef &def, const UBODescriptorSemantic semantic);
static bool HasSSBOSemantic(const StaticMaterialDef &def, const SSBODescriptorSemantic semantic);
static bool HasPerMaterialDescriptor(const StaticMaterialDef &def);

static CompileCompositorShadowBuildReport BuildCompileCompositorShadowPipelineReportForConfig(const contract::PhysicalDeviceProfileLite *profile,
                                                                                               const StaticMaterialDef &def,
                                                                                               const MaterialCreateConfig *config);
static MaterialCreateInfo *CompileCompositorMaterialForConfig(const contract::PhysicalDeviceProfileLite *profile,
                                                              const StaticMaterialDef &def,
                                                              const std::string &vs_glsl,
                                                              const std::string &fs_glsl,
                                                              const MaterialCreateConfig *config);

namespace
{
    // Trial-batch helpers only.
    // These helpers organize built-in candidate observability and do not define
    // a future renderer-facing VS/FS split material model.
    static constexpr uint32_t kDefaultDescriptorStageBits = uint32_t(ShaderStage::VertexFragment);

    struct BuiltinTrialBatchBuilderState
    {
        std::vector<CompileCompositorTrialBatchItem> items;
        std::vector<SSBOSemanticSet *> owned_ssbo_sets;
        std::vector<StaticTextureSamplerDescriptors *> owned_sampler_sets;
        std::vector<std::vector<FixedVertexEntry> *> owned_vertex_lists;
        std::vector<MaterialResourceManifest *> owned_manifests;

        BuiltinTrialBatchBuilderState()
        {
            items.reserve(16);
            owned_ssbo_sets.reserve(1);
            owned_sampler_sets.reserve(1);
            owned_vertex_lists.reserve(8);
            owned_manifests.reserve(8);
        }

        ~BuiltinTrialBatchBuilderState()
        {
            for(const auto &item:items)
                delete item.def;

            for(auto *ssbos:owned_ssbo_sets)
                delete ssbos;

            for(auto *samplers:owned_sampler_sets)
                delete samplers;

            for(auto *vertices:owned_vertex_lists)
                delete vertices;

            for(auto *manifest:owned_manifests)
                delete manifest;
        }
    };

    enum class BuiltinTrialCandidateCategory
    {
        Unsupported,
        Special,
        Ordinary3D,
        Ordinary2D,
    };

    static const BuiltinVariantEntry *FindBuiltinVariantEntry(const MaterialPreset preset)
    {
        for(size_t i=0;i<kBuiltinVariantsCount;++i)
        {
            if(kBuiltinVariants[i].preset==preset)
                return &kBuiltinVariants[i];
        }

        return nullptr;
    }

    static BuiltinTrialCandidateCategory ResolveBuiltinTrialCandidateCategory(const MaterialPreset preset)
    {
        switch(preset)
        {
            case MaterialPreset::FullscreenTriangle:
            case MaterialPreset::Checkerboard3D:
                return BuiltinTrialCandidateCategory::Special;

            case MaterialPreset::Gizmo3D:
            case MaterialPreset::SkyMinimal:
            case MaterialPreset::TerrainGrid:
            case MaterialPreset::PureColor3D:
            case MaterialPreset::VertexColor3D:
            case MaterialPreset::VertexLuminance3D:
            case MaterialPreset::VertexLuminance2D:
            case MaterialPreset::VertexPaletteColor3D:
            case MaterialPreset::Billboard2DDynamic:
            case MaterialPreset::Billboard2DFixed:
            case MaterialPreset::PBRColor3D:
            case MaterialPreset::Standard:
                return BuiltinTrialCandidateCategory::Ordinary3D;

            case MaterialPreset::PureColor2D:
            case MaterialPreset::VertexColor2D:
            case MaterialPreset::PureTexture2D:
            case MaterialPreset::Text2D:
                return BuiltinTrialCandidateCategory::Ordinary2D;

            default:
                return BuiltinTrialCandidateCategory::Unsupported;
        }
    }

    static bool AppendBuiltinTrialAssembledItem(BuiltinTrialBatchBuilderState &state,
                                                const StaticMaterialDef &candidate_def,
                                                const MaterialCreateConfig *candidate_config,
                                                const MaterialVariantKey &variant_key,
                                                const MaterialVariantDesc &variant_desc,
                                                const char *candidate_name_override,
                                                const std::string &vertex_prefix_glsl = std::string(),
                                                const std::string &fragment_prefix_glsl = std::string())
    {
        MaterialVariantKey trial_assemble_key=variant_key;
        PopulateVariantKeyVertexAttribBits(trial_assemble_key,candidate_def);

        CompositorAssembler compositor_assembler;
        const auto assembled_result=compositor_assembler.Assemble(trial_assemble_key,variant_desc);
        if(!assembled_result.success)
            return false;

        CompileCompositorTrialBatchItem trial_item{};
        trial_item.def = new StaticMaterialDef(candidate_def);
        trial_item.vs_glsl = vertex_prefix_glsl + assembled_result.vertex_glsl;
        trial_item.fs_glsl = fragment_prefix_glsl + assembled_result.fragment_glsl;
        trial_item.config = candidate_config;
        trial_item.material_name_override = candidate_name_override ? candidate_name_override : (candidate_def.name ? candidate_def.name : variant_desc.variant_name);
        state.items.push_back(std::move(trial_item));
        return true;
    }

    static void AllocateBuiltinTrial2DResources(BuiltinTrialBatchBuilderState &state,
                                                std::vector<FixedVertexEntry> *&candidate_vertices,
                                                MaterialResourceManifest *&candidate_manifest)
    {
        candidate_vertices = new std::vector<FixedVertexEntry>();
        candidate_manifest = new MaterialResourceManifest();

        state.owned_vertex_lists.push_back(candidate_vertices);
        state.owned_manifests.push_back(candidate_manifest);
    }

    static void FillBuiltinTrialStaticMaterialDef(StaticMaterialDef &candidate_def,
                                                  const char *name,
                                                  const PrimitiveType primitive_type,
                                                  const FixedVertexEntry *vertex_entries,
                                                  const uint32_t vertex_entry_count,
                                                  const UBOSemanticSet *ubos,
                                                  const SSBOSemanticSet *ssbos,
                                                  const StaticTextureSamplerDescriptors *samplers,
                                                  const ShaderDataSchema shader_data_schema)
    {
        candidate_def.name = name;
        candidate_def.primitive_type = primitive_type;
        candidate_def.vertex_entries = vertex_entries;
        candidate_def.vertex_entry_count = vertex_entry_count;
        candidate_def.ubo_descriptors = ubos;
        candidate_def.ssbo_descriptors = ssbos;
        candidate_def.texture_samplers = samplers;
        candidate_def.shader_data_schema = shader_data_schema;
    }

    static bool AppendBuiltinTrialRawItem(BuiltinTrialBatchBuilderState &state,
                                          const StaticMaterialDef &candidate_def,
                                          const MaterialCreateConfig *candidate_config,
                                          const std::string &vertex_glsl,
                                          const std::string &fragment_glsl,
                                          const char *candidate_name_override)
    {
        CompileCompositorTrialBatchItem trial_item{};
        trial_item.def = new StaticMaterialDef(candidate_def);
        trial_item.vs_glsl = vertex_glsl;
        trial_item.fs_glsl = fragment_glsl;
        trial_item.config = candidate_config;
        trial_item.material_name_override = candidate_name_override ? candidate_name_override : candidate_def.name;
        state.items.push_back(std::move(trial_item));
        return true;
    }

    // Special trial candidates are handwritten or otherwise outside the normal
    // compositor-assembled candidate flow. This remains a trial-only grouping.
    static bool TryAppendBuiltinSpecialTrialCandidate(BuiltinTrialBatchBuilderState &state,
                                                      const MaterialPreset preset)
    {
        switch(preset)
        {
            case MaterialPreset::FullscreenTriangle:
            {
                static const StaticMaterialDef fullscreen_triangle_def =
                {
                    "FullscreenTriangle",
                    PrimitiveType::Triangles,
                    nullptr,
                    0,
                    nullptr,
                    nullptr,
                    nullptr,
                    ShaderDataSchema::None,
                };
                static const std::string fullscreen_triangle_fs =
                    "#version 450\n"
                    "\n"
                    "layout(location = 0) out vec4 outColor;\n"
                    "\n"
                    "void main()\n"
                    "{\n"
                    "    outColor = vec4(fract(gl_FragCoord.xyz * 0.01), 1.0);\n"
                    "}\n";
                static Material3DCreateConfig fullscreen_triangle_cfg;

                fullscreen_triangle_cfg.camera = false;
                fullscreen_triangle_cfg.sky = false;
                fullscreen_triangle_cfg.local_to_world = false;
                fullscreen_triangle_cfg.material_instance = false;
                fullscreen_triangle_cfg.effective_feature_mask = 0;
                fullscreen_triangle_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                const PositionProvider *pp = FindBuiltinProvider(PositionProviderId::PCG_FullscreenTriangle);
                if(!pp)
                    return false;

                std::ostringstream vs_out;
                vs_out << "#version 450\n\n";
                EmitPositionInput(vs_out, *pp, 0);
                vs_out << "\nvoid main()\n{\n    gl_Position = vec4(GetPositionLocal(), 1.0);\n}\n";

                return AppendBuiltinTrialRawItem(state,
                                                 fullscreen_triangle_def,
                                                 &fullscreen_triangle_cfg,
                                                 vs_out.str(),
                                                 fullscreen_triangle_fs,
                                                 "FullscreenTriangle");
            }

            case MaterialPreset::Checkerboard3D:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                };
                static const StaticMaterialDef checkerboard_def =
                {
                    "Checkerboard3D",
                    PrimitiveType::Triangles,
                    vertex_entries,
                    uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0])),
                    nullptr,
                    nullptr,
                    nullptr,
                    ShaderDataSchema::None,
                };
                static const std::string checkerboard_vs =
                    "#version 450\n"
                    "layout(location=0) in vec3 inPosition;\n"
                    "layout(location=0) out vec3 vWorldPos;\n"
                    "void main()\n"
                    "{\n"
                    "    vWorldPos = inPosition;\n"
                    "    gl_Position = vec4(inPosition, 1.0);\n"
                    "}\n";
                static const std::string checkerboard_fs =
                    "#version 450\n"
                    "layout(location=0) in vec3 vWorldPos;\n"
                    "layout(location=0) out vec4 outColor;\n"
                    "void main()\n"
                    "{\n"
                    "    vec2 grid = floor(vWorldPos.xz * 0.5);\n"
                    "    float checker = mod(grid.x + grid.y, 2.0);\n"
                    "    vec3 c0 = vec3(0.65, 0.65, 0.65);\n"
                    "    vec3 c1 = vec3(0.25, 0.25, 0.25);\n"
                    "    vec3 col = mix(c0, c1, checker);\n"
                    "    outColor = vec4(col, 1.0);\n"
                    "}\n";
                static Material3DCreateConfig checkerboard_cfg;

                checkerboard_cfg.camera = false;
                checkerboard_cfg.sky = false;
                checkerboard_cfg.local_to_world = false;
                checkerboard_cfg.material_instance = false;
                checkerboard_cfg.effective_feature_mask = 0;
                checkerboard_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                return AppendBuiltinTrialRawItem(state,
                                                 checkerboard_def,
                                                 &checkerboard_cfg,
                                                 checkerboard_vs,
                                                 checkerboard_fs,
                                                 "Checkerboard3D");
            }

            default:
                return false;
        }
    }

    // Ordinary 3D trial candidates still represent legacy VS+FS-together
    // material units. This helper only organizes trial-batch assembly and does
    // not imply a renderer-facing stage split.
    static bool TryAppendBuiltinOrdinary3DTrialCandidate(BuiltinTrialBatchBuilderState &state,
                                                         const MaterialPreset preset,
                                                         MaterialVariantKey &key,
                                                         const BuiltinVariantEntry &entry,
                                                         const MaterialVariantDesc &desc)
    {
        StaticMaterialDef def{};
        const MaterialCreateConfig *config=nullptr;

        switch(preset)
        {
            case MaterialPreset::Gizmo3D:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                    { VAT_VEC3, VAN::Normal },
                };
                static const UBOSemanticSet ubos =
                {
                    UBODescriptorSemantic::ViewportInfo,
                    UBODescriptorSemantic::CameraInfo,
                };
                static const SSBOSemanticSet ssbos =
                {
                    SSBODescriptorSemantic::TransformData,
                    SSBODescriptorSemantic::TransformID,
                    SSBODescriptorSemantic::MaterialBindingInstanceID,
                    SSBODescriptorSemantic::MaterialBindingInstanceData,
                };
                static Material3DCreateConfig gizmo_cfg(PrimitiveType::Triangles,IncludeCamera::With,IncludeL2W::With,IncludeSky::Without);

                gizmo_cfg.material_instance = true;
                gizmo_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                FillBuiltinTrialStaticMaterialDef(def,
                                                  "Gizmo3D",
                                                  PrimitiveType::Triangles,
                                                  vertex_entries,
                                                  uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0])),
                                                  &ubos,
                                                  &ssbos,
                                                  nullptr,
                                                  ShaderDataSchema::Color4f);
                config=&gizmo_cfg;
                break;
            }

            case MaterialPreset::SkyMinimal:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                };
                static const UBOSemanticSet ubos = build3d::MakeViewportCameraSkyUBOs();
                static const SSBOSemanticSet ssbos = build3d::MakeTransformSSBOs(false);
                static SkyMinimalCreateConfig sky_cfg;

                sky_cfg.material_instance = false;
                sky_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                FillBuiltinTrialStaticMaterialDef(def,
                                                  "SkyMinimal",
                                                  PrimitiveType::Triangles,
                                                  vertex_entries,
                                                  uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0])),
                                                  &ubos,
                                                  &ssbos,
                                                  nullptr,
                                                  ShaderDataSchema::None);
                config=&sky_cfg;
                break;
            }

            case MaterialPreset::TerrainGrid:
            {
                static constexpr SamplerSlot terrain_tex_slots[] =
                {
                    SamplerSlot::Height,
                    SamplerSlot::Normal,
                };
                static const UBOSemanticSet ubos = build3d::MakeViewportCameraUBOs();
                static const SSBOSemanticSet ssbos = build3d::MakeTransformSSBOs(false);
                static const StaticTextureSamplerDescriptors samplers = []()
                {
                    StaticTextureSamplerDescriptors descriptors;
                    AddTextureSampler(descriptors, terrain_tex_slots[0], SamplerType::Sampler2D);
                    AddTextureSampler(descriptors, terrain_tex_slots[1], SamplerType::Sampler2D);
                    return descriptors;
                }();
                static TerrainGridCreateConfig terrain_cfg;

                terrain_cfg.material_instance = false;
                terrain_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                FillBuiltinTrialStaticMaterialDef(def,
                                                  "TerrainGrid",
                                                  PrimitiveType::Triangles,
                                                  nullptr,
                                                  0,
                                                  &ubos,
                                                  &ssbos,
                                                  &samplers,
                                                  ShaderDataSchema::None);
                config=&terrain_cfg;
                break;
            }

            case MaterialPreset::PureColor3D:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                };
                static const UBOSemanticSet ubos = build3d::MakeViewportCameraUBOs();
                static const SSBOSemanticSet ssbos = build3d::MakeTransformSSBOs(true);
                static Material3DCreateConfig pure_color_cfg(PrimitiveType::Triangles,IncludeCamera::With,IncludeL2W::With,IncludeSky::Without);

                pure_color_cfg.material_instance = true;
                pure_color_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                FillBuiltinTrialStaticMaterialDef(def,
                                                  "PureColor3D",
                                                  PrimitiveType::Triangles,
                                                  vertex_entries,
                                                  uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0])),
                                                  &ubos,
                                                  &ssbos,
                                                  nullptr,
                                                  ShaderDataSchema::Color4f);
                config=&pure_color_cfg;
                break;
            }

            case MaterialPreset::VertexColor3D:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                    { VAT_VEC4, VAN::Color },
                };
                static const UBOSemanticSet ubos = build3d::MakeViewportCameraUBOs();
                static const SSBOSemanticSet ssbos = build3d::MakeTransformSSBOs(false);
                static Material3DCreateConfig vertex_color_cfg(PrimitiveType::Triangles,IncludeCamera::With,IncludeL2W::With,IncludeSky::Without);

                vertex_color_cfg.material_instance = false;
                vertex_color_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                FillBuiltinTrialStaticMaterialDef(def,
                                                  "VertexColor3D",
                                                  PrimitiveType::Triangles,
                                                  vertex_entries,
                                                  uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0])),
                                                  &ubos,
                                                  &ssbos,
                                                  nullptr,
                                                  ShaderDataSchema::None);
                config=&vertex_color_cfg;
                break;
            }

            case MaterialPreset::VertexLuminance3D:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                    { VAT_FLOAT, VAN::Luminance },
                };
                static const UBOSemanticSet ubos = build3d::MakeViewportCameraUBOs();
                static const SSBOSemanticSet ssbos = build3d::MakeTransformSSBOs(true);
                static Material3DCreateConfig vertex_luminance_cfg(PrimitiveType::Triangles,IncludeCamera::With,IncludeL2W::With,IncludeSky::Without);

                vertex_luminance_cfg.material_instance = true;
                vertex_luminance_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                def.name = "VertexLuminance3D";
                def.primitive_type = PrimitiveType::Triangles;
                def.vertex_entries = vertex_entries;
                def.vertex_entry_count = uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0]));
                def.ubo_descriptors = &ubos;
                def.ssbo_descriptors = &ssbos;
                def.texture_samplers = nullptr;
                def.shader_data_schema = ShaderDataSchema::Color4f;
                config=&vertex_luminance_cfg;
                break;
            }

            case MaterialPreset::VertexLuminance2D:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC2, VAN::Position },
                    { VAT_FLOAT, VAN::Luminance },
                };
                static const UBOSemanticSet ubos = build3d::MakeViewportCameraUBOs();
                static const SSBOSemanticSet ssbos = build3d::MakeTransformSSBOs(true);
                static Material3DCreateConfig vertex_luminance_cfg(PrimitiveType::Lines,IncludeCamera::With,IncludeL2W::With,IncludeSky::Without);

                vertex_luminance_cfg.material_instance = true;
                vertex_luminance_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                def.name = "VertexLuminance2D";
                def.primitive_type = vertex_luminance_cfg.prim;
                def.vertex_entries = vertex_entries;
                def.vertex_entry_count = uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0]));
                def.ubo_descriptors = &ubos;
                def.ssbo_descriptors = &ssbos;
                def.texture_samplers = nullptr;
                def.shader_data_schema = ShaderDataSchema::Color4f;
                config=&vertex_luminance_cfg;

                key.position_provider = PositionProviderId::VAB_Vec2;
                break;
            }

            case MaterialPreset::VertexPaletteColor3D:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                    { VAT_UINT, VAN::Color },
                };
                static const UBOSemanticSet ubos = []()
                {
                    UBOSemanticSet descriptors = build3d::MakeViewportCameraUBOs();
                    descriptors.insert(UBODescriptorSemantic::ColorPalette);
                    return descriptors;
                }();
                static const SSBOSemanticSet ssbos = build3d::MakeTransformSSBOs(false);
                static Material3DCreateConfig vertex_palette_cfg(PrimitiveType::Triangles,IncludeCamera::With,IncludeL2W::With,IncludeSky::Without);

                vertex_palette_cfg.material_instance = false;
                vertex_palette_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                def.name = "VertexPaletteColor3D";
                def.primitive_type = PrimitiveType::Triangles;
                def.vertex_entries = vertex_entries;
                def.vertex_entry_count = uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0]));
                def.ubo_descriptors = &ubos;
                def.ssbo_descriptors = &ssbos;
                def.texture_samplers = nullptr;
                def.shader_data_schema = ShaderDataSchema::None;
                config=&vertex_palette_cfg;
                break;
            }

            case MaterialPreset::Billboard2DDynamic:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                };
                static const UBOSemanticSet ubos =
                {
                    UBODescriptorSemantic::ViewportInfo,
                    UBODescriptorSemantic::CameraInfo,
                };
                static const SSBOSemanticSet ssbos =
                {
                    SSBODescriptorSemantic::TransformData,
                    SSBODescriptorSemantic::TransformID,
                    SSBODescriptorSemantic::MaterialBindingInstanceID,
                    SSBODescriptorSemantic::MaterialBindingInstanceData,
                };
                static const StaticTextureSamplerDescriptors samplers =
                {
                    { SamplerSlot::BaseColor, MakeStaticTextureSamplerDescriptor(SamplerType::Sampler2D) }
                };
                static Material3DCreateConfig billboard_cfg(PrimitiveType::Triangles,IncludeCamera::With,IncludeL2W::With,IncludeSky::Without);

                billboard_cfg.material_instance = true;
                billboard_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                def.name = "BillboardDynamic";
                def.primitive_type = PrimitiveType::Triangles;
                def.vertex_entries = vertex_entries;
                def.vertex_entry_count = uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0]));
                def.ubo_descriptors = &ubos;
                def.ssbo_descriptors = &ssbos;
                def.texture_samplers = &samplers;
                def.shader_data_schema = ShaderDataSchema::BillboardSizeUVec2;
                config=&billboard_cfg;
                break;
            }

            case MaterialPreset::Billboard2DFixed:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                };
                static const UBOSemanticSet ubos =
                {
                    UBODescriptorSemantic::ViewportInfo,
                    UBODescriptorSemantic::CameraInfo,
                };
                static const SSBOSemanticSet ssbos =
                {
                    SSBODescriptorSemantic::TransformData,
                    SSBODescriptorSemantic::TransformID,
                    SSBODescriptorSemantic::MaterialBindingInstanceID,
                    SSBODescriptorSemantic::MaterialBindingInstanceData,
                };
                static const StaticTextureSamplerDescriptors samplers =
                {
                    { SamplerSlot::BaseColor, MakeStaticTextureSamplerDescriptor(SamplerType::Sampler2D) }
                };
                static Material3DCreateConfig billboard_cfg(PrimitiveType::Triangles,IncludeCamera::With,IncludeL2W::With,IncludeSky::Without);

                billboard_cfg.material_instance = true;
                billboard_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                def.name = "BillboardFixed";
                def.primitive_type = PrimitiveType::Triangles;
                def.vertex_entries = vertex_entries;
                def.vertex_entry_count = uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0]));
                def.ubo_descriptors = &ubos;
                def.ssbo_descriptors = &ssbos;
                def.texture_samplers = &samplers;
                def.shader_data_schema = ShaderDataSchema::BillboardSizeUVec2;
                config=&billboard_cfg;
                break;
            }

            case MaterialPreset::PBRColor3D:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                    { VAT_VEC3, VAN::Normal },
                };
                static const UBOSemanticSet ubos =
                {
                    UBODescriptorSemantic::CameraInfo,
                    UBODescriptorSemantic::SkyInfo,
                };
                static const SSBOSemanticSet ssbos =
                {
                    SSBODescriptorSemantic::TransformData,
                    SSBODescriptorSemantic::TransformID,
                    SSBODescriptorSemantic::MaterialBindingInstanceID,
                    SSBODescriptorSemantic::MaterialBindingInstanceData,
                };
                static Material3DCreateConfig pbr_cfg(PrimitiveType::Triangles,IncludeCamera::With,IncludeL2W::With,IncludeSky::With);

                pbr_cfg.material_instance = true;
                pbr_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);
                pbr_cfg.lighting_model = LightingModel::PBR;

                def.name = "PBRColor3D";
                def.primitive_type = PrimitiveType::Triangles;
                def.vertex_entries = vertex_entries;
                def.vertex_entry_count = uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0]));
                def.ubo_descriptors = &ubos;
                def.ssbo_descriptors = &ssbos;
                def.texture_samplers = nullptr;
                def.shader_data_schema = ShaderDataSchema::PBRColorParams;
                config=&pbr_cfg;
                break;
            }

            case MaterialPreset::Standard:
            {
                static constexpr FixedVertexEntry vertex_entries[] =
                {
                    { VAT_VEC3, VAN::Position },
                    { VAT_VEC2, VAN::TexCoord },
                    { VAT_VEC3, VAN::Normal },
                };
                static const UBOSemanticSet ubos =
                {
                    UBODescriptorSemantic::ViewportInfo,
                    UBODescriptorSemantic::CameraInfo,
                    UBODescriptorSemantic::SkyInfo,
                };
                static const SSBOSemanticSet base_ssbos =
                {
                    SSBODescriptorSemantic::TransformData,
                    SSBODescriptorSemantic::TransformID,
                    SSBODescriptorSemantic::MaterialBindingInstanceID,
                    SSBODescriptorSemantic::MaterialBindingInstanceData,
                };
                static constexpr SamplerSlot standard_tex_slots[] =
                {
                    SamplerSlot::BaseColor,
                    SamplerSlot::Normal,
                };
                static Material3DCreateConfig standard_cfg(PrimitiveType::Triangles,IncludeCamera::With,IncludeL2W::With,IncludeSky::With);
                static Material3DCreateConfig standard_cfg_with_mi;

                standard_cfg.material_instance = true;
                standard_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                const TextureSourceMode tex_slot_modes[] =
                {
                    TextureSourceMode::Simple,
                    TextureSourceMode::Simple,
                };

                SkyLightAmbientModel ambient = SkyLightAmbientModel::Simple;
                LightingModel lighting = LightingModel::Lambert;
                SSBOSemanticSet dynamic_ssbos;
                StaticTextureSamplerDescriptors dynamic_samplers;
                std::vector<const char *> unused_resources;
                bool any_array=false;
                Material3DCreateConfig cfg_with_mi;

                BuildStandardDescriptorState(&standard_cfg,
                                             standard_tex_slots,
                                             tex_slot_modes,
                                             uint32_t(sizeof(standard_tex_slots)/sizeof(standard_tex_slots[0])),
                                             false,
                                             base_ssbos,
                                             cfg_with_mi,
                                             ambient,
                                             lighting,
                                             dynamic_ssbos,
                                             dynamic_samplers,
                                             unused_resources,
                                             any_array);

                static const StaticMaterialDef standard_template =
                {
                    "Standard_v1",
                    PrimitiveType::Triangles,
                    vertex_entries,
                    uint32_t(sizeof(vertex_entries)/sizeof(vertex_entries[0])),
                    &ubos,
                    nullptr,
                    nullptr,
                    ShaderDataSchema::StandardParams,
                };

                def = BuildStandardDynamicDef(standard_template,
                                              dynamic_ssbos,
                                              dynamic_samplers,
                                              ShaderDataSchema::StandardParams,
                                              any_array);
                def.name = "Standard_v1";

                auto *owned_dynamic_ssbos = new SSBOSemanticSet(dynamic_ssbos);
                auto *owned_dynamic_samplers = new StaticTextureSamplerDescriptors(dynamic_samplers);
                state.owned_ssbo_sets.push_back(owned_dynamic_ssbos);
                state.owned_sampler_sets.push_back(owned_dynamic_samplers);
                def.ssbo_descriptors = owned_dynamic_ssbos;
                def.texture_samplers = owned_dynamic_samplers;

                standard_cfg_with_mi = cfg_with_mi;
                config=&standard_cfg_with_mi;

                key = RouteKey(MaterialPreset::Standard,0u,RuntimeKeyOverrides{});
                auto policy=BuildStandardVariantPolicy(key);
                key = policy.assemble_key;
                key.lighting_model = lighting;
                key.sky_ambient_model = ambient;
                break;
            }

            default:
                return false;
        }

        return AppendBuiltinTrialAssembledItem(state,
                                               def,
                                               config,
                                               key,
                                               desc,
                                               def.name ? def.name : entry.name);
    }

    // Ordinary 2D trial candidates are also assembled as legacy VS+FS-together
    // units. The separation here is only for trial-batch readability.
    static bool TryAppendBuiltinOrdinary2DTrialCandidate(BuiltinTrialBatchBuilderState &state,
                                                         const MaterialPreset preset,
                                                         const MaterialVariantKey &key,
                                                         const MaterialVariantDesc &desc)
    {
        StaticMaterialDef def{};

        switch(preset)
        {
            case MaterialPreset::PureColor2D:
            {
                static Material2DCreateConfig pure_color_cfg(PrimitiveType::Triangles,CoordinateSystem2D::NDC,IncludeL2W::Without);

                pure_color_cfg.material_instance = true;
                pure_color_cfg.position_format = VAT_VEC2;
                pure_color_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                std::vector<FixedVertexEntry> *vertices=nullptr;
                MaterialResourceManifest *manifest=nullptr;
                AllocateBuiltinTrial2DResources(state,vertices,manifest);
                build2d::PushBaseVertexEntries(*vertices,&pure_color_cfg);

                build2d::BuildBase2DFixedDef(def,
                                             "PureColor2D",
                                             &pure_color_cfg,
                                             *vertices,
                                             *manifest,
                                             ShaderDataSchema::Color4f);

                return AppendBuiltinTrialAssembledItem(state,
                                                       def,
                                                       &pure_color_cfg,
                                                       key,
                                                       desc,
                                                       "PureColor2D",
                                                       build2d::Build2DVertexPreamble(&pure_color_cfg,false,true),
                                                       build2d::Build2DFragmentPreamble(&pure_color_cfg,false,true));
            }

            case MaterialPreset::VertexColor2D:
            {
                static Material2DCreateConfig vertex_color_cfg(PrimitiveType::Triangles,CoordinateSystem2D::NDC,IncludeL2W::Without);

                vertex_color_cfg.material_instance = false;
                vertex_color_cfg.position_format = VAT_VEC2;
                vertex_color_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                std::vector<FixedVertexEntry> *vertices=nullptr;
                MaterialResourceManifest *manifest=nullptr;
                AllocateBuiltinTrial2DResources(state,vertices,manifest);
                build2d::PushBaseVertexEntries(*vertices,&vertex_color_cfg);
                vertices->push_back({VAT_VEC4, VAN::Color});

                build2d::BuildBase2DFixedDef(def,
                                             "VertexColor2D",
                                             &vertex_color_cfg,
                                             *vertices,
                                             *manifest,
                                             ShaderDataSchema::None);

                return AppendBuiltinTrialAssembledItem(state,
                                                       def,
                                                       &vertex_color_cfg,
                                                       key,
                                                       desc,
                                                       "VertexColor2D",
                                                       build2d::Build2DVertexPreamble(&vertex_color_cfg,false,false),
                                                       build2d::Build2DFragmentPreamble(&vertex_color_cfg,false,false));
            }

            case MaterialPreset::PureTexture2D:
            {
                static Material2DCreateConfig pure_texture_cfg(PrimitiveType::Triangles,CoordinateSystem2D::NDC,IncludeL2W::Without);

                pure_texture_cfg.material_instance = false;
                pure_texture_cfg.position_format = VAT_VEC2;
                pure_texture_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

                std::vector<FixedVertexEntry> *vertices=nullptr;
                MaterialResourceManifest *manifest=nullptr;
                AllocateBuiltinTrial2DResources(state,vertices,manifest);
                build2d::PushBaseVertexEntries(*vertices,&pure_texture_cfg);
                vertices->push_back({VAT_VEC2, VAN::TexCoord});
                AddTextureSampler(manifest->samplers, SamplerSlot::BaseColor, SamplerType::Sampler2D);

                build2d::BuildBase2DFixedDef(def,
                                             "PureTexture2D",
                                             &pure_texture_cfg,
                                             *vertices,
                                             *manifest,
                                             ShaderDataSchema::None);

                return AppendBuiltinTrialAssembledItem(state,
                                                       def,
                                                       &pure_texture_cfg,
                                                       key,
                                                       desc,
                                                       "PureTexture2D",
                                                       build2d::Build2DVertexPreamble(&pure_texture_cfg,true,false,SamplerSlot::BaseColor,false),
                                                       build2d::Build2DFragmentPreamble(&pure_texture_cfg,true,false,SamplerSlot::BaseColor,false));
            }

            case MaterialPreset::Text2D:
            {
                static Text2DMaterialCreateConfig text_cfg;

                text_cfg.prim = PrimitiveType::Triangles;
                text_cfg.position_format = VAT_IVEC2;
                text_cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);
                text_cfg.material_instance = true;

                std::vector<FixedVertexEntry> *vertices=nullptr;
                MaterialResourceManifest *manifest=nullptr;
                AllocateBuiltinTrial2DResources(state,vertices,manifest);
                build2d::PushBaseVertexEntries(*vertices,&text_cfg);
                vertices->push_back({VAT_VEC2, VAN::TexCoord});
                AddTextureSampler(manifest->samplers, SamplerSlot::Text, SamplerType::Sampler2D);

                build2d::BuildBase2DFixedDef(def,
                                             "Text2D",
                                             &text_cfg,
                                             *vertices,
                                             *manifest,
                                             ShaderDataSchema::TextColor);

                return AppendBuiltinTrialAssembledItem(state,
                                                       def,
                                                       &text_cfg,
                                                       key,
                                                       desc,
                                                       "Text2D",
                                                       build2d::Build2DVertexPreamble(&text_cfg,true,true,SamplerSlot::Text),
                                                       build2d::Build2DFragmentPreamble(&text_cfg,true,true,SamplerSlot::Text));
            }

            default:
                return false;
        }
    }

    static bool TryAppendBuiltinTrialCandidate(BuiltinTrialBatchBuilderState &state,
                                               const MaterialPreset preset)
    {
        const BuiltinTrialCandidateCategory candidate_category=ResolveBuiltinTrialCandidateCategory(preset);

        switch(candidate_category)
        {
            case BuiltinTrialCandidateCategory::Special:
                return TryAppendBuiltinSpecialTrialCandidate(state,preset);

            case BuiltinTrialCandidateCategory::Ordinary3D:
            case BuiltinTrialCandidateCategory::Ordinary2D:
                break;

            default:
                return false;
        }

        MaterialVariantKey key=RouteKey(preset,0u,RuntimeKeyOverrides{});
        const MaterialVariantDesc *desc=GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(key);
        if(!desc)
            return false;

        const BuiltinVariantEntry *entry=FindBuiltinVariantEntry(preset);
        if(!entry)
            return false;

        switch(candidate_category)
        {
            case BuiltinTrialCandidateCategory::Ordinary3D:
                return TryAppendBuiltinOrdinary3DTrialCandidate(state,preset,key,*entry,*desc);

            case BuiltinTrialCandidateCategory::Ordinary2D:
                return TryAppendBuiltinOrdinary2DTrialCandidate(state,preset,key,*desc);

            default:
                return false;
        }
    }

    static bool ResolveConfiguredCameraRequirement(const Material3DCreateConfig &cfg)
    {
        if (cfg.effective_feature_mask != 0)
            return HasFeature(cfg.effective_feature_mask, MaterialFeature::NeedsCamera);

        return cfg.camera;
    }

    static bool ResolveConfiguredSkyRequirement(const Material3DCreateConfig &cfg)
    {
        if (cfg.effective_feature_mask != 0)
            return HasFeature(cfg.effective_feature_mask, MaterialFeature::NeedsSky);

        return cfg.sky;
    }

    static void AppendDiagnosticLine(std::string *diagnostics, const std::string &line)
    {
        if (!diagnostics || line.empty())
            return;

        if (!diagnostics->empty())
            *diagnostics += '\n';

        *diagnostics += line;
    }

    static void EmitInferenceMismatchDiagnostics(
        const StaticMaterialDef &def,
        const Material3DCreateConfig &cfg,
        const bool infer_has_camera,
        const bool infer_has_sky,
        std::string *diagnostics)
    {
        const bool configured_camera = ResolveConfiguredCameraRequirement(cfg);
        const bool configured_sky = ResolveConfiguredSkyRequirement(cfg);

        auto emit_one = [&](const char *label, const bool configured, const bool inferred)
        {
            if (configured == inferred)
                return;

            std::string message;
            message.reserve(256);
            message += "[CompositorCompiler] inferred ";
            message += label;
            message += "=";
            message += inferred ? "true" : "false";
            message += " differs from configured/effective=";
            message += configured ? "true" : "false";
            message += " for material='";
            message += def.name ? def.name : "<unnamed>";
            message += "'";

            if (cfg.effective_feature_mask != 0)
            {
                char buf[96]{};
                std::snprintf(buf,
                              sizeof(buf),
                              " effective_feature_mask=0x%016llx",
                              static_cast<unsigned long long>(cfg.effective_feature_mask));
                message += buf;
            }

            message += "; compiler inference is diagnostics-only";

            std::fprintf(stderr, "%s\n", message.c_str());
            AppendDiagnosticLine(diagnostics, message);
        };

        emit_one("camera", configured_camera, infer_has_camera);
        emit_one("sky", configured_sky, infer_has_sky);
    }

    static std::string BuildShaderDataSchemaDebugText(const StaticMaterialDef &def)
    {
        if (def.shader_data_schema == ShaderDataSchema::None)
            return std::string("schema=<none>");

        const ShaderDataSchemaInfo &schema_info = GetShaderDataSchemaInfo(def.shader_data_schema);

        std::string text;
        text.reserve(128);
        text += "schema=";
        text += std::to_string(static_cast<uint32_t>(def.shader_data_schema));
        text += " file=";
        text += schema_info.glsl_schema_file ? schema_info.glsl_schema_file : "<null>";
        text += " bytes=";
        text += std::to_string(schema_info.byte_size);
        return text;
    }

    static std::string BuildShaderDataSchemaIncludeText(const ShaderDataSchemaInfo &schema_info)
    {
        if (!schema_info.glsl_schema_file || !schema_info.glsl_schema_file[0])
            return std::string();

        std::string include_text;
        include_text.reserve(48 + std::char_traits<char>::length(schema_info.glsl_schema_file));
        include_text += "#include \"common/schema/";
        include_text += schema_info.glsl_schema_file;
        include_text += "\"\n";
        return include_text;
    }

    static ShaderBuildDescriptorSpec BuildDescriptorSpecFromStaticMaterialDef(const StaticMaterialDef &def)
    {
        ShaderBuildDescriptorSpec spec{};

        if(def.ubo_descriptors)
        {
            for(const auto semantic:*def.ubo_descriptors)
                spec.ubos.push_back(semantic);
        }

        if(def.ssbo_descriptors)
        {
            for(const auto semantic:*def.ssbo_descriptors)
            {
                if(semantic==SSBODescriptorSemantic::TransformData)
                    continue;

                spec.ssbos.push_back(semantic);
            }
        }

        if(def.shader_data_schema!=ShaderDataSchema::None)
        {
            const ShaderDataSchemaInfo &schema_info=GetShaderDataSchemaInfo(def.shader_data_schema);
            spec.material_instance_schema=def.shader_data_schema;
            spec.material_instance_bytes=schema_info.byte_size;
        }

        return spec;
    }

    static MaterialCreateConfig BuildPipelineConfigFromCompositorConfig(const StaticMaterialDef &def,
                                                                       const Material3DCreateConfig *config)
    {
        MaterialCreateConfig pipeline_cfg(def.primitive_type,false);

        if(config)
        {
            pipeline_cfg=*static_cast<const MaterialCreateConfig *>(config);
            pipeline_cfg.prim=def.primitive_type;
        }

        pipeline_cfg.shader_stage_flag_bit=uint32_t(ShaderStage::VertexFragment);

        if(def.texture_samplers)
        {
            for(const auto &[slot,descriptor]:*def.texture_samplers)
            {
                pipeline_cfg.SetTextureSourceSlotEnabledOverride(slot,true);
                if(descriptor.sampler_type!=SamplerType::Sampler2D)
                    continue;
            }
        }

        const bool infer_has_l2w=HasSSBOSemantic(def,SSBODescriptorSemantic::TransformData);
        const bool infer_has_mi=HasSSBOSemantic(def,SSBODescriptorSemantic::MaterialBindingInstanceData)
                              || HasPerMaterialDescriptor(def)
                              || (def.shader_data_schema!=ShaderDataSchema::None);

        pipeline_cfg.local_to_world = pipeline_cfg.local_to_world || infer_has_l2w;
        pipeline_cfg.material_instance = pipeline_cfg.material_instance || infer_has_mi;
        return pipeline_cfg;
    }

    static void AppendShadowBuildDiagnostics(std::string &text,
                                             const ShaderGenResult<ShaderBuildResult> &result)
    {
        for(const auto &diag:result.diagnostics)
        {
            if(!text.empty())
                text += " | ";

            text += diag.subject;
            text += ": ";
            text += diag.message;
        }
    }

    static std::string SanitizeArtifactName(const char *text)
    {
        if(!text || !*text)
            return std::string("unnamed_material");

        std::string sanitized;
        sanitized.reserve(std::char_traits<char>::length(text));

        for(const char ch:std::string(text))
        {
            if((ch>='a'&&ch<='z')||(ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')||ch=='_'||ch=='-')
                sanitized.push_back(ch);
            else
                sanitized.push_back('_');
        }

        return sanitized.empty()?std::string("unnamed_material"):sanitized;
    }

    static bool EnsureDirectoryExists(const char *dir)
    {
        if(!dir || !*dir)
            return false;

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(dir),ec);
        return !ec;
    }

    static bool WriteTextFile(const std::filesystem::path &path,const std::string &text)
    {
        std::ofstream ofs(path,std::ios::out|std::ios::trunc);
        if(!ofs.is_open())
            return false;

        ofs << text;
        return ofs.good();
    }

    static std::string BuildShadowDiagnosticsText(const CompileCompositorShadowBuildReport &report,
                                                  const char *material_name)
    {
        std::string text;
        text.reserve(512);
        text += "material=";
        text += material_name ? material_name : "<unnamed>";
        text += "\n";
        text += "summary=";
        text += report.summary;
        text += "\n";

        for(const auto &diag:report.result.diagnostics)
        {
            text += "diag.subject=";
            text += diag.subject;
            text += ", diag.message=";
            text += diag.message;
            text += "\n";
        }

        return text;
    }

    static std::string BuildTrialAggregateReportText(const std::filesystem::path &trial_root)
    {
        const std::filesystem::path reports_dir=trial_root/"reports";
        std::string text;
        text += "# ShaderGen 试运行汇总报告（自动生成）\n\n";
        text += "- TrialRoot: `";
        text += trial_root.string();
        text += "`\n\n";
        text += "## Per-Material Baseline Reports\n\n";

        bool has_any=false;
        std::error_code ec;
        if(std::filesystem::exists(reports_dir,ec))
        {
            for(const auto &entry:std::filesystem::directory_iterator(reports_dir,ec))
            {
                if(ec)
                    break;

                if(!entry.is_regular_file())
                    continue;

                const auto filename=entry.path().filename().string();
                if(filename.find("_baseline_compare.md")==std::string::npos)
                    continue;

                has_any=true;
                text += "- `";
                text += filename;
                text += "`\n";
            }
        }

        if(!has_any)
            text += "- `<none>`\n";

        return text;
    }

    static std::string BuildDescriptorSpecText(const ShaderBuildDescriptorSpec &spec)
    {
        std::string text;
        text += "ubos=";
        for(size_t i=0;i<spec.ubos.size();++i)
        {
            if(i>0)
                text += ",";
            text += std::to_string(static_cast<int>(spec.ubos[i]));
        }

        text += "\nssbos=";
        for(size_t i=0;i<spec.ssbos.size();++i)
        {
            if(i>0)
                text += ",";
            text += std::to_string(static_cast<int>(spec.ssbos[i]));
        }

        text += "\nmaterial_instance_bytes=";
        text += std::to_string(spec.material_instance_bytes);
        text += "\nmaterial_instance_schema=";
        text += std::to_string(static_cast<int>(spec.material_instance_schema));
        text += "\n";
        return text;
    }

    static const char *GetShaderStageArtifactName(const ShaderStage stage)
    {
        switch(stage)
        {
            case ShaderStage::Vertex:   return "vertex";
            case ShaderStage::Fragment: return "fragment";
            case ShaderStage::Geometry: return "geometry";
            case ShaderStage::Compute:  return "compute";
            default:                    return "unknown";
        }
    }

    static std::string BuildDescriptorInfoText(const MaterialDescriptorDB &descriptor_db,
                                               const DescriptorBindingSlots &binding_contract)
    {
        std::string text;
        text += "descriptor_count=";
        text += std::to_string(descriptor_db.GetCount());
        text += "\nubos=";

        for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        {
            if(i>0)
                text += ",";
            text += std::to_string(binding_contract.ubos[i]);
        }

        text += "\nssbos=";

        for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        {
            if(i>0)
                text += ",";
            text += std::to_string(binding_contract.ssbos[i]);
        }

        text += "\n";
        return text;
    }

    static std::string BuildMaterialBlocksText(const MaterialCreateInfo &mci)
    {
        const auto &material_instance=mci.GetMaterialInstance();
        const auto &local_to_world=mci.GetLocalToWorld();

        std::string text;
        text += "material_instance.enabled=";
        text += material_instance.IsActive() ? "true" : "false";
        text += "\nmaterial_instance.stage_bits=";
        text += std::to_string(material_instance.stage_bits);
        text += "\nmaterial_instance.stride=";
        text += std::to_string(material_instance.stride);
        text += "\nmaterial_instance.schema=";
        text += std::to_string(static_cast<int>(material_instance.schema));
        text += "\nmaterial_instance.schema_file=";
        text += material_instance.schema_file;
        text += "\nlocal_to_world.enabled=";
        text += local_to_world.enabled ? "true" : "false";
        text += "\nlocal_to_world.stage_bits=";
        text += std::to_string(local_to_world.stage_bits);
        text += "\n";
        return text;
    }

    static bool WriteLegacyShaderArtifacts(const std::filesystem::path &material_root,
                                           const MaterialCreateInfo &mci)
    {
        for(const auto &[stage,shader]:mci.GetShaderMap())
        {
            if(!shader)
                continue;

            const char *stage_name=GetShaderStageArtifactName(stage);
            const std::filesystem::path glsl_path=material_root/(std::string(stage_name) + ".glsl");
            if(!WriteTextFile(glsl_path,shader->GetFinalGLSL()))
                return false;

            const uint32 *spv_data=shader->GetSPVData();
            const size_t spv_size=shader->GetSPVSize();

            if(spv_data&&spv_size>0)
            {
                std::string spv_text;
                const size_t word_count=spv_size/sizeof(uint32_t);
                spv_text.reserve(word_count*9);

                for(size_t i=0;i<word_count;++i)
                {
                    char buf[16]{};
                    std::snprintf(buf,sizeof(buf),"%08X",spv_data[i]);
                    spv_text += buf;
                    spv_text += '\n';
                }

                const std::filesystem::path spv_path=material_root/(std::string(stage_name) + ".spv.txt");
                if(!WriteTextFile(spv_path,spv_text))
                    return false;
            }
        }

        return true;
    }

    bool WriteCompileCompositorLegacyTreeInternal(const MaterialCreateInfo &mci,
                                                  const char *material_name,
                                                  const char *legacy_root)
{
    if(!EnsureDirectoryExists(legacy_root))
        return false;

    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path material_root=std::filesystem::path(legacy_root)/sanitized_name;

    if(!EnsureDirectoryExists(material_root.string().c_str()))
        return false;

    if(!WriteTextFile(material_root/"descriptor_info.txt",
                      BuildDescriptorInfoText(mci.GetDescriptorInfo(),mci.GetBindingContract())))
        return false;

    if(!WriteTextFile(material_root/"material_blocks.txt",BuildMaterialBlocksText(mci)))
        return false;

    return WriteLegacyShaderArtifacts(material_root,mci);
}

    static std::string BuildPipelineConfigText(const MaterialCreateConfig &config)
    {
        std::string text;
        text += "shader_stage_flag_bit=";
        text += std::to_string(config.shader_stage_flag_bit);
        text += "\nmaterial_instance=";
        text += config.material_instance ? "true" : "false";
        text += "\nlocal_to_world=";
        text += config.local_to_world ? "true" : "false";
        text += "\nsampler_feature_bits_override=";
        text += std::to_string(config.sampler_feature_bits_override);
        text += "\ntexture_source_bits_override=";
        text += std::to_string(config.texture_source_bits_override);
        text += "\n";
        return text;
    }

    static std::string BuildPipelineResultText(const CompileCompositorShadowBuildReport &report)
    {
        std::string text;
        text += "success=";
        text += report.result.success ? "true" : "false";
        text += "\nfinal_state=";
        text += std::to_string(static_cast<int>(report.result.value.final_state));
        text += "\ndescriptor_count=";
        text += std::to_string(report.result.value.descriptor_count);
        text += "\nlayout_finalized=";
        text += report.result.value.layout_finalized ? "true" : "false";
        text += "\nbinary_count=";
        text += std::to_string(report.result.value.binaries.size());
        text += "\n";
        return text;
    }

    static std::string BuildSpirvHexText(const ShaderBinary &binary)
    {
        std::string text;
        text.reserve(binary.spirv.size()*9);

        for(size_t i=0;i<binary.spirv.size();++i)
        {
            char buf[16]{};
            std::snprintf(buf,sizeof(buf),"%08X",binary.spirv[i]);
            text += buf;
            text += '\n';
        }

        return text;
    }

    static std::string BuildBaselineCompareReportText(const CompileCompositorShadowBuildReport &report,
                                                      const char *material_name,
                                                      const bool legacy_compile_success,
                                                      const char *legacy_summary)
    {
        std::string text;
        text.reserve(1024);
        text += "# ShaderGen 基线对比报告（自动生成）\n\n";
        text += "- Material: `";
        text += material_name ? material_name : "<unnamed>";
        text += "`\n";
        text += "- Legacy compile: `";
        text += legacy_compile_success ? "success" : "failed";
        text += "`\n";
        text += "- Pipeline shadow compile: `";
        text += report.result.success ? "success" : "failed";
        text += "`\n\n";
        text += "## Route-Switch Readiness\n\n";
        text += "- Readiness: `";
        text += report.summary;
        text += "`\n\n";
        text += "## Legacy 摘要\n\n";
        text += "- LegacySummary: `";
        text += legacy_summary ? legacy_summary : (legacy_compile_success ? "compile succeeded" : "compile failed");
        text += "`\n\n";
        text += "## Shadow Diagnostics\n\n";

        if(report.result.diagnostics.empty())
        {
            text += "- `<none>`\n";
        }
        else
        {
            for(const auto &diag:report.result.diagnostics)
            {
                text += "- `";
                text += diag.subject;
                text += "`: ";
                text += diag.message;
                text += "\n";
            }
        }

        return text;
    }

    static MaterialCreateInfo *CreatePreparedCompositorMaterial(
        const contract::PhysicalDeviceProfileLite *profile,
        const StaticMaterialDef &def,
        const std::string &vs_glsl,
        const std::string &fs_glsl,
        const Material3DCreateConfig *config,
        std::string *diagnostics)
    {
        if (diagnostics)
            diagnostics->clear();

        if (vs_glsl.empty() || fs_glsl.empty())
        {
            if (diagnostics)
                *diagnostics = "vs_glsl or fs_glsl is empty";
            return nullptr;
        }

        Material3DCreateConfig cfg = config ? *config : Material3DCreateConfig();
        cfg.prim = config ? config->prim : def.primitive_type;
        cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

        const bool infer_has_camera = HasUBOSemantic(def, UBODescriptorSemantic::CameraInfo);
        const bool infer_has_sky    = HasUBOSemantic(def, UBODescriptorSemantic::SkyInfo);
        const bool infer_has_l2w    = HasSSBOSemantic(def, SSBODescriptorSemantic::TransformData);
        const bool infer_has_mi     = HasSSBOSemantic(def, SSBODescriptorSemantic::MaterialBindingInstanceData)
                                   || HasPerMaterialDescriptor(def)
                                   || (def.shader_data_schema != ShaderDataSchema::None);

        cfg.local_to_world    = cfg.local_to_world    || infer_has_l2w;
        cfg.material_instance = cfg.material_instance || infer_has_mi;

        EmitInferenceMismatchDiagnostics(def,
                         cfg,
                         infer_has_camera,
                         infer_has_sky,
                         diagnostics);

        MaterialBuilder builder(&cfg);
        if (profile)
            builder.SetDevice(profile);

        auto FailWithBuilder = [&](const char *reason) -> MaterialCreateInfo *
        {
            if (diagnostics)
            {
                *diagnostics = reason ? reason : "<unknown>";
                *diagnostics += " (";
                *diagnostics += BuildShaderDataSchemaDebugText(def);
                *diagnostics += ")";
            }
            return nullptr;
        };

        uint32_t mi_stage_bits = uint32_t(ShaderStage::Fragment);

        if (def.ubo_descriptors)
        {
            for (const auto semantic : *def.ubo_descriptors)
            {
                if (!builder.AddUBOStruct(kDefaultDescriptorStageBits, semantic))
                    return FailWithBuilder("AddUBO() failed");
            }
        }

        if (def.ssbo_descriptors)
        {
            for (const auto semantic : *def.ssbo_descriptors)
            {
                if (semantic == SSBODescriptorSemantic::TransformData)
                {
                    builder.SetLocalToWorld(kDefaultDescriptorStageBits);
                    continue;
                }

                if (semantic == SSBODescriptorSemantic::MaterialBindingInstanceData)
                {
                    mi_stage_bits = kDefaultDescriptorStageBits;
                    continue;
                }

                if (!builder.AddSSBOStruct(kDefaultDescriptorStageBits, semantic))
                    return FailWithBuilder("AddSSBO() failed");
            }
        }

        if (def.texture_samplers)
        {
            for (const auto &[slot, descriptor] : *def.texture_samplers)
            {
                if (!RangeCheck(descriptor.sampler_type))
                    return FailWithBuilder("texture sampler slot has invalid SamplerType");

                // Use default descriptor stage bits for texture samplers
                if (!builder.AddTextureSampler(kDefaultDescriptorStageBits,
                                            descriptor.sampler_type,
                                            slot,
                                            descriptor.channel_hint))
                {
                    return FailWithBuilder("AddTextureSampler(slot) failed");
                }
            }
        }

        ShaderCreateInfoVertex *vsc = builder.GetVertexShader();
        if (vsc)
        {
            for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
            {
                const FixedVertexEntry &entry = def.vertex_entries[i];
                vsc->AddInput(entry.type, entry.attrib);
            }
        }

        if (def.shader_data_schema != ShaderDataSchema::None)
        {
            const ShaderDataSchemaInfo &schema_info = GetShaderDataSchemaInfo(def.shader_data_schema);

            if (schema_info.byte_size == 0)
                return FailWithBuilder("shader data schema has zero byte size");

            if (!builder.SetMaterialInstance(def.shader_data_schema, schema_info, mi_stage_bits))
                return FailWithBuilder("SetMaterialInstance() failed");
        }

        ShaderCreateInfoVertex *vert = builder.GetVertexShader();
        ShaderCreateInfo *frag = builder.GetStageShader(ShaderStage::Fragment);

        std::string final_vs_glsl = vs_glsl;
        std::string final_fs_glsl = fs_glsl;

        if (def.shader_data_schema != ShaderDataSchema::None)
        {
            const ShaderDataSchemaInfo &schema_info = GetShaderDataSchemaInfo(def.shader_data_schema);
            const std::string schema_include = BuildShaderDataSchemaIncludeText(schema_info);

            if (schema_include.empty())
                return FailWithBuilder("shader data schema has no GLSL include path");

            final_vs_glsl = hgl::graph::internal::InjectAfterVersion(final_vs_glsl, schema_include);
            final_fs_glsl = hgl::graph::internal::InjectAfterVersion(final_fs_glsl, schema_include);
        }

        if (vert)
            vert->SetFinalGLSL(final_vs_glsl);

        if (frag)
            frag->SetFinalGLSL(final_fs_glsl);

        MaterialCreateInfo *mci = builder.BuildSnapshotOnly();
        if (!mci)
            return FailWithBuilder("MaterialBuilder::BuildSnapshotOnly() failed");

        if (!InjectLayoutDefines(*mci))
        {
            delete mci;
            return FailWithBuilder("InjectLayoutDefines() failed");
        }

        return mci;
    }
}

bool WriteCompileCompositorLegacyTree(const MaterialCreateInfo &mci,
                                      const char *material_name,
                                      const char *legacy_root)
{
    return WriteCompileCompositorLegacyTreeInternal(mci,material_name,legacy_root);
}

std::string BuildCompileCompositorBaselineCompareCommand(const char *material_name,
                                                         const char *trial_root)
{
    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path trial_root_path(trial_root ? trial_root : "build/shadergen_trial");
    const std::filesystem::path legacy_dir=trial_root_path/"legacy"/sanitized_name;
    const std::filesystem::path pipeline_dir=trial_root_path/"pipeline"/sanitized_name;
    const std::filesystem::path report_path=trial_root_path/"reports"/(sanitized_name + "_baseline_compare.md");
    const std::filesystem::path readiness_path=trial_root_path/"reports"/(sanitized_name + "_readiness.txt");

    std::string command;
    command.reserve(512);
    command += "python shadergen_baseline_compare.py --legacy \"";
    command += legacy_dir.string();
    command += "\" --pipeline \"";
    command += pipeline_dir.string();
    command += "\" --report \"";
    command += report_path.string();
    command += "\" --readiness-file \"";
    command += readiness_path.string();
    command += "\"";
    return command;
}

bool RunCompileCompositorBaselineCompare(const char *material_name,
                                         const char *trial_root)
{
    const std::string command=BuildCompileCompositorBaselineCompareCommand(material_name,trial_root);
    return std::system(command.c_str())==0;
}

bool WriteCompileCompositorTrialAggregateReport(const char *trial_root)
{
    const std::filesystem::path trial_root_path(trial_root ? trial_root : "build/shadergen_trial");
    const std::filesystem::path reports_path=trial_root_path/"reports";

    if(!EnsureDirectoryExists(reports_path.string().c_str()))
        return false;

    return WriteTextFile(reports_path/"baseline_compare.md",
                         BuildTrialAggregateReportText(trial_root_path));
}

std::string GetCompileCompositorTrialBatchSummary(const CompileCompositorTrialBatchReport &report)
{
    std::string text;
    text += "total_count=";
    text += std::to_string(report.total_count);
    text += ", legacy_success_count=";
    text += std::to_string(report.legacy_success_count);
    text += ", pipeline_trial_success_count=";
    text += std::to_string(report.pipeline_trial_success_count);
    text += ", baseline_report_count=";
    text += std::to_string(report.baseline_report_count);
    text += ", baseline_compare_success_count=";
    text += std::to_string(report.baseline_compare_success_count);
    text += ", aggregate_report_written=";
    text += report.aggregate_report_written ? "true" : "false";
    return text;
}

CompileCompositorTrialBatchReport RunCompileCompositorTrialBatch(const contract::PhysicalDeviceProfileLite *profile,
                                                                 const std::vector<CompileCompositorTrialBatchItem> &items,
                                                                 const char *trial_root,
                                                                 const bool run_baseline_compare_script)
{
    CompileCompositorTrialBatchReport report{};
    report.total_count=items.size();

    for(const auto &item:items)
    {
        if(!item.def)
            continue;

        const char *material_name = item.material_name_override.empty()
                                  ? (item.def->name ? item.def->name : "<unnamed>")
                                  : item.material_name_override.c_str();

        CompileCompositorShadowBuildReport shadow_report = BuildCompileCompositorShadowPipelineReportForConfig(profile,*item.def,item.config);

        if(shadow_report.result.success)
            ++report.pipeline_trial_success_count;

        WriteCompileCompositorShadowBuildArtifacts(shadow_report,material_name);
        WriteCompileCompositorShadowPipelineTree(shadow_report,material_name);

        MaterialCreateInfo *mci = CompileCompositorMaterialForConfig(profile,
                                                                     *item.def,
                                                                     item.vs_glsl,
                                                                     item.fs_glsl,
                                                                     item.config);

        const bool legacy_success = mci != nullptr;

        if(legacy_success)
        {
            ++report.legacy_success_count;
            WriteCompileCompositorLegacyTree(*mci,material_name);
        }

        if(WriteCompileCompositorTrialBaselineReport(shadow_report,
                                                     material_name,
                                                     legacy_success,
                                                     legacy_success ? "legacy compile succeeded" : "legacy compile failed",
                                                     trial_root))
        {
            ++report.baseline_report_count;
        }

        if(run_baseline_compare_script && RunCompileCompositorBaselineCompare(material_name,trial_root))
            ++report.baseline_compare_success_count;

        delete mci;
    }

    report.aggregate_report_written = WriteCompileCompositorTrialAggregateReport(trial_root);
    return report;
}

CompileCompositorTrialBatchReport RunCompileCompositorBuiltinCandidateTrialBatch(const contract::PhysicalDeviceProfileLite *profile,
                                                                                 const char *trial_root,
                                                                                 const bool run_baseline_compare_script)
{
    BuiltinTrialBatchBuilderState state;

    const MaterialPreset candidate_presets[] =
    {
        MaterialPreset::PureColor2D,
        MaterialPreset::PureTexture2D,
        MaterialPreset::VertexColor2D,
        MaterialPreset::Checkerboard3D,
        MaterialPreset::FullscreenTriangle,
        MaterialPreset::PureColor3D,
        MaterialPreset::TerrainGrid,
        MaterialPreset::SkyMinimal,
        MaterialPreset::VertexColor3D,
        MaterialPreset::VertexLuminance2D,
        MaterialPreset::VertexLuminance3D,
        MaterialPreset::VertexPaletteColor3D,
        MaterialPreset::Gizmo3D,
        MaterialPreset::Billboard2DDynamic,
        MaterialPreset::Billboard2DFixed,
        MaterialPreset::PBRColor3D,
        MaterialPreset::Standard,
        MaterialPreset::Text2D,
    };

    for(const auto preset:candidate_presets)
        TryAppendBuiltinTrialCandidate(state,preset);

    return RunCompileCompositorTrialBatch(profile,state.items,trial_root,run_baseline_compare_script);
}

static CompileCompositorShadowBuildReport BuildCompileCompositorShadowPipelineReportForConfig(const contract::PhysicalDeviceProfileLite *profile,
                                                                                               const StaticMaterialDef &def,
                                                                                               const MaterialCreateConfig *config)
{
    if(const auto *cfg3d=As3D(config))
        return BuildCompileCompositorShadowPipelineReport(profile,def,cfg3d);

    if(config && (config->kind==ConfigKind::D2 || config->kind==ConfigKind::Text2D))
    {
        Material3DCreateConfig cfg3d(
            config->prim,
            IncludeCamera::Without,
            config->local_to_world ? IncludeL2W::With : IncludeL2W::Without,
            IncludeSky::Without);
        cfg3d.rt_output = config->rt_output;
        cfg3d.material_instance = config->material_instance;
        cfg3d.shader_stage_flag_bit = config->shader_stage_flag_bit;
        return BuildCompileCompositorShadowPipelineReport(profile,def,&cfg3d);
    }

    return BuildCompileCompositorShadowPipelineReport(profile,def,nullptr);
}

static MaterialCreateInfo *CompileCompositorMaterialForConfig(const contract::PhysicalDeviceProfileLite *profile,
                                                              const StaticMaterialDef &def,
                                                              const std::string &vs_glsl,
                                                              const std::string &fs_glsl,
                                                              const MaterialCreateConfig *config)
{
    if(const auto *cfg3d=As3D(config))
        return CompileCompositorMaterial(profile,def,vs_glsl,fs_glsl,cfg3d);

    if(config && (config->kind==ConfigKind::D2 || config->kind==ConfigKind::Text2D))
        return CompileCompositorMaterial(profile,def,vs_glsl,fs_glsl,static_cast<const Material2DCreateConfig *>(config));

    return CompileCompositorMaterial(profile,def,vs_glsl,fs_glsl,static_cast<const Material3DCreateConfig *>(nullptr));
}

static bool HasUBOSemantic(const StaticMaterialDef &def, const UBODescriptorSemantic semantic)
{
    if (!def.ubo_descriptors || semantic == UBODescriptorSemantic::Unknown)
        return false;

    return def.ubo_descriptors->contains(semantic);
}

static bool HasSSBOSemantic(const StaticMaterialDef &def, const SSBODescriptorSemantic semantic)
{
    if (!def.ssbo_descriptors || semantic == SSBODescriptorSemantic::Unknown)
        return false;

    return def.ssbo_descriptors->contains(semantic);
}

static bool HasPerMaterialDescriptor(const StaticMaterialDef &def)
{
    if (def.ubo_descriptors)
    {
        for (const auto semantic : *def.ubo_descriptors)
        {
            if (GetDescriptorSemanticMeta(semantic).set_type == SET_TYPE_MATERIAL)
                return true;
        }
    }

    if (def.ssbo_descriptors)
    {
        for (const auto semantic : *def.ssbo_descriptors)
        {
            if (GetDescriptorSemanticMeta(semantic).set_type == SET_TYPE_MATERIAL)
                return true;
        }
    }

    if (def.texture_samplers)
    {
        if (!def.texture_samplers->empty())
            return true;
    }

    return false;
}

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material3DCreateConfig *config)
{
    const CompileCompositorRouteDecision route_decision=ResolveCompileCompositorRouteDecision(nullptr);

    std::fprintf(stderr,
        "[CompileCompositorMaterial] material=%s route_decision: %s\n",
        def.name ? def.name : "<unnamed>",
        GetCompileCompositorRouteDecisionSummary(route_decision).c_str());

    CompileCompositorShadowBuildReport shadow_report{};
    bool has_shadow_report=false;

    if(route_decision.pipeline_trial_requested)
    {
        shadow_report=BuildCompileCompositorShadowPipelineReport(profile,def,config);
        has_shadow_report=true;

        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s pipeline_shadow: %s\n",
            def.name ? def.name : "<unnamed>",
            shadow_report.summary.empty() ? "<empty>" : shadow_report.summary.c_str());

        if(WriteCompileCompositorShadowBuildArtifacts(shadow_report,def.name))
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s pipeline_shadow_artifacts=build/shadergen_trial/reports\n",
                def.name ? def.name : "<unnamed>");
        }
        else
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s pipeline_shadow_artifacts_write_failed\n",
                def.name ? def.name : "<unnamed>");
        }

        if(WriteCompileCompositorShadowPipelineTree(shadow_report,def.name))
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s pipeline_shadow_tree=build/shadergen_trial/pipeline\n",
                def.name ? def.name : "<unnamed>");
        }
        else
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s pipeline_shadow_tree_write_failed\n",
                def.name ? def.name : "<unnamed>");
        }
    }

    std::string diagnostics;
    MaterialCreateInfo *mci = CreatePreparedCompositorMaterial(profile,
                                                               def,
                                                               vs_glsl,
                                                               fs_glsl,
                                                               config,
                                                               &diagnostics);
    if (!mci)
    {
        if(has_shadow_report)
        {
            WriteCompileCompositorTrialBaselineReport(shadow_report,
                                                      def.name,
                                                      false,
                                                      diagnostics.empty() ? "CreatePreparedCompositorMaterial failed" : diagnostics.c_str());
        }

        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: %s\n",
            def.name ? def.name : "<unnamed>",
            diagnostics.empty() ? "<unknown>" : diagnostics.c_str());
        return nullptr;
    }

    if (!mci->CompileShaderStagesToSPV())
    {
        if(has_shadow_report)
        {
            WriteCompileCompositorTrialBaselineReport(shadow_report,
                                                      def.name,
                                                      false,
                                                      "CompileShaderStagesToSPV() failed");
        }

        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s failed: CompileShaderStagesToSPV() failed (check GLSLCompiler log) (%s)\n",
            def.name ? def.name : "<unnamed>",
            BuildShaderDataSchemaDebugText(def).c_str());
        delete mci;
        return nullptr;
    }

    if (!diagnostics.empty())
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s diagnostics: %s\n",
            def.name ? def.name : "<unnamed>",
            diagnostics.c_str());
    }

    if(has_shadow_report)
    {
        if(WriteCompileCompositorTrialBaselineReport(shadow_report,
                                                     def.name,
                                                     true,
                                                     diagnostics.empty() ? "CompileCompositorMaterial legacy compile succeeded" : diagnostics.c_str()))
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s baseline_compare_report=build/shadergen_trial/reports\n",
                def.name ? def.name : "<unnamed>");
        }
        else
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s baseline_compare_report_write_failed\n",
                def.name ? def.name : "<unnamed>");
        }

        if(RunCompileCompositorBaselineCompare(def.name))
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s baseline_compare_script=build/shadergen_trial/reports\n",
                def.name ? def.name : "<unnamed>");

            if(WriteCompileCompositorTrialAggregateReport())
            {
                std::fprintf(stderr,
                    "[CompileCompositorMaterial] material=%s baseline_compare_aggregate=build/shadergen_trial/reports/baseline_compare.md\n",
                    def.name ? def.name : "<unnamed>");
            }
            else
            {
                std::fprintf(stderr,
                    "[CompileCompositorMaterial] material=%s baseline_compare_aggregate_write_failed\n",
                    def.name ? def.name : "<unnamed>");
            }
        }
        else
        {
            std::fprintf(stderr,
                "[CompileCompositorMaterial] material=%s baseline_compare_script_failed\n",
                def.name ? def.name : "<unnamed>");
        }
    }

    if(WriteCompileCompositorLegacyTree(*mci,def.name))
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s legacy_tree=build/shadergen_trial/legacy\n",
            def.name ? def.name : "<unnamed>");
    }
    else
    {
        std::fprintf(stderr,
            "[CompileCompositorMaterial] material=%s legacy_tree_write_failed\n",
            def.name ? def.name : "<unnamed>");
    }

    return mci;
}

bool PrepareCompositorGLSLForReflection(
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    std::string &out_vs_glsl,
    std::string &out_fs_glsl,
    std::string *diagnostics)
{
    MaterialCreateInfo *mci = CreatePreparedCompositorMaterial(nullptr,
                                                               def,
                                                               vs_glsl,
                                                               fs_glsl,
                                                               nullptr,
                                                               diagnostics);
    if (!mci)
        return false;

    ShaderCreateInfoVertex *vert = mci->GetVertexShader();
    ShaderCreateInfo *frag = mci->GetStageShader(ShaderStage::Fragment);

    out_vs_glsl = vert ? vert->GetFinalGLSL() : std::string();
    out_fs_glsl = frag ? frag->GetFinalGLSL() : std::string();

    delete mci;
    return true;
}

CompileCompositorRoutePlan BuildCompileCompositorRoutePlan()
{
    CompileCompositorRoutePlan plan{};
    plan.preferred_route = ShaderBuildRoute::LegacyMaterialCreateInfo;
    plan.allow_pipeline_fallback = true;
    plan.can_export_readiness = true;
    plan.can_emit_baseline_artifacts = true;
    plan.rationale = "CompileCompositorMaterial holds StaticMaterialDef + GLSL + config + profile, making it the closest unified compile/model boundary for future route-switch, fallback, readiness export, and baseline artifact emission without changing the default production path yet.";
    return plan;
}

CompileCompositorRouteDecision ResolveCompileCompositorRouteDecision(const ShaderBuildSwitchConfig *switch_config)
{
    const CompileCompositorRoutePlan plan=BuildCompileCompositorRoutePlan();

    CompileCompositorRouteDecision decision{};
    decision.resolved_route=ResolveShaderBuildRoute(switch_config);
    decision.pipeline_trial_requested=(decision.resolved_route==ShaderBuildRoute::Pipeline);
    decision.fallback_to_legacy=plan.allow_pipeline_fallback;
    decision.will_use_legacy_now=true;

    if(decision.pipeline_trial_requested)
    {
        decision.rationale = "Pipeline route requested for CompileCompositorMaterial, but the current F2.10 plan keeps the production entry on Legacy while preserving fallback/readiness export preparation.";
    }
    else
    {
        decision.rationale = "CompileCompositorMaterial remains on Legacy by default in F2.10; route-switch intent is evaluated but not yet wired into the production compile branch.";
    }

    return decision;
}

std::string GetCompileCompositorRouteDecisionSummary(const CompileCompositorRouteDecision &decision)
{
    std::string text;
    text.reserve(256 + decision.rationale.size());
    text += "resolved_route=";
    text += GetShaderBuildRouteName(decision.resolved_route);
    text += ", will_use_legacy_now=";
    text += decision.will_use_legacy_now ? "true" : "false";
    text += ", pipeline_trial_requested=";
    text += decision.pipeline_trial_requested ? "true" : "false";
    text += ", fallback_to_legacy=";
    text += decision.fallback_to_legacy ? "true" : "false";

    if(!decision.rationale.empty())
    {
        text += ", rationale=";
        text += decision.rationale;
    }

    return text;
}

CompileCompositorShadowBuildReport BuildCompileCompositorShadowPipelineReport(const contract::PhysicalDeviceProfileLite *profile,
                                                                              const StaticMaterialDef &def,
                                                                              const Material3DCreateConfig *config)
{
    CompileCompositorShadowBuildReport report{};

    ShaderBuildPipeline pipeline;
    const MaterialCreateConfig pipeline_cfg=BuildPipelineConfigFromCompositorConfig(def,config);
    const ShaderBuildDescriptorSpec descriptor_spec=BuildDescriptorSpecFromStaticMaterialDef(def);

    report.pipeline_config=pipeline_cfg;
    report.descriptor_spec=descriptor_spec;

    report.result=pipeline.Build(pipeline_cfg,profile,&descriptor_spec);
    report.evaluation=EvaluateShaderBuildResultForRouteSwitch(report.result);
    report.summary=GetShaderBuildRouteEvaluationSummary(report.evaluation);

    if(!report.result.success)
    {
        if(!report.summary.empty())
            report.summary += ", diagnostics=";

        AppendShadowBuildDiagnostics(report.summary,report.result);
    }

    return report;
}

bool WriteCompileCompositorShadowBuildArtifacts(const CompileCompositorShadowBuildReport &report,
                                                const char *material_name,
                                                const char *reports_dir)
{
    if(!EnsureDirectoryExists(reports_dir))
        return false;

    const std::filesystem::path reports_path(reports_dir);
    const std::string sanitized_name=SanitizeArtifactName(material_name);

    const std::filesystem::path readiness_path=reports_path/(sanitized_name + "_readiness.txt");
    if(!WriteShaderBuildRouteEvaluationSummary(report.evaluation,readiness_path.string().c_str()))
        return false;

    const std::filesystem::path diagnostics_path=reports_path/(sanitized_name + "_diagnostics.log");
    return WriteTextFile(diagnostics_path,BuildShadowDiagnosticsText(report,material_name));
}

bool WriteCompileCompositorTrialBaselineReport(const CompileCompositorShadowBuildReport &report,
                                               const char *material_name,
                                               const bool legacy_compile_success,
                                               const char *legacy_summary,
                                               const char *trial_root)
{
    if(!EnsureDirectoryExists(trial_root))
        return false;

    const std::filesystem::path trial_root_path(trial_root);
    const std::filesystem::path reports_path=trial_root_path/"reports";

    if(!EnsureDirectoryExists(reports_path.string().c_str()))
        return false;

    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path report_path=reports_path/(sanitized_name + "_baseline_compare.md");

    return WriteTextFile(report_path,
                         BuildBaselineCompareReportText(report,
                                                        material_name,
                                                        legacy_compile_success,
                                                        legacy_summary));
}

bool WriteCompileCompositorShadowPipelineTree(const CompileCompositorShadowBuildReport &report,
                                              const char *material_name,
                                              const char *pipeline_root)
{
    if(!EnsureDirectoryExists(pipeline_root))
        return false;

    const std::string sanitized_name=SanitizeArtifactName(material_name);
    const std::filesystem::path material_root=std::filesystem::path(pipeline_root)/sanitized_name;

    if(!EnsureDirectoryExists(material_root.string().c_str()))
        return false;

    if(!WriteTextFile(material_root/"descriptor_spec.txt",BuildDescriptorSpecText(report.descriptor_spec)))
        return false;

    if(!WriteTextFile(material_root/"pipeline_config.txt",BuildPipelineConfigText(report.pipeline_config)))
        return false;

    if(!WriteTextFile(material_root/"result_summary.txt",BuildPipelineResultText(report)))
        return false;

    if(!WriteTextFile(material_root/"readiness.txt",report.summary))
        return false;

    if(!WriteTextFile(material_root/"diagnostics.log",BuildShadowDiagnosticsText(report,material_name)))
        return false;

    for(size_t i=0;i<report.result.value.binaries.size();++i)
    {
        const ShaderBinary &binary=report.result.value.binaries[i];
        const std::filesystem::path spv_path=material_root/(std::string("stage_") + std::to_string(i) + ".spv.txt");

        if(!WriteTextFile(spv_path,BuildSpirvHexText(binary)))
            return false;
    }

    return true;
}

MaterialCreateInfo *CompileCompositorMaterial(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &    def,
    const std::string &         vs_glsl,
    const std::string &         fs_glsl,
    const Material2DCreateConfig *config)
{
    Material3DCreateConfig cfg3d(
        config ? config->prim : def.primitive_type,
        IncludeCamera::Without,
        config && config->local_to_world ? IncludeL2W::With : IncludeL2W::Without,
        IncludeSky::Without);

    if (config)
    {
        cfg3d.rt_output                         = config->rt_output;
        cfg3d.material_instance                 = config->material_instance;
        cfg3d.shader_stage_flag_bit             = config->shader_stage_flag_bit;
    }

    return CompileCompositorMaterial(profile, def, vs_glsl, fs_glsl, &cfg3d);
}

bool InjectLayoutDefines(MaterialCreateInfo &mci)
{
    ShaderCreateInfoVertex *vert = mci.GetVertexShader();
    ShaderCreateInfo       *frag = mci.GetStageShader(ShaderStage::Fragment);

    mci.Resort();
    const ShaderLayoutContract layout = hgl::graph::BuildShaderLayoutContract(mci);
    const std::string layout_defs = hgl::graph::EmitShaderLayoutDefines(layout);
    const MaterialDescriptorDB &mdi = mci.GetDescriptorInfo();
    const std::string vert_sampler_defs = vert ? hgl::graph::EmitSimpleSamplerGLSL(mdi, ShaderStage::Vertex)   : std::string();
    const std::string frag_sampler_defs = frag ? hgl::graph::EmitSimpleSamplerGLSL(mdi, ShaderStage::Fragment) : std::string();
    const std::string frag_mit_defs     = frag ? hgl::graph::EmitMaterialInstanceTextureGLSL(mdi, ShaderStage::Fragment) : std::string();

    if (!layout_defs.empty() || !vert_sampler_defs.empty() || !frag_sampler_defs.empty() || !frag_mit_defs.empty())
    {
        if (vert) vert->SetFinalGLSL(hgl::graph::internal::InjectAfterVersion(vert->GetFinalGLSL(), layout_defs + vert_sampler_defs));
        if (frag) frag->SetFinalGLSL(hgl::graph::internal::InjectAfterVersion(frag->GetFinalGLSL(), layout_defs + frag_sampler_defs + frag_mit_defs));
    }

    return true;
}

}  // namespace hgl::graph::mtl
