#pragma once

#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKResourceDomain.h>

namespace hgl::graph{

/**
* 材质实例类<br>
* 材质实例类本质只是提供一个数据区，供RenderCollector合并成一个大UBO。
*
* Phase 1: 支持可选 ResourceDomain。
*   - domain == nullptr : MI 数据由 Material 自身的数据池管理（旧路径）。
*   - domain != nullptr : MI 数据由指定 ResourceDomain 的独立数据池管理（新路径）。
*/
class MaterialInstance
{
protected:

    Material *material;

    ResourceDomain *domain;     ///< (Phase 1) 可选资源域；为 nullptr 时使用旧路径

    const VIL *vil;

    int mi_id;

public:

            Material *      GetMaterial ()      { return material; }
            ResourceDomain *GetDomain   ()      { return domain; }

    const   VIL *           GetVIL      ()const { return vil; }

private:

    friend class Material;
    friend class ResourceDomain;
    friend class MaterialManager;

    /// 旧路径构造（domain = nullptr）
    MaterialInstance(Material *, const VIL *, const int);

    /// 新路径构造（Phase 1，经由 ResourceDomain 分配槽位）
    MaterialInstance(Material *, ResourceDomain *, const VIL *, const int);

public:

    virtual ~MaterialInstance();

    const   int     GetMIID     ()const{return mi_id;}

            void *  GetMIData   ();                                         ///<取得材质实例数据
            void    WriteMIData (const void *data,const uint32 size);       ///<写入材质实例数据

        template<typename T>
            void    WriteMIData (const T &data){WriteMIData(&data,sizeof(T));}
};//class MaterialInstance
}//namespace hgl::graph

