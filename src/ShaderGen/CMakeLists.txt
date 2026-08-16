set(SHADERGEN_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/shadergen)

SET(SHADER_CREATER_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfo.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoMap.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoVertex.h)

SET(SHADER_CREATER_SOURCE_FILES ShaderCreateInfo.cpp
                                ShaderCreateInfoMap.cpp
                                ShaderCreateInfoVertex.cpp)

SOURCE_GROUP("Material Create Info\\Shader" FILES   ${SHADER_CREATER_HEADER_FILES}
                                                    ${SHADER_CREATER_SOURCE_FILES})

SET(SHADER_BUILD_CONTEXT_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderBuildContext.h)
SET(SHADER_BUILD_CONTEXT_SOURCE_FILES ShaderBuildContext.cpp)

SOURCE_GROUP("Shader Build Context" FILES   ${SHADER_BUILD_CONTEXT_HEADER_FILES}
                                            ${SHADER_BUILD_CONTEXT_SOURCE_FILES})

SET(SHADER_ARTIFACT_CONTRACT_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderStageKey.h
${SHADERGEN_INCLUDE_PATH}/ShaderKeyUtility.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderProgramKey.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderArtifactContract.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderStageBuildContext.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderLinkSpec.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderArtifactStore.h
                                   ${SHADERGEN_INCLUDE_PATH}/ShaderProgramArtifactBuilder.h
                                   ShaderArtifactStore.cpp
                                   ShaderProgramArtifactBuilder.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/graph/glsl/GLSLCodeModuleRegistry.h
                                   common/GLSLCodeModuleRegistry.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/graph/glsl/GLSLCodeModuleMetadata.h
                                   common/GLSLCodeModuleMetadata.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/graph/glsl/GLSLCodeModuleFile.h
                                   common/GLSLCodeModuleFile.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/graph/glsl/GLSLCodeModuleCapabilityResolver.h
                                   common/GLSLCodeModuleCapabilityResolver.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialDefinitionFile.h
                                   common/MaterialDefinitionFile.cpp
                                   ${ROOT_INCLUDE_PATH}/hgl/mtl/SamplerPreset.h
                                   common/SamplerPreset.cpp)

SOURCE_GROUP("Shader Artifact Contract" FILES ${SHADER_ARTIFACT_CONTRACT_FILES})

SET(SHADER_SEMANTIC_REGISTRY_FILES
    ${SHADERGEN_INCLUDE_PATH}/ShaderSemanticRegistry.h
    ${SHADERGEN_INCLUDE_PATH}/MaterialStageInterface.h
    ShaderSemanticRegistry.cpp
    MaterialStageInterface.cpp)

SOURCE_GROUP("Shader Semantic Registry" FILES ${SHADER_SEMANTIC_REGISTRY_FILES})

SET(CANONICAL_SHADER_CONTRACT_FILES
    ${SHADERGEN_INCLUDE_PATH}/CanonicalShaderContract.h
    ${SHADERGEN_INCLUDE_PATH}/DescriptorContract.h
    ${SHADERGEN_INCLUDE_PATH}/MaterialOutputContract.h
    ${SHADERGEN_INCLUDE_PATH}/MaterialCoverageContract.h
    ${SHADERGEN_INCLUDE_PATH}/ResolvedModuleGraphBuilder.h
    CanonicalShaderContract.cpp
    DescriptorContract.cpp
    MaterialOutputContract.cpp
    MaterialCoverageContract.cpp
    ResolvedModuleGraphBuilder.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialBindingContract.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/BindingTableBuilder.h
    MaterialBindingContract.cpp
    BindingTableBuilder.cpp
    common/CanonicalContractWriter.h)

SOURCE_GROUP("Canonical Shader Contract"
    FILES ${CANONICAL_SHADER_CONTRACT_FILES})

SET(DS_LAYOUT_ALLOCATOR_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/DescriptorSetLayoutAllocator.h)

SET(DS_LAYOUT_ALLOCATOR_SOURCE_FILES  DescriptorSetLayoutAllocator.cpp)

SOURCE_GROUP("Descriptor Set Layout" FILES  ${DS_LAYOUT_ALLOCATOR_HEADER_FILES}
                                            ${DS_LAYOUT_ALLOCATOR_SOURCE_FILES})

