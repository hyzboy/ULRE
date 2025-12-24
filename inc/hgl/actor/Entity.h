#pragma once

#include<cstdint>

#define ENTITY_NAMESPACE         hgl::actor
#define ENTITY_NAMESPACE_BEGIN   namespace ENTITY_NAMESPACE {
#define ENTITY_NAMESPACE_END     }

ENTITY_NAMESPACE_BEGIN

/**
 * Entity类
 * 类似寒霜引擎的Entity系统，包含对TransformComponent的索引
 */
class Entity
{
private:
    uint32_t entity_id;                 ///<实体ID
    int transform_index;                ///<TransformComponent在TransformStorage中的索引，-1表示没有Transform

public:
    Entity() : entity_id(0), transform_index(-1) {}
    Entity(uint32_t id) : entity_id(id), transform_index(-1) {}
    Entity(uint32_t id, int transform_idx) : entity_id(id), transform_index(transform_idx) {}
    
    virtual ~Entity() = default;

public:
    uint32_t GetEntityID() const { return entity_id; }
    
    int GetTransformIndex() const { return transform_index; }
    void SetTransformIndex(int index) { transform_index = index; }
    
    bool HasTransform() const { return transform_index != -1; }
};

ENTITY_NAMESPACE_END
