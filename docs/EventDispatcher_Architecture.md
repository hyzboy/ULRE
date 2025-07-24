# EventDispatcher Architecture Documentation

## Overview

The EventDispatcher architecture implements a hierarchical event system for the ULRE (Unified Low-level Rendering Engine) that follows the chain:

**RenderFramework → Scene → SceneNode**

This design allows for organized event handling where each level can process events and decide whether to continue propagation to child components.

## Architecture Components

### 1. EventDispatcher Interface

The base interface that all event-handling components implement:

```cpp
class EventDispatcher
{
public:
    virtual bool DispatchEvent(const device_input::InputEvent& event) = 0;
    virtual void AddEventListener(EventDispatcher* listener) = 0;
    virtual void RemoveEventListener(EventDispatcher* listener) = 0;

protected:
    virtual bool HandleEvent(const device_input::InputEvent& event) = 0;
};
```

### 2. BaseEventDispatcher

Provides the default implementation with listener management:

```cpp
class BaseEventDispatcher : public EventDispatcher
{
protected:
    ObjectList<EventDispatcher> listeners;

public:
    bool DispatchEvent(const device_input::InputEvent& event) override;
    void AddEventListener(EventDispatcher* listener) override;
    void RemoveEventListener(EventDispatcher* listener) override;

protected:
    virtual bool HandleEvent(const device_input::InputEvent& event) override { return false; }
};
```

### 3. RenderFramework

The top-level event entry point:

```cpp
class RenderFramework : public BaseEventDispatcher
{
protected:
    Scene* active_scene;

public:
    void SetActiveScene(Scene* scene);
    bool ProcessInputEvent(const device_input::InputEvent& event);
    virtual void Update(float deltaTime);

protected:
    bool HandleEvent(const device_input::InputEvent& event) override;
};
```

### 4. Scene

Manages the scene graph and forwards events to scene nodes:

```cpp
class Scene : public BaseEventDispatcher
{
protected:
    SceneNode* root_node;
    Camera* active_camera;
    ObjectList<SceneNode> scene_nodes;

public:
    void AddSceneNode(SceneNode* node);
    void RemoveSceneNode(SceneNode* node);
    virtual void Update(float deltaTime);

protected:
    bool HandleEvent(const device_input::InputEvent& event) override;
    void DispatchEventToNodes(const device_input::InputEvent& event);
};
```

### 5. SceneNode

Enhanced scene graph nodes with event handling:

```cpp
class SceneNode : public SceneOrient, public BaseEventDispatcher
{
public:
    ObjectList<SceneNode> SubNode;
    
    virtual bool ProcessEvent(const device_input::InputEvent& event);

protected:
    bool HandleEvent(const device_input::InputEvent& event) override;
};
```

## Event Flow

1. **Input Reception**: The application framework (e.g., VulkanApplicationFramework) receives input events
2. **Framework Processing**: RenderFramework.ProcessInputEvent() is called
3. **Scene Distribution**: Active scene receives the event and processes it
4. **Node Propagation**: Scene forwards the event to all registered scene nodes
5. **Hierarchical Traversal**: Each scene node can handle the event and/or forward it to child nodes

## Usage Examples

### Basic Setup

```cpp
// Create the event hierarchy
RenderFramework framework;
Scene* scene = new Scene();
SceneNode* gameObject = new SceneNode();

// Build the connection
framework.SetActiveScene(scene);
scene->AddSceneNode(gameObject);

// Dispatch events
device_input::InputEvent event;
event.type = MOUSE_MOVE;
event.mouse_move.x = 100;
event.mouse_move.y = 200;

bool handled = framework.ProcessInputEvent(event);
```

### Custom Event Handlers

```cpp
class GameObjectNode : public SceneNode
{
protected:
    bool HandleEvent(const device_input::InputEvent& event) override
    {
        if (event.type == MOUSE_CLICK)
        {
            // Handle mouse click on this game object
            OnMouseClick(event.mouse_click.button);
            return true; // Stop event propagation
        }
        
        // Call parent to continue propagation to children
        return SceneNode::HandleEvent(event);
    }
    
private:
    void OnMouseClick(int button)
    {
        // Custom click handling logic
    }
};
```

