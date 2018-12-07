#include"VertexBufferControl.h"
#include<GLEWCore/glew.h>

namespace hgl
{
    namespace graph
    {
        //class VertexBufferBind
        //{
        //    uint type;
        //    int old_id;

        //public:

        //    VertexBufferBind(uint t, uint binding_type, int id) :type(t)
        //    {
        //        glGetIntegerv(binding_type, &old_id);
        //        glBindBuffer(type, id);
        //    }

        //    ~VertexBufferBind()
        //    {
        //        glBindBuffer(type, old_id);
        //    }
        //};//class VertexBufferBind

        class VertexBufferControlBind:public VertexBufferControl
        {
        public:

            using VertexBufferControl::VertexBufferControl;
            ~VertexBufferControlBind()
            {
                glDeleteBuffers(1,&(this->index));
            }

            void Set(GLsizei size, void *data, GLenum data_usage)
            {
                glBindBuffer(this->type,this->index);
                glBufferData(this->type, size, data, data_usage);
            }

            void Change(GLintptr start, GLsizei size, void *data)
            {
                glBindBuffer(this->type, this->index);
                glBufferSubData(this->type, start, size, data);
            }
        };//class VertexBufferControlBind

        VertexBufferControl *CreateVertexBufferControlBind(uint type)
        {
            uint index;

            glGenBuffers(1, &index);
            return(new VertexBufferControlBind(type, index));
        }
    }//namespace graph
}//namespace hgl
