#pragma once

#include<hgl/actor/Entity.h>
#include<hgl/actor/TransformComponent.h>
#include<hgl/actor/TransformStorage.h>

#define ACTOR_NAMESPACE         hgl::actor
#define ACTOR_NAMESPACE_BEGIN   namespace ACTOR_NAMESPACE {
#define ACTOR_NAMESPACE_END     }

ACTOR_NAMESPACE_BEGIN

/**
 * Actor系统
 * 
 * 本Actor系统采用类似Frostbite Engine的ECS设计：
 * - Entity: 游戏对象，包含TransformComponent索引
 * - TransformComponent: Transform组件数据结构
 * - TransformStorage: SOA方式存储所有Transform数据
 * 
 * 详细文档请参见: doc/ECS_System.md
 */
class Actor
{
};

ACTOR_NAMESPACE_END
