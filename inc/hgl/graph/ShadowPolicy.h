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

            Plane,                          ///<压片阴影(极少三角面的片状物体专用)
            RTDF,                           ///<距离场动态阴影(静态物体专用)
            Capsule,                        ///<胶囊体动态阴影(骨骼动画专用)

            ShadowVolume,                   ///<体积阴影(超规则类物体专用，如房子)

            ENUM_CLASS_RANGE(None,ShadowVolume)
        };
    }//namespace graph
}//namespace hgl
