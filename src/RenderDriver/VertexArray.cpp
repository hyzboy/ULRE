#include<hgl/graph/VertexArray.h>
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

        bool VertexArray::Draw()
        {
            if(element_buffer)
                glDrawElements(primitive,element_buffer->GetCount(),element_buffer->GetDataType(),nullptr);
            else
            if(vertex_buffer)
                glDrawArrays(primitive,0,vertex_buffer->GetCount());
            else
                return(false);

            return(true);
        }
    }//namespace graph
}//namespace hgl
