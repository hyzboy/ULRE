#ifndef HGL_GRAPH_MESH_INCLUDE
#define HGL_GRAPH_MESH_INCLUDE

namespace hgl
{
    namespace graph
    {
        #pragma pack(push,1)
        struct MeshFileHeader
        {
            uint8 flag[4];                      ///<'MESH'
            uint8 sperator;                     ///<0x1a
            uint8 version;                      ///<1

            uint8 primitive_type;               ///<图元类型

            uint32 vertices_number;             ///<顶点数量
            uint32 faces_number;                ///<面数量

            uint8 color_channels;               ///<顶点色数量
            uint8 texcoord_channels;            ///<纹理坐标数量

            uint32 material_index;              ///<材质索引

            uint8 ntb;                          ///<ntb信息位合集

            uint32 bones_number;                ///<骨骼数量

        public:

            MeshFileHeader()
            {
                memset(this,0,sizeof(MeshFileHeader));
                flag[0]='M';
                flag[1]='E';
                flag[2]='S';
                flag[3]='H';
                sperator=0x1A;
                version=1;
            }
        };//struct MeshFileHeader
        #pragma pack(pop)

        struct MeshData
        {
            VkPrimitiveTopology primitive;
        };//
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_MESH_INCLUDE
