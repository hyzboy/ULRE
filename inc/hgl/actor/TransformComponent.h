#pragma once

#define TRANSFORM_COMPONENT_NAMESPACE         hgl::actor
#define TRANSFORM_COMPONENT_NAMESPACE_BEGIN   namespace TRANSFORM_COMPONENT_NAMESPACE {
#define TRANSFORM_COMPONENT_NAMESPACE_END     }

TRANSFORM_COMPONENT_NAMESPACE_BEGIN

// 简单的Vector3f结构，用于ECS系统
struct Vector3f
{
    float x, y, z;
    
    Vector3f() : x(0), y(0), z(0) {}
    Vector3f(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

// 简单的Vector4f结构，用于表示四元数
struct Vector4f
{
    float x, y, z, w;
    
    Vector4f() : x(0), y(0), z(0), w(1) {}
    Vector4f(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};

/**
 * TransformComponent
 * 表示一个Transform组件的数据，包含位置、旋转和缩放
 * 在Frostbite风格的ECS中，实际数据存储在TransformStorage中
 */
struct TransformComponent
{
    Vector3f position;      ///<位置
    Vector4f rotation;      ///<旋转(四元数表示: x, y, z, w)
    Vector3f scale;         ///<缩放
    
    TransformComponent() 
        : position(0, 0, 0)
        , rotation(0, 0, 0, 1)  // 单位四元数
        , scale(1, 1, 1) 
    {}
    
    TransformComponent(const Vector3f& pos, const Vector4f& rot, const Vector3f& scl)
        : position(pos)
        , rotation(rot)
        , scale(scl)
    {}
};

TRANSFORM_COMPONENT_NAMESPACE_END
