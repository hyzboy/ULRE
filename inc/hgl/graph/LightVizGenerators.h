#pragma once

#include<hgl/type/Vector3f.h>
#include<hgl/color/Color4f.h>
#include<hgl/type/List.h>
#include<hgl/graph/LineManager.h>
#include<hgl/graph/Light.h>

namespace hgl::graph
{
    /**
     * 光源可视化线段生成器
     * 用于在3D场景编辑器中绘制各种光源和摄像机观察范围的大体形状和范围
     */
    namespace light_viz_generators
    {
        /**
         * 生成点光源的可视化线段
         * 绘制一个简单的球体框架或十字标记来表示点光源位置和范围
         * @param light_mgr 线条管理器
         * @param light 点光源信息
         * @param radius 可视化范围半径
         * @param color 线条颜色
         */
        void GeneratePointLightLines(LineManager* light_mgr, const PointLight& light, float radius, const Color4f& color);

        /**
         * 生成聚光灯的可视化线段
         * 绘制圆锥形状表示聚光灯的照射范围和方向
         * @param light_mgr 线条管理器
         * @param light 聚光灯信息
         * @param length 圆锥长度
         * @param color 线条颜色
         */
        void GenerateSpotLightLines(LineManager* light_mgr, const SpotLight& light, float length, const Color4f& color);

        /**
         * 生成矩形光源的可视化线段
         * 绘制矩形框架表示面光源的形状和方向
         * @param light_mgr 线条管理器
         * @param position 光源位置
         * @param direction 光源朝向方向(归一化)
         * @param up 光源向上方向(归一化)
         * @param width 矩形宽度
         * @param height 矩形高度
         * @param color 线条颜色
         */
        void GenerateRectLightLines(LineManager* light_mgr, const Vector3f& position, const Vector3f& direction, 
                                  const Vector3f& up, float width, float height, const Color4f& color);

        /**
         * 生成摄像机观察范围的可视化线段
         * 绘制视锥体(frustum)表示摄像机的视野范围
         * @param light_mgr 线条管理器
         * @param position 摄像机位置
         * @param direction 摄像机朝向方向(归一化)
         * @param up 摄像机向上方向(归一化)
         * @param fov_y 垂直视野角度(弧度)
         * @param aspect 宽高比
         * @param near_dist 近裁剪面距离
         * @param far_dist 远裁剪面距离
         * @param color 线条颜色
         */
        void GenerateCameraFrustumLines(LineManager* light_mgr, const Vector3f& position, const Vector3f& direction,
                                      const Vector3f& up, float fov_y, float aspect, float near_dist, float far_dist, 
                                      const Color4f& color);

        // 辅助函数
        /**
         * 计算两个向量的叉积
         */
        Vector3f CrossProduct(const Vector3f& a, const Vector3f& b);

        /**
         * 归一化向量
         */
        Vector3f Normalize(const Vector3f& v);
    }
}