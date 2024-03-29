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

SET(DESC_INFO_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/ShaderDescriptorInfo.h
                            ${SHADERGEN_INCLUDE_PATH}/MaterialDescriptorInfo.h)

SET(DESC_INFO_SOURCE_FILES  ShaderDescriptorInfo.cpp
                            MaterialDescriptorInfo.cpp)

SOURCE_GROUP("Material Create Info\\Descriptor Info" FILES  ${DESC_INFO_HEADER_FILES}
                                                            ${DESC_INFO_SOURCE_FILES})

SET(GLSL_COMPILER_SOURCE	GLSLCompiler.h
                            GLSLCompiler.cpp)

SOURCE_GROUP("GLSL Compiler" FILES ${GLSL_COMPILER_SOURCE})


set(STD_MTL_HEADER_PATH  ${ROOT_INCLUDE_PATH}/hgl/graph/mtl)

SET(STD_MTL_2D_HEADER_PATH ${STD_MTL_HEADER_PATH}/2d)

SET(STD_MTL_2D_SOURCE_FILES	${STD_MTL_2D_HEADER_PATH}/VertexColor2D.h
                            2d/VertexColor2D.cpp)

SET(STANDARD_MATERIAL_SOURCE    ${STD_MTL_HEADER_PATH}/StdMaterial.h
                                ${STD_MTL_HEADER_PATH}/ShaderBuffer.h
                                StandardMaterial.cpp)

SOURCE_GROUP("Standard Material" FILES ${STANDARD_MATERIAL_SOURCE})
SOURCE_GROUP("Standard Material\\2D" FILES ${STD_MTL_2D_SOURCE_FILES})

add_cm_library(ULRE.ShaderGen "ULRE"    ${DESC_INFO_HEADER_FILES}
                                        ${DESC_INFO_SOURCE_FILES}
                                        ${MATERIAL_CREATE_INFO_HEADER_FILES}
                                        ${MATERIAL_CREATE_INFO_SOURCE_FILES}
                                        ${SHADER_CREATER_HEADER_FILES}
                                        ${SHADER_CREATER_SOURCE_FILES}
                                        ${GLSL_COMPILER_SOURCE}
                                        ${STANDARD_MATERIAL_SOURCE}
                                        ${STD_MTL_2D_SOURCE_FILES}
)
