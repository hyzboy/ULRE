SET(SHADER_RESOURCE_FILES   ${ROOT_INCLUDE_PATH}/hgl/graph/shader/ShaderResource.h
                            shader/ShaderResource.cpp)

file(GLOB SG_MATERIAL_HEADER ${ROOT_INCLUDE_PATH}/hgl/graph/material/*.*)

file(GLOB SG_MATERIAL_SOURCE material/*.*)

SOURCE_GROUP("Material" FILES   ${SG_MATERIAL_HEADER}
                                ${SG_MATERIAL_SOURCE})

SOURCE_GROUP("Material\\Shader" FILES ${SHADER_RESOURCE_FILES})

SET(SG_TEXTURE_SOURCE   ${ROOT_INCLUDE_PATH}/hgl/graph/TextureLoader.h
                        Texture2DLoader.cpp)

SOURCE_GROUP("Material\\Texture" FILES ${SG_TEXTURE_SOURCE})

SET(SG_VAD_SOURCE   ${ROOT_INCLUDE_PATH}/hgl/graph/VertexAttribData.h
                    ${ROOT_INCLUDE_PATH}/hgl/graph/VertexAttribDataAccess.h
                    VertexAttribData.cpp)

SOURCE_GROUP("VertexAttribData" FILES ${SG_VAD_SOURCE})

SET(TILE_SOURCE ${ROOT_INCLUDE_PATH}/hgl/graph/TileData.h
                TileData.cpp)

SOURCE_GROUP("Tile" FILES ${TILE_SOURCE})

SET(SCENE_GRAPH_HEADER  ${ROOT_INCLUDE_PATH}/hgl/graph/Light.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/SceneNode.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/SceneOrient.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/RenderableInstance.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/RenderList.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/InlineGeometry.h
                        #${ROOT_INCLUDE_PATH}/hgl/graph/Mesh.h
                        #${ROOT_INCLUDE_PATH}/hgl/graph/Material.h
                        #${ROOT_INCLUDE_PATH}/hgl/graph/Spline.h
                        )

SET(SCENE_GRAPH_SOURCE  RenderList.cpp
                        SceneNode.cpp
                        SceneOrient.cpp
                        InlineGeometry.cpp
                        #InlinePipeline.cpp
                        #Material.cpp
                        #Mesh.cpp
                        #SceneFile.cpp
                        )

SET(RENDERABLE_FILES    ${ROOT_INCLUDE_PATH}/hgl/graph/RenderableCreater.h
                        RenderableCreater.cpp)

SOURCE_GROUP("Renderable" FILES ${RENDERABLE_FILES})

SET(FONT_MANAGE_SOURCE  ${ROOT_INCLUDE_PATH}/hgl/graph/font/Font.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/font/FontManage.h
                        font/Font.cpp)

SET(FONT_SOURCE ${ROOT_INCLUDE_PATH}/hgl/graph/font/FontSource.h
                font/FontSource.cpp
                font/FontSourceSingle.cpp
                font/FontSourceMulti.cpp
                font/FontSourceManage.cpp)

SET(TILE_FONT_SOURCE    ${ROOT_INCLUDE_PATH}/hgl/graph/font/TileFont.h
                        font/TileFont.cpp)

SET(FONT_LAYOUT_SOURCE  ${ROOT_INCLUDE_PATH}/hgl/graph/font/TextLayout.h
                        font/TextLayout.cpp)

SET(TEXT_RENDERABLE_SOURCE  ${ROOT_INCLUDE_PATH}/hgl/graph/font/TextRenderable.h
                            font/TextRenderable.cpp)

SOURCE_GROUP("Font"             FILES ${FONT_MANAGE_SOURCE})
SOURCE_GROUP("Font\\Source"     FILES ${FONT_SOURCE})
SOURCE_GROUP("Font\\TileFont"   FILES ${TILE_FONT_SOURCE})
SOURCE_GROUP("Font\\Layout"     FILES ${FONT_LAYOUT_SOURCE} ${TEXT_RENDERABLE_SOURCE})

IF(WIN32)
    SET(FONT_SOURCE_OS  font/FontSourceWin.cpp
                        font/FontSourceWin.h)

    SOURCE_GROUP("Font\\Source\\Windows" FILES ${FONT_SOURCE_OS})
ENDIF(WIN32)

SOURCE_GROUP("Header Files" FILES ${SCENE_GRAPH_HEADER})
SOURCE_GROUP("Source Files" FILES ${SCENE_GRAPH_SOURCE})

add_cm_library(ULRE.SceneGraph "ULRE"   ${SCENE_GRAPH_HEADER}
                                        ${SCENE_GRAPH_SOURCE}

                                        ${SHADER_RESOURCE_FILES}

                                        ${SG_TEXTURE_SOURCE}
                                        ${SG_MATERIAL_HEADER}
                                        ${SG_MATERIAL_SOURCE}
                                        ${TILE_SOURCE}
                                        ${SG_VAD_SOURCE}

                                        ${RENDERABLE_FILES}

                                        ${FONT_MANAGE_SOURCE}
                                        ${FONT_SOURCE}
                                        ${FONT_SOURCE_OS}
                                        ${TILE_FONT_SOURCE}
                                        ${FONT_LAYOUT_SOURCE}
                                        ${TEXT_RENDERABLE_SOURCE})
