add_library(ULRE.RenderDriver STATIC    GLCore/RenderDriverGLCore.cpp
                                        GLSL.cpp
                                        Shader.cpp
                                        VertexArray.cpp
                                        VertexBuffer.cpp
                                        VertexBufferControlDSA.cpp
                                        VertexBufferControlBind.cpp
                                        OpenGLDebug.cpp
                                        TextureFormat.cpp
                                        Texture1D.cpp
                                        Texture1DDSA.cpp
                                        Texture2D.cpp
                                        Texture2DDSA.cpp)

