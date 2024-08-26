#ifndef HGL_GRAPH_SCENE_NODE_INCLUDE
#define HGL_GRAPH_SCENE_NODE_INCLUDE

#include<hgl/type/ObjectList.h>
#include<hgl/graph/SceneOrient.h>
#include<hgl/graph/AABB.h>
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
            //AABB WorldBoundingBox;                                                                                      ///<世界坐标绑定盒

            Renderable *render_obj=nullptr;                                                                             ///<可渲染实例

        public:

            ObjectList<SceneNode> SubNode;                                                                              ///<子节点

        public:

            SceneNode()=default;
            SceneNode(SceneNode *);
            SceneNode(                      Renderable *ri  )                   {render_obj=ri;}
            SceneNode(const Matrix4f &mat                   ):SceneOrient(mat)  {}
            SceneNode(const Matrix4f &mat,  Renderable *ri  ):SceneOrient(mat)  {render_obj=ri;}

            virtual ~SceneNode()=default;

            void Clear()
            {
                SubNode.Clear();
                render_obj=nullptr;
            }

            const bool IsEmpty()const
            {
                if(render_obj)return(false);
                if(SubNode.GetCount())return(false);

                return(true);
            }

            Renderable *GetRenderable(){return render_obj;}
            void        SetRenderable(Renderable *);

            SceneNode *AddSubNode(SceneNode *sn)
            {
                if(!sn)
                    return(nullptr);

                SubNode.Add(sn);
                return sn;
            }

            SceneNode *CreateSubNode()
            {
                SceneNode *sn=new SceneNode();

                SubNode.Add(sn);
                return sn;
            }

            SceneNode *CreateSubNode(Renderable *ri)
            {
                if(!ri)
                    return(nullptr);

                SceneNode *sn=new SceneNode(ri);

                SubNode.Add(sn);
                return sn;
            }

            SceneNode *CreateSubNode(const Matrix4f &mat)
            {
                SceneNode *sn=new SceneNode(mat);

                SubNode.Add(sn);
                return sn;
            }

            SceneNode *CreateSubNode(const Matrix4f &mat,Renderable *ri)
            {
                if(!ri)
                    return(nullptr);

                SceneNode *sn=new SceneNode(mat,ri);

                SubNode.Add(sn);
                return sn;
            }

            SceneNode *CreateSubNode(SceneNode *node)
            {
                if(!node)
                    return(nullptr);

                SceneNode *sn=new SceneNode(node);

                SubNode.Add(sn);
                return node;
            }

            SceneNode *CreateSubNode(const Matrix4f &mat,SceneNode *node)
            {
                if(!node)
                    return(nullptr);

                SceneNode *sn=new SceneNode(mat);
                sn->CreateSubNode(node);

                SubNode.Add(sn);
                return sn;
            }

        public: //坐标相关方法

            virtual         void        SetBoundingBox      (const AABB &bb){BoundingBox=bb;}                           ///<设置绑定盒

            virtual         void        RefreshMatrix       () override;           ///<刷新世界变换
            virtual         void        RefreshBoundingBox  ();                                                         ///<刷新绑定盒

            virtual const   AABB &      GetBoundingBox      ()const{return BoundingBox;}                                ///<取得绑定盒
            virtual const   AABB &      GetLocalBoundingBox ()const{return LocalBoundingBox;}                           ///<取得本地坐标绑定盒
//            virtual const   AABB &      GetWorldBoundingBox ()const{return WorldBoundingBox;}                           ///<取得世界坐标绑定盒
        };//class SceneNode
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_NODE_INCLUDE
