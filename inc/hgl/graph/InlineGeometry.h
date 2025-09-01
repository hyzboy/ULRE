#pragma once

#include<hgl/graph/VK.h>
#include<hgl/math/Vector.h>
#include<hgl/type/RectScope.h>
#include<hgl/type/Size2.h>
#include<hgl/color/Color4f.h>
#include<hgl/graph/AABB.h>

namespace hgl::graph
{
    class PrimitiveCreater;

    namespace inline_geometry
    {
        /**
            * 矩形创建信息(扇形/三角形条)
            */
        struct RectangleCreateInfo
        {
            RectScope2f scope;
        };//struct RectangleCreateInfo

        Primitive *CreateRectangle(PrimitiveCreater *pc,const RectangleCreateInfo *rci);

        /**
            * 创建延迟渲染用全屏平面
            */
        Primitive *CreateGBufferCompositionRectangle(PrimitiveCreater *pc);

        /**
            * 圆角矩形创建信息(扇形/线圈)
            */
        struct RoundRectangleCreateInfo:public RectangleCreateInfo
        {
            float radius;           ///<圆角半径
            uint32_t round_per;     ///<圆角精度
        };//struct RoundRectangleCreateInfo:public RectangleCreateInfo

        Primitive *CreateRoundRectangle(PrimitiveCreater *pc,const RoundRectangleCreateInfo *rci);

        /**
            * 圆形创建信息
            */
        struct CircleCreateInfo
        {
            Vector2f center;            ///<圆心坐标
            Vector2f radius;            ///<半径
            uint field_count=8;         ///<分段数量

            bool has_center;            ///<是否有圆心点

            Vector4f center_color;      ///<圆心颜色
            Vector4f border_color;      ///<边缘颜色
        };//struct CircleCreateInfo

        /**
            * 创建一个2D圆形(扇形/线圈)
            */
        Primitive *CreateCircle2D(PrimitiveCreater *pc,const CircleCreateInfo *cci);
            
        /**
            * 创建一个3D圆形(扇形/圆形，XY，Z永远为0)
            */
        Primitive *CreateCircle3D(PrimitiveCreater *pc,const CircleCreateInfo *cci);

        /**
            * 创建一个使用三角形绘制的3D圆形(扇形/圆形，XY，Z永远为0)
            */
        Primitive *CreateCircle3DByIndexTriangles(PrimitiveCreater *pc,const CircleCreateInfo *cci);

        /**
            * 平面网格创建信息<br>
            * 会创建一个在XY平面上居中的网格，单个格子尺寸为1。
            */
        struct PlaneGridCreateInfo
        {
            Size2u grid_size;       ///<格子数量

            Size2u sub_count;       ///<细分格子数量

            uint8 lum;              ///<一般线条亮度
            uint8 sub_lum;          ///<细分及边界线条亮度
        };//struct PlaneGridCreateInfo

        /**
            * 创建一个平面网格(线条)
            */
        Primitive *CreatePlaneGrid2D(PrimitiveCreater *pc,const PlaneGridCreateInfo *pgci);            //创建一个平面网格(线条)

        Primitive *CreatePlaneGrid3D(PrimitiveCreater *pc,const PlaneGridCreateInfo *pgci);

        /**
            * 创建一个平面正方形(三角形)
            */
        Primitive *CreatePlaneSqaure(PrimitiveCreater *pc);

        struct CubeCreateInfo
        {
            bool normal;
            bool tangent;
            bool tex_coord;

            enum class ColorType
            {
                NoColor=0,      ///<没有颜色
                SameColor,      ///<一个颜色
                FaceColor,      ///<每个面一个颜色(请写入6个颜色值)
                VertexColor,    ///<每个顶点一个颜色(请写入24个颜色值)

                ENUM_CLASS_RANGE(NoColor,VertexColor)
            };

            ColorType color_type;
            Vector4f color[24];

        public:

            CubeCreateInfo()
            {
                normal=false;
                tangent=false;
                tex_coord=false;

                color_type=ColorType::NoColor;
            }
        };//struct CubeCreateInfo

        /**
            * 创建一个立方体(三角形)
            */
        Primitive *CreateCube(PrimitiveCreater *pc,const CubeCreateInfo *cci);

        struct BoundingBoxCreateInfo
        {
            bool normal;

            enum class ColorType
            {
                NoColor=0,      ///<没有颜色
                SameColor,      ///<一个颜色
                VertexColor,    ///<每个顶点一个颜色(请写入8个颜色值)

                ENUM_CLASS_RANGE(NoColor,VertexColor)
            };

            ColorType color_type;
            Vector4f color[8];

        public:

            BoundingBoxCreateInfo(bool n=false,ColorType ct=ColorType::NoColor)
            {
                normal=n;

                color_type=ct;
            }
        };//struct BoundingBoxCreateInfo

