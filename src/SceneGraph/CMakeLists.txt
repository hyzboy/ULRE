SET(SHADER_RESOURCE_FILES   ${ROOT_INCLUDE_PATH}/hgl/graph/shader/ShaderResource.h
                            shader/ShaderResource.cpp)


file(GLOB SG_MATERIAL_HEADER ${ROOT_INCLUDE_PATH}/hgl/graph/material/*.*)

file(GLOB SG_MATERIAL_SOURCE material/*.*)

SOURCE_GROUP("Material" FILES   ${SG_MATERIAL_HEADER}
                                ${SG_MATERIAL_SOURCE})

SOURCE_GROUP("Material\\Shader" FILES ${SHADER_RESOURCE_FILES})


SET(SG_VAB_SOURCE    ${ROOT_INCLUDE_PATH}/hgl/graph/VertexAttribData.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/VertexAttribBuffer.h)

SOURCE_GROUP("VertexAttribBuffer" FILES   ${SG_VAB_SOURCE})

SET(SCENE_GRAPH_HEADER  ${ROOT_INCLUDE_PATH}/hgl/graph/Light.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/SceneDB.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/SceneNode.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/SceneOrient.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/RenderableInstance.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/RenderList.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/InlineGeometry.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/TileData.h
                        #${ROOT_INCLUDE_PATH}/hgl/graph/Mesh.h
                        #${ROOT_INCLUDE_PATH}/hgl/graph/Material.h
                        #${ROOT_INCLUDE_PATH}/hgl/graph/Spline.h
                        )

SET(SCENE_GRAPH_SOURCE  RenderList.cpp
                        SceneDB.cpp
                        SceneNode.cpp
                        SceneOrient.cpp
                        InlineGeometry.cpp
                        TileData.cpp
                        #InlinePipeline.cpp
                        #Material.cpp
                        #Mesh.cpp
                        #SceneFile.cpp
                        )

SET(RENDERABLE_FILES    ${ROOT_INCLUDE_PATH}/hgl/graph/RenderableCreater.h
                        RenderableCreater.cpp)

SOURCE_GROUP("Renderable" FILES ${RENDERABLE_FILES})

file(GLOB FONT_HEADER ${ROOT_INCLUDE_PATH}/hgl/graph/font/*.*)

SET(FONT_SOURCE font/Font.cpp
                font/FontSource.cpp
                font/FontSourceSingle.cpp
                font/FontSourceMulti.cpp
                font/FontSourceManage.cpp
                font/TileFont.cpp)

SOURCE_GROUP("Font" FILES ${FONT_HEADER} ${FONT_SOURCE})

IF(WIN32)
    SET(FONT_SOURCE_OS  font/FontSourceWin.cpp
                        font/FontSourceWin.h)

    SOURCE_GROUP("Font\\Windows" FILES ${FONT_SOURCE_OS})

    SET(FONT_SOURCE ${FONT_SOURCE} ${FONT_SOURCE_OS})
ENDIF(WIN32)

SOURCE_GROUP("Header Files" FILES ${SCENE_GRAPH_HEADER})
SOURCE_GROUP("Source Files" FILES ${SCENE_GRAPH_SOURCE})

add_cm_library(ULRE.SceneGraph "ULRE"   ${SCENE_GRAPH_HEADER}
                                        ${SCENE_GRAPH_SOURCE}

                                        ${SHADER_RESOURCE_FILES}

                                        ${SG_MATERIAL_HEADER}
                                        ${SG_MATERIAL_SOURCE}

                                        ${SG_VAB_SOURCE}

                                        ${RENDERABLE_FILES}

                                        ${FONT_HEADER} 
                                        ${FONT_SOURCE})
