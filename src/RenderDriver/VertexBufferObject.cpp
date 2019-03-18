#include<hgl/graph/VertexBufferObject.h>

namespace hgl
{
    namespace graph
    {
        class VertexBufferObjectDSA:public VertexBufferObject
        {
        public:

            using VertexBufferObject::VertexBufferObject;
            ~VertexBufferObjectDSA()
            {
                glDeleteBuffers(1,&buffer_index);
            }

            void Set(GLsizei size,void *data)
            {
                glNamedBufferData(buffer_index,size,data,user_pattern);
            }

            void Change(GLintptr start,GLsizei size,void *data)
            {
                glNamedBufferSubData(buffer_index,start,size,data);
            }
        };//class VertexBufferControlDSA

        VertexBufferObject *CreateVertexBufferObjectDSA(uint type)
        {
            uint index;

            glCreateBuffers(1,&index);
        }

        VertexBufferObject *CreateVBO(const GLenum &buf_type,
                                      const uint &data_type,const uint &data_bytes,const uint &data_comp,
                                      const GLsizeiptr &size,
                                      const GLenum &dsup)
        {
        }
    }//namespace graph
}//namespace hgl
