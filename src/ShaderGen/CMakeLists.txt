set(SHADERGEN_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/shadergen)


SET(SHADER_CREATER_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderCreater.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreaterMap.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreaterVertex.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreaterGeometry.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreaterFragment.h)

SET(SHADER_CREATER_SOURCE_FILES ShaderCreater.cpp
                                ShaderCreaterVertex.cpp
                                ShaderCreaterGeometry.cpp
                                ShaderCreaterFragment.cpp)

SOURCE_GROUP("Material Creater\\Shader Creater" FILES   ${SHADER_CREATER_HEADER_FILES}
                                                        ${SHADER_CREATER_SOURCE_FILES})

SET(MATERIAL_CREATER_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/MaterialCreater.h)
SET(MATERIAL_CREATER_SOURCE_FILES MaterialCreater.cpp)

SOURCE_GROUP("Material Creater" FILES   ${MATERIAL_CREATER_HEADER_FILES}
                                        ${MATERIAL_CREATER_SOURCE_FILES})

SET(SHADERGEN_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/ShaderGenNamespace.h
                            ${SHADERGEN_INCLUDE_PATH}/ShaderDescriptorManager.h
                            ${SHADERGEN_INCLUDE_PATH}/MaterialDescriptorManager.h)

SET(SHADERGEN_SOURCE_FILES  
#PMC.cpp
#                            VertexPolicy.cpp
                            ShaderDescriptorManager.cpp
                            MaterialDescriptorManager.cpp
)

#SOURCE_GROUP("Pixel Material Components" FILES  ${SHADERGEN_HEADER_FILES} 
#                                                ${SHADERGEN_SOURCE_FILES})

add_cm_library(ULRE.ShaderGen "ULRE"    ${SHADERGEN_HEADER_FILES} 
                                        ${SHADERGEN_SOURCE_FILES}
                                        ${MATERIAL_CREATER_HEADER_FILES}
                                        ${MATERIAL_CREATER_SOURCE_FILES}
                                        ${SHADER_CREATER_HEADER_FILES}
                                        ${SHADER_CREATER_SOURCE_FILES}
)
