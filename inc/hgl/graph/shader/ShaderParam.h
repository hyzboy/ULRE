#ifndef HGL_SHADER_PARAM_TYPE_INCLUDE
#define HGL_SHADER_PARAM_TYPE_INCLUDE

#define BEGIN_SHADER_NAMESPACE    namespace hgl{namespace graph{namespace shader{
#undef END_SHADER_NAMESPACE }}}

BEGIN_SHADER_NAMESPACE
    /**
     * 参数类型
     */
    enum class ParamType
    {
        BOOL=1,

        FLOAT,INT,UINT,MATRIX,          //不区分1/2/3/4通道数量的类型

        FLOAT_1,
        FLOAT_2,
        FLOAT_3,
        FLOAT_4,

        INT_1,
        INT_2,
        INT_3,
        INT_4,

        UINT_1,
        UINT_2,
        UINT_3,
        UINT_4,

        MAT3x3,
        MAT3x4,
        MAT4x4,

        TEXTURE_1D,
        TEXTURE_2D,
        TEXTURE_3D,
        TEXTURE_CUBE,

        TEXTURE_1D_ARRAY,
        TEXTURE_2D_ARRAY,
        TEXTURE_CUBE_ARRAY,

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

        BEGIN_RANGE =FLOAT_1,
        END_RANGE   =NODE,
        RANGE_SIZE  =(END_RAGE-BEGIN_RANGE+1)
    };//enum class ParamType

    /**
     * 参数定义
     */
    struct Param
    {
        UTF8String name;            //参数名称
        ParamType type;             //类型
    };//struct Param
    /**
     * 数值类输入参数定义
     */
    template<typename T>
    struct ParamNumber:public Param
    {
        bool is_const;
        T min_value;
        T max_value;
    };//struct ParamNumber:public Param

#define SHADER_INPUT_PARAM(name,type) input_params.Add(new Param(#name,ParamType::type);
#define SHADER_OUTPUT_PARAM(name,type) output_params.Add(new Param(#name,ParamType::type);

END_SHADER_NAMESPACE
#endif//HGL_SHADER_PARAM_TYPE_INCLUDE
