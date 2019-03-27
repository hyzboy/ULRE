#include<hgl/graph/Renderable.h>

namespace hgl
{
    namespace graph
    {
        /**
        * 取得可绘制数据数量
        * @return 可绘制的顶点数量
        */
        uint Renderable::GetDrawCount()
        {
            ElementBufferObject *obj=vao->GetElement();

            if(!obj)
                obj=vao->GetPosition();

            if(!obj)
                return 0;

            return obj->GetCount();
        }

        bool Renderable::Draw()
        {
            glBindVertexArray(vao->GetVAO());

            ElementBufferObject *element_buffer=vao->GetElement();

            if(!element_buffer)
            {
                VertexBufferObject *position_buffer=vao->GetPosition();

                if(!position_buffer)
                    return(false);

                glDrawArrays(primitive,0,position_buffer->GetCount());
            }
            else
            {
                glDrawElements(primitive,element_buffer->GetCount(),element_buffer->GetDataType(),nullptr);
            }

            return(true);
        }
    }//namespace graph
}//namespace hgl
