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

    static CompileCompositorShadowBuildReport BuildCompileCompositorShadowPipelineReportForConfig(const contract::PhysicalDeviceProfileLite *profile,
                                                                                                   const StaticMaterialDef &def,
                                                                                                   const MaterialCreateConfig *config)
    {
        return BuildCompileCompositorShadowPipelineReport(profile,def,config);
    }

} // namespace

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
        else
        {
            ++report.pipeline_trial_failure_count;
            report.pipeline_trial_failed_materials.emplace_back(material_name ? material_name : "<unnamed>");
        }

        WriteCompileCompositorShadowBuildArtifacts(shadow_report,material_name);
        WriteCompileCompositorShadowPipelineTree(shadow_report,material_name);

        MaterialCreateInfo *mci = nullptr;

        if(const auto *cfg3d=As3D(item.config))
            mci = CompileCompositorMaterial(profile,*item.def,item.vs_glsl,item.fs_glsl,cfg3d);
        else if(item.config && (item.config->kind==ConfigKind::D2 || item.config->kind==ConfigKind::Text2D))
            mci = CompileCompositorMaterial(profile,
                                            *item.def,
                                            item.vs_glsl,
                                            item.fs_glsl,
                                            static_cast<const Material2DCreateConfig *>(item.config));
        else
            mci = CompileCompositorMaterial(profile,
                                            *item.def,
                                            item.vs_glsl,
                                            item.fs_glsl,
                                            static_cast<const Material3DCreateConfig *>(nullptr));

        const bool direct_compile_success = mci != nullptr;

        if(direct_compile_success)
        {
            ++report.legacy_success_count;
            WriteCompileCompositorLegacyTree(*mci,material_name);
        }
        else
        {
            ++report.legacy_failure_count;
            report.legacy_failed_materials.emplace_back(material_name ? material_name : "<unnamed>");
        }

        if(WriteCompileCompositorTrialBaselineReport(shadow_report,
                                                     material_name,
                                                     direct_compile_success,
                                                     direct_compile_success ? "direct compile succeeded" : "direct compile failed",
                                                     trial_root))
        {
            ++report.baseline_report_count;
        }

        if(run_baseline_compare_script && RunCompileCompositorBaselineCompare(material_name,trial_root))
            ++report.baseline_compare_success_count;

        delete mci;
    }

    report.aggregate_report_written = WriteCompileCompositorTrialAggregateReport(trial_root,&report);
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

}  // namespace hgl::graph::mtl

