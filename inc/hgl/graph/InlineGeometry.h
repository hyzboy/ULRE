#ifndef HGL_GRAPH_INLINE_GEOMETRY_INCLUDE
#define HGL_GRAPH_INLINE_GEOMETRY_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/math/Vector.h>
#include<hgl/type/RectScope.h>
#include<hgl/type/Color4f.h>
#include<hgl/graph/AABB.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 矩形创建信息(扇形/三角形条)
         */
        struct RectangleCreateInfo
        {
            RectScope2f scope;
        };//struct RectangleCreateInfo

        Renderable *CreateRenderableRectangle(RenderResource *db,const VAB *vab,const RectangleCreateInfo *rci);

        /**
         * 创建延迟渲染用全屏平面
         */
        Renderable *CreateRenderableGBufferComposition(RenderResource *db,const VAB *vab);

        /**
         * 圆角矩形创建信息(扇形/线圈)
         */
        struct RoundRectangleCreateInfo:public RectangleCreateInfo
        {
            float radius;           ///<圆角半径
            uint32_t round_per;     ///<圆角精度
        };//struct RoundRectangleCreateInfo:public RectangleCreateInfo

        Renderable *CreateRenderableRoundRectangle(RenderResource *db,const VAB *vab,const RoundRectangleCreateInfo *rci);

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
        Renderable *CreateRenderableCircle(RenderResource *db,const VAB *vab,const CircleCreateInfo *cci);

        /**
         * 平面网格创建信息
         */
        struct PlaneGridCreateInfo
        {
            Vector3f coord[4];
            Vector2u step;

            Vector2u side_step;   //到边界的步数

            Color4f color;          //一般线条颜色
            Color4f side_color;     //边界线条颜色
        };//struct PlaneGridCreateInfo

        /**
         * 创建一个平面网格(线条)
         */
        Renderable *CreateRenderablePlaneGrid(RenderResource *db,const VAB *vab,const PlaneGridCreateInfo *pgci);

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
        Renderable *CreateRenderablePlane(RenderResource *db,const VAB *vab,const PlaneCreateInfo *pci);

        struct CubeCreateInfo
        {
            bool has_normal;
            bool has_tangent;
            bool has_tex_coord;

            bool has_color;
            Vector4f color;

        public:

            CubeCreateInfo()
            {
                has_normal=false;
                has_tangent=false;
                has_tex_coord=false;

                has_color=false;
            }
        };//struct CubeCreateInfo

        /**
         * 创建一个立方体(三角形)
         */
        Renderable *CreateRenderableCube(RenderResource *db,const VAB *vab,const CubeCreateInfo *cci);

        /**
         *  创建一个绑定盒(线条)
         */
        Renderable *CreateRenderableBoundingBox(RenderResource *db,const VAB *vab,const CubeCreateInfo *cci);

        /**
         * 创建一个球心坐标为0,0,0，半径为1的球体(三角形)
         */
        Renderable *CreateRenderableSphere(RenderResource *db,const VAB *vab,const uint numberSlices);

        struct DomeCreateInfo
        {
            float radius;
            uint numberSlices;
        };//struct DomeCreateInfo

        /**
         * 创建一个穹顶(三角形)
         */
        Renderable *CreateRenderableDome(RenderResource *db,const VAB *vab, const DomeCreateInfo *);

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
        Renderable *CreateRenderableTorus(RenderResource *db,const VAB *vab,const TorusCreateInfo *tci);

        struct CylinderCreateInfo
        {
            float   halfExtend,     //高度
                    radius;         //半径
            uint    numberSlices;
        };//struct CylinderCreateInfo

        /**
         * 创建一个圆柱(三角形)
         */
        Renderable *CreateRenderableCylinder(RenderResource *db,const VAB *vab,const CylinderCreateInfo *cci);

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
        Renderable *CreateRenderableCone(RenderResource *db,const VAB *vab,const ConeCreateInfo *cci);

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
        Renderable *CreateRenderableAxis(RenderResource *db,const VAB *vab,const AxisCreateInfo *aci);
    }//namespace graph
};//namespace hgl
#endif//HGL_GRAPH_INLINE_GEOMETRY_INCLUDE
