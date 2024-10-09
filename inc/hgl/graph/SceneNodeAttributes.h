#pragma once
#include<hgl/type/DataType.h>
#include<hgl/graph/ShadowPolicy.h>

namespace hgl
{
    namespace graph
    {
    #pragma pack(push,1)
        /**
         * 场景节点变换属性<br>
         */
        struct SceneNodeTransformAttributes
        {
            uint moveable:1;                ///<可移动
            uint rotatable:1;               ///<可旋转
            uint scalable:1;                ///<可缩放

            //为什么要 移动、旋转、缩放 三个分开而不是一个整体

            // 一、不可移动、不可旋转、不可缩放的通常用于Lightmap等可以完全预计算的东西
            // 二、物理引擎对于可缩放是独立支持的，所以缩放要分开
            // 三、逻辑处理对移动一般有特别响应，但旋转不一定(也有可能只处理旋转)，所以移动和旋转要分开

                // 比如RTS中的激光坦克、电磁坦克，由于是圆形范围攻击的，所以不需要关心旋转属性，只需要关心移动属性即可
                //      同理，地面雷达，因为是扇形旋转扫描敌人的，所以只关心旋转
                //      而士兵、坦克等单位，既需要移动，又需要旋转，但不会缩放
                // 再比如风车，它不能移动，但可以旋转。但我们判断是否靠近是否看到，根本不需要关心旋转属性
        };

        /**
         * 场景节点可视属性<br>
         */
        struct SceneNodeVisualAttributes
        {
            uint visible:1;                 ///<是否可见

            uint render_color:1;            ///<渲染颜色
            uint render_normal:1;           ///<渲染法线
            uint render_depth:1;            ///<渲染深度

            uint render_at_reflect:1;       ///<在反射时渲染

            uint cast_shadow:1;             ///<投射阴影
            uint cast_static_shadow:1;      ///<投射静态阴影(预计算阴影)
            uint cast_dynamic_shadow:1;     ///<投射动态阴影

            uint receive_static_shadow:1;   ///<接收静态阴影
            uint receive_dynamic_shadow:1;  ///<接收动态阴影

            uint receive_static_light:1;    ///<接收静态光照
            uint receive_dynamic_light:1;   ///<接收动态光照

            ObjectDynamicShadowPolicy dynamic_shadow_policy:8;    ///<动态阴影策略

//            uint8 light_channels;           ///<接收的光通道
        };

        constexpr const size_t SceneNodeVisualAttributesBytes=sizeof(SceneNodeVisualAttributes);

        /**
        * 场景节点裁剪属性<br>
        */
        struct SceneNodeCullAttribute
        {
            uint32 min_distance;            ///<最小裁剪距离
            uint32 max_distance;            ///<最大裁剪距离

            uint32 min_volume;              ///<最小裁剪体积
            uint32 max_volume;              ///<最大裁剪体积
        };

        struct SceneNodeAttributes
        {
            uint editor_only:1;             ///<仅编辑器中使用
            uint can_tick:1;                ///<可被Tick

            SceneNodeTransformAttributes    transform;      ///<变换属性
            SceneNodeVisualAttributes       visual;         ///<可视属性

            SceneNodeCullAttribute          cull;           ///<裁剪属性
        };

        constexpr const size_t SceneNodeAttributesBytes=sizeof(SceneNodeAttributes);

    #pragma pack(pop)
    }//namespace graph
}//namespace hgl
