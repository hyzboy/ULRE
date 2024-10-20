#pragma once

#include<hgl/type/TypeInfo.h>
#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

class SceneNode;

class ComponentData
{
};//class ComponentData

class ComponentManager;

class Component
{
    ComponentData *data;

    SceneNode *owner_node;

    ComponentManager *manager;

public:

    virtual const size_t GetTypeHash()=0;                                       ///<取得当前组件类型

    SceneNode *GetOwnerNode()const{return owner_node;}                          ///<取得当前组件所属节点

    ComponentManager *GetManager()const{return manager;}                        ///<取得当前组件所属管理器

public:

    Component()
    {
        data=nullptr;
        owner_node=nullptr;
        manager=nullptr;
    }
    Component(SceneNode *sn,ComponentData *cd,ComponentManager *cm);
    virtual ~Component()=default;

    virtual void Update(const double delta_time){};                             ///<更新组件
};//class Component

class ComponentManager
{
    ObjectList<Component> component_list;

public:

    virtual void Update(const double delta_time)
    {
        for(Component *c:component_list)
        {
            c->Update(delta_time);
        }
    }
};//class ComponentManager

VK_NAMESPACE_END
