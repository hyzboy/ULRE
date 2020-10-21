#ifndef HGL_GRAPH_INLINE_GEOMETRY_INCLUDE
#define HGL_GRAPH_INLINE_GEOMETRY_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/math/Vector.h>
#include<hgl/type/RectScope.h>
#include<hgl/type/Color4f.h>
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

        vulkan::Renderable *CreateRenderableRectangle(vulkan::Database *db,vulkan::Material *mtl,const RectangleCreateInfo *rci);

        /**
         * 创建延迟渲染用全屏平面
         */
        vulkan::Renderable *CreateRenderableGBufferComposition(vulkan::Database *db,vulkan::Material *mtl);

        /**
         * 圆角矩形创建信息(扇形/线圈)
         */
        struct RoundRectangleCreateInfo:public RectangleCreateInfo
        {
            float radius;           ///<圆角半径
            uint32_t round_per;     ///<圆角精度
        };//struct RoundRectangleCreateInfo:public RectangleCreateInfo

        vulkan::Renderable *CreateRenderableRoundRectangle(vulkan::Database *db,vulkan::Material *mtl,const RoundRectangleCreateInfo *rci);

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
        vulkan::Renderable *CreateRenderableCircle(vulkan::Database *db,vulkan::Material *mtl,const CircleCreateInfo *cci);

        /**
         * 平面网格创建信息
         */
        struct PlaneGridCreateInfo
        {
            Vector3f coord[4];
            vec2<uint> step;

            vec2<uint> side_step;   //到边界的步数

            Color4f color;          //一般线条颜色
            Color4f side_color;     //边界线条颜色
        };//struct PlaneGridCreateInfo

        /**
         * 创建一个平面网格(线条)
         */
        vulkan::Renderable *CreateRenderablePlaneGrid(vulkan::Database *db,vulkan::Material *mtl,const PlaneGridCreateInfo *pgci);

        struct PlaneCreateInfo
        {
            Vector2f tile;

        public:

            PlaneCreateInfo()
            {
                tile.Set(1,1);
            }
        };//struct PlaneCreateInfo

        /**
         * 创建一个平面(三角形)
         */
        vulkan::Renderable *CreateRenderablePlane(vulkan::Database *db,vulkan::Material *mtl,const PlaneCreateInfo *pci);

        struct CubeCreateInfo
        {
            Vector3f center;
            Vector3f size;
            Vector2f tile;

            bool has_color;
            Vector4f color;

        public:
        
            void Set(const AABB &box)
            {
                center=box.CenterPoint().xyz();
                size=box.Size().xyz();
            }

            CubeCreateInfo()
            {
                center.Set(0,0,0);
                size.Set(1,1,1);
                tile.Set(1,1);

                has_color=false;
            }

            CubeCreateInfo(const AABB &box)
            {
                Set(box);
                tile.Set(1,1);
                
                has_color=false;
            }
        };//struct CubeCreateInfo

        /**
         * 创建一个立方体(三角形)
         */
        vulkan::Renderable *CreateRenderableCube(vulkan::Database *db,vulkan::Material *mtl,const CubeCreateInfo *cci);
        
        /**
         *  创建一个绑定盒(线条)
         */
        vulkan::Renderable *CreateRenderableBoundingBox(vulkan::Database *db,vulkan::Material *mtl,const CubeCreateInfo *cci);
        
        /**
         * 创建一个球心坐标为0,0,0，半径为1的球体(三角形)
         */
        vulkan::Renderable *CreateRenderableSphere(vulkan::Database *db,vulkan::Material *mtl,const uint numberSlices);

        struct DomeCreateInfo
        {
            float radius;
            uint numberSlices;
        };//struct DomeCreateInfo

        /**
         * 创建一个穹顶(三角形)
         */
        vulkan::Renderable *CreateRenderableDome(vulkan::Database *db,vulkan::Material *mtl, const DomeCreateInfo *);

        struct TorusCreateInfo
        {
            float   innerRadius,
                    outerRadius;

            uint    numberSlices,
                    numberStacks;
        };//struct TorusCreateInfo
        
        /**
         * 创建一个圆环(三角形)
         */
        vulkan::Renderable *CreateRenderableTorus(vulkan::Database *db,vulkan::Material *mtl,const TorusCreateInfo *tci);

        struct CylinderCreateInfo
        {
            float   halfExtend,     //高度
                    radius;         //半径
            uint    numberSlices;
        };//struct CylinderCreateInfo

        /**
         * 创建一个圆柱(三角形)
         */
        vulkan::Renderable *CreateRenderableCylinder(vulkan::Database *db,vulkan::Material *mtl,const CylinderCreateInfo *cci);

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
        vulkan::Renderable *CreateRenderableCone(vulkan::Database *db,vulkan::Material *mtl,const ConeCreateInfo *cci);
        
        struct AxisCreateInfo
        {
            Vector3f root;
            Vector3f size;
            Color4f color[3];

        public:

            void RestartColor()
            {
                color[0].Set(1,0,0,1);
                color[1].Set(0,1,0,1);
                color[2].Set(0,0,1,1);
            }

            void Set(const AABB &box)
            {
                root=box.CenterPoint().xyz();
                size=box.HalfSize().xyz();
            }

            AxisCreateInfo()
            {
                root.Set(0,0,0);
                size.Set(1,1,1);
                RestartColor();
            }

            AxisCreateInfo(const AABB &box)
            {
                Set(box);
                RestartColor();
            }
        };//struct AxisCreateInfo

        /**
         * 创建一个坐标线(线条)
         */
        vulkan::Renderable *CreateRenderableAxis(vulkan::Database *db,vulkan::Material *mtl,const AxisCreateInfo *aci);
    }//namespace graph
};//namespace hgl
#endif//HGL_GRAPH_INLINE_GEOMETRY_INCLUDE
