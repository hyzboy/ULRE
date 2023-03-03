set(SHADERGEN_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/shadergen)

SET(SHADERGEN_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/ShadingProgram.h
                            ${SHADERGEN_INCLUDE_PATH}/PMC.h
                            ${SHADERGEN_INCLUDE_PATH}/VertexInputStage.h
                            ${SHADERGEN_INCLUDE_PATH}/ShaderModule.h
                            ${SHADERGEN_INCLUDE_PATH}/VertexShaderModule.h
                            ${SHADERGEN_INCLUDE_PATH}/ShaderCommon.h
                            ${SHADERGEN_INCLUDE_PATH}/ShaderDescriptorManager.h
                            ${SHADERGEN_INCLUDE_PATH}/MaterialDescriptorManager.h
                            ${SHADERGEN_INCLUDE_PATH}/VertexPolicy.h
                            ${SHADERGEN_INCLUDE_PATH}/MaterialGen.h
)

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
                                        ShaderGen.cpp
                                        MaterialCreater.cpp)
