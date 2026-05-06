set(SHADERGEN_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/shadergen)
set(STD_MTL_HEADER_PATH     ${ROOT_INCLUDE_PATH}/hgl/mtl)
set(MTL_TYPE_INCLUDE_PATH   ${ROOT_INCLUDE_PATH}/hgl/mtl)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Shader Create Info
SET(SHADER_CREATER_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfo.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderStageMap.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoVertex.h)
SET(SHADER_CREATER_SOURCE_FILES ShaderCreateInfo.cpp
                                ShaderStageMap.cpp
                                ShaderCreateInfoVertex.cpp)
SOURCE_GROUP("Material Create Info\\Shader" FILES   ${SHADER_CREATER_HEADER_FILES}
                                                    ${SHADER_CREATER_SOURCE_FILES})

# Material Create Info
SET(MATERIAL_CREATE_INFO_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/MaterialCreateInfo.h)
SET(MATERIAL_CREATE_INFO_SOURCE_FILES MaterialCreateInfo.cpp)
SOURCE_GROUP("Material Create Info" FILES   ${MATERIAL_CREATE_INFO_HEADER_FILES}
                                            ${MATERIAL_CREATE_INFO_SOURCE_FILES})

# Material Create Info / Builders
SET(MATERIAL_CREATE_INFO_BUILDER_FILES ${SHADERGEN_INCLUDE_PATH}/BindingContractBuilder.h
                                       ${SHADERGEN_INCLUDE_PATH}/DescriptorLayoutBuilder.h
                                       BindingContractBuilder.cpp
                                       DescriptorLayoutBuilder.cpp)
SOURCE_GROUP("Material Create Info\\Builders" FILES ${MATERIAL_CREATE_INFO_BUILDER_FILES})

# Material Create Info / Binders
SET(MATERIAL_CREATE_INFO_BINDER_FILES ${SHADERGEN_INCLUDE_PATH}/MaterialDescriptorStageBinder.h
                                      MaterialDescriptorStageBinder.cpp)
SOURCE_GROUP("Material Create Info\\Binders" FILES ${MATERIAL_CREATE_INFO_BINDER_FILES})

# Material Create Info / Configurators
SET(MATERIAL_CREATE_INFO_CONFIGURATOR_FILES ${SHADERGEN_INCLUDE_PATH}/MaterialInstanceConfigurator.h
                                            MaterialInstanceConfigurator.cpp)
SOURCE_GROUP("Material Create Info\\Configurators" FILES ${MATERIAL_CREATE_INFO_CONFIGURATOR_FILES})

# Material Create Info / Compilers
SET(MATERIAL_CREATE_INFO_COMPILER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderStageCompiler.h
                                        ${SHADERGEN_INCLUDE_PATH}/ShaderSetCompiler.h
                                        ShaderStageCompiler.cpp
                                        ShaderSetCompiler.cpp)
SOURCE_GROUP("Material Create Info\\Compilers" FILES ${MATERIAL_CREATE_INFO_COMPILER_FILES})

# Material Create Info / Stage Set
SET(MATERIAL_CREATE_INFO_STAGE_SET_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderStageBuildSet.h
                                         ShaderStageBuildSet.cpp)
SOURCE_GROUP("Material Create Info\\Stage Set" FILES ${MATERIAL_CREATE_INFO_STAGE_SET_FILES})

# Descriptor Info
SET(DESC_INFO_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/ShaderStageIO.h
                            ${SHADERGEN_INCLUDE_PATH}/MaterialDescriptorDB.h
                            ${STD_MTL_HEADER_PATH}/DescriptorSemanticRegistry.h)
SET(DESC_INFO_SOURCE_FILES  ShaderStageIO.cpp
                            MaterialDescriptorDB.cpp)
SOURCE_GROUP("Material Create Info\\Descriptor Info" FILES  ${DESC_INFO_HEADER_FILES}
                                                            ${DESC_INFO_SOURCE_FILES})

# GLSL Compiler
SET(GLSL_COMPILER_SOURCE    GLSLCompiler.h
                            GLSLCompiler.cpp
                            SPVParseData.h
                            TBuiltInResourceCompat.h)
SOURCE_GROUP("GLSL Compiler" FILES ${GLSL_COMPILER_SOURCE})

