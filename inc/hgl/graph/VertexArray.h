#ifndef HGL_GRAPH_VERTEX_ARRAY_INCLUDE
#define HGL_GRAPH_VERTEX_ARRAY_INCLUDE

#include<hgl/type/List.h>
#include<hgl/graph/VertexBuffer.h>
#include<hgl/graph/PixelCompoment.h>
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

            GLuint vao;

            uint primitive;                                                                                             ///<绘制的图元类型

            ObjectList<VertexBufferBase> vertex_buffer_list;                                                            ///<顶点数据缓冲区

            VertexBufferBase *element_buffer;

            VertexBufferBase *position_buffer;
            int position_compoment;                                                                                     ///<位置属性格式

            List<Pair<VertexBufferBase *,PixelCompoment>> color_buffer;
            PixelCompoment color_compoment;                                                                             ///<颜色属性格式
            VertexBufferBase *color_buffer;

        public:

            VertexArray(uint prim,uint max_vertex_attrib);
            ~VertexArray();

            static int          GetMaxVertexAttrib();

            const uint          GetPrimitive        ()const{return primitive;}                                          ///<取得要绘制的图元类型
            const GLuint        GetVAO              ()const{return vao;}                                                ///<取得VAO对象

        public: //通用顶点缓冲区设置

            int                 AddVertexAttribBuffer     (int shader_location,VertexBufferBase *);                     ///<设置顶点缓冲区数据
            VertexBufferBase *  GetVertexAttribBuffer     (int index){return vertex_buffer_list[index];}                ///<取得顶点缓冲区数据
            bool                ClearVertexAttribBuffer   (int index){return vertex_buffer_list.Delete(index);}         ///<清除顶点缓冲区数据
            void                ClearVertexAttribBuffers  (){vertex_buffer_list.Clear();}                               ///<清除所有顶点缓冲区数据

        public: //特殊缓冲区独立设置函数

            bool                SetElementBuffer    (VertexBufferBase *eb);                                             ///<设置索引缓冲区数据
            bool                SetPositionBuffer   (int shader_location,VertexBufferBase *vb);                         ///<设置位置缓冲区数据

            bool                AddColorBuffer      (int shader_location,VertexBufferBase *vb,PixelCompoment cf);       ///<添加一个颜色缓冲区数据

            int                 GetPositionCompoment()const{return position_compoment;}                                 ///<取得位置数据成分数量
            PixelCompoment      GetColorCompoment   ()const{return color_compoment;}                                    ///<取得颜色数据成份格式

        public:

            int                 GetDrawCount    ();                                                                     ///<取得可绘制的数据总数量
            bool                Draw();                                                                                 ///<绘制
        };//class VertexArray

        //新设计内容，如碰到此处编译失败请在GIT上退回到上一版本
        //1.ColorBuffer可能存在多个，所以上面的SetColorBuffer可能要考虑多个的情况
        //2.将VertexArray类拆分成独立的VAO类和VAOCreater类，所有创建VAO相关的全部放到VAOCreater类中，创建完删除自身并返回VAO对象
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_ARRAY_INCLUDE