SET(GLSL_COMPILER_SOURCE	GLSLCompiler.h
                            GLSLCompiler.cpp)

SOURCE_GROUP("GLSL Compiler" FILES ${GLSL_COMPILER_SOURCE})

set(STD_MTL_HEADER_PATH  ${ROOT_INCLUDE_PATH}/hgl/mtl)

SET(SHADERGEN_COMMON_FILES  ${ROOT_INCLUDE_PATH}/hgl/graph/ShaderBufferSources.h
                            ${SHADERGEN_INCLUDE_PATH}/ShaderLibraryPath.h
                            ${STD_MTL_HEADER_PATH}/SamplerName.h
                            common/GLSLCodeModule.cpp
                            common/ShaderResourceManifest.cpp
                            ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/SkyInfo.h
                            ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/ViewportInfo.h
                            ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/UBOShaderSources.h
                            ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/ColorPalette.h
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

SET(STD_MTL_2D_SOURCE_FILES	2d/M_Text.cpp
)

SET(STD_MTL_3D_SOURCE_FILES	3d/M_PureColor.cpp
)

SET(STD_MTL_SOURCE  ${ROOT_INCLUDE_PATH}/hgl/graph/ShaderBufferSource.h
)

SOURCE_GROUP("Standard Material" FILES ${STD_MTL_SOURCE})

SOURCE_GROUP("Standard Material\\2D" FILES ${STD_MTL_2D_SOURCE_FILES})
SOURCE_GROUP("Standard Material\\3D" FILES ${STD_MTL_3D_SOURCE_FILES})

SET(SHADER_LIBRARY_FILES    MaterialDefinitionRegistry.cpp
                            ${STD_MTL_HEADER_PATH}/MaterialDefinitionRegistry.h)

SOURCE_GROUP("Shader Library" FILES ${SHADER_LIBRARY_FILES})

SET(FIXED_MTL_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderCompilerProfileAPI.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenContract.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenPhysicalDeviceProfileAdapter.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenPhysicalDeviceProfileJson.h
                    ${SHADERGEN_INCLUDE_PATH}/contract/ShaderGenProfileTargetVersion.h

)

SOURCE_GROUP("Fixed Material" FILES ${FIXED_MTL_FILES})

SET(MATERIAL_COMPILER_FILES  ${SHADERGEN_INCLUDE_PATH}/MaterialShaderCompiler.h
                             MaterialShaderCompiler.cpp)

SOURCE_GROUP("Material Compiler" FILES ${MATERIAL_COMPILER_FILES})

SET(COMPOSITOR_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/CompositorAssembler.h)
SET(COMPOSITOR_SOURCE_FILES  CompositorAssembler.cpp)

SOURCE_GROUP("Compositor" FILES ${COMPOSITOR_HEADER_FILES}
                                ${COMPOSITOR_SOURCE_FILES})

SET(NEW_MTL_TYPE_FILES  ${ROOT_INCLUDE_PATH}/hgl/mtl/SurfaceType.h
                        ${ROOT_INCLUDE_PATH}/hgl/mtl/BlendMode.h
                        ${ROOT_INCLUDE_PATH}/hgl/mtl/PassType.h
                        ${SHADERGEN_INCLUDE_PATH}/ShaderPermutationKey.h
                        ShaderPermutationKey.cpp
)

SOURCE_GROUP("Material Types" FILES ${NEW_MTL_TYPE_FILES})

add_cm_library(ULRE.ShaderGen "ULRE"    ${SHADERGEN_INCLUDE_PATH}

                                        ${DS_LAYOUT_ALLOCATOR_HEADER_FILES}
                                        ${DS_LAYOUT_ALLOCATOR_SOURCE_FILES}
                                        ${SHADER_BUILD_CONTEXT_HEADER_FILES}
                                        ${SHADER_BUILD_CONTEXT_SOURCE_FILES}
                                        ${SHADER_ARTIFACT_CONTRACT_FILES}
                                        ${SHADER_SEMANTIC_REGISTRY_FILES}
                                        ${CANONICAL_SHADER_CONTRACT_FILES}
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
