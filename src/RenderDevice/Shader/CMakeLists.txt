SET(GRAPH_SHADER_HEADER_PATH ${ROOT_INCLUDE_PATH}/hgl/graph/shader)
SET(GRAPH_SHADER_NODE_PATH ${GRAPH_SHADER_HEADER_PATH}/node)
SET(GRAPH_SHADER_PARAM_PATH ${GRAPH_SHADER_HEADER_PATH}/param)

file(GLOB GRAPH_SHADER_NODE_HEADER_FILES ${GRAPH_SHADER_NODE_PATH}/*.h)

source_group("Shader Maker\\Node" FILES ${GRAPH_SHADER_NODE_HEADER_FILES})

file(GLOB GRAPH_SHADER_PARAM_HEADER_FILES ${GRAPH_SHADER_PARAM_PATH}/*.h)

source_group("Shader Maker\\Param" FILES ${GRAPH_SHADER_PARAM_HEADER_FILES})

SET(GRAPH_SHADER_MAKER_SOURCE DefaultShader.cpp)

source_group("Shader Maker" FILES ${GRAPH_SHADER_MAKER_SOURCE})

SET(GRAPH_SPV_SOURCE    ${GRAPH_SHADER_HEADER_PATH}/glsl2spv.h
                        glsl2spv.cpp)

SOURCE_GROUP("SPIR-V" FILES ${GRAPH_SPV_SOURCE})

add_cm_library(ULRE.Shader "ULRE"   ${GRAPH_SHADER_NODE_HEADER_FILES}
                                    ${GRAPH_SHADER_PARAM_HEADER_FILES}
                                    ${GRAPH_SHADER_MAKER_SOURCE}
                                    ${GRAPH_SPV_SOURCE})
                                    
