#ifndef HGL_DATA_TYPE_TINY_INCLUDE
#define HGL_DATA_TYPE_TINY_INCLUDE

namespace hgl
{
    using i8=int8;
    using i16=int16;
    using i32=int32;
    using i64=int64;

    using u8=uint8;
    using u16=uint16;
    using u32=uint32;
    using u64=uint64;

    using f32=float;
    using f64=double;

    #define enum_int(name)  enum name:int
    #define enum_uint(name) enum name:uint

    using void_pointer=void *;

    using uchar     = unsigned char;    ///< 无符号字符型
    using ushort    = unsigned short;   ///< 无符号短整型
    using uint      = unsigned int;     ///< 无符号整型
    using ulong     = unsigned long;    ///< 无符号长整型
}//namespace hgl
#endif//HGL_DATA_TYPE_TINY_INCLUDE
