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
    ComponentManager *manager;

    SceneNode *owner;

public:

    ComponentManager *GetManager()const{return manager;}

    SceneNode *GetOwner()const{return owner;}

    virtual const size_t GetTypeHash()=0;

public:


};//class Component

class ComponentManager
{
};//class ComponentManager

VK_NAMESPACE_END
