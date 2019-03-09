﻿#include<hgl/graph/VertexArray.h>
#include<GLEWCore/glew.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            static int HGL_MAX_VERTEX_ATTRIBS=0;
        }

        int VertexArray::GetMaxVertexAttrib()
        {
            if(HGL_MAX_VERTEX_ATTRIBS<=0)
                glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&HGL_MAX_VERTEX_ATTRIBS);

            return HGL_MAX_VERTEX_ATTRIBS;
        }

        VertexArray::VertexArray(uint prim,uint max_vertex_attrib)
        {
            if(max_vertex_attrib>GetMaxVertexAttrib())
                max_vertex_attrib=HGL_MAX_VERTEX_ATTRIBS;

            primitive=prim;

            vertex_buffer_list.PreMalloc(max_vertex_attrib);

            vertex_compoment=-1;
            color_compoment=HGL_PC_NONE;

            element_buffer=nullptr;

            glCreateVertexArrays(1,&vao);
        }

        VertexArray::~VertexArray()
        {
            glDeleteVertexArrays(1,&vao);
        }

        /**
        * 添加一个顶点数据缓冲区
        * @param vb 数据缓冲区
        * @return 绑定点索引
        * @return -1 失败
        */
        int VertexArray::AddVertexAttribBuffer(int shader_location, VertexBufferBase *vb)
        {
            if(!vb)return(false);
            if(vb->GetBufferType()!=GL_ARRAY_BUFFER)return(false);

            const int binding_index = vertex_buffer_list.GetCount();            //一个VAO中的绑定点，必须从0开始，而且必须紧密排列

            glVertexArrayAttribBinding(vao, shader_location, binding_index);

            if(vb->GetDataType()==GL_INT    )   glVertexArrayAttribIFormat( vao,shader_location,vb->GetComponent(),vb->GetDataType(),0);else
            if(vb->GetDataType()==GL_DOUBLE )   glVertexArrayAttribLFormat( vao,shader_location,vb->GetComponent(),vb->GetDataType(),0);else
                                                glVertexArrayAttribFormat(  vao,shader_location,vb->GetComponent(),vb->GetDataType(),GL_FALSE,0);

            glEnableVertexArrayAttrib(vao, shader_location);
            glVertexArrayVertexBuffer(vao, binding_index, vb->GetBufferIndex(), 0, vb->GetStride());

            vertex_buffer_list.Add(vb);

            return binding_index;
        }

        bool VertexArray::SetElementBuffer(VertexBufferBase *eb)
        {
            if(!eb)return(false);
            if(eb->GetBufferType()!=GL_ELEMENT_ARRAY_BUFFER)return(false);
            element_buffer=eb;

            glVertexArrayElementBuffer(vao, eb->GetBufferIndex());
            return(true);
        }

        bool VertexArray::SetVertexBuffer(int shader_location, VertexBufferBase *vb)
        {
            if(!vb)return(false);

            if(!AddVertexAttribBuffer(shader_location,vb)<0)
                return(false);

            vertex_compoment=vb->GetComponent();
            vertex_buffer=vb;
            return(true);
        }

        bool VertexArray::SetColorBuffer(int shader_location, VertexBufferBase *vb,PixelCompoment cf)
        {
            if(!vb)return(false);
            if(cf<=HGL_PC_NONE||cf>=HGL_PC_END)return(false);

            if(AddVertexAttribBuffer(shader_location,vb)<0)
                return(false);

            color_compoment=cf;
            color_buffer=vb;
            return(true);
        }

        /**
        * 取得可绘制数据数量
        * @return 可绘制数量数量
        * @return -1 出错
        */
        int VertexArray::GetDrawCount()
        {
            if(element_buffer)
                return element_buffer->GetCount();

            if(vertex_buffer)
                return vertex_buffer->GetCount();

            return(-1);
        }

        bool VertexArray::Draw()
        {
            glBindVertexArray(vao);

            if (element_buffer)
                glDrawElements(primitive, element_buffer->GetCount(), element_buffer->GetDataType(), nullptr);
            else
            if(vertex_buffer)
                glDrawArrays(primitive,0,vertex_buffer->GetCount());
            else
                return(false);

            return(true);
        }
    }//namespace graph
}//namespace hgl
