# EventDispatcher 独占模式设计文档

## 概述

本文档描述了为ULRE渲染引擎设计的EventDispatcher独占模式功能。该功能允许某个EventDispatcher在更高级别暂时处于独占状态，从而可以拦截和独占处理所有事件，而不是按照正常的层级传递。

## 问题陈述

原有的EventDispatcher系统采用层级传递的方式：
```
Window -> RenderFramework -> Scene -> SceneNode
```

但在某些场景下，需要某个EventDispatcher能够在更高级别暂时处于独占状态，例如：
- 游戏中弹出对话框时，需要独占所有输入事件
- 摄像机控制时，需要独占鼠标事件
- 调试模式下，调试界面需要独占所有事件

## 设计方案

### 核心概念

1. **独占模式(Exclusive Mode)**: 当某个EventDispatcher处于独占模式时，其父级EventDispatcher只会将事件传递给该独占EventDispatcher，而不会传递给其他子EventDispatcher。

2. **更高级别独占(Higher Level Exclusive)**: 允许EventDispatcher请求在其父级或更高级别的EventDispatcher中设置独占模式。

### 关键接口

#### EventDispatcher基类
```cpp
class EventDispatcher
{
    // 设置独占模式
    virtual bool SetExclusiveMode(EventDispatcher* dispatcher);
    
    // 退出独占模式
    virtual void ExitExclusiveMode();
    
    // 请求在更高级别处于独占状态
    virtual bool RequestExclusiveAtHigherLevel(int levels = 1);
    
    // 释放在更高级别的独占状态
    virtual void ReleaseExclusiveAtHigherLevel(int levels = 1);
    
    // 查询独占状态
    bool IsExclusiveMode() const;
    EventDispatcher* GetExclusiveDispatcher() const;
};
```

### 工作原理

1. **正常模式**: 事件从父EventDispatcher传递给所有子EventDispatcher
2. **独占模式**: 事件只传递给被设置为独占的子EventDispatcher
3. **更高级别独占**: 通过`RequestExclusiveAtHigherLevel()`方法，可以在指定级别的父EventDispatcher中设置独占模式

### 使用场景

#### 场景1: 对话框独占
```cpp
class DialogEventHandler : public io::WindowEvent
{
public:
    void OpenDialog()
    {
        // 在父级别设置独占，游戏主循环不会收到事件
        RequestExclusiveAtHigherLevel(1);
    }
    
    void CloseDialog()
    {
        // 释放独占状态
        ReleaseExclusiveAtHigherLevel(1);
    }
};
```

#### 场景2: 摄像机控制独占
```cpp
class CameraController : public io::MouseEvent
{
public:
    void StartCameraControl()
    {
        // 在更高级别独占鼠标事件
        RequestExclusiveAtHigherLevel(2);
    }
    
    void StopCameraControl()
    {
        ReleaseExclusiveAtHigherLevel(2);
    }
};
```

### 层级结构示例

```
Window (Level 0)
└── RenderFramework (Level 1)
    └── Scene (Level 2)
        ├── GameLogic (Level 3)
        ├── UIDialog (Level 3)
        └── CameraController (Level 3)
```

当UIDialog调用`RequestExclusiveAtHigherLevel(1)`时：
- Scene (Level 2) 设置UIDialog为独占EventDispatcher
- GameLogic和CameraController将不会收到事件
- 只有UIDialog能够处理事件

当CameraController调用`RequestExclusiveAtHigherLevel(2)`时：
- RenderFramework (Level 1) 设置Scene为独占EventDispatcher
- 其他Scene将不会收到事件
- 在Scene内部，CameraController仍需要设置自己的独占模式

## 实现细节

### 数据结构
```cpp
class EventDispatcher
{
protected:
    ObjectList<EventDispatcher> child_dispatchers;  // 子事件分发器列表
    EventDispatcher* parent_dispatcher = nullptr;   // 父事件分发器
    EventDispatcher* exclusive_dispatcher = nullptr; // 独占模式的事件分发器
    bool is_exclusive_mode = false;                 // 是否处于独占模式
};
```

### 事件分发逻辑
```cpp
template<typename EventType>
void DispatchToChildren(const EventType& event)
{
    if (is_exclusive_mode && exclusive_dispatcher)
    {
        // 独占模式下只分发给独占分发器
        exclusive_dispatcher->HandleEvent(event);
    }
    else
    {
        // 正常模式下分发给所有子分发器
        for (EventDispatcher* child : child_dispatchers)
        {
            child->HandleEvent(event);
        }
    }
}
```

## 线程安全

当前实现假设所有EventDispatcher操作都在主线程中进行。如果需要多线程支持，需要添加适当的锁机制。

## 性能考虑

1. 独占模式的切换开销很小，主要是设置几个指针和布尔值
2. 事件分发时的额外开销是一个布尔值检查
3. 内存开销是每个EventDispatcher增加两个指针和一个布尔值

## 扩展性

该设计支持以下扩展：
1. 多重独占模式（一个EventDispatcher可以在多个级别设置独占）
2. 条件独占模式（基于事件类型的独占）
3. 临时独占模式（基于时间的独占）

## 使用示例

完整的使用示例请参考：`inc/hgl/example/ExclusiveEventDispatcherExample.h`

## 测试

可以运行以下测试程序来验证功能：
```bash
./example/EventDispatcherExclusiveTest
```