        /**
            *  创建一个绑定盒(线条)
            */
        Primitive *CreateBoundingBox(PrimitiveCreater *pc,const BoundingBoxCreateInfo *cci);

        /**
            * 创建一个球心坐标为0,0,0，半径为1的球体(三角形)
            */
        Primitive *CreateSphere(PrimitiveCreater *,const uint numberSlices);

        /**
            * 创建一个穹顶(三角形)
            */
        Primitive *CreateDome(PrimitiveCreater *pc, const uint numberSlices);

        struct TorusCreateInfo
        {
            float   innerRadius,
                    outerRadius;

            uint    numberSlices,
                    numberStacks;

            Vector2f uv_scale={1.0,1.0};
        };//struct TorusCreateInfo

        /**
            * 创建一个圆环(三角形)
            */
        Primitive *CreateTorus(PrimitiveCreater *pc,const TorusCreateInfo *tci);

        struct CylinderCreateInfo
        {
            float   halfExtend,     //高度
                    radius;         //半径
            uint    numberSlices;
        };//struct CylinderCreateInfo

        /**
            * 创建一个圆柱(三角形)
            */
        Primitive *CreateCylinder(PrimitiveCreater *,const CylinderCreateInfo *cci);

        // 新增：空心圆柱（管）创建信息
        struct HollowCylinderCreateInfo
        {
            float halfExtend;           // 高度的一半
            float innerRadius;          // 内半径
            float outerRadius;          // 外半径
            uint  numberSlices;         // 圆切分段数

            // 端面（圆环）纹理平铺：径向与周向平铺次数
            float cap_radial_tiles = 1.0f;    // 从内圈到外圈的平铺次数
            float cap_angular_tiles = 1.0f;   // 沿圆周的平铺次数
        };

        /**
            * 创建一个空心圆柱（管）(三角形)
            */
        Primitive *CreateHollowCylinder(PrimitiveCreater *pc,const HollowCylinderCreateInfo *hcci);

        struct ConeCreateInfo
        {
            float   halfExtend,     //高度
                    radius;         //半径
            uint    numberSlices,   //圆切分精度
                    numberStacks;   //柱高层数
        };//struct ConeCreateInfo

        /**
            * 创建一个圆锥(三角形)
            */
        Primitive *CreateCone(PrimitiveCreater *,const ConeCreateInfo *cci);

        struct AxisCreateInfo
        {
            float size;
            Color4f color[3];

        public:

            AxisCreateInfo()
            {
                size=1.0f;
                color[0].Set(1,0,0,1);
                color[1].Set(0,1,0,1);
                color[2].Set(0,0,1,1);
            }
        };//struct AxisCreateInfo

        /**
            * 创建一个坐标线(线条)
            */
        Primitive *CreateAxis(PrimitiveCreater *pc,const AxisCreateInfo *aci);

        Primitive *CreateSqaureArray(PrimitiveCreater *pc,const uint row,const uint col);

        // 新增：HexSphere（基于二十面体细分的测地球体，输出三角网格）
        struct HexSphereCreateInfo
        {
            uint  subdivisions = 0;   // 细分次数，0=基础二十面体
            float radius       = 1.0f;
            Vector2f uv_scale  = {1.0f,1.0f};
        };

        Primitive *CreateHexSphere(PrimitiveCreater *pc,const HexSphereCreateInfo *hsci);

        // 新增：2D轮廓旋转生成3D几何体
        struct RevolutionCreateInfo
        {
            Vector2f*   profile_points;     ///<2D轮廓点数组(X为距离轴的距离，Y为沿轴的高度)
            uint        profile_count;      ///<轮廓点数量
            uint        revolution_slices;  ///<旋转分段数量
            float       start_angle;        ///<起始角度（度）
            float       sweep_angle;        ///<扫过角度（度），360为完整旋转
            Vector3f    revolution_axis;    ///<旋转轴方向（默认为Z轴(0,0,1)）
            Vector3f    revolution_center;  ///<旋转中心点（默认为原点(0,0,0)）
            bool        close_profile;      ///<是否闭合轮廓（连接首尾点）
            Vector2f    uv_scale;           ///<UV缩放

        public:
            RevolutionCreateInfo()
            {
                profile_points = nullptr;
                profile_count = 0;
                revolution_slices = 16;
                start_angle = 0.0f;
                sweep_angle = 360.0f;
                revolution_axis = Vector3f(0, 0, 1);    // Z轴向上
                revolution_center = Vector3f(0, 0, 0);
                close_profile = true;
                uv_scale = Vector2f(1.0f, 1.0f);
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
        Primitive *CreateRevolution(PrimitiveCreater *pc, const RevolutionCreateInfo *rci);

    }//namespace inline_geometry
}//namespace hgl::graph
