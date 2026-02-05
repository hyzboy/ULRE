# MouseEvent Design Fix Documentation

## Problem Statement

The original MouseEvent system had design issues:
1. `MouseEventData` only contained button field, but was missing the action/event type information
2. `MouseEventID` was not a descriptive name - `MouseAction` is more appropriate
3. `InputSystem.cpp` needed updating to handle the corrected structure
4. **NEW**: Having both `MouseAction` (which encodes button) and `button` field was redundant

## Solution

### 1. Renamed MouseEventID to MouseAction
- More descriptive name that clearly indicates what it represents
- Provides type alias `using MouseEventID = MouseAction` for backward compatibility

### 2. Fixed MouseEventData Structure

**Initial Problem:**
```cpp
struct MouseEventData {
    // Only had button, x, y, wheel_delta
    // Missing the action type!
};
```

**First Fix (had redundancy):**
```cpp
struct MouseEventData {
    MouseAction action;     // Move, LeftDown, RightDown, etc.
    MouseButton button;     // REDUNDANT! Action already encodes button
    int16 x;
    int16 y;
    int16 wheel_delta;
};
```

**Final Fix (removed redundancy):**
```cpp
struct MouseEventData {
    MouseAction action;     // Contains both action AND button info (LeftDown, RightDown, etc.)
    int16 x;                // X coordinate
    int16 y;                // Y coordinate
    int16 wheel_delta;      // Wheel delta
};
```

### 3. Why MouseAction is Sufficient

The `MouseAction` enum already encodes which button:
- `LeftDown`, `LeftUp` → Left button
- `RightDown`, `RightUp` → Right button
- `MiddleDown`, `MiddleUp` → Middle button
- `Move`, `Wheel` → No specific button

Therefore, a separate `button` field would be redundant.

### 4. Helper Function for Button Extraction

If code needs to extract the button from an action:
```cpp
MouseButton GetButtonFromAction(MouseAction action);
```

This function returns:
- `MouseButton::Left` for LeftDown/LeftUp
- `MouseButton::Right` for RightDown/RightUp
- `MouseButton::Middle` for MiddleDown/MiddleUp
- `MouseButton::Left` (default) for Move/Wheel

### 5. Event System Architecture

Created complete event system headers:
- `EventDispatcher.h` - Base event dispatcher interface with EventProcResult, InputEventSource, EventHeader
- `MouseEvent.h` - Mouse event types, data structures, and MouseEvent base class
- `KeyboardEvent.h` - Keyboard event types, data structures, and KeyboardStateEvent base class
- `WindowEvent.h` - Window event base class

### 6. Updated Code

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

1. **Correct Data Structure**: MouseEventData now includes action which contains both action type and button information
2. **No Redundancy**: Removed redundant button field since action already encodes it
3. **Better Naming**: MouseAction is more descriptive than MouseEventID
4. **Backward Compatible**: Type alias ensures existing code using MouseEventID still works
5. **Complete Event System**: Full event handling infrastructure is now in place
6. **Type Safety**: Strongly typed enums prevent accidental misuse
7. **Cleaner API**: Less confusion about which field to use

## Files Created

- `inc/hgl/io/event/EventDispatcher.h`
- `inc/hgl/io/event/MouseEvent.h`
- `inc/hgl/io/event/KeyboardEvent.h`
- `inc/hgl/io/event/WindowEvent.h`

## Files Modified

- `src/ecs/InputSystem.cpp`
- `src/SceneGraph/RenderFramework.cpp`
