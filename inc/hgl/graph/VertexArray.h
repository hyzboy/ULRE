#ifndef HGL_GRAPH_VERTEX_ARRAY_INCLUDE
#define HGL_GRAPH_VERTEX_ARRAY_INCLUDE

#include<hgl/type/List.h>
#include<hgl/graph/VertexBuffer.h>
#include<hgl/graph/PixelCompoment.h>
#include<hgl/math/Math.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 顶点阵列数据
         */
        class VertexArray
        {
        protected:

            uint primitive;                                                                                             ///<绘制的图元类型

            ObjectList<VertexBufferBase> vertex_buffer_list;                                                            ///<顶点数据缓冲区

            AABB aabb;                                                                                                  ///<AABB绑定盒
            OBB obb;                                                                                                    ///<OBB绑定盒

            int vertex_compoment;                                                                                       ///<顶点属性格式
            PixelCompoment color_compoment;                                                                             ///<颜色属性格式

            VertexBufferBase *element_buffer;
            VertexBufferBase *vertex_buffer;
            VertexBufferBase *color_buffer;

        private:

            bool                _SetVertexBuffer    (VertexBufferBase *);                                               ///<真实设置顶点缓冲区数据
            bool                _SetElementBuffer   ();                                                                 ///<真实设置索引缓冲区数据

        public:

            VertexArray(uint prim,uint max_vertex_attrib);
            ~VertexArray()=default;

            uint                GetPrimitive        ()const{return primitive;}                                          ///<取得要绘制的图元类型

        public: //通用顶点缓冲区设置

            int                 AddVertexAttribBuffer     (VertexBufferBase *);                                         ///<设置顶点缓冲区数据
            VertexBufferBase *  GetVertexAttribBuffer     (int index){return vertex_buffer_list[index];}                ///<取得顶点缓冲区数据
            bool                ClearVertexAttribBuffer   (int index){return vertex_buffer_list.Delete(index);}         ///<清除顶点缓冲区数据
            void                ClearVertexAttribBuffers  (){vertex_buffer_list.Clear();}                               ///<清除所有顶点缓冲区数据

        public: //索引缓冲区设置

            bool                SetElementBuffer(VertexBufferBase *vb)
            {
                if(!vb)return(false);
                element_buffer=vb;
                return(true);
            }

        public: //顶点格式相关

            bool                SetVertexBuffer(VertexBufferBase *vb)
            {
                if(!vb)return(false);

                vertex_compoment=vb->GetComponent();

                return(AddVertexAttribBuffer(vb)>=0);
            }

            int                 GetVertexCompoment()const{return vertex_compoment;}                                     ///<取得顶点数据成分数量

        public: //颜色格式相关

            PixelCompoment      GetColorCompoment()const{return color_compoment;}                                       ///<取得顶点颜色格式

            bool                SetColor(VertexBufferBase *vb,PixelCompoment cf)
            {
                if(!vb)return(false);
                if(cf<=HGL_PC_NONE||cf>=HGL_PC_END)return(false);

                color_compoment=cf;

                return(AddVertexAttribBuffer(vb)>=0);
            }

        public: //绑定盒相关

            void                SetBoundingBox  (const Vector3f &min_v,Vector3f &max_v)
            {
                aabb.minPoint=POINT_VEC(min_v);
                aabb.maxPoint=POINT_VEC(max_v);

                obb.SetFrom(aabb);
            }

            const AABB &        GetAABB         ()const{return aabb;}                                                   ///<取得AABB绑定盒
            const OBB &         GetOBB          ()const{return obb;}                                                    ///<取得OBB绑定盒

            const Vector3f      GetCenter       ()const                                                                 ///<取得中心点
            {
                return POINT_TO_FLOAT3(obb.CenterPoint());
            }

            void                GetBoundingBox  (Vector3f &min_v,Vector3f &max_v)                                       ///<取得最小顶点和最大顶点
            {
                min_v=POINT_TO_FLOAT3(aabb.minPoint);
                max_v=POINT_TO_FLOAT3(aabb.maxPoint);
            }

        public:

            int                 GetDrawCount    ();                                                                     ///<取得可绘制的数据总数量
        };//class VertexArray
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_ARRAY_INCLUDE
