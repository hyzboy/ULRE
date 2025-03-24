#pragma once

#include<hgl/type/DataType.h>
#include<hgl/type/SortedSet.h>
#include<hgl/type/List.h>

#define COMPONENT_NAMESPACE         hgl::graph
#define COMPONENT_NAMESPACE_BEGIN   namespace COMPONENT_NAMESPACE {
#define COMPONENT_NAMESPACE_END     }

COMPONENT_NAMESPACE_BEGIN

    class ComponentManager;
    class SceneNode;

    struct ComponentData{};

    /**
    * 基础组件<br>
    * 是一切组件的基类
    */
    class Component
    {
        SceneNode *         OwnerNode;
        ComponentManager *  Manager;
        ComponentData *     Data;

    public:

        Component()=delete;
        Component(SceneNode *sn,ComponentData *cd,ComponentManager *cm)
        {
            OwnerNode=sn;
            Data=cd;
            Manager=cm;
        }

        virtual ~Component()=default;

    public:

        SceneNode *         GetOwnerNode()const{return OwnerNode;}
        ComponentManager *  GetManager  ()const{return Manager;}
        ComponentData *     GetData     ()const{return Data;}

    public:

        virtual void Update(const double delta_time)=0;

    public: //事件

        virtual void OnFocusLost(){}                                            ///<焦点丢失事件
        virtual void OnFocusGained(){}                                          ///<焦点获得事件
    };//class Component

    using ComponentSet=SortedSet<Component *>;

    class ComponentManager
    {
        ComponentSet component_set;

    public:

        virtual ~ComponentManager()=default;

    public:

        virtual size_t          ComponentHashCode()const=0;

        virtual Component *     CreateComponent(SceneNode *,ComponentData *)=0;

                int             GetComponentCount()const{return component_set.GetCount();}

                ComponentSet &  GetComponents(){return component_set;}

                int             GetComponents(List<Component *> &comp_list,SceneNode *);

        virtual void            UpdateComponents(const double delta_time);

        virtual void            JoinComponent(Component *c){if(!c)return;component_set.Add(c);}
        virtual void            UnjonComponent(Component *c){if(!c)return;component_set.Delete(c);}

    public: //事件

        virtual void            OnFocusLost(){}                                             ///<焦点丢失事件
        virtual void            OnFocusGained(){}                                           ///<焦点获得事件
    };//class ComponentManager

COMPONENT_NAMESPACE_END
