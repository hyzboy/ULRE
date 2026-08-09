set(SHADERGEN_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/shadergen)

SET(SHADER_CREATER_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfo.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoMap.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoVertex.h)

SET(SHADER_CREATER_SOURCE_FILES ShaderCreateInfo.cpp
                                ShaderCreateInfoMap.cpp
                                ShaderCreateInfoVertex.cpp)

SOURCE_GROUP("Material Create Info\\Shader" FILES   ${SHADER_CREATER_HEADER_FILES}
                                                    ${SHADER_CREATER_SOURCE_FILES})

SET(SHADER_PROGRAM_BUILD_SPEC_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderProgramBuildSpec.h)
SET(SHADER_PROGRAM_BUILD_SPEC_SOURCE_FILES ShaderProgramBuildSpec.cpp)

SOURCE_GROUP("ShaderProgram Build Spec" FILES   ${SHADER_PROGRAM_BUILD_SPEC_HEADER_FILES}
                                                ${SHADER_PROGRAM_BUILD_SPEC_SOURCE_FILES})

SET(SHADER_ARTIFACT_CONTRACT_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderStageKey.h
${SHADERGEN_INCLUDE_PATH}/ShaderKeyUtility.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderProgramKey.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderArtifactContract.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderGenMigration.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderStageBuildSpec.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderProgramLinkSpec.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderArtifactStore.h
                                   ShaderArtifactStore.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/graph/glsl/GLSLCodeModuleRegistry.h
                                   common/GLSLCodeModuleRegistry.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/graph/glsl/GLSLCodeModuleMetadata.h
                                   common/GLSLCodeModuleMetadata.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/graph/glsl/GLSLCodeModuleFile.h
                                   common/GLSLCodeModuleFile.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/graph/glsl/GLSLCodeModuleCapabilityResolver.h
                                   common/GLSLCodeModuleCapabilityResolver.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialDefinitionFile.h
                                   common/MaterialDefinitionFile.cpp)

SOURCE_GROUP("Shader Artifact Contract" FILES ${SHADER_ARTIFACT_CONTRACT_FILES})

SET(SHADER_SEMANTIC_REGISTRY_FILES
    ${SHADERGEN_INCLUDE_PATH}/ShaderSemanticRegistry.h
    ${SHADERGEN_INCLUDE_PATH}/MaterialStageInterface.h
    ShaderSemanticRegistry.cpp
    MaterialStageInterface.cpp)

SOURCE_GROUP("Shader Semantic Registry" FILES ${SHADER_SEMANTIC_REGISTRY_FILES})

SET(CANONICAL_SHADER_CONTRACT_FILES
    ${SHADERGEN_INCLUDE_PATH}/CanonicalShaderContract.h
    ${SHADERGEN_INCLUDE_PATH}/MaterialDescriptorContract.h
    ${SHADERGEN_INCLUDE_PATH}/ResolvedModuleGraphBuilder.h
    ${SHADERGEN_INCLUDE_PATH}/ShadowShaderContractBuilder.h
    ${SHADERGEN_INCLUDE_PATH}/ShadowShaderKeyBuilder.h
    ${SHADERGEN_INCLUDE_PATH}/ShadowResourceAcquirePlanBuilder.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialProgramContract.h
    CanonicalShaderContract.cpp
    MaterialDescriptorContract.cpp
    ResolvedModuleGraphBuilder.cpp
    ShadowShaderContractBuilder.cpp
    ShadowShaderKeyBuilder.cpp
    ShadowResourceAcquirePlanBuilder.cpp
    MaterialProgramContract.cpp
    common/CanonicalContractWriter.h)

SOURCE_GROUP("Canonical Shader Contract"
    FILES ${CANONICAL_SHADER_CONTRACT_FILES})

SET(SURFACE_PROFILE_FILES
    ${ROOT_INCLUDE_PATH}/hgl/mtl/SurfaceProfile.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/SurfaceProfileRegistry.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/SurfaceProfileFile.h
    SurfaceProfileRegistry.cpp
    common/SurfaceProfileFile.cpp)

SOURCE_GROUP("Material Surface Profile" FILES ${SURFACE_PROFILE_FILES})

SET(DESC_INFO_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/ShaderDescriptorInfo.h
                            ${SHADERGEN_INCLUDE_PATH}/MaterialDescriptorInfo.h)

SET(DESC_INFO_SOURCE_FILES  ShaderDescriptorInfo.cpp
                            MaterialDescriptorInfo.cpp)

SOURCE_GROUP("Material Create Info\\Descriptor Info" FILES  ${DESC_INFO_HEADER_FILES}
                                                            ${DESC_INFO_SOURCE_FILES})

SET(GLSL_COMPILER_SOURCE	GLSLCompiler.h
                            GLSLCompiler.cpp)

SOURCE_GROUP("GLSL Compiler" FILES ${GLSL_COMPILER_SOURCE})

set(STD_MTL_HEADER_PATH  ${ROOT_INCLUDE_PATH}/hgl/mtl)

