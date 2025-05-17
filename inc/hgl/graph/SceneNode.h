#pragma once

#include<hgl/type/ObjectList.h>
#include<hgl/type/IDName.h>
#include<hgl/graph/SceneOrient.h>
#include<hgl/graph/AABB.h>
#include<hgl/component/Component.h>

namespace hgl
{
    namespace graph
    {
        using SceneNodeID   =uint64;
        using SceneNodeName =U16IDName;

        /**
        * 场景节点数据类<br>
        * 从场景坐标变换(SceneOrient)类继承，
        * 每个场景节点中可能包括一个可渲染数据实例，或是完全不包含(用于坐标变换的父节点，或是灯光/摄像机之类)。
        */
        class SceneNode:public SceneOrient                                                                              ///场景节点类
        {
            SceneNode *ParentNode;                                                                                      ///<上级节点

            SceneNodeID NodeID;                                                                                         ///<节点ID
            SceneNodeName NodeName;                                                                                     ///<节点名称

        protected:

            AABB BoundingBox;                                                                                           ///<绑定盒
            AABB LocalBoundingBox;                                                                                      ///<本地坐标绑定盒
            //AABB WorldBoundingBox;                                                                                      ///<世界坐标绑定盒

            Mesh *render_obj=nullptr;                                                                             ///<可渲染实例

        protected:

            ObjectList<SceneNode> ChildNode;                                                                            ///<子节点
            ObjectList<Component> ComponentList;                                                                        ///<组件列表

        public:

            const SceneNodeID &     GetNodeID   ()const { return NodeID; }                                              ///<取得节点ID
            const SceneNodeName &   GetNodeName ()const { return NodeName; }                                            ///<取得节点名称

            const ObjectList<SceneNode> &GetChildNode()const { return ChildNode; }                                      ///<取得子节点列表

        public:

            SceneNode()=default;
            SceneNode(const SceneNode &)=delete;
            SceneNode(const SceneNode *)=delete;
            SceneNode(const SceneOrient &so                 ):SceneOrient(so)   {}
            SceneNode(                      Mesh *ri  )                   {render_obj=ri;}
            SceneNode(const Matrix4f &mat                   ):SceneOrient(mat)  {}
            SceneNode(const Matrix4f &mat,  Mesh *ri  ):SceneOrient(mat)  {render_obj=ri;}

        public:

            virtual ~SceneNode()=default;

            void Clear() override
            {
                SceneOrient::Clear();

                ParentNode=nullptr;

                BoundingBox.SetZero();
                LocalBoundingBox.SetZero();

                ChildNode.Clear();
                ComponentList.Clear();
                render_obj=nullptr;
            }

            const bool ChildNodeIsEmpty()const
            {
                if(render_obj)return(false);
                if(ChildNode.GetCount())return(false);

                return(true);
            }

                    void        SetParent(SceneNode *sn) {ParentNode=sn;}
                    SceneNode * GetParent()      noexcept{return ParentNode;}
            const   SceneNode * GetParent()const noexcept{return ParentNode;}

                    void        SetRenderable(Mesh *);
                    Mesh *GetRenderable()      noexcept{return render_obj;}
            const   Mesh *GetRenderable()const noexcept{return render_obj;}

            SceneNode *Add(SceneNode *sn)
            {
                if(!sn)
                    return(nullptr);

                ChildNode.Add(sn);
                sn->SetParent(this);
                return sn;
            }

        public: //坐标相关方法

            virtual         void        SetBoundingBox      (const AABB &bb){BoundingBox=bb;}                           ///<设置绑定盒

            virtual         void        RefreshMatrix       () override;                                                ///<刷新世界变换
            virtual         void        RefreshBoundingBox  ();                                                         ///<刷新绑定盒

            virtual const   AABB &      GetBoundingBox      ()const{return BoundingBox;}                                ///<取得绑定盒
            virtual const   AABB &      GetLocalBoundingBox ()const{return LocalBoundingBox;}                           ///<取得本地坐标绑定盒
//            virtual const   AABB &      GetWorldBoundingBox ()const{return WorldBoundingBox;}                           ///<取得世界坐标绑定盒

        public: //组件相关方法

                            bool        ComponentIsEmpty    ()const {return ComponentList.GetCount()==0;}               ///<是否没有组件
            virtual         int         GetComponentCount   ()const {return ComponentList.GetCount();}                  ///<取得组件数量
            virtual         void        AddComponent        (Component *comp) {ComponentList.Add(comp);}                ///<添加一个组件
            virtual         void        RemoveComponent     (Component *comp) {ComponentList.DeleteByValue(comp);}      ///<删除一个组件
                            bool        Contains            (Component *comp) {return ComponentList.Contains(comp);}    ///<是否包含指定组件

                            bool        HasComponent        (const ComponentManager *);                                 ///<是否有指定组件管理器的组件
            virtual         int         GetComponents       (ArrayList<Component *> &comp_list,const ComponentManager *);    ///<取得所有组件

        };//class SceneNode

        SceneNode *Duplication(SceneNode *);                                                                            ///<复制一个场景节点
    }//namespace graph
}//namespace hgl
