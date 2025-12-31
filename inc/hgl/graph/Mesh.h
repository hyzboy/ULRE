#pragma once
#include<hgl/CoreType.h>
#include<hgl/graph/VKPrimitiveType.h>

namespace hgl::graph
{
    #pragma pack(push,1)
    struct MeshFileHeader
    {
        uint8   flag[4] = { 'M', 'E', 'S', 'H' }; ///<'MESH'
        uint8   sperator = 0x1A;                     ///<0x1a
        uint8   version = 1;                         ///<1

        uint8   primitive_type = 0;               ///<图元类型

        uint32  vertices_number = 0;             ///<顶点数量
        uint32  faces_number = 0;                ///<面数量

        uint8   color_channels = 0;               ///<顶点色数量
        uint8   texcoord_channels = 0;            ///<纹理坐标数量

        uint32  material_index = 0;              ///<材质索引

        uint8   ntb = 0;                          ///<ntb信息位合集

        uint32  bones_number = 0;                ///<骨骼数量

    };//struct MeshFileHeader
    #pragma pack(pop)

    struct MeshData
    {
        PrimitiveType prim_type;
    };//
}//namespace hgl::graph
