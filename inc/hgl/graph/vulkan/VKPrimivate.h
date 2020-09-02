#ifndef HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE
#define HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE

#include<hgl/TypeFunc.h>

/**
 * 图元类型枚举
 */
enum class Prim
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

    Rectangles=0x100,                   ///<矩形(并非原生支持。以画点形式在每个点的Position中传递Left,Top,Width,Height。在Geometry Shader中转换为2个三角形。用于2D游戏或UI)

    ENUM_CLASS_RANGE(Points,Patchs)
};//

#endif//HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE
