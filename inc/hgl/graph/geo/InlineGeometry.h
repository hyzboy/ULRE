#pragma once

#include<hgl/graph/VK.h>
#include<hgl/math/Vector.h>
#include<hgl/type/RectScope.h>
#include<hgl/type/Size2.h>
#include<hgl/color/Color4f.h>
#include<hgl/math/geometry/AABB.h>

namespace hgl::graph
{
    class GeometryCreater;

    namespace inline_geometry
    {
        /**
            * 矩形创建信息(扇形/三角形条)
            */
        struct RectangleCreateInfo
        {
            RectScope2f scope;
        };//struct RectangleCreateInfo

        Geometry *CreateRectangle(GeometryCreater *pc,const RectangleCreateInfo *rci);

        /**
            * 创建延迟渲染用全屏平面
            */
        Geometry *CreateGBufferCompositionRectangle(GeometryCreater *pc);

        /**
            * 圆角矩形创建信息(扇形/线圈)
            */
        struct RoundRectangleCreateInfo:public RectangleCreateInfo
        {
            float radius;           ///<圆角半径
            uint32_t round_per;     ///<圆角精度
        };//struct RoundRectangleCreateInfo:public RectangleCreateInfo

        Geometry *CreateRoundRectangle(GeometryCreater *pc,const RoundRectangleCreateInfo *rci);

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
        Geometry *CreateCircle2D(GeometryCreater *pc,const CircleCreateInfo *cci);
            
        /**
            * 创建一个3D圆形(扇形/圆形，XY，Z永远为0)
            */
        Geometry *CreateCircle3D(GeometryCreater *pc,const CircleCreateInfo *cci);

        /**
            * 创建一个使用三角形绘制的3D圆形(扇形/圆形，XY，Z永远为0)
            */
        Geometry *CreateCircle3DByIndexTriangles(GeometryCreater *pc,const CircleCreateInfo *cci);

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
        Geometry *CreatePlaneGrid2D(GeometryCreater *pc,const PlaneGridCreateInfo *pgci);            //创建一个平面网格(线条)

        Geometry *CreatePlaneGrid3D(GeometryCreater *pc,const PlaneGridCreateInfo *pgci);

        /**
            * 创建一个平面正方形(三角形)
            */
        Geometry *CreatePlaneSqaure(GeometryCreater *pc);

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
        Geometry *CreateCube(GeometryCreater *pc,const CubeCreateInfo *cci);

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
        Geometry *CreateBoundingBox(GeometryCreater *pc,const BoundingBoxCreateInfo *cci);

        /**
            * 创建一个球心坐标为0,0,0，半径为1的球体(三角形)
            */
        Geometry *CreateSphere(GeometryCreater *,const uint numberSlices);

        /**
            * 创建一个穹顶(三角形)
            */
        Geometry *CreateDome(GeometryCreater *pc, const uint numberSlices);

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
        Geometry *CreateTorus(GeometryCreater *pc,const TorusCreateInfo *tci);

        struct CylinderCreateInfo
        {
            float   halfExtend,     //高度
                    radius;         //半径
            uint    numberSlices;
        };//struct CylinderCreateInfo

        /**
            * 创建一个圆柱(三角形)
            */
        Geometry *CreateCylinder(GeometryCreater *,const CylinderCreateInfo *cci);

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
        Geometry *CreateHollowCylinder(GeometryCreater *pc,const HollowCylinderCreateInfo *hcci);

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
        Geometry *CreateCone(GeometryCreater *,const ConeCreateInfo *cci);

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
        Geometry *CreateAxis(GeometryCreater *pc,const AxisCreateInfo *aci);

        Geometry *CreateSqaureArray(GeometryCreater *pc,const uint row,const uint col);

        // 新增：HexSphere（基于二十面体细分的测地球体，输出三角网格）
        struct HexSphereCreateInfo
        {
            uint  subdivisions = 0;   // 细分次数，0=基础二十面体
            float radius       = 1.0f;
            Vector2f uv_scale  = {1.0f,1.0f};
        };

        Geometry *CreateHexSphere(GeometryCreater *pc,const HexSphereCreateInfo *hsci);

        // 新增：胶囊体创建信息
        struct CapsuleCreateInfo
        {
            float halfHeight;    // 中间圆柱部分的半高（不包含半球）
            float radius;        // 半球和圆柱半径
            uint  numberSlices;  // 圆周分段数
            uint  numberStacks;  // 半球分段数（每个半球）

            CapsuleCreateInfo()
            {
                halfHeight = 1.0f;
                radius = 0.5f;
                numberSlices = 16;
                numberStacks = 8;
            }
        };

        /**
         * 创建一个胶囊体(三角形)
         */
        Geometry *CreateCapsule(GeometryCreater *pc,const CapsuleCreateInfo *cci);
         
        // 新增：Tapered 胶囊体创建信息（上/下半球半径可不相同，中间为圆台）
        struct TaperedCapsuleCreateInfo
        {
            float halfHeight;     // 中间部分半高（从 -halfHeight 到 +halfHeight）
            float bottomRadius;   // 底部（-halfHeight 端）半径
            float topRadius;      // 顶部（+halfHeight 端）半径
            uint  numberSlices;   // 圆周分段数
            uint  numberStacks;   // 半球分段数（每个半球）

            TaperedCapsuleCreateInfo()
            {
                halfHeight = 1.0f;
                bottomRadius = 0.5f;
                topRadius = 0.5f;
                numberSlices = 16;
                numberStacks = 8;
            }
        };

        /**
         * 创建一个可锥缩的胶囊体(三角形)
         */
        Geometry *CreateTaperedCapsule(GeometryCreater *pc,const TaperedCapsuleCreateInfo *tcci);
         
    }//namespace inline_geometry
}//namespace hgl::graph
