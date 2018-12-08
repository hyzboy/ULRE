#include<GLEWCore/glew.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            enum GLAPI_SUPPORT
            {
                gasNoSupport=-1,
                gasNoCheck=0,
                gasSupport=1
            };

            static GLAPI_SUPPORT support_dsa=gasNoCheck;
        }

        bool IsSupportDSA()
        {
            if(support_dsa==gasSupport)return(true);

            //texture api
            if(glCreateTextures
             &&glGenerateTextureMipmap
             &&glGetTextureLevelParameteriv
             &&glGetTextureParameteriv
             &&glGetCompressedTextureImage
             &&glGetTextureImage
             &&glBindTextureUnit
             &&glCompressedTextureSubImage1D
             &&glCompressedTextureSubImage2D
             &&glTextureSubImage1D
             &&glTextureSubImage2D

             //sampler object
             &&glGenSamplers
             &&glBindSampler
             &&glSamplerParameteri
             &&glSamplerParameterfv

             //vbo
             &&glCreateBuffers
             &&glNamedBufferData
             &&glNamedBufferSubData

             //vao
             &&glCreateVertexArrays
             &&glDeleteVertexArrays
             &&glVertexArrayAttribBinding
             &&glVertexArrayAttribFormat
             &&glVertexArrayAttribIFormat
             &&glVertexArrayAttribLFormat
             &&glEnableVertexArrayAttrib
             &&glVertexArrayVertexBuffer
            )
            {
                support_dsa=gasSupport;
                return(true);
            }

            support_dsa=gasNoSupport;
            return(false);
        }
    }
}//namespace hgl
