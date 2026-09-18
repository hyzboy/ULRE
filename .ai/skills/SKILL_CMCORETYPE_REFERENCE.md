# SKILL: CMCoreType 底层库完整 API 参考

> **何时使用本文档**：当你需要基础类型定义、内存/对齐/位操作、数学工具、颜色类型、枚举工具、时间常量、平台宏等底层功能时。
> CMCoreType 是 ULRE 引擎最核心的底层库（子模块 `CMCoreType/inc/hgl/`），**基本不会变动**。
>
> 已在 `SKILL_HGL_TYPES_REFERENCE.md` 中覆盖的内容（String、Collection、IO、MemoryUtil、BitOperations、CompareUtil、Hash）本文件不再重复。

---

## 目录

1. [基础类型定义（CoreType.h）](#1-基础类型定义coretype-h)
2. [内存安全宏（Macro.h）](#2-内存安全宏macroh)
3. [平台与编译器宏（Platform.h）](#3-平台与编译器宏platformh)
4. [对齐工具（AlignUtil.h）](#4-对齐工具alignutilh)
5. [内存分配工具（MemoryAlloc.h）](#5-内存分配工具memoryalloch)
6. [非平凡类型内存管理（ObjectUtil.h）](#6-非平凡类型内存管理objectutilh)
7. [枚举工具（EnumUtil.h）](#7-枚举工具enumutilh)
8. [常量定义（Constants.h）](#8-常量定义constantsh)
9. [类型极值（TypeLimits.h）](#9-类型极值typelimitsh)
10. [Mipmap 工具（MipmapUtil.h）](#10-mipmap-工具mipmaputil-h)
11. [数学工具（math/）](#11-数学工具math)
12. [时间常量（time/TimeConst.h）](#12-时间常量timetimeconsth)
13. [颜色类型（color/）](#13-颜色类型color)
14. [数组查找工具（ArrayItemProcess.h）](#14-数组查找工具arrayitemprocessh)
15. [数组写入辅助（ArrayWriter.h）](#15-数组写入辅助arraywriterh)
16. [数组重排辅助（ArrayRearrangeHelper.h）](#16-数组重排辅助arrayrearrangehelperh)
17. [哈希扩展（QuickHash / HashMergeGolden64）](#17-哈希扩展quickhash--hashmergeGolden64)
18. [基类与事件系统（_Object / EventFunc / Property）](#18-基类与事件系统_object--eventfunc--property)
19. [字符类型（CharType.h 概览）](#19-字符类型chartypeh-概览)

---

## 1. 基础类型定义（CoreType.h）

```cpp
#include <hgl/CoreType.h>
```

### 整型别名

| HGL类型 | 等价标准类型 | 别名 |
|---------|-------------|------|
| `int8` | `std::int8_t` | `i8` |
| `uint8` | `std::uint8_t` | `u8` |
| `int16` | `std::int16_t` | `i16` |
| `uint16` | `std::uint16_t` | `u16` |
| `int32` | `std::int32_t` | `i32` |
| `uint32` | `std::uint32_t` | `u32` |
| `int64` | `std::int64_t` | `i64` |
| `uint64` | `std::uint64_t` | `u64` |

### 浮点别名

| HGL类型 | 说明 | 别名 |
|---------|------|------|
| `half_float` | 16位半精度浮点（存储为uint16） | `f16` |
| `float32` / `f32` | 32位单精度浮点 | — |
| `float64` / `f64` | 64位双精度浮点 | — |

### 其他类型

| HGL类型 | 说明 |
|---------|------|
| `u8char` | UTF-8字符类型（C++20: `char8_t`，否则 `char`） |
| `uchar` | 无符号字符 `unsigned char` |
| `ushort` | 无符号短整型 `unsigned short` |
| `uint` | 无符号整型 `unsigned int` |
| `ulong` | 无符号长整型 `unsigned long` |
| `void_pointer` | `void *` |

### 宏定义

```cpp
// 创建底层为int或uint的枚举
enum_int(MyEnum)   { A, B, C };   // enum MyEnum : int
enum_uint(MyFlags) { F1, F2 };    // enum MyFlags : uint
```

---

## 2. 内存安全宏（Macro.h）

```cpp
#include <hgl/Macro.h>
```

> 这些宏用于安全释放 `new`/`new[]`/`hgl_malloc` 分配的内存，防止悬空指针。

| 宏 | 说明 |
|----|------|
| `SAFE_CLEAR(name)` | 安全 `delete name; name=nullptr` |
| `SAFE_CLEAR_ARRAY(name)` | 安全 `delete[] name; name=nullptr` |
| `SAFE_FREE(name)` | 安全 `hgl_free(name); name=nullptr` |
| `SAFE_CLEAR_OBJECT_ARRAY_OBJECT(name,num)` | 逐个 delete 指针数组中的对象，然后 delete[] 数组 |
| `SAFE_CLEAR_OBJECT_ARRAY(name)` | 用范围for遍历删除对象（容器中的指针） |
| `FREE_OBJECT_ARRAY(name,num)` | 逐个 delete 对象再 `hgl_free` 数组本身 |
| `SAFE_CLEAR_STD_MAP(name)` | 清理 std::map 中的值指针（谨慎使用，内部用std::map） |
| `NEW_NULL_ARRAY(name,type,count)` | 分配并清零数组 `name = new type[count]; memset(...)` |
| `RETURN_OBJECT_OF_ARRAY(array,index,max_count)` | 边界检查后返回数组元素，越界返回nullptr |

```cpp
#include <hgl/Macro.h>

MyObject *obj = new MyObject();
SAFE_CLEAR(obj);    // obj 被 delete，并置为 nullptr

uint8 *buf = new uint8[1024];
SAFE_CLEAR_ARRAY(buf);  // buf 被 delete[]，并置为 nullptr

void *raw = hgl_malloc(256);
SAFE_FREE(raw);     // raw 被 hgl_free，并置为 nullptr
```

---

## 3. 平台与编译器宏（Platform.h）

```cpp
#include <hgl/platform/Platform.h>
```

### 禁止复制/移动宏

```cpp
// 禁止复制构造和赋值（用于不可复制类）
NO_COPY(MyClass)

// 禁止移动构造和赋值
NO_MOVE(MyClass)

// 同时禁止复制和移动
NO_COPY_NO_MOVE(MyClass)
```

用法：
```cpp
class GPUBuffer {
    NO_COPY_NO_MOVE(GPUBuffer)
public:
    GPUBuffer() = default;
};
```

### 操作系统/CPU/编译器标识宏

| 宏 | 说明 |
|----|------|
| `HGL_OS` | 当前OS标识，与 `HGL_OS_Windows`、`HGL_OS_Linux`、`HGL_OS_Android` 等比较 |
| `HGL_CPU` | 当前CPU架构，与 `HGL_CPU_X86_64`、`HGL_CPU_ARMv8` 等比较 |
| `HGL_COMPILER` | 当前编译器，与 `HGL_COMPILER_LLVM`、`HGL_COMPILER_GNU`、`HGL_COMPILER_Microsoft` 等比较 |
| `HGL_ENDIAN` | `HGL_LITTLE_ENDIAN` 或 `HGL_BIG_ENDIAN` |
| `__HGL_FUNC__` | 当前函数名（跨编译器，等价于 `__PRETTY_FUNCTION__`/`__FUNCSIG__`） |

### 内存/字符串格式宏

```cpp
HGL_FMT_I64    // 64位有符号整数printf格式: "%I64d"(MSVC) 或 "%lld"(其他)
HGL_FMT_U64    // 64位无符号整数printf格式
HGL_THREAD_LOCAL_STORAGE   // 线程本地存储修饰符: __declspec(thread) 或 __thread
```

### 目录分隔符

```cpp
HGL_WINDOW_DIRECTORY_SEPARATOR   // '\'
HGL_UNIX_DIRECTORY_SEPARATOR      // '/'
HGL_DIRECTORY_SEPARATOR           // 当前平台的分隔符
HGL_INCORRECT_DIRECTORY_SEPARATOR // 另一平台的分隔符（用于替换）
```

---

## 4. 对齐工具（AlignUtil.h）

```cpp
#include <hgl/type/AlignUtil.h>
```

| 函数 | 说明 |
|------|------|
| `align_to(value, alignment)` | 向上对齐到 alignment 的倍数（通用版，任意正整数对齐） |
| `align_to_pow2(value, alignment)` | 向上对齐到2的幂（位运算优化，alignment必须为2的幂） |
| `align_up(value, alignment)` | 同 `align_to_pow2`（推荐名称） |
| `align_down(value, alignment)` | 向下对齐到alignment的倍数（2的幂） |
| `floor_to_pow2(value)` | 向下取整到最大不超过value的2的幂 |
| `divide_ceil(x, y)` | 向上取整除法：`(x + y - 1) / y` |
| `is_aligned(value, alignment)` | 检查value是否已对齐 |

```cpp
#include <hgl/type/AlignUtil.h>

// 通用对齐（alignment可以是任意正整数）
uint32 a = align_to(13u, 4u);       // 16
uint32 b = align_to(16u, 4u);       // 16

// 2的幂对齐（位运算，更快）
uint32 c = align_to_pow2(13u, 16u); // 16
uint32 d = align_up(100u, 64u);     // 128
uint32 e = align_down(100u, 64u);   // 64

// 向上取整除法
uint32 f = divide_ceil(10u, 3u);    // 4

// 检查对齐
bool ok = is_aligned(256u, 16u);    // true
bool ok2 = is_aligned(100u, 16u);   // false

// 向下取整到2的幂
uint32 g = floor_to_pow2(13u);      // 8
```

**已废弃别名**（向后兼容，请勿在新代码中使用）：
- `hgl_align` → 使用 `align_to`
- `hgl_align_pow2` → 使用 `align_to_pow2`
- `power_to_2` → 使用 `std::bit_ceil`
- `power_to_2_down` → 使用 `floor_to_pow2`
- `divide_rounding_up` → 使用 `divide_ceil`

---

## 5. 内存分配工具（MemoryAlloc.h）

```cpp
#include <hgl/type/MemoryAlloc.h>
```

> 适用于**平凡可复制（TriviallyCopyable）**类型。

### 零初始化分配

```cpp
// 分配单个对象并清零
MyPOD *p = zero_new<MyPOD>();

// 分配数组并清零
MyPOD *arr = zero_new<MyPOD>(100);
```

### 复制分配

```cpp
// 从指针复制数组
MyPOD *copy = new_copy(src_ptr, count);

// 从指针复制单个对象
MyPOD *obj_copy = new_copy(src_ptr);
```

### 原始数组内存（C风格）

```cpp
// 分配（使用 hgl_malloc）
MyPOD *buf = array_alloc<MyPOD>(count);

// 重新分配（使用 hgl_realloc）
buf = array_realloc<MyPOD>(buf, new_count);

// 释放（使用 hgl_free）
array_free(buf);
```

---

## 6. 非平凡类型内存管理（ObjectUtil.h）

```cpp
#include <hgl/type/ObjectUtil.h>
```

> 适用于**非平凡类型**（有自定义构造/析构函数）。供容器内部使用。

### 原始内存分配

```cpp
// 分配未初始化内存（带对齐，不构造对象）
T *raw = allocate_raw_memory<T>(count);

// 释放（不调用析构）
deallocate_raw_memory(raw);
```

### Placement 构造/析构

```cpp
// 默认构造
construct_at(ptr);

// 复制构造
construct_at_copy(ptr, value);

// 移动构造
construct_at_move(ptr, std::move(value));

// 显式析构
destroy_at(ptr);

// 范围析构
destroy_range(first_ptr, last_ptr);   // [first, last)
destroy_range(data_ptr, count);       // data[0..count-1]
```

### 批量构造

```cpp
// 在未初始化内存中批量复制构造
copy_construct_range(dst, src, count);

// 在未初始化内存中批量移动构造
move_construct_range(dst, src, count);

// 在未初始化内存中批量默认构造
default_construct_range(dst, count);
```

### 内存重分配（保留已有对象）

```cpp
// 重新分配内存，移动已有对象到新内存，释放旧内存
T *new_mem = reallocate_and_move(old_data, old_count, new_capacity);
```

---

## 7. 枚举工具（EnumUtil.h）

```cpp
#include <hgl/type/EnumUtil.h>
```

### ENUM_CLASS_RANGE 宏

在枚举类中声明范围信息，配合工具函数使用：

```cpp
enum class MyType
{
    TypeA = 0,
    TypeB,
    TypeC,

    ENUM_CLASS_RANGE(TypeA, TypeC)
    // 展开为: BEGIN_RANGE=TypeA, END_RANGE=TypeC, RANGE_SIZE=3
};
```

### 工具函数

| 函数/宏 | 说明 |
|---------|------|
| `ToInt(enum_value)` | 枚举值转 int |
| `FromInt<T>(int_value)` | int 转枚举类型 T |
| `RangeSize<T>()` | 枚举范围大小（需要 `ENUM_CLASS_RANGE`） |
| `RangeCheck(value)` | 检查枚举值是否在合法范围内 |
| `ENUM_CLASS_FOR(EnumType, CType, varName)` | 遍历枚举范围的for循环 |
| `RANGE_CHECK_RETURN(value, return_value)` | 范围检查失败则返回指定值 |
| `RANGE_CHECK_RETURN_ZERO(value)` | 范围检查失败则返回0 |
| `RANGE_CHECK_RETURN_FALSE(value)` | 范围检查失败则返回false |
| `RANGE_CHECK_RETURN_NULLPTR(value)` | 范围检查失败则返回nullptr |

```cpp
#include <hgl/type/EnumUtil.h>

// 定义带范围的枚举
enum class ShaderStage { Vertex=0, Fragment, Compute, ENUM_CLASS_RANGE(Vertex,Compute) };

// 转int
int n = ToInt(ShaderStage::Fragment);  // 1

// int转枚举
ShaderStage s = FromInt<ShaderStage>(2);  // Compute

// 范围检查
bool ok = RangeCheck(ShaderStage::Fragment);  // true

// 遍历枚举
ENUM_CLASS_FOR(ShaderStage, int, stage) {
    // stage 依次为 0, 1, 2
}

// 函数中范围检查
void UseStage(ShaderStage stage) {
    RANGE_CHECK_RETURN_NULLPTR(stage);
    // ...
}
```

---

## 8. 常量定义（Constants.h）

```cpp
#include <hgl/type/Constants.h>
```

### 大小常量

| 常量 | 值 |
|------|---|
| `HGL_SIZE_1KB` | `1024` |
| `HGL_SIZE_1MB` | `1024*1024` |
| `HGL_SIZE_1GB` | `1024*1024*1024` |
| `HGL_SIZE_1TB` | `1024³ * 1024ULL` |
| `HGL_SIZE_1PB` | `1024⁴ * 1024ULL` |
| `HGL_SIZE_1EB` | `1024⁵ * 1024ULL` |

### 整数极值常量

| 常量 | 类型 | 值 |
|------|------|---|
| `HGL_U8_MAX` | uint8 | 0xFF |
| `HGL_U16_MAX` | uint16 | 0xFFFF |
| `HGL_U32_MAX` | uint32 | 0xFFFFFFFF |
| `HGL_U64_MAX` | uint64 | 0xFFFFFFFFFFFFFFFF |
| `HGL_S8_MAX/MIN` | int8 | 127 / -128 |
| `HGL_S16_MAX/MIN` | int16 | 32767 / -32768 |
| `HGL_S32_MAX/MIN` | int32 | 2147483647 / -2147483648 |
| `HGL_S64_MAX/MIN` | int64 | 最大/最小64位有符号整数 |

### 十六进制字符表

```cpp
// 小写 hex 字符: "0123456789abcdef"
constexpr char LowerHexChar[16];

// 大写 hex 字符: "0123456789ABCDEF"
constexpr char UpperHexChar[16];
```

---

## 9. 类型极值（TypeLimits.h）

```cpp
#include <hgl/type/TypeLimits.h>
```

| 函数 | 说明 |
|------|------|
| `numeric_max<T>()` | 类型T的最大值（`std::numeric_limits<T>::max()`） |
| `numeric_min<T>()` | 类型T的最小值（整数为min，浮点为lowest） |
| `numeric_lowest<T>()` | 类型T的最低值（浮点用`lowest()`） |
| `numeric_epsilon<T>()` | 浮点数精度（epsilon） |
| `numeric_infinity<T>()` | 浮点数无穷大 |
| `numeric_quiet_nan<T>()` | 浮点数 quiet NaN |
| `unsigned_half<T>()` | 无符号整数类型的一半值 |
| `is_signed<T>()` | 类型是否有符号 |
| `bit_width<T>()` | 类型的位数 |

```cpp
#include <hgl/type/TypeLimits.h>

int32 max_i32   = numeric_max<int32>();     // 2147483647
uint64 max_u64  = numeric_max<uint64>();
float  max_f32  = numeric_max<float>();
double eps      = numeric_epsilon<double>();
float  inf      = numeric_infinity<float>();
float  nan      = numeric_quiet_nan<float>();
```

---

## 10. Mipmap 工具（MipmapUtil.h）

```cpp
#include <hgl/type/MipmapUtil.h>
```

```cpp
// 1D纹理的mip层数
uint levels1D = GetMipLevel(width);

// 2D纹理的mip层数（取宽高最大值计算）
uint levels2D = GetMipLevel(width, height);

// 3D纹理的mip层数（取宽高深最大值计算）
uint levels3D = GetMipLevel(width, height, depth);
```

---

## 11. 数学工具（math/）

### 11.1 Clamp（ClampUtil.h）

```cpp
#include <hgl/math/ClampUtil.h>
```

```cpp
// 限制到 [min_value, max_value]（通用模板）
float v = Clamp(1.5f, 0.0f, 1.0f);    // 1.0f
int   i = Clamp(-5, 0, 100);           // 0

// 限制到 [0, 1]
float n = Clamp(2.0f);                  // 1.0f

// 快速限制到 uint8 [0, 255]
uint8 ub = ClampU8(300);               // 255
uint8 ub2 = ClampU8(-1);              // 0

// 快速限制到 uint16 [0, 65535]
uint16 us = ClampU16(70000);          // 65535
```

> **注意**：这是 `hgl::math` 命名空间中的 `Clamp`（大写），而非 `hgl::CompareUtil` 中的 `hgl::clamp`（小写）。
> 浮点渲染值用 `Clamp`，通用比较用 `hgl::clamp`。

---

### 11.2 浮点精度常量与比较（FloatPrecision.h）

```cpp
#include <hgl/math/FloatPrecision.h>
```

#### 常量（`hgl::math` 命名空间）

| 常量 | 值 | 说明 |
|------|---|------|
| `float_min` | `std::numeric_limits<float>::min()` | 最小正规化浮点数 |
| `float_max` | `std::numeric_limits<float>::max()` | 最大浮点数 |
| `float_epsilon` | `std::numeric_limits<float>::epsilon()` | 浮点精度 |
| `double_min/max/epsilon` | 对应 double 版本 | — |
| `half_float_error` | `0.001f` | 半精度浮点误差阈值 |
| `float_error` | `0.0001f` | 浮点比较误差阈值 |
| `double_error` | `0.00000001` | 双精度比较误差阈值 |

#### 近似比较函数

```cpp
using namespace hgl::math;

// 近似零检测
bool z1 = IsNearlyZero(0.00001f);              // true
bool z2 = IsNearlyZero(0.1, 0.01);             // false

// 近似相等检测
bool eq1 = IsNearlyEqual(1.0f, 1.00001f);      // true
bool eq2 = IsNearlyEqual(1.0, 1.0000001);       // true

// 数组近似相等
float a[] = {1.0f, 2.0f}, b[] = {1.00001f, 2.00001f};
bool eq3 = IsNearlyEqualArray(a, b, 2);         // true
```

---

### 11.3 浮点数验证（FloatValidation.h）

```cpp
#include <hgl/math/FloatValidation.h>
```

所有函数均在 `hgl::math` 命名空间，支持 `half_float`、`float`、`double`。

| 函数 | 说明 |
|------|------|
| `IsNaN(v)` | 是否为 NaN |
| `IsInfinite(v)` | 是否为无穷大（正或负） |
| `IsPositiveInfinite(v)` | 是否为正无穷大 |
| `IsNegativeInfinite(v)` | 是否为负无穷大 |
| `IsZero(v)` | 是否为零（正零或负零） |
| `IsPositiveZero(v)` | 是否为正零 |
| `IsNegativeZero(v)` | 是否为负零 |
| `IsFinite(v)` | 是否为有限数（非NaN、非无穷） |
| `IsDenormalized(v)` | 是否为非规格化数 |
| `IsNormalized(v)` | 是否为规格化数 |
| `IsValid(v)` | 是否有效（非NaN且非无穷） |

```cpp
#include <hgl/math/FloatValidation.h>
using namespace hgl::math;

float f = std::numeric_limits<float>::quiet_NaN();
bool is_nan = IsNaN(f);          // true
bool is_fin = IsFinite(f);       // false
bool is_ok  = IsValid(1.0f);     // true
bool is_ok2 = IsValid(f);        // false

// 适用于half_float
half_float hf = 0x7C01;  // NaN
bool hf_nan = IsNaN(hf); // true
```

---

### 11.4 浮点控制（FloatControl.h）

```cpp
#include <hgl/math/FloatControl.h>
```

浮点数位操作（`hgl::math` 命名空间）：

```cpp
// 拆分浮点数为符号位、指数和尾数
bool sign; uint exp; uint mant;
SplitFloat32(sign, exp, mant, 3.14f);

bool sign64; uint exp64; uint64 mant64;
SplitFloat64(sign64, exp64, mant64, 3.14);

// 合并回浮点数
half_float hf = MergeFloat16(sign, exp, mant);
float      f  = MergeFloat32(sign, exp, mant);
double     d  = MergeFloat64(sign, exp64, mant64);
```

---

### 11.5 物理常量（PhysicsConstants.h）

```cpp
#include <hgl/math/PhysicsConstants.h>
```

| 常量 | 值 | 说明 |
|------|---|------|
| `HGL_SILVER_RATIO` | `2.4142...` | 白银比例 |
| `HGL_SPEED_OF_SOUND` | `331.3` m/s | 0°C干燥空气中的音速 |
| `HGL_SPEED_OF_LIGHT` | `299792458` m/s | 光速 |
| `HGL_ABSOLUTE_ZERO` | `-273.15` °C | 绝对零度 |
| `HGL_UNIVERSAL_GRAVITATION` | `6.67430e-11` | 万有引力常数 |
| `HGL_EARTH_GRAVITATIONAL_ACCELERATION` | `9.80665` m/s² | 地球重力加速度 |
| `HGL_EARTH_MASS` | `5.9722e+24` kg | 地球质量 |
| `HGL_EARTH_RADIUS` | `6371000` m | 地球半径 |

---

### 11.6 二进制常量（BinaryConstants.h）

```cpp
#include <hgl/math/BinaryConstants.h>
// 包含常用的二进制幂次常量（1<<n 系列）
```

---

## 12. 时间常量（time/TimeConst.h）

```cpp
#include <hgl/time/TimeConst.h>
```

### 枚举

```cpp
// 星期枚举
enum class Weekday { Sunday=0, Monday, ..., Saturday };

// 月份枚举（1月=1开始）
enum class Month { January=1, February, ..., December };
```

### 纳秒/微秒/毫秒常量

| 常量 | 值 | 说明 |
|------|---|------|
| `HGL_NANO_SEC_PER_SEC` | `1000000000` | 纳秒/秒 |
| `HGL_NANO_SEC_PER_MICRO` | `1000` | 纳秒/微秒 |
| `HGL_MICRO_SEC_PER_SEC` | `1000000` | 微秒/秒 |
| `HGL_MILLI_SEC_PER_SEC` | `1000` | 毫秒/秒 |

### 秒级时间常量

| 常量 | 值（秒） |
|------|---------|
| `HGL_TIME_ONE_SECOND` | 1 |
| `HGL_TIME_ONE_MINUTE` | 60 |
| `HGL_TIME_HALF_HOUR` | 1800 |
| `HGL_TIME_ONE_HOUR` | 3600 |
| `HGL_TIME_HALF_DAY` | 43200 |
| `HGL_TIME_ONE_DAY` | 86400 |
| `HGL_TIME_ONE_WEEK` | 604800 |
| `HGL_TIME_ONE_YEAR` | 31536000 |

### Windows FILETIME / UUIDv7 常量

```cpp
HGL_WIN_TICKS_PER_SEC    = 10000000      // 100ns 单位/秒
HGL_WIN_TO_UNIX_EPOCH_SEC = 11644473600LL // 1601→1970 秒差
HGL_UUID7_TIMESTAMP_BITS  = 48           // UUIDv7 时间戳位数
HGL_UUID7_TIMESTAMP_MASK  = 0x0000FFFFFFFFFFFFULL  // 48位掩码
```

---

## 13. 颜色类型（color/）

### 颜色类型总览

| 头文件 | 类型 | 描述 |
|--------|------|------|
| `<hgl/color/Color3f.h>` | `Color3f` | RGB 浮点颜色（继承 glm::vec3） |
| `<hgl/color/Color4f.h>` | `Color4f` | RGBA 浮点颜色（继承 glm::vec4） |
| `<hgl/color/Color3ub.h>` | `Color3ub` | RGB uint8颜色 |
| `<hgl/color/Color4ub.h>` | `Color4ub` | RGBA uint8颜色 |
| `<hgl/color/LinearColor3f.h>` | `LinearColor3f` | 线性色彩空间RGB |
| `<hgl/color/LinearColor4f.h>` | `LinearColor4f` | 线性色彩空间RGBA |
| `<hgl/color/Color.h>` | `enum class COLOR` | 预定义颜色枚举（200+种颜色） |
| `<hgl/color/ColorFormat.h>` | — | 颜色格式转换 |
| `<hgl/color/ColorLerp.h>` | — | 颜色插值 |
| `<hgl/color/sRGBConvert.h>` | — | sRGB颜色空间转换 |
| `<hgl/color/HSL.h>` | `HSLf` | HSL颜色空间 |
| `<hgl/color/HSV.h>` | `HSVf` | HSV颜色空间 |
| `<hgl/color/OKLab.h>` | — | OKLab感知均匀颜色空间 |
| `<hgl/color/XYZ.h>` | — | CIE XYZ颜色空间 |
| `<hgl/color/YCbCr.h>` | — | YCbCr颜色空间 |
| `<hgl/color/YCoCg.h>` | — | YCoCg颜色空间 |
| `<hgl/color/Lum.h>` | — | 亮度计算 |
| `<hgl/color/CMYKf.h>` | `CMYKf` | CMYK浮点颜色 |
| `<hgl/color/CMYKub.h>` | `CMYKub` | CMYK uint8颜色 |
| `<hgl/color/ColorPacking.h>` | — | 颜色打包/解包 |

### Color4f 常用接口

```cpp
#include <hgl/color/Color4f.h>

Color4f red(1.0f, 0.0f, 0.0f, 1.0f);   // RGBA
Color4f grey(0.5f);                       // 灰度，alpha=1

red.set(0.8f, 0.1f, 0.1f, 1.0f);        // 重设颜色
red.set255(200, 50, 50, 255);            // 0-255范围设置

// 格式转换
uint32 rgba8 = red.toRGBA8();            // uint32 RGBA打包
uint32 bgra8 = red.toBGRA8();            // BGRA打包（Vulkan常用）

// 插值
Color4f start(1,0,0,1), end(0,0,1,1);
start.lerp(end, 0.5f);                   // 混合到紫色

// 转换为float指针（传给Vulkan等API）
glClearColor(red.x, red.y, red.z, red.w);
// 或
const float *ptr = (const float *)red;   // 直接转换
```

### 预定义颜色枚举（Color.h）

```cpp
#include <hgl/color/Color.h>

// 通过枚举获取颜色
Color4f red = GetColor4f(COLOR::Red);
Color4f blue_a50 = GetColor4f(COLOR::Blue, 0.5f);  // 50%透明度

uint32 rgba = GetRGBA(COLOR::Green);
uint32 abgr = GetABGR(COLOR::Cyan);

// 部分预定义颜色
// COLOR::Red, Blue, Green, White, Black, Yellow, Cyan, Magenta
// COLOR::DarkRed, DarkBlue, DarkGreen, ...
// COLOR::AliceBlue, Coral, Crimson, Khaki, ...（200+种）
```

---

## 14. 数组查找工具（ArrayItemProcess.h）

```cpp
#include <hgl/type/ArrayItemProcess.h>
```

### 无序数组查找

```cpp
// 在无序数组中查找，找到返回索引，未找到返回-1
int idx = FindDataPositionInArray(data_ptr, count, target_value);

// 适配HGL容器（有GetData()/GetCount()方法）
int64 idx2 = FindDataPositionInArray(hgl_array, target_value);
```

### 有序数组查找（二分）

```cpp
// 在有序数组中二分查找，找到返回索引，未找到返回-1，O(log n)
int64 idx = FindDataPositionInSortedArray(data_ptr, count, target);

// 适配HGL容器
int64 idx2 = FindDataPositionInSortedArray(sorted_array, target);
```

### 有序数组插入位置查找（lower_bound）

```cpp
// 查找插入位置（标准lower_bound算法）
// 返回true=元素已存在，false=元素不存在(pos为插入位置)
int64 pos;
bool exists = FindInsertPositionInSortedArray(&pos, data_ptr, count, value);
if (!exists) {
    // 在 pos 位置插入 value
}

// 适配HGL容器
bool exists2 = FindInsertPositionInSortedArray(&pos, sorted_array, value);
```

---

## 15. 数组写入辅助（ArrayWriter.h）

```cpp
#include <hgl/type/ArrayWriter.h>
```

轻量级数组写入工具，适合批量填充C风格数组：

```cpp
// ArrayWriter<CountType, ValueType>
// 构造时重置count为0，每次<<写入一个值
uint32 count;
int values[10];

hgl::ArrayWriter<uint32, int> writer(&count, values);
writer << 1 << 2 << 3;
// count == 3, values == {1, 2, 3}
```

---

## 16. 数组重排辅助（ArrayRearrangeHelper.h）

```cpp
#include <hgl/type/ArrayRearrangeHelper.h>
```

用于将一个数组按指定分段顺序重新排列：

```cpp
// 有10个元素的数组，分成{3,4,3}三段，按{2,0,1}顺序重排
int old_data[10] = {0,1,2,3,4,5,6,7,8,9};
int new_data[10];

ArrayRearrange(new_data, old_data, 10,
               {3, 4, 3},    // 分段：[0..2], [3..6], [7..9]
               {2, 0, 1});   // 顺序：先放第2段，再第0段，再第1段

// new_data == {7,8,9, 0,1,2, 3,4,5,6}
```

---

## 17. 哈希扩展（QuickHash / HashMergeGolden64）

### 快速哈希（QuickHash.h）

```cpp
#include <hgl/util/hash/QuickHash.h>
```

```cpp
// 对任意类型计算最优哈希
// - 整数/枚举：直接转换（零开销）
// - 指针：使用地址
// - 其他类型：使用 wyhash

uint64 h1 = ComputeOptimalHash(42);            // 整数，直接转
uint64 h2 = ComputeOptimalHash(some_ptr);      // 指针地址
uint64 h3 = ComputeOptimalHash(my_struct);     // wyhash

// 原始字节块
uint64 h4 = ComputeOptimalHash(data_ptr, byte_size);
```

### 哈希合并（HashMergeGolden64.h）

```cpp
#include <hgl/util/hash/HashMergeGolden64.h>
```

```cpp
// 将两个哈希合并为一个（黄金比例混合，减少碰撞）
size_t merged = hgl::hash::HashMergeGolden64(seed, value);

// 典型用法：组合多个字段的哈希
size_t h = 0;
h = HashMergeGolden64(h, ComputeOptimalHash(a));
h = HashMergeGolden64(h, ComputeOptimalHash(b));
```

---

## 18. 基类与事件系统（_Object / EventFunc / Property）

### 最终基类（_Object.h）

```cpp
#include <hgl/type/_Object.h>
```

```cpp
// _Object 是引擎所有支持事件的类的最终基类
class MyComponent : public _Object {
public:
    virtual ~MyComponent() override = default;
};

// 获取成员函数指针（跨编译器）
void *fp = GetMemberFuncPointer(MyClass, MyMethod);
```

### 事件函数（EventFunc.h）

```cpp
#include <hgl/platform/compiler/EventFunc.h>

class Button : public _Object {
    DefEvent(void, OnClick, ());          // 定义一个 void() 事件

    DefEvent(void, OnValueChanged, (int)); // 定义一个 void(int) 事件
};

class Handler : public _Object {
    void HandleClick() { /* ... */ }
    void HandleValue(int v) { /* ... */ }

    void BindEvents(Button *btn) {
        SetEventCall(btn->OnClick, this, Handler, HandleClick);
        SetEventCall(btn->OnValueChanged, this, Handler, HandleValue);
    }
};

// 调用事件（安全版，检查是否绑定）
SafeCallEvent(btn->OnClick, ());
SafeCallEvent(btn->OnValueChanged, (42));
```

### 属性（Property.h）

```cpp
#include <hgl/platform/compiler/Property.h>

class MyObj : public _Object {
    int _value;
    int GetValue() const { return _value; }
    void SetValue(int v) { _value = v; }

public:
    Property<int> Value;

    MyObj() {
        cmSetProperty(Value, this, MyObj::GetValue, MyObj::SetValue);
    }
};

// 使用
MyObj obj;
int v = obj.Value;   // 调用 GetValue
obj.Value = 42;      // 调用 SetValue
```

---

## 19. 字符类型（CharType.h 概览）

> 详见 `SKILL_HGL_TYPES_REFERENCE.md` 第8节。本节仅说明字符类型宏。

```cpp
#include <hgl/type/CharType.h>  // 包含所有字符处理工具
```

### 字符串字面量宏（定义于 Platform.h 的 OS 头文件）

| 宏 | 说明 | 示例 |
|----|------|------|
| `U8_TEXT(s)` | UTF-8字符串字面量 | `U8_TEXT("你好")` |
| `OS_TEXT(s)` | 平台路径字符串字面量 | `OS_TEXT("/data/file")` |
| `U16_TEXT(s)` | UTF-16字符串字面量 | `U16_TEXT("text")` |
| `U32_TEXT(s)` | UTF-32字符串字面量 | `U32_TEXT("text")` |

### 字符类型定义

| 类型 | 说明 |
|------|------|
| `u8char` | UTF-8字符（`char8_t` 或 `char`） |
| `u16char` | UTF-16字符（`char16_t`） |
| `u32char` | UTF-32字符（`char32_t`） |
| `oschar` | 平台路径字符（Windows=`wchar_t`，其他=`char`） |

---

## 参考文件位置（CMCoreType submodule）

```
CMCoreType/inc/hgl/
├── CoreType.h              # 基础类型别名
├── Macro.h                 # 安全释放宏
├── color/                  # 颜色类型
│   ├── Color3f.h, Color4f.h, Color3ub.h, Color4ub.h
│   ├── Color.h             # 预定义颜色枚举
│   ├── LinearColor3f.h, LinearColor4f.h
│   ├── ColorLerp.h, sRGBConvert.h, ColorFormat.h
│   ├── HSL.h, HSV.h, OKLab.h, XYZ.h, YCbCr.h, YCoCg.h
│   ├── Lum.h, CMYKf.h, CMYKub.h, ColorPacking.h
├── math/                   # 数学工具
│   ├── ClampUtil.h         # Clamp, ClampU8, ClampU16
│   ├── FloatPrecision.h    # float_error, IsNearlyZero, IsNearlyEqual
│   ├── FloatValidation.h   # IsNaN, IsInfinite, IsFinite, IsValid...
│   ├── FloatControl.h      # SplitFloat32/64, MergeFloat16/32/64
│   ├── PhysicsConstants.h  # HGL_SPEED_OF_LIGHT, HGL_EARTH_RADIUS...
│   ├── BinaryConstants.h   # 二进制幂次常量
├── platform/
│   ├── Platform.h          # OS/CPU/Compiler检测, NO_COPY/NO_MOVE
│   ├── Exit.h              # 退出函数
│   ├── FuncLoad.h          # 动态库函数加载
│   ├── compiler/
│   │   ├── EventFunc.h     # DefEvent, SetEventCall, SafeCallEvent
│   │   ├── Property.h      # Property<T>, PropertyRead<T>
│   │   ├── GNU.h, LLVM.h, Microsoft.h, Intel.h
├── time/
│   └── TimeConst.h         # 时间常量, Weekday, Month
├── type/
│   ├── AlignUtil.h         # align_to, align_up, align_down...
│   ├── ArrayItemProcess.h  # FindDataPositionInArray...
│   ├── ArrayRearrangeHelper.h
│   ├── ArrayWriter.h
│   ├── BitOperations.h     # 位操作（已在主文档覆盖）
│   ├── CharType.h          # 字符分类函数
│   ├── CompareUtil.h       # clamp, min, max（已在主文档覆盖）
│   ├── Constants.h         # HGL_SIZE_1KB, HGL_U32_MAX...
│   ├── EnumUtil.h          # ENUM_CLASS_RANGE, ToInt, RangeCheck...
│   ├── MemoryAlloc.h       # zero_new, new_copy, array_alloc...
│   ├── MemoryUtil.h        # mem_copy, mem_zero（已在主文档覆盖）
│   ├── MipmapUtil.h        # GetMipLevel
│   ├── ObjectUtil.h        # allocate_raw_memory, construct_at...
│   ├── Str.*.h             # 字符串函数（已在主文档覆盖）
│   ├── StdByteBuffer.h     # ByteWriter/ByteReader（内部使用STL）
│   ├── TypeLimits.h        # numeric_max, numeric_min...
│   ├── _Object.h           # _Object基类, GetMemberFuncPointer
├── util/hash/
│   ├── FNV1a.h             # FNV1aInit/Append（已在主文档覆盖）
│   ├── HashMergeGolden64.h # HashMergeGolden64
│   ├── QuickHash.h         # ComputeOptimalHash
│   ├── wyhash.h, wyhash32.h, SecureHash.h
```
