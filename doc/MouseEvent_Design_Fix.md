# MouseEvent Design Fix Documentation

## Problem Statement

The original MouseEvent system had design issues:
1. `MouseEventData` only contained position data, missing both button and action information
2. `MouseEventID` was not a descriptive name - `MouseAction` is more appropriate
3. The design needed to separate button (which button) from action (what action)

## Solution

### 1. Renamed MouseEventID to MouseAction
- More descriptive name that clearly indicates what it represents
- Provides type alias `using MouseEventID = MouseAction` for backward compatibility

### 2. Separated Button and Action Design

**Key Design Principle**: Keep button and action separated for clarity and flexibility.

**MouseAction** - What action is being performed:
- `Move` - Mouse movement
- `Down` - Button pressed down
- `Up` - Button released
- `Wheel` - Mouse wheel scrolled

**MouseButton** - Which button is involved:
- `Left` - Left mouse button
- `Right` - Right mouse button
- `Middle` - Middle mouse button

### 3. Final MouseEventData Structure

```cpp
struct MouseEventData {
    MouseAction action;     // What action (Move, Down, Up, Wheel)
    MouseButton button;     // Which button (Left, Right, Middle)
    int16 x;                // X coordinate
    int16 y;                // Y coordinate
    int16 wheel_delta;      // Wheel delta
};
```

### 4. Why This Design?

**Separation of Concerns:**
- `action` tells you WHAT happened (Down, Up, Move, Wheel)
- `button` tells you WHICH button (Left, Right, Middle)

**Benefits:**
- Clear and intuitive
- Easy to add new actions without changing button definitions
- Easy to add new buttons without changing action definitions
- Consistent with general event handling patterns

**Example Usage:**
```cpp
if (action == MouseAction::Down && button == MouseButton::Left) {
    // Left button was pressed
}
if (action == MouseAction::Up && button == MouseButton::Right) {
    // Right button was released
}
```

### 5. Event System Architecture

Created complete event system headers:
- `EventDispatcher.h` - Base event dispatcher interface with EventProcResult, InputEventSource, EventHeader
- `MouseEvent.h` - Mouse event types, data structures, and MouseEvent base class
- `KeyboardEvent.h` - Keyboard event types, data structures, and KeyboardStateEvent base class
- `WindowEvent.h` - Window event base class

### 6. Updated Code

**InputSystem.cpp:**
```cpp
io::MouseAction action = io::MouseAction(header.id);
const io::MouseEventData *med = (const io::MouseEventData *)&data;
io::MouseButton button = med->button;

switch (action) {
    case io::MouseAction::Down:
        // Use button to determine which button was pressed
        break;
    case io::MouseAction::Up:
        // Use button to determine which button was released
        break;
    // ...
}
```

**RenderFramework.cpp:**
- Unchanged - only uses Move action which doesn't require button information

## Benefits

1. **Clear Separation**: Button and action are independent concepts
2. **Flexible Design**: Easy to extend with new actions or buttons
3. **Better Naming**: MouseAction is more descriptive than MouseEventID
4. **Backward Compatible**: Type alias ensures existing code using MouseEventID still works
5. **Complete Event System**: Full event handling infrastructure is now in place
6. **Type Safety**: Strongly typed enums prevent accidental misuse
7. **Intuitive API**: Natural to check both action and button separately

## Files Created

- `inc/hgl/io/event/EventDispatcher.h`
- `inc/hgl/io/event/MouseEvent.h`
- `inc/hgl/io/event/KeyboardEvent.h`
- `inc/hgl/io/event/WindowEvent.h`

## Files Modified

- `src/ecs/InputSystem.cpp`
- `src/SceneGraph/RenderFramework.cpp`
