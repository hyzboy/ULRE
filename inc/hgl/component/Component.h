#pragma once

#include<hgl/type/DataType.h>
#include<hgl/type/SortedSet.h>
#include<hgl/type/ArrayList.h>

/**
*   Component/Data/Manager 体系设计简要说明
*
*   本体系参考AMD FidelityFX，但并不完全一致。
*
*   AMD FidelityFX中，Component存放于Entity下，而我方中与其类似的定义为SceneNode。
*   不管是Entity还是SceneNode，它们都提供空间变换，以及子节点、Component的管理。
*   而AMD FidelityFX中的Scene，类似于我方的Scene，用于储存一个场景世界的根节点及其它世界唯一数据。
*
*   ComponentData是每个Component的数据，用于向Component或是其它模块提供数据。
*   ComponentManager是Component的管理器，用于管理Component的创建、销毁、更新等。
*
*   需要注意的是：同AMD FidelityFX一样，大部分ComponentManager与Scene基本无关。
*   因为同样的数据可能出现在多个World之中。
*   仅有那些与Scene密切相关的Component它对应的Manager才会出现在Scene中，比如CameraManager/LightManager。
*   而如MeshComponent之类的纯资源型就会是独立存在的。
*
*   Component是组件的基类，所有组件都从这里派生。
*
*   SceneComponent是场景组件基类，只要是放在场景中的都从它派生，
* 
*   PrimitiveComponent是图元组件的基类，所有图元组件都从这里派生。
*   它再度派生出的任何Component都必须是一个有3D空间的几何图元。
*   引擎中的空间、物理、等都由PrimitiveComponent提供数据进行计算。

*   RenderComponent是可渲染组件的基类，所有可渲染组件都从这里派生。
*
*   MeshComponent是静态网格组件，它是一个具体的RenderComponent实现。
* 
*/

#define COMPONENT_NAMESPACE         hgl::graph
#define COMPONENT_NAMESPACE_BEGIN   namespace COMPONENT_NAMESPACE {
#define COMPONENT_NAMESPACE_END     }

COMPONENT_NAMESPACE_BEGIN

class ComponentManager;
class SceneNode;

struct ComponentData
{
public:

    ComponentData()=default;
    virtual ~ComponentData()=default;

    virtual const size_t GetHashCode()const=0;              ///<取得ComponentData的类型哈希值
    virtual const size_t GetComponentHashCode()const=0;     ///<取得Component的类型哈希值
    virtual const size_t GetManagerHashCode()const=0;       ///<取得ComponentManager的类型哈希值
};//struct ComponentData

