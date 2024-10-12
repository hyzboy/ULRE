#pragma once

#include<hgl/TypeFunc.h>

namespace hgl
{
    namespace graph
    {
        /**
        * 全局动态阴影策略
        */
        enum class GlobalDynamicShadowPolicy
        {
            None,                           ///<不产生全局动态阴影

            Cascade,                        ///<级联阴影
            ParallelSplit,                  ///<平行分割阴影
            Virtual,                        ///<虚拟阴影

            ENUM_CLASS_RANGE(None,Virtual)
        };

        /**
        * 对象动态阴影策略<br>
        * 注：动态阴影会使用屏幕空间技术，不管使用以下何种技术，会全部合成到一个屏幕空间shadow map，再统一做blur之类的操作
        */
        enum class ObjectDynamicShadowPolicy
        {
            None,                           ///<不产生动态阴影

            Global,                         ///<使用全局动态阴影

            PerObject,                      ///<独立对象阴影(就是每个物件独立走普通shadowmap得到一张深度图，缓存到硬盘)

            Plane,                          ///<压片阴影(极少三角面的片状物体专用)
            Capsule,                        ///<胶囊体阴影(一般用于骨骼动画模型阴影，每根骨骼一个胶囊)
            Cube,                           ///<立方体阴影(一般用于一些建筑物，比如楼房直接使用一个Cube做Raymarch)
            MeshSDF,                        ///<模型3D距离场阴影

            ENUM_CLASS_RANGE(None,MeshSDF)
        };
    }//namespace graph
}//namespace hgl
