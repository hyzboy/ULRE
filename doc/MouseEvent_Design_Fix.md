# MouseEvent Design Fix Documentation

## Problem Statement

The original MouseEvent system had design issues:
1. `MouseEventData` only contained button field, but was missing the action/event type information
2. `MouseEventID` was not a descriptive name - `MouseAction` is more appropriate
3. `InputSystem.cpp` needed updating to handle the corrected structure

## Solution

### 1. Renamed MouseEventID to MouseAction
- More descriptive name that clearly indicates what it represents
- Provides type alias `using MouseEventID = MouseAction` for backward compatibility

### 2. Fixed MouseEventData Structure
**Before:**
```cpp
struct MouseEventData {
    // Only had button, x, y, wheel_delta
    // Missing the action type!
};
```

**After:**
```cpp
struct MouseEventData {
    MouseAction action;     // NEW: Action type field
    MouseButton button;     // Mouse button
    int16 x;                // X coordinate
    int16 y;                // Y coordinate
    int16 wheel_delta;      // Wheel delta
};
```

### 3. Event System Architecture

Created complete event system headers:
- `EventDispatcher.h` - Base event dispatcher interface with EventProcResult, InputEventSource, EventHeader
- `MouseEvent.h` - Mouse event types, data structures, and MouseEvent base class
- `KeyboardEvent.h` - Keyboard event types, data structures, and KeyboardStateEvent base class
- `WindowEvent.h` - Window event base class

### 4. Updated Code

**InputSystem.cpp:**
```cpp
// Changed from:
io::MouseEventID event_id = io::MouseEventID(header.id);

// To:
io::MouseAction action = io::MouseAction(header.id);
```

**RenderFramework.cpp:**
```cpp
// Changed from:
if(io::MouseEventID(header.id) == io::MouseEventID::Move)

// To:
if(io::MouseAction(header.id) == io::MouseAction::Move)
```

## Benefits

1. **Correct Data Structure**: MouseEventData now includes all necessary information (action + button + position + wheel_delta)
2. **Better Naming**: MouseAction is more descriptive than MouseEventID
3. **Backward Compatible**: Type alias ensures existing code using MouseEventID still works
4. **Complete Event System**: Full event handling infrastructure is now in place
5. **Type Safety**: Strongly typed enums prevent accidental misuse

## Files Created

- `inc/hgl/io/event/EventDispatcher.h`
- `inc/hgl/io/event/MouseEvent.h`
- `inc/hgl/io/event/KeyboardEvent.h`
- `inc/hgl/io/event/WindowEvent.h`

## Files Modified

- `src/ecs/InputSystem.cpp`
- `src/SceneGraph/RenderFramework.cpp`
