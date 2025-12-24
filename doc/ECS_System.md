# Frostbite-like ECS System

本项目实现了一套类似寒霜引擎(Frostbite Engine)的ECS(Entity Component System)系统。

## 组件说明

### Entity (实体)
`Entity`是基本的游戏对象，包含：
- `entity_id`: 实体的唯一标识符
- `transform_index`: TransformComponent在TransformStorage中的索引（-1表示没有Transform）

#### 主要方法
- `GetEntityID()`: 获取实体ID
- `GetTransformIndex()`: 获取Transform组件的索引
- `SetTransformIndex(int)`: 设置Transform组件的索引
- `HasTransform()`: 检查实体是否有Transform组件

### TransformComponent (变换组件)
`TransformComponent`表示一个变换组件的数据结构，包含：
- `position`: 位置 (Vector3f)
- `rotation`: 旋转 (Vector4f，表示四元数 x, y, z, w)
- `scale`: 缩放 (Vector3f)

### TransformStorage (变换存储)
`TransformStorage`使用SOA(Structure of Arrays)方式存储Transform数据，以提高缓存命中率和并行处理效率。

#### SOA布局
数据按属性分组存储在独立的连续数组中：
- `positions`: 所有实体的位置数组
- `rotations`: 所有实体的旋转数组
- `scales`: 所有实体的缩放数组

#### 主要方法
- `AddTransform(position, rotation, scale)`: 添加一个Transform，返回索引
- `RemoveTransform(index)`: 删除指定索引的Transform（支持槽位复用）
- `GetPosition(index)` / `SetPosition(index, position)`: 获取/设置位置
- `GetRotation(index)` / `SetRotation(index, rotation)`: 获取/设置旋转
- `GetScale(index)` / `SetScale(index, scale)`: 获取/设置缩放
- `GetPositions()` / `GetRotations()` / `GetScales()`: 获取SOA数组指针（用于批量处理）
- `GetCapacity()`: 获取总容量（包括已删除的槽位）
- `GetActiveCount()`: 获取活跃Transform数量
- `IsValidIndex(index)`: 检查索引是否有效且未被删除
- `Clear()`: 清空所有数据

## 特性

1. **SOA数据布局**: 所有Transform数据以SOA方式存储，提高缓存效率
2. **槽位复用**: 删除的Transform槽位可以被新Transform复用，减少内存碎片
3. **批量访问**: 可直接访问底层数组进行批量处理
4. **索引引用**: Entity通过整数索引引用Transform，而不是指针

## 使用示例

```cpp
#include<hgl/actor/Entity.h>
#include<hgl/actor/TransformComponent.h>
#include<hgl/actor/TransformStorage.h>

using namespace hgl::actor;

// 创建TransformStorage
TransformStorage storage;

// 创建Entity
Entity entity(1);

// 添加Transform
Vector3f pos(10.0f, 20.0f, 30.0f);
Vector4f rot(0.0f, 0.0f, 0.0f, 1.0f);  // 单位四元数
Vector3f scale(1.0f, 1.0f, 1.0f);

int transform_index = storage.AddTransform(pos, rot, scale);
entity.SetTransformIndex(transform_index);

// 访问和修改Transform
Vector3f new_pos(100.0f, 200.0f, 300.0f);
storage.SetPosition(transform_index, new_pos);

// 批量处理（SOA优势）
Vector3f* positions = storage.GetPositions();
for (int i = 0; i < storage.GetCapacity(); ++i) {
    if (storage.IsValidIndex(i)) {
        // 处理位置数据...
    }
}
```

## 测试

运行测试程序：
```bash
g++ -I./inc -std=c++17 example/Basic/ECS_Test.cpp -o ecs_test
./ecs_test
```

## 设计理念

本ECS系统参考了Frostbite Engine的设计：
- **数据导向设计(Data-Oriented Design)**: SOA布局提高缓存命中率
- **组件索引而非指针**: 使用整数索引便于序列化和多线程访问
- **内存管理优化**: 槽位复用减少内存分配和碎片

## 文件结构

```
inc/hgl/actor/
├── Entity.h              # Entity类定义
├── TransformComponent.h  # TransformComponent结构定义
└── TransformStorage.h    # TransformStorage类定义（SOA存储）

example/Basic/
└── ECS_Test.cpp         # ECS系统测试程序
```
