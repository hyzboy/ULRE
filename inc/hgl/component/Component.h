#pragma once

#include<hgl/type/DataType.h>
#include<hgl/type/SortedSet.h>

namespace hgl::graph
{
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

        virtual void Update(const double delta_time)=0;

    public: //事件

        virtual void OnFocusLost(){}                                            ///<焦点丢失事件
        virtual void OnFocusGained(){}                                          ///<焦点获得事件
    };//class Component

    class ComponentManager
    {
        SortedSet<Component *> ComponentSet;

    public:

        virtual ~ComponentManager()=default;

    public:

        virtual size_t      ComponentHashCode()const=0;

        virtual Component * CreateComponent(SceneNode *,ComponentData *)=0;

                int         GetComponentCount()const{return ComponentSet.GetCount();}

        virtual void        UpdateComponents(const double delta_time)
        {
            Component **cc=ComponentSet.GetData();

            for(int i=0;i<ComponentSet.GetCount();i++)
                cc[i]->Update(delta_time);
        }

        virtual void        JoinComponent(Component *c){if(!c)return;ComponentSet.Add(c);}
        virtual void        UnjonComponent(Component *c){if(!c)return;ComponentSet.Delete(c);}

    public: //事件

        virtual void OnFocusLost(){}                                             ///<焦点丢失事件
        virtual void OnFocusGained(){}                                           ///<焦点获得事件
    };//class ComponentManager
}//namespace hgl::graph
