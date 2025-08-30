/**
 * InstancedMeshComponent 设计文档
 * 
 * 本文件描述了InstancedMeshComponent系统的设计理念和使用方法
 */

#pragma once

/**
 * 设计目标
 * ========
 * 
 * 原有的MaterialRenderList系统：
 * - 自动收集具有相同材质的MeshComponent
 * - 内部管理AssignBuffer（LocalToWorld矩阵ID，MaterialInstance ID）
 * - 通过InstanceRate更新的Vertex Attribute传递数据
 * - 走Instance/Indirect渲染路径
 * 
 * 新的InstancedMeshComponent系统：
 * - 直接手动管理AssignBuffer
 * - 不通过MaterialRenderList，有专用的渲染类
 * - 更直接的控制和更手动的功能
 * - 适合需要精确控制实例化渲染的场景
 */

/**
 * 核心组件
 * ========
 * 
 * 1. InstancedMeshComponent
 *    - 继承自RenderComponent
 *    - 直接管理实例数据（LocalToWorld矩阵，MaterialInstance）
 *    - 管理自己的VAB和缓冲区
 *    - 提供添加/删除/更新实例的接口
 * 
 * 2. InstancedMeshRenderer
 *    - 专用的渲染器，不使用MaterialRenderList
 *    - 直接处理InstancedMeshComponent的渲染
 *    - 管理VAB绑定和绘制调用
 * 
 * 3. InstanceData
 *    - 单个实例的数据结构
 *    - 包含LocalToWorld变换矩阵和MaterialInstance指针
 */

/**
 * 使用流程
 * ========
 * 
 * 1. 创建InstancedMeshComponent
 *    ```cpp
 *    InstancedMeshComponentManager* manager = InstancedMeshComponentManager::GetDefaultManager();
 *    InstancedMeshComponent* comp = manager->CreateComponent(mesh, max_instances);
 *    comp->SetVulkanDevice(device);
 *    ```
 * 
 * 2. 添加实例数据
 *    ```cpp
 *    Matrix4f transform = Matrix4f::Identity();
 *    transform.SetTranslate(Vector3f(x, y, z));
 *    comp->AddInstance(transform, material_instance);
 *    ```
 * 
 * 3. 渲染
 *    ```cpp
 *    InstancedMeshRenderer* renderer = new InstancedMeshRenderer(device);
 *    renderer->Render(comp, command_buffer);
 *    ```
 * 
 * 4. 动态更新
 *    ```cpp
 *    comp->UpdateInstance(index, new_transform, new_material);
 *    comp->RemoveInstance(index);
 *    ```
 */

/**
 * 与MaterialRenderList的区别
 * ========================
 * 
 * MaterialRenderList方式：
 * ```cpp
 * // 创建多个MeshComponent
 * MeshComponent* comp1 = manager->CreateComponent(mesh);
 * MeshComponent* comp2 = manager->CreateComponent(mesh);
 * MeshComponent* comp3 = manager->CreateComponent(mesh);
 * 
 * // 添加到MaterialRenderList（自动管理）
 * MaterialRenderList* render_list = new MaterialRenderList(device, true, pipeline_index);
 * render_list->Add(comp1);
 * render_list->Add(comp2);
 * render_list->Add(comp3);
 * 
 * // 渲染（内部管理AssignBuffer）
 * render_list->Render(command_buffer);
 * ```
 * 
 * InstancedMeshComponent方式：
 * ```cpp
 * // 创建一个InstancedMeshComponent
 * InstancedMeshComponent* comp = manager->CreateComponent(mesh, 100);
 * 
 * // 直接添加实例数据
 * comp->AddInstance(transform1, material1);
 * comp->AddInstance(transform2, material2);
 * comp->AddInstance(transform3, material3);
 * 
 * // 使用专用渲染器渲染
 * InstancedMeshRenderer* renderer = new InstancedMeshRenderer(device);
 * renderer->Render(comp, command_buffer);
 * ```
 */

/**
 * 适用场景
 * ========
 * 
 * InstancedMeshComponent适合：
 * - 需要精确控制实例化渲染的场景
 * - 大量相同网格体但不同变换/材质的渲染
 * - 动态添加/删除实例的场景
 * - 不希望使用MaterialRenderList自动化管理的场景
 * 
 * MaterialRenderList适合：
 * - 传统的场景图渲染
 * - 需要自动化管理的场景
 * - 复杂的材质和管线管理
 * - 与现有SceneNode系统紧密集成的场景
 */

/**
 * 技术细节
 * ========
 * 
 * 1. AssignBuffer管理
 *    - InstancedMeshComponent直接管理自己的VAB
 *    - 格式：RG16UI (R存LocalToWorld ID，G存MaterialInstance ID)
 *    - 支持动态更新，但需要手动调用UpdateBuffers()
 * 
 * 2. 内存管理
 *    - 预分配最大实例数量的缓冲区
 *    - 支持动态增长（可选实现）
 *    - 自动清理GPU资源
 * 
 * 3. 渲染优化
 *    - 支持索引绘制和非索引绘制
 *    - 可以批量渲染多个InstancedMeshComponent
 *    - 最小化状态切换
 */