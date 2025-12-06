#pragma once

#include<hgl/graph/VK.h>
#include<hgl/math/Vector.h>

namespace hgl::graph
{
    class GeometryCreater;

    namespace inline_geometry
    {
        // 新增：2D轮廓旋转生成3D几何体
        struct RevolutionCreateInfo
        {
            math::Vector2f*   profile_points;     ///<2D轮廓点数组(X为距离轴的距离，Y为沿轴的高度)
            uint        profile_count;      ///<轮廓点数量
            uint        revolution_slices;  ///<旋转分段数量
            float       start_angle;        ///<起始角度（度）
            float       sweep_angle;        ///<扫过角度（度），360为完整旋转
            math::Vector3f    revolution_axis;    ///<旋转轴方向（默认为Z轴(0,0,1)）
            math::Vector3f    revolution_center;  ///<旋转中心点（默认为原点(0,0,0)）
            bool        close_profile;      ///<是否闭合轮廓（连接首尾点）
            math::Vector2f    uv_scale;           ///<UV缩放
            float       normal_smooth_angle;///<平滑阈值 (度)，超过此夹角则拆分法线

        public:
            RevolutionCreateInfo()
            {
                profile_points = nullptr;
                profile_count = 0;
                revolution_slices = 16;
                start_angle = 0.0f;
                sweep_angle = 360.0f;
                revolution_axis = math::Vector3f(0, 0, 1);    // Z轴向上
                revolution_center = math::Vector3f(0, 0, 0);
                close_profile = true;
                uv_scale = math::Vector2f(1.0f, 1.0f);
                normal_smooth_angle = 30.0f; // 默认30度阈值
            }
        };

        /**
         * 创建一个由2D轮廓旋转生成的3D几何体
         * 比如半圆弧旋转成球体，方形轮廓旋转成圆柱体等
         *
         * @param pc 图元创建器
         * @param rci 旋转创建信息
         * @return 创建的图元，失败返回nullptr
         *
         * 使用说明：
         * - profile_points: 2D轮廓点数组，x为距离旋转轴的距离，y为沿轴的高度
         * - revolution_slices: 旋转分段数，决定圆形的平滑度
         * - sweep_angle: 扫过角度，360度为完整旋转，小于360度会生成端面
         * - revolution_axis: 旋转轴方向，默认为Z轴(0,0,1)
         * - close_profile: 是否闭合轮廓，影响首尾点是否连接
         *
         * 示例：
         * 半圆弧 + 360度旋转 = 球体
         * 矩形轮廓 + 360度旋转 = 圆柱体
         * "]"形轮廓 + 360度旋转 = 空心圆柱
         */
        Geometry *CreateRevolution(GeometryCreater *pc, const RevolutionCreateInfo *rci);
    }//namespace inline_geometry
}//namespace hgl::graph