#define COMPONENT_DATA_CLASS_BODY(name)  static constexpr   const size_t StaticHashCode         (){return hgl::GetTypeHash<name##ComponentData>();} \
                                         static constexpr   const size_t StaticComponentHashCode(){return hgl::GetTypeHash<name##Component>();} \
                                         static constexpr   const size_t StaticManagerHashCode  (){return hgl::GetTypeHash<name##ComponentManager>();}   \
                                                            const size_t GetHashCode            ()const override{return name##ComponentData::StaticHashCode();} \
                                                            const size_t GetComponentHashCode   ()const override{return name##ComponentData::StaticComponentHashCode();} \
                                                            const size_t GetManagerHashCode     ()const override{return name##ComponentData::StaticManagerHashCode();}

using ComponentDataPtr=SharedPtr<ComponentData>;

/**
* 基础组件<br>
* 是一切组件的基类
*/
class Component
{
    static uint unique_id_count;

    uint unique_id;

    SceneNode *         OwnerNode;
    ComponentManager *  Manager;
    ComponentDataPtr    Data;

protected:

    friend class ComponentManager;

    virtual void OnDetachManager(ComponentManager *cm)
    {
        if(cm==Manager)
            Manager=nullptr;
    }

public:

    Component()=delete;
    Component(ComponentDataPtr,ComponentManager *);
    virtual ~Component();

    virtual const size_t GetHashCode()const=0;              ///<取得Component的类型哈希值
    virtual const size_t GetDataHashCode()const=0;          ///<取得ComponentData的类型哈希值
    virtual const size_t GetManagerHashCode()const=0;       ///<取得ComponentManager的类型哈希值

public:

    uint                GetUniqueID ()const{return unique_id;}

    SceneNode *         GetOwnerNode()const{return OwnerNode;}
    ComponentManager *  GetManager  ()const{return Manager;}
    ComponentDataPtr    GetData     ()const{return Data;}

public:

    virtual bool        ChangeData(ComponentDataPtr cdp);

public:

    virtual Component *Duplication();

    //virtual void Update(const double delta_time)=0;

public: //事件

    virtual void OnAttach(SceneNode *node){if(node)OwnerNode=node;}         ///<附加到节点事件
    virtual void OnDetach(SceneNode *){OwnerNode=nullptr;}                  ///<从节点分离事件

    virtual void OnFocusLost(){}                                            ///<焦点丢失事件
    virtual void OnFocusGained(){}                                          ///<焦点获得事件
};//class Component

#define COMPONENT_CLASS_BODY(name)  static  name##ComponentManager *GetDefaultManager   ()              {return name##ComponentManager::GetDefaultManager();} \
                                            name##ComponentManager *GetManager          ()const         {return (name##ComponentManager *)Component::GetManager();}  \
                                    static  constexpr const size_t  StaticHashCode      ()              {return hgl::GetTypeHash<name##Component>();} \
                                            const size_t            GetHashCode         ()const override{return name##Component::StaticHashCode();} \
                                            const size_t            GetDataHashCode     ()const override{return name::ComponentData::StaticHashCode();} \
                                            const size_t            GetManagerHashCode  ()const override{return name##ComponentManager::StaticHashCode();}

using ComponentSet=SortedSet<Component *>;
using ComponentList=ArrayList<Component *>;

class ComponentManager
{
    ComponentSet component_set;

protected:

    friend class Component; //Component可以直接访问ComponentManager的成员

    virtual void            AttachComponent(Component *c){if(!c)return;component_set.Add(c);}
    virtual void            DetachComponent(Component *c){if(!c)return;component_set.Delete(c);}

public:

    virtual const size_t    GetComponentHashCode()const=0;          ///<取得Component的类型哈希值
    virtual const size_t    GetDataHashCode()const=0;               ///<取得ComponentData的类型哈希值
    virtual const size_t    GetHashCode()const=0;                   ///<取得ComponentManager的类型哈希值

    virtual ~ComponentManager();

public:

    virtual Component *     CreateComponent(ComponentDataPtr)=0;

            const size_t    GetComponentCount()const{return component_set.GetCount();}

            ComponentSet &  GetComponents(){return component_set;}

            int             GetComponents(ComponentList &comp_list,SceneNode *);

    virtual void            UpdateComponents(const double delta_time);

public: //事件

    virtual void            OnFocusLost(){}                                             ///<焦点丢失事件
    virtual void            OnFocusGained(){}                                           ///<焦点获得事件
};//class ComponentManager

#define COMPONENT_MANAGER_CLASS_BODY(name)      static  name##ComponentManager *    GetDefaultManager       ()              {return GetComponentManager<name##ComponentManager>(true);}   \
                                                static  constexpr   const size_t    StaticHashCode          ()              {return hgl::GetTypeHash<name##ComponentManager>();}    \
                                                static  constexpr   const size_t    StaticDataHashCode      ()              {return hgl::GetTypeHash<name##ComponentData>();} \
                                                static  constexpr   const size_t    StaticComponentHashCode ()              {return hgl::GetTypeHash<name##Component>();}   \
                                                                    const size_t    GetComponentHashCode    ()const override{return name##ComponentManager::StaticComponentHashCode();} \
                                                                    const size_t    GetDataHashCode         ()const override{return name##ComponentManager::StaticDataHashCode();} \
                                                                    const size_t    GetHashCode             ()const override{return name##ComponentManager::StaticHashCode();}

bool RegistryComponentManager(ComponentManager *);
ComponentManager *GetComponentManager(const size_t hash_code);

template<typename T> inline T *GetComponentManager(bool create_default=true)
{
    T *cm=(T *)GetComponentManager(T::StaticHashCode());

    if(!cm&&create_default)
    {
        cm=new T;

        RegistryComponentManager(cm);
    }

    return cm;
}
COMPONENT_NAMESPACE_END