### Integration with VulkanApplicationFramework

```cpp
class MyVulkanApp : public VulkanApplicationFramework
{
private:
    RenderFramework* render_framework;

public:
    bool Init(int w, int h) override
    {
        if (!VulkanApplicationFramework::Init(w, h))
            return false;
            
        // Initialize event system
        render_framework = new RenderFramework();
        
        // Setup scene
        Scene* scene = new Scene();
        render_framework->SetActiveScene(scene);
        
        return true;
    }
    
    // Forward input events to the event system
    void OnMouseMove(int x, int y) override
    {
        device_input::InputEvent event;
        event.type = MOUSE_MOVE;
        event.mouse_move.x = x;
        event.mouse_move.y = y;
        
        if (render_framework)
            render_framework->ProcessInputEvent(event);
            
        // Continue with existing mouse handling
        VulkanApplicationFramework::OnMouseMove(x, y);
    }
};
```

## Event Types

The system supports various input events defined in `hgl::device_input::InputEvent`:

- **Mouse Events**: Movement, clicks, wheel
- **Keyboard Events**: Key press/release
- **Joystick Events**: Axis movement, button press/release
- **Wacom/Tablet Events**: Stylus input with pressure
- **Character Input**: Text input events

## Event Handling Best Practices

### 1. Event Consumption
Return `true` from `HandleEvent()` to stop event propagation:

```cpp
bool HandleEvent(const device_input::InputEvent& event) override
{
    if (ShouldHandleThisEvent(event))
    {
        ProcessEvent(event);
        return true; // Event consumed, stop propagation
    }
    
    return false; // Continue propagation
}
```

### 2. Hierarchical Processing
Always call parent implementation for proper child node propagation:

```cpp
bool HandleEvent(const device_input::InputEvent& event) override
{
    // Process at this level first
    bool handled = MyCustomEventHandling(event);
    
    if (!handled)
    {
        // Continue to children
        handled = SceneNode::HandleEvent(event);
    }
    
    return handled;
}
```

### 3. Event Filtering
Filter events at higher levels to reduce processing:

```cpp
// In Scene::HandleEvent()
bool HandleEvent(const device_input::InputEvent& event) override
{
    // Filter out events that shouldn't reach scene nodes
    if (event.type == SYSTEM_ONLY_EVENT)
        return false; // Don't propagate to nodes
        
    DispatchEventToNodes(event);
    return false;
}
```

## Performance Considerations

- **Event Filtering**: Filter events early in the hierarchy to avoid unnecessary processing
- **Selective Propagation**: Use event consumption to stop propagation when appropriate
- **Listener Management**: Remove unused listeners to avoid unnecessary event processing
- **Batch Processing**: Consider batching similar events for better performance

## Thread Safety

The current implementation is **not thread-safe**. If you need to dispatch events from multiple threads:

1. Use mutex protection around event dispatch calls
2. Consider a queue-based system for cross-thread event handling
3. Ensure listener management is thread-safe

## Integration Points

### With Existing Framework
- **VulkanApplicationFramework**: Use helper functions to integrate with existing event handling
- **GUI System**: Can be integrated as another event listener in the hierarchy
- **Physics System**: Scene nodes can forward physics-relevant events

### Extension Points
- **Custom Event Types**: Extend `InputEvent` union for application-specific events
- **Event Filters**: Implement custom filtering logic at any hierarchy level
- **Event Transformation**: Transform events as they propagate (e.g., coordinate space conversion)

## Files Overview

- `inc/hgl/graph/EventDispatcher.h` - Core interface and base implementation
- `inc/hgl/graph/RenderFramework.h` - Top-level framework class  
- `inc/hgl/graph/Scene.h` - Scene management with event handling
- `inc/hgl/graph/SceneNode.h` - Enhanced scene node with events (modified)
- `src/SceneGraph/RenderFramework.cpp` - Framework implementation
- `src/SceneGraph/Scene.cpp` - Scene implementation  
- `src/SceneGraph/SceneNode.cpp` - Node event handling (modified)
- `example/test_event_dispatcher.cpp` - Usage example
- `example/common/VulkanAppWithEventDispatcher.h` - Integration guide