#ifndef HGL_GRAPH_SCENE_NODE_INCLUDE
#define HGL_GRAPH_SCENE_NODE_INCLUDE

#include<hgl/type/List.h>
#include<hgl/graph/SceneOrient.h>
#include<hgl/graph/VK.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 场景节点数据类<br>
        * 从场景坐标变换(SceneOrient)类继承，
        * 每个场景节点中可能包括一个可渲染数据实例，或是完全不包含(用于坐标变换的父节点，或是灯光/摄像机之类)。
        */
        class SceneNode:public SceneOrient                                                                              ///场景节点类
        {
        protected:

            AABB BoundingBox;                                                                                           ///<绑定盒
            AABB LocalBoundingBox;                                                                                      ///<本地坐标绑定盒
            AABB WorldBoundingBox;                                                                                      ///<世界坐标绑定盒

            Vector4f Center;                                                                                            ///<中心点
            Vector4f LocalCenter;                                                                                       ///<本地坐标中心点
            Vector4f WorldCenter;                                                                                       ///<世界坐标中心点

        public:

            ObjectList<SceneNode> SubNode;                                                                              ///<子节点
            
            RenderableInstance *render_obj=nullptr;                                                                     ///<可渲染实例

        public:

            SceneNode()=default;
            SceneNode(                      RenderableInstance *ri  )                   {render_obj=ri;}
            SceneNode(const Matrix4f &mat                           ):SceneOrient(mat)  {}
            SceneNode(const Matrix4f &mat,  RenderableInstance *ri  ):SceneOrient(mat)  {render_obj=ri;}

            virtual ~SceneNode(){}

            void Clear()
            {
                SubNode.ClearData();
                render_obj=nullptr;
            }

        public: //坐标相关方法

            virtual         void        SetBoundingBox      (const AABB &bb){BoundingBox=bb;}                           ///<设置绑定盒

            virtual         void        RefreshMatrix       (const Matrix4f *mat=nullptr);                              ///<刷新世界变换矩阵
            virtual         void        RefreshBoundingBox  ();                                                         ///<刷新绑定盒

            virtual const   AABB &      GetBoundingBox      ()const{return BoundingBox;}                                ///<取得绑定盒
            virtual const   AABB &      GetLocalBoundingBox ()const{return LocalBoundingBox;}                           ///<取得本地坐标绑定盒
            virtual const   AABB &      GetWorldBoundingBox ()const{return WorldBoundingBox;}                           ///<取得世界坐标绑定盒

            virtual const   Vector4f &  GetCenter           ()const{return Center;}                                     ///<取得中心点
            virtual const   Vector4f &  GetLocalCenter      ()const{return LocalCenter;}                                ///<取得本地坐标中心点
            virtual const   Vector4f &  GetWorldCenter      ()const{return WorldCenter;}                                ///<取得世界坐标中心点
        };//class SceneNode
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_NODE_INCLUDE
