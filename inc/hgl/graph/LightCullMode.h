#pragma once

#include<hgl/graph/VKNamespace.h>
#include<hgl/TypeFunc.h>

VK_NAMESPACE_BEGIN

/**
* 光照剔除模式
*/
enum class LightingCullingMode
{
    None,           ///<不剔除

    /*
    * 基于Tile的剔除模式
    * 按屏幕XY坐标划分成多个Tile，再配合znear/zfar形成一个Volume。所有光源和Volume计算相交性
    */
    Tile,           ///<瓦片剔除

    /**
    * 基于Tile的剔除模式的改进型
    * 同Tile方法，得出Tile后，再通过遍历Tile内所有象素，得出当前Tile的最远z值和最近z值。
    * 根据XY与zNear/zFar得出一个Volume，计算所有光源与Volume相交性。
    */
    TileVolume,     ///<瓦片体积剔除

    /**
    * 基于Tile的剔除模式的改进型
    * 同TileVolume方法得出Volume后，再将Volume按深度划分成多个Volume。
    * 剔除掉没有象素的Volume，再将剩下的Volume与光源计算相交性。
    */
    Cluster,        ///<集簇剔除

    ENUM_CLASS_RANGE(None,Cluster)
};//enum class LightingCullingMode

VK_NAMESPACE_END
