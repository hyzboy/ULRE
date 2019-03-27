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
            ElementBuffer *eb=vao->GetElement();

            if(eb)
                return eb->GetCount();

            ArrayBuffer *pb=vao->GetPosition();

            if(!pb)
                return 0;

            return pb->GetCount();
        }

        bool Renderable::Draw()
        {
            glBindVertexArray(vao->GetVAO());

            ElementBuffer *eb=vao->GetElement();

            if(!eb)
            {
                ArrayBuffer *pb=vao->GetPosition();

                if(!pb)
                    return(false);

                glDrawArrays(primitive,0,pb->GetCount());
            }
            else
            {
                glDrawElements(primitive,eb->GetCount(),eb->GetDataType(),nullptr);
            }

            return(true);
        }
    }//namespace graph
}//namespace hgl