# Common
SET(SHADERGEN_COMMON_FILES  ${STD_MTL_HEADER_PATH}/UBOCommon.h
                            ${STD_MTL_HEADER_PATH}/SamplerSlot.h
                            ${STD_MTL_HEADER_PATH}/ShaderVariableType.h
                            ${SHADERGEN_INCLUDE_PATH}/ShaderLibraryPath.h
                            ${SHADERGEN_INCLUDE_PATH}/ShaderWriter.h
                            ShaderVariableType.cpp
                            ShaderLibraryPath.cpp
                            ShaderWriter.cpp
                            common/UBOCommon.cpp)
SOURCE_GROUP("Common" FILES ${SHADERGEN_COMMON_FILES})

# Common / Material Functions
SET(SHADERGEN_COMMON_MATFUNC_FILES  common/MFSkyLight.h)
SOURCE_GROUP("Common\\Material Functions" FILES ${SHADERGEN_COMMON_MATFUNC_FILES})

# Common / Variant Policy
SET(SHADERGEN_COMMON_VARIANT_POLICY_FILES common/DefaultVariantRouter.h
                                          common/DefaultVariantRouter.cpp)
SOURCE_GROUP("Common\\Variant Policy" FILES ${SHADERGEN_COMMON_VARIANT_POLICY_FILES})

# Fixed Material Config

# VertexAttrib Macro Map
SET(SHADERGEN_VERTEX_ATTRIB_MACRO_MAP_FILES
    ${SHADERGEN_INCLUDE_PATH}/VertexAttribMacroMap.h
    VertexAttribMacroMap.cpp)
SOURCE_GROUP("VertexAttrib Macro Map" FILES ${SHADERGEN_VERTEX_ATTRIB_MACRO_MAP_FILES})

list(APPEND SHADERGEN_COMMON_FILES ${SHADERGEN_VERTEX_ATTRIB_MACRO_MAP_FILES})
SET(FIXED_MTL_FILES ${SHADERGEN_INCLUDE_PATH}/GLSLCompilerConfig.h
                    ${SHADERGEN_INCLUDE_PATH}/device/DeviceProfile.h
                    ${SHADERGEN_INCLUDE_PATH}/device/DeviceProfileAdapter.h
                    ${SHADERGEN_INCLUDE_PATH}/device/DeviceProfileJson.h
                    ${SHADERGEN_INCLUDE_PATH}/device/DeviceProfileTargetVersion.h)
SOURCE_GROUP("Fixed Material" FILES ${FIXED_MTL_FILES})

# Standard Material Base
SET(STD_MTL_SOURCE  ${STD_MTL_HEADER_PATH}/MaterialCreateConfig.h
                    ${STD_MTL_HEADER_PATH}/MaterialBuildFlags.h
                    ${STD_MTL_HEADER_PATH}/ShaderDataSchema.h
                    ${STD_MTL_HEADER_PATH}/StaticMaterialDef.h
                    ${STD_MTL_HEADER_PATH}/MaterialResourceManifest.h
                    MaterialCreateConfig.cpp
                    ShaderDataSchema.cpp
                    GLSLStructSize.cpp)
SOURCE_GROUP("Standard Material" FILES ${STD_MTL_SOURCE})

# Material Key
SET(MATERIAL_KEY_FILES  ${MTL_TYPE_INCLUDE_PATH}/MaterialKey.h
                        MaterialKey.cpp)
SOURCE_GROUP("Material Key" FILES ${MATERIAL_KEY_FILES})

# Recipe To Key
SET(RECIPE_TO_KEY_FILES ${MTL_TYPE_INCLUDE_PATH}/RecipeToKey.h
                        RecipeToKey.cpp)
SOURCE_GROUP("Material Key" FILES ${RECIPE_TO_KEY_FILES})

# Pass Expansion
SET(PASS_EXPANSION_FILES ${MTL_TYPE_INCLUDE_PATH}/PassExpansion.h
                         PassExpansion.cpp)
SOURCE_GROUP("Material Key" FILES ${PASS_EXPANSION_FILES})

# Static Material Def Registry
SET(STATIC_DEF_REGISTRY_FILES ${MTL_TYPE_INCLUDE_PATH}/StaticMaterialDefRegistry.h
                               StaticMaterialDefRegistry.cpp)
SOURCE_GROUP("Standard Material" FILES ${STATIC_DEF_REGISTRY_FILES})

