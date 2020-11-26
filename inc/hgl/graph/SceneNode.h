#ifndef HGL_GRAPH_SCENE_NODE_INCLUDE
#define HGL_GRAPH_SCENE_NODE_INCLUDE

#include<hgl/type/List.h>
#include<hgl/graph/SceneOrient.h>
#include<hgl/graph/VK.h>
namespace hgl
{
    namespace graph
    {
        class SceneNode;
        struct Camera;
        class RenderList;
        
        using RenderListCompFunc=float (*)(Camera *,SceneNode *,SceneNode *);                       ///<渲染列表排序比较函数

        float CameraLengthComp(Camera *,SceneNode *,SceneNode *);                                   ///<摄像机距离比较函数

        using FilterSceneNodeFunc=bool (*)(const SceneNode *,void *);                               ///<场景节点过滤函数重定义

        bool FrustumClipFilter(const SceneNode *,void *);                                           ///<平截头截剪函数

        /**
        * 场景节点数据类<br>
        * 从场景坐标变换(SceneOrient)类继承
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

            List<RenderableInstance *> renderable_instances;                                                            ///<可渲染实例

        public:

            SceneNode()=default;
            SceneNode(const Matrix4f &mat)
            {
                SetLocalMatrix(mat);
            }

            virtual ~SceneNode()
            {
                ClearSubNode();
            }

            SceneNode *CreateSubNode()
            {
                SceneNode *sn=new SceneNode();

                SubNode.Add(sn);
                return sn;
            }

            SceneNode *CreateSubNode(const Matrix4f &mat)
            {
                SceneNode *sn=new SceneNode(mat);

                SubNode.Add(sn);
                return sn;
            }

            void Add(SceneNode *n){if(n)SubNode.Add(n);}                                                                ///<增加一个子节点
            void ClearSubNode(){SubNode.ClearData();}                                                                   ///<清除子节点

            void Add(RenderableInstance *ri){if(ri)renderable_instances.Add(ri);}                                       ///<增加渲染实例

            void Add(RenderableInstance *ri,const Matrix4f &mat)
            {
                SceneNode *sn=new SceneNode(mat);

                sn->Add(ri);
                SubNode.Add(sn);
            }

            void ClearRenderableInstance(){renderable_instances.ClearData();}                                           ///<清除渲染实例

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

        public: //渲染列表相关方法

            virtual bool ExpendToList(RenderList *,FilterSceneNodeFunc func=nullptr,void *func_data=nullptr);           ///<展开到渲染列表
                    bool ExpendToList(RenderList *rl,Frustum *f)                                                        ///<展开到渲染列表(使用平截头裁剪)
                        {return ExpendToList(rl,FrustumClipFilter,f);}

                    bool ExpendToList(RenderList *,Camera *,RenderListCompFunc=nullptr);                                ///<展开到渲染列表(使用摄像机平截头裁剪并排序)
        };//class SceneNode
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_NODE_INCLUDE
