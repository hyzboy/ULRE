#ifndef HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE
#define HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE

#include<vulkan/vulkan.h>

#define PRIM_POINTS             VK_PRIMITIVE_TOPOLOGY_POINT_LIST                    ///<点
#define PRIM_LINES              VK_PRIMITIVE_TOPOLOGY_LINE_LIST                     ///<线
#define PRIM_LINE_STRIP         VK_PRIMITIVE_TOPOLOGY_LINE_STRIP                    ///<连续线
#define PRIM_TRIANGLES          VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST                 ///<三角形
#define PRIM_TRIANGLE_STRIP     VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP                ///<三角形条
#define PRIM_TRIANGLE_FAN       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN                  ///<扇形
#define PRIM_LINES_ADJ          VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY      ///<代表一个有四个顶点的Primitive,其中第二个点与第三个点会形成线段,而第一个点与第四个点则用来提供2,3邻近点的信息.
#define PRIM_LINE_STRIP_ADJ     VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY     ///<与LINES_ADJACENCY类似,第一个点跟最后一个点提供信息,剩下的点则跟Line Strip一样形成线段.
#define PRIM_TRIANGLES_ADJ      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY  ///<代表一个有六个顶点的Primitive,其中第1,3,5个顶点代表一个Triangle,而地2,4,6个点提供邻近信息.(由1起算)
#define PRIM_TRIANGLE_STRIP_ADJ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY ///<4+2N个Vertices代表N个Primitive,其中1,3,5,7,9...代表原本的Triangle strip形成Triangle,而2,4,6,8,10...代表邻近提供信息的点.(由1起算)
#define PRIM_PATCHS             VK_PRIMITIVE_TOPOLOGY_PATCH_LIST 
#define PRIM_RECTANGLE          0x100                                               ///<矩形(并非原生支持。实现在以画点形式在每个点的Position中传递Left,Top,Width,Height。在Geometry Shader中将转换为2个三角形。用于2D游戏)
#define PRIM_BEGIN              VK_PRIMITIVE_TOPOLOGY_BEGIN_RANGE 
#define PRIM_END                VK_PRIMITIVE_TOPOLOGY_END_RANGE 
#define PRIM_RANGE              VK_PRIMITIVE_TOPOLOGY_RANGE_SIZE 

#endif//HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE
