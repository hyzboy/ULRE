#ifndef HGL_GRAPH_SHADER_PARAM_INCLUDE
#define HGL_GRAPH_SHADER_PARAM_INCLUDE

#include<hgl/graph/shader/common.h>

BEGIN_SHADER_PARAM_NAMESPACE
/**
 * 参数类型
 */
enum class ParamType
{
    BOOL=1,

    FLOAT,INT,UINT,MATRIX,          //不区分1/2/3/4通道数量的类型

    Float1,
    Float2,
    Float3,
    Float4,

    Integer1,
    Integer2,
    Integer3,
    Integer4,

    UInteger1,
    UInteger2,
    UInteger3,
    UInteger4,

    Matrix3x3,
    Matrix3x4,
    Matrix4x4,

    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube,
    TextureRect,

    Texture1DArray,
    Texture2DArray,
    TextureCubeArray,

    FLOAT_1_STREAM,
    FLOAT_2_STREAM,
    FLOAT_3_STREAM,
    FLOAT_4_STREAM,

    INT_1_STREAM,
    INT_2_STREAM,
    INT_3_STREAM,
    INT_4_STREAM,

    UINT_1_STREAM,
    UINT_2_STREAM,
    UINT_3_STREAM,
    UINT_4_STREAM,

    ARRAY_1D,               //阵列
    ARRAY_2D,               //2D阵列

    UBO,                    //UBO name
    NODE,                   //另一个节点，只可做为输入参数

    BEGIN_RANGE =Float1,
    END_RANGE   =NODE,
    RANGE_SIZE  =(END_RANGE-BEGIN_RANGE+1)
};//enum class ParamType
END_SHADER_PARAM_NAMESPACE
#endif//HGL_GRAPH_SHADER_PARAM_INCLUDE

