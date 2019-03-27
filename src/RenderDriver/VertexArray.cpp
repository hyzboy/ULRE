#include<hgl/graph/VertexArray.h>
#include<GLEWCore/glew.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            static GLint HGL_MAX_VERTEX_ATTRIBS=0;
        }

        int VertexArray::GetMaxVertexAttrib()
        {
            if(HGL_MAX_VERTEX_ATTRIBS<=0)
                glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&HGL_MAX_VERTEX_ATTRIBS);

            return HGL_MAX_VERTEX_ATTRIBS;
        }

        VertexArray::VertexArray(uint max_vertex_attrib)
        {
            if(max_vertex_attrib>GetMaxVertexAttrib())
                max_vertex_attrib=HGL_MAX_VERTEX_ATTRIBS;

            vbo_list.PreMalloc(max_vertex_attrib);
            
            element_buffer=nullptr;

            glCreateVertexArrays(1,&vao);
        }

        VertexArray::~VertexArray()
        {
            glDeleteVertexArrays(1,&vao);
        }

        /**
        * 添加一个顶点数据缓冲区
        * @param shader_location 这个缓冲区对应的SHADER地址
        * @param vb 数据缓冲区
        * @return 绑定点索引
        * @return -1 失败
        */
        int VertexArray::AddBuffer(int shader_location,VertexBufferObject *vb)
        {
            if(!vb)return(false);
            if(vb->GetBufferType()!=GL_ARRAY_BUFFER)return(false);

            const int binding_index = vbo_list.GetCount();            //一个VAO中的绑定点，必须从0开始，而且必须紧密排列

            glVertexArrayAttribBinding(vao, shader_location, binding_index);

            if(vb->GetDataType()==GL_INT    )   glVertexArrayAttribIFormat( vao,shader_location,vb->GetComponent(),vb->GetDataType(),0);else
            if(vb->GetDataType()==GL_DOUBLE )   glVertexArrayAttribLFormat( vao,shader_location,vb->GetComponent(),vb->GetDataType(),0);else
                                                glVertexArrayAttribFormat(  vao,shader_location,vb->GetComponent(),vb->GetDataType(),GL_FALSE,0);

            glEnableVertexArrayAttrib(vao,shader_location);
            glVertexArrayVertexBuffer(vao,binding_index,vb->GetBufferIndex(),0,vb->GetStride());

            vbo_list.Add(vb);

            return binding_index;
        }

        /**
         * 设置索引缓冲区
         */
        bool VertexArray::SetElement(ElementBufferObject *eb)
        {
            if(!eb)return(false);
            if(eb->GetBufferType()!=GL_ELEMENT_ARRAY_BUFFER)return(false);
            element_buffer=eb;

            glVertexArrayElementBuffer(vao,eb->GetBufferIndex());
            return(true);
        }

        /**
         * 设置一个顶点缓冲区
         * @param shader_location 这个缓冲区对应的SHADER地址
         * @param vb 数据缓冲区
         */
        bool VertexArray::SetPosition(int shader_location,VertexBufferObject *vb)
        {
            if(!vb)return(false);

            if(!AddBuffer(shader_location,vb)<0)
                return(false);

            position_buffer=vb;
            return(true);
        }
    }//namespace graph
}//namespace hgl
