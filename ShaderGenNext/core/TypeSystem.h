#pragma once

#include <string>
#include <cstdint>
#include <type_traits>

namespace hgl::shader_next {

/**
 * 着色器类型系统
 * 
 * 提供编译期和运行时的类型检查
 */

// 基础类型枚举
enum class BaseType : uint8_t {
    Float,
    Int,
    UInt,
    Bool,
    Double,
    Void,
    Struct,
    Sampler,
    Image
};

// 向量大小
enum class VectorSize : uint8_t {
    Scalar = 1,
    Vec2 = 2,
    Vec3 = 3,
    Vec4 = 4
};

// 矩阵类型
struct MatrixType {
    uint8_t rows;
    uint8_t cols;
    
    constexpr MatrixType(uint8_t r, uint8_t c) : rows(r), cols(c) {}
    constexpr bool isSquare() const { return rows == cols; }
};

/**
 * ShaderType - 着色器类型描述符
 */
class ShaderType {
    BaseType base_;
    VectorSize vec_size_;
    MatrixType mat_type_;
    bool is_array_;
    uint32_t array_size_;
    std::string struct_name_;
    
public:
    constexpr ShaderType(BaseType base, VectorSize vec = VectorSize::Scalar)
        : base_(base), vec_size_(vec), mat_type_(0, 0), 
          is_array_(false), array_size_(0) {}
    
    constexpr ShaderType(BaseType base, MatrixType mat)
        : base_(base), vec_size_(VectorSize::Scalar), mat_type_(mat),
          is_array_(false), array_size_(0) {}
    
    // 类型查询
    BaseType baseType() const { return base_; }
    VectorSize vectorSize() const { return vec_size_; }
    MatrixType matrixType() const { return mat_type_; }
    bool isArray() const { return is_array_; }
    uint32_t arraySize() const { return array_size_; }
    
    bool isScalar() const { return vec_size_ == VectorSize::Scalar; }
    bool isVector() const { return vec_size_ != VectorSize::Scalar; }
    bool isMatrix() const { return mat_type_.rows > 0; }
    bool isStruct() const { return base_ == BaseType::Struct; }
    bool isSampler() const { return base_ == BaseType::Sampler; }
    
    // 类型大小（字节）
    size_t sizeInBytes() const;
    
    // 对齐要求
    size_t alignment() const;
    
    // 类型名称
    std::string name() const;
    
    // 设置为数组
    ShaderType& asArray(uint32_t size) {
        is_array_ = true;
        array_size_ = size;
        return *this;
    }
    
    // 比较
    bool operator==(const ShaderType& other) const;
    bool operator!=(const ShaderType& other) const { return !(*this == other); }
};

// 预定义类型
namespace types {
    constexpr ShaderType Float = ShaderType(BaseType::Float);
    constexpr ShaderType Float2 = ShaderType(BaseType::Float, VectorSize::Vec2);
    constexpr ShaderType Float3 = ShaderType(BaseType::Float, VectorSize::Vec3);
    constexpr ShaderType Float4 = ShaderType(BaseType::Float, VectorSize::Vec4);
    
    constexpr ShaderType Int = ShaderType(BaseType::Int);
    constexpr ShaderType Int2 = ShaderType(BaseType::Int, VectorSize::Vec2);
    constexpr ShaderType Int3 = ShaderType(BaseType::Int, VectorSize::Vec3);
    constexpr ShaderType Int4 = ShaderType(BaseType::Int, VectorSize::Vec4);
    
    constexpr ShaderType UInt = ShaderType(BaseType::UInt);
    constexpr ShaderType UInt2 = ShaderType(BaseType::UInt, VectorSize::Vec2);
    constexpr ShaderType UInt3 = ShaderType(BaseType::UInt, VectorSize::Vec3);
    constexpr ShaderType UInt4 = ShaderType(BaseType::UInt, VectorSize::Vec4);
    
    constexpr ShaderType Bool = ShaderType(BaseType::Bool);
    constexpr ShaderType Bool2 = ShaderType(BaseType::Bool, VectorSize::Vec2);
    constexpr ShaderType Bool3 = ShaderType(BaseType::Bool, VectorSize::Vec3);
    constexpr ShaderType Bool4 = ShaderType(BaseType::Bool, VectorSize::Vec4);
    
    constexpr ShaderType Mat2 = ShaderType(BaseType::Float, MatrixType(2, 2));
    constexpr ShaderType Mat3 = ShaderType(BaseType::Float, MatrixType(3, 3));
    constexpr ShaderType Mat4 = ShaderType(BaseType::Float, MatrixType(4, 4));
    constexpr ShaderType Mat2x3 = ShaderType(BaseType::Float, MatrixType(2, 3));
    constexpr ShaderType Mat2x4 = ShaderType(BaseType::Float, MatrixType(2, 4));
    constexpr ShaderType Mat3x2 = ShaderType(BaseType::Float, MatrixType(3, 2));
    constexpr ShaderType Mat3x4 = ShaderType(BaseType::Float, MatrixType(3, 4));
    constexpr ShaderType Mat4x2 = ShaderType(BaseType::Float, MatrixType(4, 2));
    constexpr ShaderType Mat4x3 = ShaderType(BaseType::Float, MatrixType(4, 3));
    
    constexpr ShaderType Void = ShaderType(BaseType::Void);
}

/**
 * 类型特性 - 用于编译期类型检查
 */
template<typename T>
struct IsShaderType : std::false_type {};

// C++ 类型到着色器类型的映射
template<> struct IsShaderType<float> : std::true_type {
    static constexpr ShaderType value = types::Float;
};

template<> struct IsShaderType<int> : std::true_type {
    static constexpr ShaderType value = types::Int;
};

template<> struct IsShaderType<unsigned int> : std::true_type {
    static constexpr ShaderType value = types::UInt;
};

template<> struct IsShaderType<bool> : std::true_type {
    static constexpr ShaderType value = types::Bool;
};

// 向量类型（需要自定义向量类）
// template<> struct IsShaderType<vec2> : std::true_type { ... };
// template<> struct IsShaderType<vec3> : std::true_type { ... };
// template<> struct IsShaderType<vec4> : std::true_type { ... };

/**
 * 类型转换和验证
 */
class TypeConverter {
public:
    // 解析类型字符串 (e.g., "vec3", "mat4", "sampler2D")
    static ShaderType parse(const std::string& type_str);
    
    // 类型兼容性检查
    static bool canConvert(const ShaderType& from, const ShaderType& to);
    
    // 自动类型提升（如 float + int -> float）
    static ShaderType promote(const ShaderType& a, const ShaderType& b);
    
    // 生成 GLSL 类型名
    static std::string toGLSL(const ShaderType& type);
    
    // 生成 HLSL 类型名
    static std::string toHLSL(const ShaderType& type);
    
    // 生成 Metal 类型名
    static std::string toMetal(const ShaderType& type);
};

} // namespace hgl::shader_next
