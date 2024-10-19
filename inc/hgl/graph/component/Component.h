#pragma once

#include<hgl/type/TypeInfo.h>
#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

class SceneNode;

class SceneComponentData
{
};//class SceneComponentData

class SceneComponentManager;

class SceneComponent
{
    SceneComponentManager *manager;

    SceneNode *owner;

public:

    SceneComponentManager *GetManager()const{return manager;}

    SceneNode *GetOwner()const{return owner;}

    virtual const size_t GetTypeHash()=0;

public:


};//class SceneComponent

class SceneComponentManager
{
};//class SceneComponentManager

VK_NAMESPACE_END
