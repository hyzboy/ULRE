SET(SCENE_GRAPH_HEADER  ${ROOT_INCLUDE_PATH}/hgl/graph/AABox.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/Camera.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/Light.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/SceneNode.h
                        ${ROOT_INCLUDE_PATH}/hgl/graph/SceneOrient.h
						${ROOT_INCLUDE_PATH}/hgl/graph/VertexBufferCreater.h
						${ROOT_INCLUDE_PATH}/hgl/graph/VertexBuffer.h
						${ROOT_INCLUDE_PATH}/hgl/graph/InlineGeometry.h)

SET(SCENE_GRAPH_SOURCE  AABox.cpp
                        Camera.cpp
#                        RenderList.cpp
                        SceneNode.cpp
                        SceneOrient.cpp
						InlineGeometry.cpp)

SOURCE_GROUP("Header Files" FILES ${SCENE_GRAPH_HEADER})
SOURCE_GROUP("Source Files" FILES ${SCENE_GRAPH_SOURCE})

add_library(ULRE.SceneGraph STATIC  ${SCENE_GRAPH_HEADER}
                                    ${SCENE_GRAPH_SOURCE})
