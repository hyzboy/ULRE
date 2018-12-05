#include<hgl/graph/VertexArray.h>
#include<GLEWCore/glew.h>

namespace hgl
{
    namespace graph
    {
        VertexArray::VertexArray(uint prim,uint max_vertex_attrib)
        {
            primitive=prim;

            vertex_buffer_list.PreMalloc(max_vertex_attrib);

            vertex_compoment=-1;
            color_compoment=HGL_PC_NONE;

            element_buffer=nullptr;

            glCreateVertexArrays(1,&vao);
            shader_location=new int[max_vertex_attrib];
        }

        VertexArray::~VertexArray()
        {
            delete[] shader_location;
            glDeleteVertexArrays(1,&vao);
        }

        /**
        * 添加一个顶点数据缓冲区
        * @param vb 数据缓冲区
        * @return 缓冲区索引
        * @return -1 失败
        */
        int VertexArray::AddVertexAttribBuffer(VertexBufferBase *vb)
        {
            if(!vb)return(false);

            const int index=vertex_buffer_list.Add(vb);

            _SetVertexBuffer(vb);       //各种真实渲染器处理

            return(index);
        }

        bool VertexArray::SetElementBuffer(VertexBufferBase *vb)
        {
            if(!vb)return(false);
            element_buffer=vb;
            return(true);
        }

        bool VertexArray::SetVertexBuffer(VertexBufferBase *vb)
        {
            if(!vb)return(false);

            if(!AddVertexAttribBuffer(vb)<0)
                return(false);

            vertex_compoment=vb->GetComponent();
            vertex_buffer=vb;
            return(true);
        }

        bool VertexArray::SetColorBuffer(VertexBufferBase *vb,PixelCompoment cf)
        {
            if(!vb)return(false);
            if(cf<=HGL_PC_NONE||cf>=HGL_PC_END)return(false);

            if(AddVertexAttribBuffer(vb)<0)
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
    }//namespace graph
}//namespace hgl