# Standard Material / 2D
SET(STD_MTL_2D_SOURCE_FILES ${STD_MTL_HEADER_PATH}/Material2DCreateConfig.h
                            2d/Build2DCommon.h
                            2d/MaterialFactory2D.h
                            2d/MaterialFactory2D.cpp
                            2d/M_VertexColor2D.cpp
                            2d/M_PureColor2D.cpp
                            2d/M_PureTexture2D.cpp
                            2d/M_Text.cpp)
SOURCE_GROUP("Standard Material\\2D" FILES ${STD_MTL_2D_SOURCE_FILES})

# Standard Material / 3D
SET(STD_MTL_3D_SOURCE_FILES ${STD_MTL_HEADER_PATH}/Material3DCreateConfig.h
                            ${STD_MTL_HEADER_PATH}/SkyLight.h
                            3d/MaterialFactory3DCommon.h
                            3d/MaterialFactory3DCommon.cpp
                            3d/M_Checkerboard3D.cpp
                            3d/M_FullscreenTriangle.cpp
                            3d/M_PureColor3D.cpp
                            3d/M_VertexColor3D.cpp
                            3d/M_VertexLum3D.cpp
                            3d/M_VertexLum2D.cpp
                            3d/M_BillboardDynamicSize.cpp
                            3d/M_BillboardFixedSize.cpp
                            3d/M_Gizmo3D.cpp
                            3d/M_SkyMinimal.cpp
                            3d/StandardVariantRouter.h
                            3d/StandardVariantRouter.cpp
                            3d/StandardDescriptorBuilder.h
                            3d/StandardDescriptorBuilder.cpp
                            3d/M_Standard.cpp
                            3d/M_PBRColor3D.cpp
                            3d/M_VertexPaletteColor3D.cpp
                            3d/M_TerrainGrid.cpp)
SOURCE_GROUP("Standard Material\\3D" FILES ${STD_MTL_3D_SOURCE_FILES})

# Position Provider Registry
SET(POSITION_PROVIDER_FILES ${ROOT_INCLUDE_PATH}/hgl/common/PositionProvider.h
                            ${SHADERGEN_INCLUDE_PATH}/PositionProviderRegistry.h
                            PositionProviderRegistry.cpp)
SOURCE_GROUP("Position Provider" FILES ${POSITION_PROVIDER_FILES})

# Shader Library
SET(SHADER_LIBRARY_FILES    MaterialLibrary.cpp
                            MaterialFactory3D.cpp
                            MaterialFactory3DRegistration.h
                            MaterialSpecValidator.cpp
                            VariantRegistry.cpp
                            VariantRegistryStatsSink.cpp
                            ${STD_MTL_HEADER_PATH}/MaterialLibrary.h
                            ${STD_MTL_HEADER_PATH}/MaterialSpec.h
                            ${STD_MTL_HEADER_PATH}/MaterialPreset.h
                            ${STD_MTL_HEADER_PATH}/MaterialVariantDesc.h
                            ${STD_MTL_HEADER_PATH}/MaterialVariantRegistry.h
                            ${MTL_TYPE_INCLUDE_PATH}/MaterialVariantKey.h
                            ${MTL_TYPE_INCLUDE_PATH}/SurfaceType.h
                            ${MTL_TYPE_INCLUDE_PATH}/RenderAlphaMode.h
                            ${MTL_TYPE_INCLUDE_PATH}/PassType.h)
SOURCE_GROUP("Shader Library" FILES ${SHADER_LIBRARY_FILES})

# Material Compiler
SET(MATERIAL_COMPILER_FILES ${SHADERGEN_INCLUDE_PATH}/CompositorCompiler.h
                            CompositorCompiler.cpp)
SOURCE_GROUP("Material Compiler" FILES ${MATERIAL_COMPILER_FILES})

# Material Compiler / Requirements
SET(SHADER_REQUIRE_SCANNER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderResourceScanner.h
                                 ShaderResourceScanner.cpp
                                 MaterialResourceManifest.cpp)
SOURCE_GROUP("Material Compiler\\Requirements" FILES ${SHADER_REQUIRE_SCANNER_FILES})

# Material Compiler / Sampler
SET(SAMPLER_GLSL_EMITTER_FILES ${SHADERGEN_INCLUDE_PATH}/SamplerGLSLEmitter.h
                               SamplerGLSLEmitter.cpp)
SOURCE_GROUP("Material Compiler\\Sampler" FILES ${SAMPLER_GLSL_EMITTER_FILES})

