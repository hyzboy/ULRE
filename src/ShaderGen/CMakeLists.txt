set(SHADERGEN_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/shadergen)
set(STD_MTL_HEADER_PATH     ${ROOT_INCLUDE_PATH}/hgl/mtl)
set(MTL_TYPE_INCLUDE_PATH   ${ROOT_INCLUDE_PATH}/hgl/mtl/new)

# Add inja include path
include_directories(${ULRE_3RDPTY_ROOT_PATH}/inja/single_include)
include_directories(${ULRE_3RDPTY_ROOT_PATH}/inja/third_party/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Shader Create Info
SET(SHADER_CREATER_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfo.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoMap.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoVertex.h)
SET(SHADER_CREATER_SOURCE_FILES ShaderCreateInfo.cpp
                                ShaderCreateInfoMap.cpp
                                ShaderCreateInfoVertex.cpp)
SOURCE_GROUP("Material Create Info\\Shader" FILES   ${SHADER_CREATER_HEADER_FILES}
                                                    ${SHADER_CREATER_SOURCE_FILES})

# Material Create Info
SET(MATERIAL_CREATE_INFO_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/MaterialCreateInfo.h)
SET(MATERIAL_CREATE_INFO_SOURCE_FILES MaterialCreateInfo.cpp)
SOURCE_GROUP("Material Create Info" FILES   ${MATERIAL_CREATE_INFO_HEADER_FILES}
                                            ${MATERIAL_CREATE_INFO_SOURCE_FILES})

# Descriptor Info
SET(DESC_INFO_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/ShaderDescriptorInfo.h
                            ${SHADERGEN_INCLUDE_PATH}/MaterialDescriptorInfo.h
                            ${STD_MTL_HEADER_PATH}/DescriptorBindingContract.h)
SET(DESC_INFO_SOURCE_FILES  ShaderDescriptorInfo.cpp
                            MaterialDescriptorInfo.cpp)
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
                            ${STD_MTL_HEADER_PATH}/SamplerName.h
                            ${STD_MTL_HEADER_PATH}/ShaderVariableType.h
                            ${SHADERGEN_INCLUDE_PATH}/ShaderGenPathConfig.h
                            ShaderVariableType.cpp
                            ShaderGenPathConfig.cpp
                            common/UBOCommon.cpp)
SOURCE_GROUP("Common" FILES ${SHADERGEN_COMMON_FILES})

# Common / Material Functions
SET(SHADERGEN_COMMON_MATFUNC_FILES  common/MFSkyLight.h)
SOURCE_GROUP("Common\\Material Functions" FILES ${SHADERGEN_COMMON_MATFUNC_FILES})

# Fixed Material Config
SET(FIXED_MTL_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderCompilerProfileAPI.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenContract.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenPhysicalDeviceProfileAdapter.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenPhysicalDeviceProfileJson.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenProfileTargetVersion.h)
SOURCE_GROUP("Fixed Material" FILES ${FIXED_MTL_FILES})

# Standard Material Base
SET(STD_MTL_SOURCE  ${STD_MTL_HEADER_PATH}/MaterialCreateConfig.h
                    ${STD_MTL_HEADER_PATH}/StdMaterial.h
                    ${STD_MTL_HEADER_PATH}/FixedMaterialDef.h
                    ${STD_MTL_HEADER_PATH}/FixedVertexEntry.h
                    ${STD_MTL_HEADER_PATH}/MaterialVertexFormat.h
                    MaterialCreateConfig.cpp
                    GLSLStructSize.cpp)
SOURCE_GROUP("Standard Material" FILES ${STD_MTL_SOURCE})

# Standard Material / 2D
SET(STD_MTL_2D_SOURCE_FILES ${STD_MTL_HEADER_PATH}/Material2DCreateConfig.h
                            2d/Build2DCommon.h
                            2d/FixedDefFactory2D.h
                            2d/FixedDefFactory2D.cpp
                            2d/M_VertexColor2D.cpp
                            2d/M_PureColor2D.cpp
                            2d/M_PureTexture2D.cpp
                            2d/M_Text.cpp)
SOURCE_GROUP("Standard Material\\2D" FILES ${STD_MTL_2D_SOURCE_FILES})

