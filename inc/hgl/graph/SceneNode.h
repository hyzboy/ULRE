#pragma once

#include<hgl/type/ObjectList.h>
#include<hgl/type/IDName.h>
#include<hgl/graph/NodeTransform.h>
#include<hgl/graph/AABB.h>
#include<hgl/graph/OBB.h>
#include<hgl/component/Component.h>

namespace hgl::io
{
    class EventDispatcher; // 前向声明事件分发器
}//

namespace hgl::graph
{
    class Scene;        //场景主类
    //每个SceneNode都要记录Scene来区分自己属于那个场景
    //这个场景可能是一个确实是一个场景，也有可能只是一个StaticMesh

    class CameraControl;

    using SceneNodeID   =int64;

    using SceneNodeList =ObjectList<SceneNode>;

    HGL_DEFINE_U16_IDNAME(SceneNodeName)

    /**
    * 场景节点数据类<br>
    * 从场景坐标变换(NodeTransform)类继承，
    * 每个场景节点中可能包括一个可渲染数据实例，或是完全不包含(用于坐标变换的父节点，或是灯光/摄像机之类)。
    */
    class SceneNode:public NodeTransform                                                                              ///场景节点类
    {
        friend class Scene;

        Scene *main_scene=nullptr;                                                                                  ///<主场景

        SceneNode *parent_node=nullptr;                                                                             ///<上级节点

        SceneNodeID node_id=-1;                                                                                     ///<节点ID
        SceneNodeName node_name;                                                                                    ///<节点名称

        void OnChangeScene(Scene *);

    protected:

        AABB local_bounding_box;                                                                                    ///<本地坐标绑定盒
        OBB world_bounding_box;                                                                                     ///<世界坐标绑定盒

    protected:

        SceneNodeList child_nodes;                                                                                  ///<子节点

        /**
        * 组件合集，一个SceneNode下可能会包含多个组件，同时一个组件也可能被多个SceneNode使用。
        * 所以这里只保留一个指针，不拥有组件的生命周期，组件的生命周期由其对应的ComponentManager管理。
        */
        ComponentSet component_set;                                                                                 ///<组件合集

    public:

        Scene *             GetScene()const{ return main_scene; }                                                   ///<取得主场景
        RenderFramework *   GetRenderFramework()const;                                                              ///<取得渲染框架
        CameraControl *     GetCameraControl()const;                                                                ///<取得默认摄像机控制器

        const SceneNodeID &     GetNodeID   ()const { return node_id; }                                             ///<取得节点ID
        const SceneNodeName &   GetNodeName ()const { return node_name; }                                           ///<取得节点名称

        const SceneNodeList &   GetChildNode()const { return child_nodes; }                                         ///<取得子节点列表

    protected:

        SceneNode(const SceneNode &)=delete;
        SceneNode(const SceneNode *)=delete;

        SceneNode():NodeTransform(){}
        SceneNode(Scene *s):NodeTransform(),main_scene(s){}                                                           ///<从Scene构造
        SceneNode(Scene *s,const NodeTransform &so):NodeTransform(so),main_scene(s){}                                 ///<从NodeTransform复制构造
        SceneNode(Scene *s,const Matrix4f &mat):NodeTransform(mat),main_scene(s){}                                    ///<从Matrix4f复制构造

    public:

        virtual ~SceneNode();

    public:

        virtual SceneNode * CreateNode()const{return(new SceneNode(GetScene()));}                                   ///<创建一个同类的节点对象

        virtual void        CloneChildren(SceneNode *node) const                                                    ///<复制子节点到指定节点
        {
            for(SceneNode *sn:GetChildNode())
                node->AddChild(sn->Clone());
        }

        virtual void        CloneComponents(SceneNode *node) const                                                  ///<复制组件到指定节点
        {
            for(Component *c:GetComponents())
                node->AttachComponent(c->Clone());
        }

        virtual SceneNode * Clone() const                                                                           ///<复制一个场景节点
        {
            if(!this)
                return nullptr;

            SceneNode *node=CreateNode();

            node->SetTransformState(GetTransformState());  //复制本地矩阵

            CloneChildren(node);    //复制子节点
            CloneComponents(node);    //复制组件

            return node;
        }

        void Reset() override
        {
            SetParent(nullptr); //清除父节点

            local_bounding_box.Clear();
            world_bounding_box.Clear();

            child_nodes.Clear();
            component_set.Clear();

            NodeTransform::Reset();
        }

        const bool HasChildren()const{return !child_nodes.IsEmpty();}

        virtual void        SetParent(SceneNode *sn)
        {
            if(parent_node==sn)
                return; //如果父节点没有变化，则不需要处理

            parent_node=sn;

            OnChangeScene(sn?sn->GetScene():nullptr);
        }

                SceneNode * GetParent()      noexcept{return parent_node;}
        const   SceneNode * GetParent()const noexcept{return parent_node;}

        SceneNode *AddChild(SceneNode *sn)
        {
            if(!sn)
                return(nullptr);

            child_nodes.Add(sn);
            sn->SetParent(this);
            return sn;
        }

    public: //事件相关

        virtual io::EventDispatcher *GetEventDispatcher(){return nullptr;}                                          ///<取得事件分发器(如果返回nullptr,则表示该节点不支持事件分发)

    public: //坐标相关方法

        virtual         void        UpdateWorldTransform() override;                                                ///<刷新世界变换
        virtual         void        RefreshBoundingBox  ();                                                         ///<刷新绑定盒

        virtual const   AABB &      GetLocalBoundingBox ()const{return local_bounding_box;}                         ///<取得本地坐标绑定盒
//            virtual const   AABB &      GetWorldBoundingBox ()const{return WorldBoundingBox;}                           ///<取得世界坐标绑定盒

    public: //组件相关方法

                        bool        ComponentIsEmpty    ()const{return component_set.IsEmpty();}                    ///<是否没有组件
        virtual const   int64       GetComponentCount   ()const{return component_set.GetCount();}                   ///<取得组件数量
        virtual         bool        AttachComponent     (Component *comp)                                           ///<添加一个组件
        {
            if(!comp)return(false);

            if(component_set.Add(comp)<0)
                return(false);

            comp->OnAttach(this);            //调用组件的OnAttach方法
            return(true);
        }

        virtual         void        DetachComponent     (Component *comp)                                           ///<删除一个组件
        {
            if (!comp)return;

            component_set.Delete(comp);

            comp->OnDetach(this);            //调用组件的OnDetach方法
        }

                        bool        Contains            (Component *comp){return component_set.Contains(comp);}     ///<是否包含指定组件

                        bool        HasComponent        (const ComponentManager *);                                 ///<是否有指定组件管理器的组件
        virtual         int         GetComponents       (ComponentList &comp_list,const ComponentManager *);        ///<取得所有组件

                const ComponentSet &GetComponents       ()const{return component_set;}
    };//class SceneNode
}//namespace hgl::graph
