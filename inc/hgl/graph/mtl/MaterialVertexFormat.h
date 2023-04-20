#ifndef HGL_GRAPH_MATERIAL_VERTEX_FORMAT_INCLUDE
#define HGL_GRAPH_MATERIAL_VERTEX_FORMAT_INCLUDE

#include<hgl/type/DataType.h>

namespace hgl
{
    namespace graph
    {
/**
* Local 2 World �����ŷ���
* 
*   1.Push Constants    ����
* 
*   2.UBO �д��matrix4f���У�vertex attrib�д��ID
*       UBOͨ�����Ϊ16k/64k��һ��matrix4fΪ64�ֽڡ�
* 
*   3.Vertex Attribute
*/


        /**
        * ���ʶ���ṹ
        */
        union MaterialVertexFormat
        {
            struct
            {
                uint position:3;
                bool normal:1;
                bool tangent:1;
                bool bitangent:1;
                uint color:4;
                uint texcoord:4;

                bool local2world:1;
                bool skeleton:1;
            };

            uint32 format;
        };//union MaterialVertexFormat


    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_MATERIAL_VERTEX_FORMAT_INCLUDE