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

            VulkanDevice *      device;

            CameraInfo *        camera_info;                    ///<相机信息

            uint                renderable_count;               ///<可渲染对象数量
            MaterialRenderMap   mrl_map;                        ///<按材质分类的渲染列表

        protected:

            virtual bool ExpendNode(SceneNode *);

        public:

            const CameraInfo *GetCameraInfo()const{return camera_info;}

        public:

            RenderList(VulkanDevice *);
            virtual ~RenderList()=default;
            
            virtual void SetCameraInfo(CameraInfo *ci){camera_info=ci;}             ///<设置相机信息
            virtual bool Expend(SceneNode *);                                       ///<展开场景树到渲染列表

                    bool IsEmpty()const{return !renderable_count;}                  ///<是否是空的

            virtual bool Render(RenderCmdBuffer *);                                 ///<渲染所有对象

            virtual void UpdateLocalToWorld();                                      ///<更新所有对象的变换数据
            virtual void UpdateMaterialInstance(MeshComponent *);                   ///<有对象互换了材质实例

            virtual void Clear();                                                   ///<彻底清理
        };//class RenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