# Material Compiler / Layout
SET(SHADER_LAYOUT_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/ShaderLayoutContract.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderLayoutResolver.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderLayoutEmitter.h)
SET(SHADER_LAYOUT_SOURCE_FILES  ShaderLayoutResolver.cpp
                                ShaderLayoutEmitter.cpp)
SOURCE_GROUP("Material Compiler\\Layout" FILES  ${SHADER_LAYOUT_HEADER_FILES}
                                                ${SHADER_LAYOUT_SOURCE_FILES})

# Compositor
SET(COMPOSITOR_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/CompositorAssembler.h
                             ${SHADERGEN_INCLUDE_PATH}/CompositorFeatureFlags.h
                             ${SHADERGEN_INCLUDE_PATH}/CompositorTemplateRouter.h
                             ${SHADERGEN_INCLUDE_PATH}/MaterialBuilder.h)
SET(COMPOSITOR_SOURCE_FILES  CompositorAssembler.cpp
                             CompositorTemplateRouter.cpp
                             MaterialBuilder.cpp)
SOURCE_GROUP("Compositor" FILES ${COMPOSITOR_HEADER_FILES}
                                ${COMPOSITOR_SOURCE_FILES})

# Compositor / Internal
SET(COMPOSITOR_INTERNAL_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/internal/GLSLSourceUtils.h)
SET(COMPOSITOR_INTERNAL_SOURCE_FILES internal/GLSLSourceUtils.cpp)
SOURCE_GROUP("Compositor\\Internal" FILES ${COMPOSITOR_INTERNAL_HEADER_FILES}
                                          ${COMPOSITOR_INTERNAL_SOURCE_FILES})

add_cm_library(ULRE.ShaderGen "ULRE"    
    ${SHADERGEN_INCLUDE_PATH}
    ${DESC_INFO_HEADER_FILES}
    ${DESC_INFO_SOURCE_FILES}
    ${MATERIAL_CREATE_INFO_HEADER_FILES}
    ${MATERIAL_CREATE_INFO_SOURCE_FILES}
    ${MATERIAL_CREATE_INFO_BUILDER_FILES}
    ${MATERIAL_CREATE_INFO_BINDER_FILES}
    ${MATERIAL_CREATE_INFO_CONFIGURATOR_FILES}
    ${MATERIAL_CREATE_INFO_COMPILER_FILES}
    ${MATERIAL_CREATE_INFO_STAGE_SET_FILES}
    ${SHADER_CREATER_HEADER_FILES}
    ${SHADER_CREATER_SOURCE_FILES}
    ${GLSL_COMPILER_SOURCE}
    ${SHADERGEN_COMMON_FILES}
    ${SHADERGEN_COMMON_MATFUNC_FILES}
    ${SHADERGEN_COMMON_VARIANT_POLICY_FILES}
    ${POSITION_PROVIDER_FILES}
    ${SHADER_LIBRARY_FILES}
    ${STD_MTL_SOURCE}
    ${MATERIAL_KEY_FILES}
    ${RECIPE_TO_KEY_FILES}
    ${PASS_EXPANSION_FILES}
    ${STATIC_DEF_REGISTRY_FILES}
    ${STD_MTL_2D_SOURCE_FILES}
    ${STD_MTL_3D_SOURCE_FILES}
    ${FIXED_MTL_FILES}
    ${MATERIAL_COMPILER_FILES}
    ${SHADER_REQUIRE_SCANNER_FILES}
    ${SAMPLER_GLSL_EMITTER_FILES}
    ${SHADER_LAYOUT_HEADER_FILES}
    ${SHADER_LAYOUT_SOURCE_FILES}
    ${COMPOSITOR_HEADER_FILES}
    ${COMPOSITOR_SOURCE_FILES}
    ${COMPOSITOR_INTERNAL_HEADER_FILES}
    ${COMPOSITOR_INTERNAL_SOURCE_FILES}
)

# On MSVC, ULRE.ShaderGen is a static library. The self-registering preset
# factories in the M_*.cpp translation units have no externally-referenced
# symbols, so the linker would otherwise dead-strip them before their static
# initializers run.  /WHOLEARCHIVE forces the linker to include every .obj
# from this lib, ensuring all AutoReg_* constructors execute at startup.
if(MSVC)
    target_link_options(ULRE.ShaderGen INTERFACE
        /WHOLEARCHIVE:$<TARGET_FILE:ULRE.ShaderGen>)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
