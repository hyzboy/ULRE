FILE(GLOB_RECURSE GRAPH_INC_FILES "${ROOT_INCLUDE_PATH}/hgl/graph/*.h")
SET(GRAPH_SRC_FILES OpenGLDebug.cpp
                    OpenGLExt.cpp
                    GLSL.cpp
                    Shader.cpp
                    BufferData.cpp
                    BufferObject.cpp
#                     VertexArray.cpp
#                     VertexBuffer.cpp
#                     VertexBufferObject.cpp
                    TextureFormat.cpp
                    Texture1D.cpp
                    Texture1DDSA.cpp
                    Texture2D.cpp
                    Texture2DDSA.cpp)

source_group("Headers" FILES ${GRAPH_INC_FILES})
source_group("Sources" FILES ${GRAPH_SRC_FILES})

add_library(ULRE.RenderDriver STATIC    ${GRAPH_INC_FILES}
                                        ${GRAPH_SRC_FILES})