SET(SHADERGEN_COMMON_FILES  ${ROOT_INCLUDE_PATH}/hgl/graph/ShaderBufferSources.h
                            ${SHADERGEN_INCLUDE_PATH}/ShaderLibraryPath.h
                            ${STD_MTL_HEADER_PATH}/SamplerName.h
                            ${STD_MTL_HEADER_PATH}/ShaderVariableType.h
                            ShaderVariableType.cpp
                            common/GLSLCodeModule.cpp
                            common/ShaderResourceManifest.cpp
                            common/MFCamera.h
                            ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/SkyInfo.h
                            ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/ViewportInfo.h
                            ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/UBOShaderSources.h
                            ${ROOT_INCLUDE_PATH}/hgl/graph/glsl/GLSLCodeModule.h
                            ${ROOT_INCLUDE_PATH}/hgl/graph/glsl/ShaderResourceManifest.h
)

SOURCE_GROUP("Common" FILES ${SHADERGEN_COMMON_FILES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SSBO_HEADER_PATH ${ROOT_INCLUDE_PATH}/hgl/graph/ssbo)

SET(SSBO_STRUCT_FILES   ${SSBO_HEADER_PATH}/LitMaterialData.h
                        ${SSBO_HEADER_PATH}/TextureSlot.h
                        ${SSBO_HEADER_PATH}/SSBOTypes.h)

SOURCE_GROUP("Standard Material\\SSBO" FILES ${SSBO_STRUCT_FILES})

SET(STD_MTL_2D_SOURCE_FILES	2d/Build2DCommon.h
                            2d/M_Text.cpp
)

SET(STD_MTL_3D_SOURCE_FILES	3d/M_PureColor.cpp
)

SET(STD_MTL_SOURCE  ${STD_MTL_HEADER_PATH}/ShaderBufferSource.h
                    GLSLStructSize.cpp
)

SOURCE_GROUP("Standard Material" FILES ${STD_MTL_SOURCE})

SOURCE_GROUP("Standard Material\\2D" FILES ${STD_MTL_2D_SOURCE_FILES})
SOURCE_GROUP("Standard Material\\3D" FILES ${STD_MTL_3D_SOURCE_FILES})

SET(SHADER_LIBRARY_FILES    MaterialLibrary.cpp
                            ${STD_MTL_HEADER_PATH}/MaterialLibrary.h)

SOURCE_GROUP("Shader Library" FILES ${SHADER_LIBRARY_FILES})

SET(FIXED_MTL_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderCompilerProfileAPI.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenContract.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenPhysicalDeviceProfileAdapter.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenPhysicalDeviceProfileJson.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenProfileTargetVersion.h

)

SOURCE_GROUP("Fixed Material" FILES ${FIXED_MTL_FILES})

SET(MATERIAL_COMPILER_FILES  ${SHADERGEN_INCLUDE_PATH}/MaterialCompiler.h
                             MaterialCompiler.cpp)

SOURCE_GROUP("Material Compiler" FILES ${MATERIAL_COMPILER_FILES})

SET(COMPOSITOR_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/CompositorAssembler.h)
SET(COMPOSITOR_SOURCE_FILES  CompositorAssembler.cpp)

SOURCE_GROUP("Compositor" FILES ${COMPOSITOR_HEADER_FILES}
                                ${COMPOSITOR_SOURCE_FILES})

set(NEW_MTL_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/mtl/new)

SET(NEW_MTL_TYPE_FILES  ${NEW_MTL_INCLUDE_PATH}/SurfaceType.h
                        ${NEW_MTL_INCLUDE_PATH}/BlendMode.h
                        ${NEW_MTL_INCLUDE_PATH}/PassType.h
                        ${NEW_MTL_INCLUDE_PATH}/MaterialCategory.h
                        ${NEW_MTL_INCLUDE_PATH}/NewShaderPermutationKey.h
                        ${NEW_MTL_INCLUDE_PATH}/DeviceQualityProfile.h
                        ${NEW_MTL_INCLUDE_PATH}/DescriptorSetBindings.h
                        NewShaderPermutationKey.cpp
                        DeviceQualityProfile.cpp
)

SOURCE_GROUP("New Material Types" FILES ${NEW_MTL_TYPE_FILES})

add_cm_library(ULRE.ShaderGen "ULRE"    ${SHADERGEN_INCLUDE_PATH}

                                        ${DESC_INFO_HEADER_FILES}
                                        ${DESC_INFO_SOURCE_FILES}
                                        ${SHADER_PROGRAM_BUILD_SPEC_HEADER_FILES}
                                        ${SHADER_PROGRAM_BUILD_SPEC_SOURCE_FILES}
                                        ${SHADER_ARTIFACT_CONTRACT_FILES}
                                        ${SHADER_SEMANTIC_REGISTRY_FILES}
                                        ${CANONICAL_SHADER_CONTRACT_FILES}
                                        ${SURFACE_PROFILE_FILES}
                                        ${SHADER_CREATER_HEADER_FILES}
                                        ${SHADER_CREATER_SOURCE_FILES}
                                        ${GLSL_COMPILER_SOURCE}
                                        ${SHADERGEN_COMMON_FILES}
                                        ${SHADER_LIBRARY_FILES}
                                        ${STD_MTL_SOURCE}
                                        ${STD_MTL_2D_SOURCE_FILES}
                                        ${STD_MTL_3D_SOURCE_FILES}
                                        ${SSBO_STRUCT_FILES}
                                        ${FIXED_MTL_FILES}
                                        ${MATERIAL_COMPILER_FILES}
                                        ${COMPOSITOR_HEADER_FILES}
                                        ${COMPOSITOR_SOURCE_FILES}
                                        ${NEW_MTL_TYPE_FILES}
)
