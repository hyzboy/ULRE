#pragma once

#include<hgl/graph/VKNamespace.h>
#include<hgl/TypeFunc.h>

VK_NAMESPACE_BEGIN

/**
 * 静态模型LOD策略
 */
enum class StaticMeshLODPolicy:uint8
{
    None=0,                                                                     ///<无LOD

    DiscardDetail,                                                              ///<丢弃细节

    AnotherMesh,                                                                ///<另一个模型

    Billboard,                                                                  ///<广告牌

    //Voxel,                                                                      ///<体素

    //MeshSDF,                                                                    ///<网格SDF

    //MeshCard,                                                                   ///<网格卡片

    ENUM_CLASS_RANGE(None,Billboard)
};//enum class StaticMeshLODPolicy

VK_NAMESPACE_END
