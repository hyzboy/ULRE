#pragma once

#include<hgl/type/ObjectList.h>
#include<hgl/type/IDName.h>
#include<hgl/graph/SceneOrient.h>
#include<hgl/graph/AABB.h>
#include<hgl/component/Component.h>

namespace hgl::graph
{
    using SceneNodeID   =int64;

    HGL_DEFINE_U16_IDNAME(SceneNodeName)

    /**
    * 场景节点数据类<br>
    * 从场景坐标变换(SceneOrient)类继承，
    * 每个场景节点中可能包括一个可渲染数据实例，或是完全不包含(用于坐标变换的父节点，或是灯光/摄像机之类)。
    */
    class SceneNode:public SceneOrient                                                                              ///场景节点类
    {
        SceneNode *ParentNode=nullptr;                                                                              ///<上级节点

        SceneNodeID NodeID=-1;                                                                                      ///<节点ID
        SceneNodeName NodeName;                                                                                     ///<节点名称

    protected:

        AABB BoundingBox;                                                                                           ///<绑定盒
        AABB LocalBoundingBox;                                                                                      ///<本地坐标绑定盒
        //AABB WorldBoundingBox;                                                                                      ///<世界坐标绑定盒

    protected:

        ObjectList<SceneNode> ChildNode;                                                                            ///<子节点

        /**
        * 组件合集，一个SceneNode下可能会包含多个组件，同时一个组件也可能被多个SceneNode使用。
        * 所以这里只保留一个指针，不拥有组件的生命周期，组件的生命周期由其对应的ComponentManager管理。
        */
        SortedSet<Component *> ComponentSet;                                                                        ///<组件合集

    public:

        const SceneNodeID &     GetNodeID   ()const { return NodeID; }                                              ///<取得节点ID
        const SceneNodeName &   GetNodeName ()const { return NodeName; }                                            ///<取得节点名称

        const ObjectList<SceneNode> &GetChildNode()const { return ChildNode; }                                      ///<取得子节点列表

    public:

        SceneNode()=default;
        SceneNode(const SceneNode &)=delete;
        SceneNode(const SceneNode *)=delete;
        SceneNode(const SceneOrient &so           ):SceneOrient(so)   {}
        SceneNode(const Matrix4f &mat             ):SceneOrient(mat)  {}

    public:

        virtual ~SceneNode()=default;

        void Clear() override
        {
            SceneOrient::Clear();

            ParentNode=nullptr;

            BoundingBox.SetZero();
            LocalBoundingBox.SetZero();

            ChildNode.Clear();
            ComponentSet.Clear();
        }

        const bool ChildNodeIsEmpty()const
        {
            if(ChildNode.GetCount())return(false);

            return(true);
        }

                void        SetParent(SceneNode *sn) {ParentNode=sn;}
                SceneNode * GetParent()      noexcept{return ParentNode;}
        const   SceneNode * GetParent()const noexcept{return ParentNode;}

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

                        bool        ComponentIsEmpty    ()const{return ComponentSet.GetCount()==0;}                 ///<是否没有组件
        virtual         int         GetComponentCount   ()const{return ComponentSet.GetCount();}                    ///<取得组件数量
        virtual         bool        AttachComponent     (Component *comp)                                           ///<添加一个组件
        {
            if(!comp)return(false);

            if(ComponentSet.Add(comp)<0)
                return(false);

            comp->OnAttach(this);            //调用组件的OnAttach方法
            return(true);
        }

        virtual         void        DetachComponent     (Component *comp)                                           ///<删除一个组件
        {
            if (!comp)return;

            ComponentSet.Delete(comp);

            comp->OnDetach(this);            //调用组件的OnDetach方法
        }

                        bool        Contains            (Component *comp){return ComponentSet.Contains(comp);}      ///<是否包含指定组件

                        bool        HasComponent        (const ComponentManager *);                                 ///<是否有指定组件管理器的组件
        virtual         int         GetComponents       (ArrayList<Component *> &comp_list,const ComponentManager *);    ///<取得所有组件

        const SortedSet<Component *> &    GetComponents       ()const{return ComponentSet;}
    };//class SceneNode

    SceneNode *Duplication(SceneNode *);                                                                            ///<复制一个场景节点
}//namespace hgl::graph
