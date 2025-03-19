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

        virtual void OnFocusLost(){}                                            ///<焦点丢失事件
        virtual void OnFocusGained(){}                                          ///<焦点获得事件
    };//class Component

    //Component *CreateComponent(const ObjectBaseInfo &,ComponentData *);

    //template<typename T,typename ...ARGS> inline T *NewComponentSCL(const SourceCodeLocation &scl,ARGS...args)
    //{
    //    static size_t new_count=0;
    //    ObjectBaseInfo obi;

    //    obi.hash_code   =GetTypeHash<T>();
    //    obi.unique_id   =new_count;
    //    obi.scl         =scl;

    //    ++new_count;
    //    T *obj=new T(obi);
    //    return obj;
    //}

    //#define NewComponent(T,...) NewComponentSCL<T>(HGL_SOURCE_LOCATION __VA_OPT__(,) __VA_ARGS__)

    class ComponentManager
    {
        SortedSet<Component *> ComponentSet;

    public:

        virtual ~ComponentManager()=default;

    public:

        virtual size_t      ComponentHashCode()const=0;

        virtual Component * CreateComponent(SceneNode *,ComponentData *)=0;

        virtual int         GetComponentCount()const=0;
    };//class ComponentManager
}//namespace hgl::graph
