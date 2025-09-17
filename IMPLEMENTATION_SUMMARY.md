# EventDispatcher 独占模式实现总结

## 已完成的功能

本次实现为ULRE渲染引擎添加了EventDispatcher独占模式功能，解决了"某一个EventPatcher需要在更高级别处暂时处于独占状态"的需求。

## 核心功能

### 1. 基础EventDispatcher类 (`inc/hgl/io/event/EventDispatcher.h`)
- 支持层级事件分发
- 独占模式管理
- 更高级别独占请求机制

### 2. 关键方法
```cpp
// 设置独占模式
bool SetExclusiveMode(EventDispatcher* dispatcher);

// 请求在更高级别处于独占状态
bool RequestExclusiveAtHigherLevel(int levels = 1);

// 释放独占状态
void ReleaseExclusiveAtHigherLevel(int levels = 1);
```

### 3. 事件处理类
- `WindowEvent`: 窗口事件基类，支持独占模式事件分发
- `MouseEvent`: 鼠标事件处理，包含坐标跟踪
- `KeyboardEvent`: 键盘事件处理，按键状态管理

## 使用场景

### 场景1: UI对话框独占
```cpp
class DialogEventHandler : public io::WindowEvent
{
    void OpenDialog()
    {
        // 在父级别设置独占，游戏主循环不会收到事件
        RequestExclusiveAtHigherLevel(1);
    }
};
```

### 场景2: 摄像机控制独占
```cpp
class CameraController : public io::MouseEvent
{
    void StartCameraControl()
    {
        // 在更高级别独占鼠标事件
        RequestExclusiveAtHigherLevel(2);
    }
};
```

## 测试验证

创建了完整的测试程序 (`example/EventDispatcherExclusiveTest.cpp`)，验证了：
- ✅ 正常事件传播
- ✅ 对话框独占模式
- ✅ 摄像机控制独占
- ✅ 多级独占模式交互

## 编译和运行

```bash
# 编译测试程序
g++ -std=c++17 -I inc example/EventDispatcherExclusiveTest.cpp -o test_exclusive_dispatcher

# 运行测试
./test_exclusive_dispatcher
```

## 设计优势

1. **最小化修改**: 不改变现有代码结构，只添加新功能
2. **向后兼容**: 现有EventDispatcher使用方式完全不变
3. **灵活性**: 支持多级独占，可以精确控制独占范围
4. **类型安全**: 使用模板和dynamic_cast确保类型安全
5. **易于使用**: 简单的API，清晰的语义

## 文档

详细设计文档请参考: `doc/EventDispatcher_Exclusive_Mode.md`

## 适用性

此实现独立于ULRE的submodule依赖，使用标准C++容器和类型，可以在当前仓库状态下直接编译和使用。