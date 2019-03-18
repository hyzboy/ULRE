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

            void Update() override
            {
                glNamedBufferData(buffer_index,total_bytes,data,user_pattern);
            }

            void Change(const GLsizeiptr start,const GLsizeiptr size) override
            {
                glNamedBufferSubData(buffer_index,start,size,((char *)data)+start);
            }

            void Change(const GLsizeiptr start,const GLsizeiptr size,const void *new_data) override
            {
                glNamedBufferSubData(buffer_index,start,size,new_data);
            }
        };//class VertexBufferControlDSA

        VertexBufferObject *CreateVertexBufferObjectDSA(uint type)
        {
            uint index;

            glCreateBuffers(1,&index);
        }

        VertexBufferObject *CreateVBO(const GLenum &buf_type,
                                      const GLenum &dsup,
                                      const uint &data_type,const uint &data_bytes,const uint &data_comp,
                                      const GLsizeiptr &size)
        {
        }
    }//namespace graph
}//namespace hgl
