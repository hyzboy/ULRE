set(SHADERGEN_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/shadergen)


SET(SHADER_CREATER_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfo.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoMap.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoVertex.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoGeometry.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoFragment.h)

SET(SHADER_CREATER_SOURCE_FILES ShaderCreateInfo.cpp
                                ShaderCreateInfoVertex.cpp
                                ShaderCreateInfoGeometry.cpp
                                ShaderCreateInfoFragment.cpp)

SOURCE_GROUP("Material Create Info\\Shader" FILES   ${SHADER_CREATER_HEADER_FILES}
                                                    ${SHADER_CREATER_SOURCE_FILES})

SET(MATERIAL_CREATE_INFO_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/MaterialCreateInfo.h)
SET(MATERIAL_CREATE_INFO_SOURCE_FILES MaterialCreateInfo.cpp)

SOURCE_GROUP("Material Create Info" FILES   ${MATERIAL_CREATE_INFO_HEADER_FILES}
                                            ${MATERIAL_CREATE_INFO_SOURCE_FILES})

SET(SHADERGEN_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/ShaderGenNamespace.h
                            ${SHADERGEN_INCLUDE_PATH}/ShaderDescriptorInfo.h
                            ${SHADERGEN_INCLUDE_PATH}/MaterialDescriptorInfo.h)

SET(SHADERGEN_SOURCE_FILES  
#PMC.cpp
#                            VertexPolicy.cpp
                            ShaderDescriptorInfo.cpp
                            MaterialDescriptorInfo.cpp
)

SET(GLSL_COMPILER_SOURCE	GLSLCompiler.h
                            GLSLCompiler.cpp)

SOURCE_GROUP("GLSL Compiler" FILES ${GLSL_COMPILER_SOURCE})

SOURCE_GROUP("Material Create Info\\Descriptor Info" FILES  ${SHADERGEN_HEADER_FILES} 
                                                            ${SHADERGEN_SOURCE_FILES})

SET(STANDARD_MATERIAL_SOURCE    ${SHADERGEN_INCLUDE_PATH}/StandardMaterial.h
                                StandardMaterial.cpp)

SOURCE_GROUP("Standard Material" FILES ${STANDARD_MATERIAL_SOURCE})

add_cm_library(ULRE.ShaderGen "ULRE"    ${SHADERGEN_HEADER_FILES} 
                                        ${SHADERGEN_SOURCE_FILES}
                                        ${MATERIAL_CREATE_INFO_HEADER_FILES}
                                        ${MATERIAL_CREATE_INFO_SOURCE_FILES}
                                        ${SHADER_CREATER_HEADER_FILES}
                                        ${SHADER_CREATER_SOURCE_FILES}
                                        ${GLSL_COMPILER_SOURCE}
                                        ${STANDARD_MATERIAL_SOURCE}
)
