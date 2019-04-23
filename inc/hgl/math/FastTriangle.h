#ifndef HGL_ALGORITHM_MATH_FAST_TRIANGLE_FUNCTION_INCLUDE
#define HGL_ALGORITHM_MATH_FAST_TRIANGLE_FUNCTION_INCLUDE

namespace hgl
{
    double Lsin(int angle);                                     ///<低精度sin计算,注意传入的参数为角度而非弧度
    double Lcos(int angle);                                     ///<低精度cos计算,注意传入的参数为角度而非弧度
    void Lsincos(int angle, double &s, double &c);              ///<低精度sin+cos计算,注意传入的参数为角度而非弧度

    /**
     * 低精度atan函数
     */
    double inline Latan(double z)
    {
        constexpr double n1 = 0.97239411f;
        constexpr double n2 = -0.19194795f;

        return (n1 + n2 * z * z) * z;
    }

    double Latan2(double y, double x);                          ///<低精度atan2函数
}//namespace hgl
#endif//HGL_ALGORITHM_MATH_FAST_TRIANGLE_FUNCTION_INCLUDE
