#pragma once

#include<hgl/vk/VKMaterial.h>

namespace hgl::graph{

/**
* 材质实例类<br>
* 材质实例类本质只是提供一个数据区，供RenderCollector合并成一个大UBO。
*/
class MaterialInstance
{
protected:

    Material *material;

    const VIL *vil;

    int mi_id;

public:

            Material *  GetMaterial ()      {return material;}

    const   VIL *       GetVIL      ()const {return vil;}

private:

    friend class Material;

    MaterialInstance(Material *,const VIL *,const int);

public:

    virtual ~MaterialInstance()
    {
        material->ReleaseMI(mi_id);
    }

    const   int     GetMIID     ()const{return mi_id;}                          ///<取得材质实例ID
            void *  GetMIData   (){return material->GetMIData(mi_id);}          ///<取得材质实例数据
            void    WriteMIData (const void *data,const uint32 size);           ///<写入材质实例数据

        template<typename T>
            void    WriteMIData (const T &data){WriteMIData(&data,sizeof(T));}  ///<写入材质实例数据
};//class MaterialInstance
}//namespace hgl::graph
