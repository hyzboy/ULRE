/**
 * ECS System Test - Frostbite风格的Entity Component System测试
 * 
 * 本测试演示了Entity、TransformComponent和TransformStorage的基本使用方法
 */

#include<hgl/actor/Entity.h>
#include<hgl/actor/TransformComponent.h>
#include<hgl/actor/TransformStorage.h>
#include<iostream>

using namespace hgl::actor;

int main()
{
    std::cout << "===== Frostbite-like ECS System Test =====" << std::endl << std::endl;
    
    // 创建TransformStorage (SOA存储)
    TransformStorage storage;
    
    std::cout << "1. 创建TransformStorage" << std::endl;
    std::cout << "   初始容量: " << storage.GetCapacity() << std::endl;
    std::cout << "   活跃数量: " << storage.GetActiveCount() << std::endl << std::endl;
    
    // 创建Entity并添加Transform
    Entity entity1(1);
    std::cout << "2. 创建Entity (ID=1)" << std::endl;
    std::cout << "   是否有Transform: " << (entity1.HasTransform() ? "是" : "否") << std::endl;
    std::cout << "   Transform索引: " << entity1.GetTransformIndex() << std::endl << std::endl;
    
    // 为Entity添加Transform
    Vector3f pos1(10.0f, 20.0f, 30.0f);
    Vector4f rot1(0.0f, 0.0f, 0.0f, 1.0f);  // 单位四元数
    Vector3f scale1(1.0f, 1.0f, 1.0f);
    
    int transform_index1 = storage.AddTransform(pos1, rot1, scale1);
    entity1.SetTransformIndex(transform_index1);
    
    std::cout << "3. 为Entity添加Transform" << std::endl;
    std::cout << "   Transform索引: " << entity1.GetTransformIndex() << std::endl;
    std::cout << "   是否有Transform: " << (entity1.HasTransform() ? "是" : "否") << std::endl;
    std::cout << "   位置: (" << storage.GetPosition(transform_index1).x << ", "
              << storage.GetPosition(transform_index1).y << ", "
              << storage.GetPosition(transform_index1).z << ")" << std::endl;
    std::cout << "   Storage容量: " << storage.GetCapacity() << std::endl;
    std::cout << "   Storage活跃数量: " << storage.GetActiveCount() << std::endl << std::endl;
    
    // 创建第二个Entity
    Entity entity2(2);
    Vector3f pos2(100.0f, 200.0f, 300.0f);
    Vector4f rot2(0.0f, 0.707f, 0.0f, 0.707f);  // Y轴旋转90度
    Vector3f scale2(2.0f, 2.0f, 2.0f);
    
    int transform_index2 = storage.AddTransform(pos2, rot2, scale2);
    entity2.SetTransformIndex(transform_index2);
    
    std::cout << "4. 创建第二个Entity (ID=2)" << std::endl;
    std::cout << "   Transform索引: " << entity2.GetTransformIndex() << std::endl;
    std::cout << "   位置: (" << storage.GetPosition(transform_index2).x << ", "
              << storage.GetPosition(transform_index2).y << ", "
              << storage.GetPosition(transform_index2).z << ")" << std::endl;
    std::cout << "   缩放: (" << storage.GetScale(transform_index2).x << ", "
              << storage.GetScale(transform_index2).y << ", "
              << storage.GetScale(transform_index2).z << ")" << std::endl;
    std::cout << "   Storage容量: " << storage.GetCapacity() << std::endl;
    std::cout << "   Storage活跃数量: " << storage.GetActiveCount() << std::endl << std::endl;
    
    // 修改Transform数据
    Vector3f new_pos(500.0f, 600.0f, 700.0f);
    storage.SetPosition(transform_index1, new_pos);
    
    std::cout << "5. 修改Entity1的位置" << std::endl;
    std::cout << "   新位置: (" << storage.GetPosition(transform_index1).x << ", "
              << storage.GetPosition(transform_index1).y << ", "
              << storage.GetPosition(transform_index1).z << ")" << std::endl << std::endl;
    
    // SOA访问演示 - 批量处理
    std::cout << "6. SOA批量访问演示" << std::endl;
    std::cout << "   直接访问位置数组:" << std::endl;
    const Vector3f* positions = storage.GetPositions();
    if (positions)
    {
        for (int i = 0; i < storage.GetCapacity(); ++i)
        {
            if (storage.IsValidIndex(i))
            {
                std::cout << "   Index[" << i << "]: ("
                          << positions[i].x << ", "
                          << positions[i].y << ", "
                          << positions[i].z << ")" << std::endl;
            }
        }
    }
    std::cout << std::endl;
    
    // 删除Transform
    storage.RemoveTransform(transform_index1);
    entity1.SetTransformIndex(-1);
    
    std::cout << "7. 删除Entity1的Transform" << std::endl;
    std::cout << "   Entity1是否有Transform: " << (entity1.HasTransform() ? "是" : "否") << std::endl;
    std::cout << "   Storage容量: " << storage.GetCapacity() << std::endl;
    std::cout << "   Storage活跃数量: " << storage.GetActiveCount() << std::endl;
    std::cout << "   Index[" << transform_index1 << "]是否有效: " 
              << (storage.IsValidIndex(transform_index1) ? "是" : "否") << std::endl << std::endl;
    
    // 创建第三个Entity，测试槽位复用
    Entity entity3(3);
    Vector3f pos3(1000.0f, 2000.0f, 3000.0f);
    Vector4f rot3(0.0f, 0.0f, 0.0f, 1.0f);
    Vector3f scale3(3.0f, 3.0f, 3.0f);
    
    int transform_index3 = storage.AddTransform(pos3, rot3, scale3);
    entity3.SetTransformIndex(transform_index3);
    
    std::cout << "8. 创建第三个Entity (测试槽位复用)" << std::endl;
    std::cout << "   Transform索引: " << entity3.GetTransformIndex() << std::endl;
    std::cout << "   位置: (" << storage.GetPosition(transform_index3).x << ", "
              << storage.GetPosition(transform_index3).y << ", "
              << storage.GetPosition(transform_index3).z << ")" << std::endl;
    std::cout << "   Storage容量: " << storage.GetCapacity() << std::endl;
    std::cout << "   Storage活跃数量: " << storage.GetActiveCount() << std::endl << std::endl;
    
    std::cout << "===== 测试完成 =====" << std::endl;
    
    return 0;
}
