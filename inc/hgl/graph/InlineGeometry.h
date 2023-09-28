#ifndef HGL_GRAPH_INLINE_GEOMETRY_INCLUDE
#define HGL_GRAPH_INLINE_GEOMETRY_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/math/Vector.h>
#include<hgl/type/RectScope.h>
#include<hgl/type/Size2.h>
#include<hgl/color/Color4f.h>
#include<hgl/graph/AABB.h>
namespace hgl
{
    namespace graph
    {
        namespace inline_geometry
        {
            /**
             * 矩形创建信息(扇形/三角形条)
             */
            struct RectangleCreateInfo
            {
                RectScope2f scope;
            };//struct RectangleCreateInfo

            Primitive *CreateRectangle(RenderResource *db,const VIL *vil,const RectangleCreateInfo *rci);

            /**
             * 创建延迟渲染用全屏平面
             */
            Primitive *CreateGBufferCompositionRectangle(RenderResource *db,const VIL *vil);

            /**
             * 圆角矩形创建信息(扇形/线圈)
             */
            struct RoundRectangleCreateInfo:public RectangleCreateInfo
            {
                float radius;           ///<圆角半径
                uint32_t round_per;     ///<圆角精度
            };//struct RoundRectangleCreateInfo:public RectangleCreateInfo

            Primitive *CreateRoundRectangle(RenderResource *db,const VIL *vil,const RoundRectangleCreateInfo *rci);

            /**
             * 圆形创建信息
             */
            struct CircleCreateInfo
            {
                Vector2f center;            ///<圆心坐标
                Vector2f radius;            ///<半径
                uint field_count=8;         ///<分段次数

                bool has_color  =false;

                Vector4f center_color;      ///<圆心颜色
                Vector4f border_color;      ///<边缘颜色
            };//struct CircleCreateInfo

            /**
             * 创建一个2D圆形(扇形/线圈)
             */
            Primitive *CreateCircle(RenderResource *db,const VIL *vil,const CircleCreateInfo *cci);

            /**
             * 平面网格创建信息<br>
             * 会创建一个在XY平面上居中的网格，单个格子尺寸为1。
             */
            struct PlaneGridCreateInfo
            {
                Size2u grid_size;       ///<格子数量

                Size2u sub_count;       ///<细分格子数量

                float lum;              ///<一般线条颜色
                float sub_lum;          ///<细分及边界线条颜色
            };//struct PlaneGridCreateInfo

            /**
             * 创建一个平面网格(线条)
             */
            Primitive *CreatePlaneGrid(RenderResource *db,const VIL *vil,const PlaneGridCreateInfo *pgci);

            struct PlaneCreateInfo
            {
                Vector2f tile;

            public:

                PlaneCreateInfo()
                {
                    tile.x=1.0f;
                    tile.y=1.0f;
                }
            };//struct PlaneCreateInfo

            /**
             * 创建一个平面(三角形)
             */
            Primitive *CreatePlane(RenderResource *db,const VIL *vil,const PlaneCreateInfo *pci);

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
            Primitive *CreateCube(RenderResource *db,const VIL *vil,const CubeCreateInfo *cci);

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

                BoundingBoxCreateInfo()
                {
                    normal=false;

                    color_type=ColorType::NoColor;
                }
            };//struct BoundingBoxCreateInfo

            /**
             *  创建一个绑定盒(线条)
             */
            Primitive *CreateBoundingBox(RenderResource *db,const VIL *vil,const BoundingBoxCreateInfo *cci);

            /**
             * 创建一个球心坐标为0,0,0，半径为1的球体(三角形)
             */
            Primitive *CreateSphere(RenderResource *db,const VIL *vil,const uint numberSlices);

            /**
             * 创建一个穹顶(三角形)
             */
            Primitive *CreateDome(RenderResource *db,const VIL *vil, const uint numberSlices);

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
            Primitive *CreateTorus(RenderResource *db,const VIL *vil,const TorusCreateInfo *tci);

            struct CylinderCreateInfo
            {
                float   halfExtend,     //高度
                        radius;         //半径
                uint    numberSlices;
            };//struct CylinderCreateInfo

            /**
             * 创建一个圆柱(三角形)
             */
            Primitive *CreateCylinder(RenderResource *db,const VIL *vil,const CylinderCreateInfo *cci);

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
            Primitive *CreateCone(RenderResource *db,const VIL *vil,const ConeCreateInfo *cci);

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
            Primitive *CreateAxis(RenderResource *db,const VIL *vil,const AxisCreateInfo *aci);
        }//namespace inline_geometry
    }//namespace graph
};//namespace hgl
#endif//HGL_GRAPH_INLINE_GEOMETRY_INCLUDE
