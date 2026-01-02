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
        using namespace hgl::math;

        /**
            * 矩形创建信息(扇形/三角形条)
            */
        struct RectangleCreateInfo
        {
            RectScope2f scope;
            uint segments_x = 1;    ///< X方向细分数量
            uint segments_y = 1;    ///< Y方向细分数量
            bool normal = false;    ///< 是否生成法线
            bool tex_coord = true;  ///< 是否生成纹理坐标
            
            RectangleCreateInfo() = default;
            
            RectangleCreateInfo(const RectScope2f& s, uint seg_x = 1, uint seg_y = 1)
                : scope(s), segments_x(seg_x), segments_y(seg_y) {}
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
            uint segments_x = 1;    ///< X方向细分数量
            uint segments_y = 1;    ///< Y方向细分数量
            uint segments_z = 1;    ///< Z方向细分数量
            
            bool normal = true;
            bool tangent = false;
            bool tex_coord = true;

            enum class ColorType
            {
                NoColor=0,      ///<没有颜色
                SameColor,      ///<一个颜色
                FaceColor,      ///<每个面一个颜色(请写入6个颜色值)

                ENUM_CLASS_RANGE(NoColor,FaceColor)
            };

            ColorType color_type = ColorType::NoColor;
            Vector4f color[6];

        public:

            CubeCreateInfo() = default;
            
            CubeCreateInfo(uint seg_x, uint seg_y, uint seg_z)
                : segments_x(seg_x), segments_y(seg_y), segments_z(seg_z) {}
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
                color[0].set(1,0,0,1);
                color[1].set(0,1,0,1);
                color[2].set(0,0,1,1);
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
        
        // 新增：截锥体创建信息
        struct FrustumCreateInfo
        {
            float bottom_radius = 1.0f;  // 底面半径
            float top_radius = 0.5f;     // 顶面半径
            float height = 2.0f;         // 高度
            uint  numberSlices = 16;     // 圆周分段数
            bool  generate_caps = true;  // 是否生成顶底面
        };

        /**
         * 创建一个截锥体(三角形)
         * 用途：视锥体可视化、灯光范围、杯子建模
         */
        Geometry *CreateFrustum(GeometryCreater *pc, const FrustumCreateInfo *fci);

        // 新增：箭头创建信息
        enum class ArrowCrossSection
        {
            Circular,   // 圆形截面
            Square      // 方形截面
        };

        enum class ArrowTailStyle
        {
            Flat,           // 平尾
            Concave         // 倒三角凹陷尾
        };

        struct ArrowCreateInfo
        {
            float shaft_radius = 0.1f;           // 杆部半径（或半宽，对于方形）
            float shaft_length = 2.0f;           // 杆部长度
            float head_radius = 0.3f;            // 箭头半径（或半宽，对于方形）
            float head_length = 0.5f;            // 箭头长度
            uint  numberSlices = 16;             // 圆周分段数（仅圆形截面使用）
            ArrowCrossSection cross_section = ArrowCrossSection::Circular;  // 截面形状
            ArrowTailStyle tail_style = ArrowTailStyle::Flat;  // 尾部样式
            float tail_concave_depth = 0.2f;     // 尾部凹陷深度（仅Concave使用）
        };

        /**
         * 创建一个箭头(三角形)
         * 用途：调试可视化、方向指示、矢量显示
         */
        Geometry *CreateArrow(GeometryCreater *pc, const ArrowCreateInfo *aci);

        // 新增：圆角立方体创建信息
        struct RoundedBoxCreateInfo
        {
            Vector3f size = Vector3f(1.0f, 1.0f, 1.0f);  // 盒子尺寸
            float    edge_radius = 0.1f;                 // 边缘圆角半径
            uint     edge_segments = 4;                  // 圆角细分段数
            uint     face_segments = 1;                  // 平面细分段数（可选）
        };

        /**
         * 创建一个圆角立方体(三角形)
         * 用途：现代UI元素、物理碰撞体、柔和建模
         */
        Geometry *CreateRoundedBox(GeometryCreater *pc, const RoundedBoxCreateInfo *rbci);

        // 新增：管道弯头创建信息
        struct PipeElbowCreateInfo
        {
            float inner_radius = 0.3f;     // 内半径
            float outer_radius = 0.5f;     // 外半径
            float bend_angle = 90.0f;      // 弯曲角度（度）
            float bend_radius = 1.0f;      // 弯曲半径（中心线到弯曲中心的距离）
            uint  pipe_segments = 16;      // 管道截面圆周分段数
            uint  bend_segments = 16;      // 沿弯曲路径的分段数
            bool  generate_caps = true;    // 是否生成端面
        };

        /**
         * 创建一个管道弯头(三角形)
         * 用途：管线系统、工业场景、建筑可视化
         */
        Geometry *CreatePipeElbow(GeometryCreater *pc, const PipeElbowCreateInfo *peci);

        // 新增：螺旋体创建信息
        struct HelixCreateInfo
        {
            float radius = 1.0f;           // 螺旋半径
            float height = 4.0f;           // 总高度
            float coil_radius = 0.1f;      // 管道/线圈半径
            uint  turns = 4;               // 圈数
            uint  segments_per_turn = 32;  // 每圈分段数
            uint  coil_segments = 8;       // 管道截面分段数（圆形截面）
            bool  solid = true;            // true=实心管道, false=线框
        };

        /**
         * 创建一个螺旋体(三角形)
         * 用途：弹簧、楼梯扶手、DNA双螺旋、装饰元素
         */
        Geometry *CreateHelix(GeometryCreater *pc, const HelixCreateInfo *hci);

        // 新增：实心齿轮创建信息
        struct SolidGearCreateInfo
        {
            float outer_radius = 1.0f;      // 外半径（齿顶圆半径）
            float root_radius = 0.8f;       // 根圆半径（齿根圆半径）
            float thickness = 0.2f;         // 齿轮厚度
            uint  tooth_count = 12;         // 齿数
            float tooth_width_ratio = 0.5f; // 齿宽比例（0-1，齿宽占齿距的比例）
            uint  segments_per_tooth = 2;   // 每个齿的细分段数
        };

        /**
         * 创建一个实心齿轮(三角形)
         * 用途：机械可视化、工业场景、齿轮动画
         */
        Geometry *CreateSolidGear(GeometryCreater *pc, const SolidGearCreateInfo *sgci);

        // 新增：空心齿轮创建信息
        struct HollowGearCreateInfo
        {
            float outer_radius = 1.0f;      // 外半径（齿顶圆半径）
            float root_radius = 0.8f;       // 根圆半径（齿根圆半径）
            float inner_radius = 0.3f;      // 内孔半径
            float thickness = 0.2f;         // 齿轮厚度
            uint  tooth_count = 12;         // 齿数
            float tooth_width_ratio = 0.5f; // 齿宽比例（0-1，齿宽占齿距的比例）
            uint  segments_per_tooth = 2;   // 每个齿的细分段数
        };

        /**
         * 创建一个空心齿轮(三角形)
         * 用途：机械可视化、工业场景、齿轮传动系统
         */
        Geometry *CreateHollowGear(GeometryCreater *pc, const HollowGearCreateInfo *hgci);

        // 新增：心形几何体创建信息
        struct HeartCreateInfo
        {
            float size = 1.0f;              // 心形整体大小
            float depth = 0.3f;             // 心形厚度（Z方向）
            uint  segments = 32;            // 心形轮廓分段数（越大越平滑）
            uint  depth_segments = 2;       // 厚度方向的分段数
        };

        /**
         * 创建一个心形几何体(三角形)
         * 用途：装饰元素、游戏道具、UI图标、情人节主题
         */
        Geometry *CreateHeart(GeometryCreater *pc, const HeartCreateInfo *hci);

        // 新增：星形几何体创建信息
        struct StarCreateInfo
        {
            float outer_radius = 1.0f;      // 外半径（尖端）
            float inner_radius = 0.4f;      // 内半径（凹陷）
            float depth = 0.2f;             // 星形厚度（Z方向）
            uint  points = 5;               // 星角数量
            uint  segments_per_point = 1;   // 每个角的细分段数
        };

        /**
         * 创建一个星形几何体(三角形)
         * 用途：装饰元素、评分系统、徽章、UI图标
         */
        Geometry *CreateStar(GeometryCreater *pc, const StarCreateInfo *sci);

        // 新增：正多边形几何体创建信息
        struct PolygonCreateInfo
        {
            float radius = 1.0f;            // 外接圆半径
            float depth = 0.2f;             // 多边形厚度（Z方向）
            uint  sides = 6;                // 边数（>=3）
            bool  centered = true;          // true=中心在原点，false=顶点在原点
        };

        /**
         * 创建一个正多边形几何体(三角形)
         * 用途：基础形状、UI元素、平台、装饰
         */
        Geometry *CreatePolygon(GeometryCreater *pc, const PolygonCreateInfo *pci);

        // 新增：楔形/三角棱柱创建信息
        struct WedgeCreateInfo
        {
            float width = 1.0f;             // 底边宽度（X方向）
            float depth = 1.0f;             // 深度（Y方向）
            float height = 1.0f;            // 高度（Z方向）
            bool  slope_direction_x = true; // true=沿X方向倾斜，false=沿Y方向倾斜
        };

        /**
         * 创建一个楔形/三角棱柱(三角形)
         * 用途：斜坡、建筑元素、物理斜面
         */
        Geometry *CreateWedge(GeometryCreater *pc, const WedgeCreateInfo *wci);

        // 新增：水滴形几何体创建信息
        struct TeardropCreateInfo
        {
            float radius = 0.5f;            // 圆形部分半径
            float length = 1.5f;            // 总长度（从圆心到尖端）
            uint  segments = 32;            // 轮廓分段数
            uint  radial_segments = 16;     // 径向分段数（旋转体）
        };

        /**
         * 创建一个水滴形几何体(三角形)
         * 用途：液体模拟、装饰元素、自然形状
         */
        Geometry *CreateTeardrop(GeometryCreater *pc, const TeardropCreateInfo *tci);

        // 新增：蛋形/椭球体创建信息
        struct EggCreateInfo
        {
            float radius = 0.5f;            // 基础半径
            float length_ratio = 1.5f;      // 长度与半径的比例
            float bottom_scale = 1.0f;      // 底部缩放（1.0=对称，<1=底部更尖）
            uint  slices = 32;              // 经线分段数
            uint  stacks = 16;              // 纬线分段数
        };

        /**
         * 创建一个蛋形/椭球体(三角形)
         * 用途：自然物体、装饰元素、复活节主题
         */
        Geometry *CreateEgg(GeometryCreater *pc, const EggCreateInfo *eci);

        // 新增：月牙形几何体创建信息
        struct CrescentCreateInfo
        {
            float outer_radius = 1.0f;      // 外圆半径
            float inner_radius = 0.8f;      // 内圆半径
            float thickness = 0.2f;         // 厚度（Z方向）
            float offset = 0.3f;            // 内圆偏移距离
            uint  segments = 32;            // 圆弧分段数
        };

        /**
         * 创建一个月牙形几何体(三角形)
         * 用途：月亮图标、装饰元素、UI设计
         */
        Geometry *CreateCrescent(GeometryCreater *pc, const CrescentCreateInfo *cci);

        // 新增：直线楼梯创建信息
        struct StraightStairsCreateInfo
        {
            float step_width = 1.0f;        // 台阶宽度
            float step_depth = 0.3f;        // 台阶深度
            float step_height = 0.2f;       // 台阶高度
            uint  step_count = 10;          // 台阶数量
            float side_thickness = 0.05f;   // 侧板厚度
            bool  generate_sides = true;    // 是否生成侧板
        };

        /**
         * 创建一个直线楼梯(三角形)
         * 用途：建筑可视化、关卡设计、室内场景
         */
        Geometry *CreateStraightStairs(GeometryCreater *pc, const StraightStairsCreateInfo *ssci);

        // 新增：螺旋楼梯创建信息
        struct SpiralStairsCreateInfo
        {
            float inner_radius = 0.5f;      // 内半径（中心柱）
            float outer_radius = 2.0f;      // 外半径
            float step_height = 0.2f;       // 每级台阶高度
            float total_angle = 360.0f;     // 总旋转角度（度）
            uint  step_count = 12;          // 台阶数量
            bool  clockwise = true;         // true=顺时针, false=逆时针
        };

        /**
         * 创建一个螺旋楼梯(三角形)
         * 用途：塔楼、螺旋建筑、现代设计
         */
        Geometry *CreateSpiralStairs(GeometryCreater *pc, const SpiralStairsCreateInfo *ssci);

        // 新增：拱形创建信息
        struct ArchCreateInfo
        {
            float width = 2.0f;             // 拱门宽度
            float height = 3.0f;            // 拱门高度
            float depth = 0.5f;             // 拱门深度
            float thickness = 0.3f;         // 墙体厚度
            uint  segments = 16;            // 弧线分段数
            bool  pointed = false;          // true=尖拱, false=圆拱
        };

        /**
         * 创建一个拱形(三角形)
         * 用途：建筑入口、装饰拱门、哥特式建筑
         */
        Geometry *CreateArch(GeometryCreater *pc, const ArchCreateInfo *aci);

        // 新增：直管创建信息
        struct TubeCreateInfo
        {
            float length = 2.0f;            // 管道长度
            float outer_radius = 0.3f;      // 外半径
            float inner_radius = 0.25f;     // 内半径
            uint  segments = 16;            // 圆周分段数
            bool  generate_caps = true;     // 是否生成端面
        };

        /**
         * 创建一个直管(三角形)
         * 用途：管道系统、工业场景、连接件
         */
        Geometry *CreateTube(GeometryCreater *pc, const TubeCreateInfo *tci);

        // 新增：链节创建信息
        struct ChainLinkCreateInfo
        {
            float major_radius = 0.5f;      // 链节长轴半径
            float minor_radius = 0.3f;      // 链节短轴半径
            float wire_radius = 0.05f;      // 线材半径
            uint  major_segments = 16;      // 长轴分段数
            uint  minor_segments = 8;       // 短轴分段数
        };

        /**
         * 创建一个链节(三角形)
         * 用途：链条、装饰、工业元素
         */
        Geometry *CreateChainLink(GeometryCreater *pc, const ChainLinkCreateInfo *clci);

        // 新增：六角螺栓创建信息
        struct BoltCreateInfo
        {
            float head_radius = 0.5f;       // 螺栓头外接圆半径
            float head_height = 0.3f;       // 螺栓头高度
            float shaft_radius = 0.3f;      // 螺杆半径
            float shaft_length = 2.0f;      // 螺杆长度
            uint  head_segments = 6;        // 螺栓头边数（通常为6）
            bool  generate_threads = false; // 是否生成螺纹（简化版）
        };

        /**
         * 创建一个六角螺栓(三角形)
         * 用途：机械场景、工业可视化、装配模拟
         */
        Geometry *CreateBolt(GeometryCreater *pc, const BoltCreateInfo *bci);

        // 新增：六角螺母创建信息
        struct NutCreateInfo
        {
            float outer_radius = 0.5f;      // 外接圆半径
            float inner_radius = 0.3f;      // 内孔半径
            float height = 0.3f;            // 螺母高度
            uint  sides = 6;                // 边数（通常为6）
        };

        /**
         * 创建一个六角螺母(三角形)
         * 用途：机械场景、工业可视化、装配模拟
         */
        Geometry *CreateNut(GeometryCreater *pc, const NutCreateInfo *nci);

        // 新增：钻石创建信息
        struct DiamondCreateInfo
        {
            float radius = 1.0f;            // 钻石外接圆半径
            float height = 1.5f;            // 钻石总高度
            float top_ratio = 0.4f;         // 顶部高度占比 (0.0-1.0)
            float table_ratio = 0.5f;       // 台面大小比例 (0.0-1.0)
            float pavilion_ratio = 0.7f;    // 亭部宽度比例 (0.0-1.0)
            uint  facets = 8;               // 刻面数（围绕Y轴的分段数）
        };

        /**
         * 创建一个钻石(三角形)
         * 用途：宝石可视化、装饰元素、游戏道具
         */
        Geometry *CreateDiamond(GeometryCreater *pc, const DiamondCreateInfo *dci);

        // 新增：水晶创建信息
        struct CrystalCreateInfo
        {
            float base_radius = 0.5f;       // 底部半径
            float height = 2.0f;            // 总高度
            float tip_sharpness = 0.1f;     // 尖端锐度 (0.0-1.0, 越小越尖)
            uint  sides = 6;                // 侧面数
            uint  segments = 4;             // 沿高度的分段数
            float irregularity = 0.0f;      // 不规则度 (0.0-1.0)
        };

        /**
         * 创建一个水晶(三角形)
         * 用途：游戏资源、魔法元素、装饰品
         */
        Geometry *CreateCrystal(GeometryCreater *pc, const CrystalCreateInfo *cci);

        // 新增：十字架创建信息
        struct CrossCreateInfo
        {
            float vertical_length = 2.0f;   // 竖杆长度
            float horizontal_length = 1.5f; // 横杆长度
            float thickness = 0.2f;         // 厚度
            float depth = 0.2f;             // 深度（Z轴）
            float horizontal_offset = 0.7f; // 横杆位置（从顶部往下）
        };

        /**
         * 创建一个十字架(三角形)
         * 用途：宗教符号、游戏标记、UI图标、装饰元素
         */
        Geometry *CreateCross(GeometryCreater *pc, const CrossCreateInfo *cci);

        // 新增：丝带创建信息
        struct RibbonCreateInfo
        {
            float length = 3.0f;            // 丝带长度
            float width = 0.5f;             // 丝带宽度
            float thickness = 0.02f;        // 丝带厚度
            uint  segments = 32;            // 沿长度的分段数
            float twist_turns = 1.0f;       // 扭转圈数
            float wave_amplitude = 0.0f;    // 波浪幅度 (0.0表示无波浪)
            float wave_frequency = 2.0f;    // 波浪频率
        };

        /**
         * 创建一个丝带(三角形)
         * 用途：装饰元素、飘带、奖章、礼品包装
         */
        Geometry *CreateRibbon(GeometryCreater *pc, const RibbonCreateInfo *rci);

        struct PhoneShapedBoxCreateInfo
        {
            Vector3f size = Vector3f(1.0f, 1.0f, 0.1f);  // 盒子尺寸 (宽,高,厚)
            float    xy_edge_radius = 0.1f;              // XY平面边缘圆角半径 (大圆角)
            float    z_edge_radius = 0.01f;              // Z轴边缘圆角半径 (小圆角)
            uint     xy_edge_segments = 8;               // XY平面圆角细分段数
            uint     z_edge_segments = 2;                // Z轴圆角细分段数
        };

        /**
         * 创建一个手机形状的盒子(三角形)
         * 用途：手机造型、平板设备、类iPhone设计
         * 特点：XY平面大圆角，Z轴方向小圆角，类似现代智能手机外形
         */
        Geometry *CreatePhoneShapedBox(GeometryCreater *pc, const PhoneShapedBoxCreateInfo *psci);

        struct TorusKnotCreateInfo
        {
            float major_radius = 3.0f;      // 主半径 (大圆环半径)
            float minor_radius = 1.0f;      // 次半径 (管道半径)
            uint  p = 2;                    // P参数 (绕大圆环的圈数)
            uint  q = 3;                    // Q参数 (绕小圆环的圈数)
            uint  major_segments = 128;     // 沿路径的分段数
            uint  minor_segments = 16;      // 管道圆周分段数
        };

        /**
         * 创建一个环面纽结(三角形)
         * 用途：数学可视化、装饰、艺术设计
         * 特点：参数化的环面纽结曲线，扫掠圆形截面
         */
        Geometry *CreateTorusKnot(GeometryCreater *pc, const TorusKnotCreateInfo *tkci);

        struct MobiusStripCreateInfo
        {
            float radius = 2.0f;            // 中心圆半径
            float width = 1.0f;             // 条带宽度
            uint  twists = 1;               // 扭转次数 (通常为1)
            uint  length_segments = 64;     // 沿长度方向分段数
            uint  width_segments = 8;       // 沿宽度方向分段数
        };

        /**
         * 创建一个莫比乌斯带(三角形)
         * 用途：数学教学、拓扑可视化、艺术装置
         * 特点：单面不可定向曲面，扭转180度后首尾相连
         */
        Geometry *CreateMobiusStrip(GeometryCreater *pc, const MobiusStripCreateInfo *msci);

        struct KleinBottleCreateInfo
        {
            float major_radius = 2.0f;      // 主半径
            float minor_radius = 1.0f;      // 次半径
            uint  u_segments = 64;          // U方向分段数
            uint  v_segments = 32;          // V方向分段数
        };

        /**
         * 创建一个克莱因瓶(三角形)
         * 用途：数学可视化、拓扑学演示、科学展示
         * 特点：无定向闭合曲面，4维物体在3维空间的投影
         */
        Geometry *CreateKleinBottle(GeometryCreater *pc, const KleinBottleCreateInfo *kbci);

        struct SuperellipsoidCreateInfo
        {
            Vector3f size = Vector3f(1.0f, 1.0f, 1.0f);  // 椭球尺寸
            float    n1 = 2.0f;                          // 东西方向指数 (1=菱形, 2=椭圆, >2=方形)
            float    n2 = 2.0f;                          // 南北方向指数 (1=菱形, 2=椭圆, >2=方形)
            uint     u_segments = 32;                    // 经度分段数
            uint     v_segments = 16;                    // 纬度分段数
        };

        /**
         * 创建一个超椭球体(三角形)
         * 用途：软体建模、有机形状、可调圆滑度的盒子
         * 特点：通过指数参数在球体和立方体之间连续过渡
         */
        Geometry *CreateSuperellipsoid(GeometryCreater *pc, const SuperellipsoidCreateInfo *seci);
         
    }//namespace inline_geometry
}//namespace hgl::graph
