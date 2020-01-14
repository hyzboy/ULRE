#ifndef HGL_GRAPH_SHADER_PARAM_TYPE_INCLUDE
#define HGL_GRAPH_SHADER_PARAM_TYPE_INCLUDE

#include<hgl/graph/shader/common.h>
#include<hgl/type/BaseString.h>

SHADER_PARAM_NAMESPACE_BEGIN
/**
 * 参数类型
 */
enum class ParamType
{
    BOOL=0,
    FLOAT,DOUBLE,INT,UINT,MATRIX,          //不区分1/2/3/4通道数量的类型

    Bool1,
    Bool2,
    Bool3,
    Bool4,

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

    Double1,
    Double2,
    Double3,
    Double4,

    Matrix2,
    Matrix3,
    Matrix4,
    Matrix3x2,
    Matrix2x3,
    Matrix4x2,
    Matrix2x4,
    Matrix4x3,
    Matrix3x4,

    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube,
    TextureRect,

    Texture1DArray,
    Texture2DArray,
    TextureCubeArray,

    Texture1DShadow,
    Texture2DShadow,
    TextureCubeShadow,
    TextureRectShadow,

    Texture1DArrayShadow,
    Texture2DArrayShadow,
    TextureCubeArrayShadow,

    Texture2DMultiSample,
    Texture2DMultiSampleArray,

    TBO,

    ARRAY_1D,               //阵列
    ARRAY_2D,               //2D阵列

    UBO,                    //UBO name
    NODE,                   //另一个节点，只可做为输入参数

    BEGIN_RANGE =BOOL,
    END_RANGE   =NODE,
    RANGE_SIZE  =(END_RANGE-BEGIN_RANGE+1)
};//enum class ParamType

const char *GetTypename(const ParamType);
SHADER_PARAM_NAMESPACE_END
#endif//HGL_GRAPH_SHADER_PARAM_TYPE_INCLUDE
