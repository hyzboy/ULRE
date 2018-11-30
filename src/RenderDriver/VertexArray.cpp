#include<hgl/graph/VertexArray.h>

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
