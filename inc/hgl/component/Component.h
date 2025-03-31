#pragma once

#include<hgl/type/DataType.h>
#include<hgl/type/SortedSet.h>
#include<hgl/type/List.h>

/**
*   Component/Data/Manager 体系设计简要说明
*
*   本体系参考AMD FidelityFX，但并不完全一致。
*
*   AMD FidelityFX中，Component存放于Entity下，而我方中与其类似的定义为SceneNode。
*   不管是Entity还是SceneNode，它们都提供空间变换，以及子节点、Component的管理。
*   而AMD FidelityFX中的Scene，类似于我方的SceneWorld，用于储存一个场景世界的根节点及其它世界唯一数据。
*
*   ComponentData是每个Component的数据，用于向Component或是其它模块提供数据。
*   ComponentManager是Component的管理器，用于管理Component的创建、销毁、更新等。
*
*   需要注意的是：同AMD FidelityFX一样，大部分ComponentManager与SceneWorld基本无关。
*   因为同样的数据可能出现在多个World之中。
*   仅有那些与SceneWorld密切相关的Component它对应的Manager才会出现在SceneWorld中，比如CameraManager/LightManager。
*   而如StaticMeshComponent之类的纯资源型就会是独立存在的。
*
*   Component是组件的基类，所有组件都从这里派生。
*
*
*   RenderComponent是可渲染组件的基类，所有可渲染组件都从这里派生。它继承于Component。
*
*   PrimitiveComponent是图元组件的基类，所有图元组件都从这里派生。它继承于RenderComponent。
*   它再度派生出的任何Component都必须是一个有3D空间的几何图元。
*   引擎中的空间、物理、等都由PrimitiveComponent提供数据进行计算。
*
*   StaticMeshComponent是静态网格组件，它是一个具体的PrimitiveComponent实现。
* 
*/

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

    //virtual void Update(const double delta_time)=0;

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
