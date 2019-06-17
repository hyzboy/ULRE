#ifndef HGL_GRAPH_INLINE_GEOMETRY_INCLUDE
#define HGL_GRAPH_INLINE_GEOMETRY_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/math/Vector.h>
#include<hgl/type/RectScope.h>
#include<hgl/type/Color4f.h>
namespace hgl
{
    namespace graph
    {
        class SceneDB;

        /**
         * 矩形创建信息
         */
        struct RectangleCreateInfo
        {
            RectScope2f scope;
        };//struct RectangleCreateInfo

        vulkan::Renderable *CreateRectangle(SceneDB *db,vulkan::Material *mtl,const RectangleCreateInfo *rci);

        /**
         * 圆角矩形创建信息
         */
        struct RoundRectangleCreateInfo:public RectangleCreateInfo
        {
            float radius;           ///<圆角半径
            uint32_t round_per;     ///<圆角精度
        };//struct RoundRectangleCreateInfo:public RectangleCreateInfo

        vulkan::Renderable *CreateRoundRectangle(SceneDB *db,vulkan::Material *mtl,const RoundRectangleCreateInfo *rci);

        /**
         * 圆形创建信息
         */
        struct CircleCreateInfo
        {
            Vector2f center;            ///<圆心坐标
            Vector2f radius;            ///<半径
            uint field_count;           ///<分段次数
        };//struct CircleCreateInfo

        vulkan::Renderable *CreateCircle(SceneDB *db,vulkan::Material *mtl,const CircleCreateInfo *rci);

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

        vulkan::Renderable *CreatePlaneGrid(SceneDB *db,vulkan::Material *mtl,const PlaneGridCreateInfo *pgci);

        struct PlaneCreateInfo
        {
            Vector2f tile;
        };//struct PlaneCreateInfo

        vulkan::Renderable *CreatePlane(SceneDB *db,vulkan::Material *mtl,const PlaneCreateInfo *pci);

        struct CubeCreateInfo
        {
            Vector2f tile;
        };//struct CubeCreateInfo

        /**
         * 创建一个中心坐标为0,0,0，长宽高为1的立方体
         */
        vulkan::Renderable *CreateCube(SceneDB *db,vulkan::Material *mtl,const CubeCreateInfo *cci);
        
        /**
         * 创建一个球心坐标为0,0,0，半径为1的球体
         */
        vulkan::Renderable *CreateRenderableSphere(SceneDB *db,vulkan::Material *mtl,const uint numberSlices);
    }//namespace graph
};//namespace hgl
#endif//HGL_GRAPH_INLINE_GEOMETRY_INCLUDE
