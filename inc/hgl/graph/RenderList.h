#ifndef HGL_GRAPH_RENDER_LIST_INCLUDE
#define HGL_GRAPH_RENDER_LIST_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/MaterialRenderMap.h>
#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/VKMaterial.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 渲染对象列表<br>
         * 该类会长期保存使用过的材质信息，避重新分配造成的时间和空间浪费。如需彻底清空列表请使用Clear()函数
         */
        class RenderList
        {
        protected:  

            GPUDevice *         device;

            uint                renderable_count;               ///<可渲染对象数量
            MaterialRenderMap   mrl_map;                        ///<按材质分类的渲染列表

        protected:

            virtual bool ExpendNode(SceneNode *);

        public:

            RenderList(GPUDevice *);
            virtual ~RenderList();
            
            virtual bool Expend(SceneNode *);                   ///<展开场景树到渲染列表

            virtual bool Render(RenderCmdBuffer *);             ///<渲染所有对象

            virtual void Clear();                               ///<彻底清理
        };//class RenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
