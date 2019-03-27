#include"VertexBufferControl.h"
#include<GLEWCore/glew.h>

namespace hgl
{
    namespace graph
    {
        class VertexBufferControlDSA:public VertexBufferControl
        {
        public:

            using VertexBufferControl::VertexBufferControl;

            void Set(GLsizei size, void *data,GLenum data_usage)
            {
                glNamedBufferData(this->index, size, data, data_usage);
            }

            void Change(GLintptr start, GLsizei size, void *data)
            {
                glNamedBufferSubData(this->index, start, size, data);
            }
        };//class VertexBufferControlDSA

        VertexBufferControl *CreateVertexBufferControlDSA(uint type)
        {
            uint index;

            glCreateBuffers(1,&index);
            return(new VertexBufferControlDSA(type,index));
        }

        void DeleteVertexBufferControlDSA(VertexBufferControl *vbc)
        {
            SAFE_CLEAR(vbc);
        }
    }//namespace graph
}//namespace hgl
