#pragma once

#include<hgl/CoreType.h>
#include<hgl/type/EnumUtil.h>

namespace hgl::graph
{
    /**
     * 图元类型枚举
     */
    enum class PrimitiveType:uint32
    {
        Points=0,                           ///<点
        Lines,                              ///<线
        LineStrip,                          ///<连续线
        Triangles,                          ///<三角形
        TriangleStrip,                      ///<三角形条
        Fan,                                ///<扇形
        LinesAdj,                           ///<代表一个有四个顶点的Primitive,其中第二个点与第三个点会形成线段,而第一个点与第四个点则用来提供2,3邻近点的信息.
        LineStripAdj,                       ///<与LINES_ADJACENCY类似,第一个点跟最后一个点提供信息,剩下的点则跟Line Strip一样形成线段.
        TrianglesAdj,                       ///<代表一个有六个顶点的Primitive,其中第1,3,5个顶点代表一个Triangle,而地2,4,6个点提供邻近信息.(由1起算)
        TriangleStripAdj,                   ///<4+2N个Vertices代表N个Primitive,其中1,3,5,7,9...代表原本的Triangle strip形成Triangle,而2,4,6,8,10...代表邻近提供信息的点.(由1起算)
        Patchs,

        //2D元素
        SolidRectangles=0x100,              ///<实心矩形(并非原生支持。以画点形式在每个点的Position中传递Left,Top,Right,Bottom。在Geometry Shader中转换为2个三角形。用于2D游戏或UI)
                                            //下一步计划改为Position中只传递Left,Top，改在Size中传递Width,Height

        SolidCircles,                       ///<实心圆(以画点形式在Position中指定圆心，名为Size的VAB中指定半径)

        WireRectangles=0x200,               ///<线框矩形    
        WireCircles,                        ///<空心圆(以画点形式在Position中指定圆心，名为Sizes的VAB中指定半径)

        ////3D元素
        //SolidCube=0x300,                    ///<立方体(以画点形式在第一个点中指定一个顶点，在另一个名为Size的VAB中指定尺寸，否则在UBO中指定)
        //                                    ///<如果存在一个名为Rotation的VAB，则其中的值代表每个Cube的3D旋转角度，否则Rotation在UBO中指定。
        //SolidSphere,                        ///<球体(以画点形式在Position中指定球心，名为Size的VAB中指定半径)
        //SolidCylinder,                      ///<圆柱体(以画点形式在Position中指定圆心，名为Size的VAB中指定半径和高度，名为Rotation的VAB指定旋转角度)
        //SolidCone,                          ///<圆锥体(以画点形式在Position中指定圆心，名为Size的VAB中指定半径和高度，名为Rotation的VAB指定旋转角度)
        //SolidCapsule,                       ///<胶囊体(以画点形式在Position中指定胶囊中心，名为Size的VAB中指定半径和高度，名为Rotation的VAB指定旋转角度)

        //SolidAxis,                          ///<坐标轴(以画点形式在Position中指定坐标轴原点，名为Size的VAB中指定长度)

        //WireCube=0x400,                     ///<线框立方体
        //WireSphere,                         ///<线框球体
        //WireCylinder,                       ///<线框圆柱体
        //WireCone,                           ///<线框圆锥体
        //WireCapsule,                        ///<线框胶囊体

        //WireAxis,

        //特别元素
        Billboard=0x500,                    ///<2D广告牌(以画点形式在Position中指定公告板的3D世界坐标)
                                            ///<如果存在另一个名为Size的VAB，则其中的值代表每个Billboard的尺寸，否则Size在UBO中指定。
                                            ///<如果存在一个名为Rotation的VAB，则其中的值代表每个Billboard的2D旋转角度，否则Rotation在UBO中指定。

        OBB=0x600,                          ///<OBB(以画点形式在Position中指定OBB的中心，名为Size的VAB中指定半径，名为Axis[0/1/2]的VAB中指定3个轴向)
                                            
        ENUM_CLASS_RANGE(Points,Patchs),

        Error
    };//

    const char *GetPrimName(const PrimitiveType &prim);
    const PrimitiveType ParsePrimitiveType(const char *name,int len=0);

    bool CheckGeometryShaderIn(const PrimitiveType &);
    bool CheckGeometryShaderOut(const PrimitiveType &);
}//namespace hgl::graph