# Standard Material / 3D
SET(STD_MTL_3D_SOURCE_FILES ${STD_MTL_HEADER_PATH}/Material3DCreateConfig.h
                            ${STD_MTL_HEADER_PATH}/SkyLight.h
                            3d/FixedDefFactory3D.h
                            3d/FixedDefFactory3D.cpp
                            3d/M_PureColor3D.cpp
                            3d/M_VertexColor3D.cpp
                            3d/M_VertexLum3D.cpp
                            3d/M_VertexLum2D.cpp
                            3d/M_BillboardDynamicSize.cpp
                            3d/M_BillboardFixedSize.cpp
                            3d/M_Gizmo3D.cpp
                            3d/M_SkyMinimal.cpp
                            3d/StandardVariantPolicy.h
                            3d/StandardVariantPolicy.cpp
                            3d/StandardProfileAdapter.h
                            3d/StandardProfileAdapter.cpp
                            3d/StandardDescriptorBuilder.h
                            3d/StandardDescriptorBuilder.cpp
                            3d/M_Standard.cpp
                            3d/M_PBRColor3D.cpp
                            3d/M_VertexPattleColor3D.cpp
                            3d/M_TerrainGrid.cpp)
SOURCE_GROUP("Standard Material\\3D" FILES ${STD_MTL_3D_SOURCE_FILES})

# Shader Library
SET(SHADER_LIBRARY_FILES    MaterialLibrary.cpp
                            MaterialProfileAssetValidation.cpp
                            VariantRegistry.cpp
                            ShaderPermutationKey.cpp
                            ${STD_MTL_HEADER_PATH}/MaterialLibrary.h
                            ${STD_MTL_HEADER_PATH}/MaterialProfileAsset.h
                            ${STD_MTL_HEADER_PATH}/MaterialPreset.h
                            ${STD_MTL_HEADER_PATH}/MaterialVariantDesc.h
                            ${MTL_TYPE_INCLUDE_PATH}/MaterialVariantKey.h
                            ${MTL_TYPE_INCLUDE_PATH}/SurfaceType.h
                            ${MTL_TYPE_INCLUDE_PATH}/BlendMode.h
                            ${MTL_TYPE_INCLUDE_PATH}/PassType.h
                            ${MTL_TYPE_INCLUDE_PATH}/ShaderPermutationKey.h)
SOURCE_GROUP("Shader Library" FILES ${SHADER_LIBRARY_FILES})

# Material Compiler
SET(MATERIAL_COMPILER_FILES ${SHADERGEN_INCLUDE_PATH}/MaterialCompiler.h
                            MaterialCompiler.cpp)
SOURCE_GROUP("Material Compiler" FILES ${MATERIAL_COMPILER_FILES})

# Material Compiler / Requirements
SET(SHADER_REQUIRE_SCANNER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderRequireScanner.h
                                 ShaderRequireScanner.cpp)
SOURCE_GROUP("Material Compiler\\Requirements" FILES ${SHADER_REQUIRE_SCANNER_FILES})

# Material Compiler / Sampler
SET(SAMPLER_GLSL_EMITTER_FILES ${SHADERGEN_INCLUDE_PATH}/SimpleSamplerGLSLEmitter.h
                               SimpleSamplerGLSLEmitter.cpp)
SOURCE_GROUP("Material Compiler\\Sampler" FILES ${SAMPLER_GLSL_EMITTER_FILES})

# Material Compiler / Layout
SET(SHADER_LAYOUT_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/ShaderLayoutContract.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderLayoutBuilder.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderLayoutDefineEmitter.h)
SET(SHADER_LAYOUT_SOURCE_FILES  ShaderLayoutBuilder.cpp
                                ShaderLayoutDefineEmitter.cpp)
SOURCE_GROUP("Material Compiler\\Layout" FILES  ${SHADER_LAYOUT_HEADER_FILES}
                                                ${SHADER_LAYOUT_SOURCE_FILES})

# Compositor
SET(COMPOSITOR_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/CompositorAssembler.h
                             ${SHADERGEN_INCLUDE_PATH}/CompositorFeatureFlags.h)
SET(COMPOSITOR_SOURCE_FILES  CompositorAssembler.cpp)
SOURCE_GROUP("Compositor" FILES ${COMPOSITOR_HEADER_FILES}
                                ${COMPOSITOR_SOURCE_FILES})

add_cm_library(ULRE.ShaderGen "ULRE"    
    ${SHADERGEN_INCLUDE_PATH}
    ${DESC_INFO_HEADER_FILES}
    ${DESC_INFO_SOURCE_FILES}
    ${MATERIAL_CREATE_INFO_HEADER_FILES}
    ${MATERIAL_CREATE_INFO_SOURCE_FILES}
    ${SHADER_CREATER_HEADER_FILES}
    ${SHADER_CREATER_SOURCE_FILES}
    ${GLSL_COMPILER_SOURCE}
    ${SHADERGEN_COMMON_FILES}
    ${SHADERGEN_COMMON_MATFUNC_FILES}
    ${SHADER_LIBRARY_FILES}
    ${STD_MTL_SOURCE}
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
)
