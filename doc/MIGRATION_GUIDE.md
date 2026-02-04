# Migration Guide: Staged Buffer System

## Introduction

This guide helps you migrate existing ULRE code to use the new staged buffer system for better GPU performance.

## Quick Start

### No Changes Required!

The staged buffer system is **completely backward compatible**. Your existing code will continue to work without modifications:

```cpp
// This still works exactly as before
VAB *vab = device->CreateVAB(format, count, data);
IndexBuffer *ibo = device->CreateIBO(IndexType::U16, count, data);
DeviceBuffer *ubo = device->CreateUBO(size, data);
```

### Opt-In to Better Performance

To use staged buffers for improved performance:

```cpp
// New: Create a staged buffer
StagedBuffer *staged = device->CreateStagedBuffer(
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    size,
    initial_data
);

// Use the device buffer for rendering
VkBuffer gpu_buffer = staged->GetDeviceBuffer();
```

## When to Migrate

### ✅ Good Candidates for Migration

1. **Static Geometry**
   - Vertex buffers that never change
   - Index buffers that never change
   - Immutable mesh data

2. **Infrequently Updated Data**
   - Level-of-detail (LOD) data
   - Per-scene configuration
   - Material properties updated rarely

3. **Large Buffers**
   - Texture data being uploaded
   - Large compute shader buffers
   - Terrain data

### ❌ Poor Candidates for Migration

1. **Frequently Updated Buffers**
   - Per-frame uniform buffers (camera, time, etc.)
   - Particle system vertex buffers (every frame)
   - Dynamic UI elements

2. **Small Buffers**
   - < 64KB buffers (overhead exceeds benefit)
   - Push constants (already fast)

## Migration Steps

### Step 1: Identify Target Buffers

Look for buffer creation code in your application:

```cpp
// Find patterns like these
device->CreateVAB(...)
device->CreateIBO(...)
device->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, ...)
```

### Step 2: Assess Update Frequency

For each buffer, determine:
- How often is it updated?
- Is the data static after initial upload?
- Is it large enough to benefit from staging?

### Step 3: Replace with StagedBuffer

#### Example 1: Static Vertex Buffer

**Before:**
```cpp
VAB *CreateMeshVAB(const Mesh &mesh)
{
    return device->CreateVAB(
        VK_FORMAT_R32G32B32_SFLOAT,
        mesh.vertex_count,
        mesh.vertices
    );
}
```

**After:**
```cpp
StagedBuffer *CreateMeshVAB(const Mesh &mesh)
{
    return device->CreateStagedBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        mesh.vertex_count * sizeof(Vertex),
        mesh.vertices
    );
}

// Usage in rendering
void RenderMesh(RenderCmdBuffer *cmd, StagedBuffer *vab)
{
    VkBuffer buffer = vab->GetDeviceBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
    // ... draw calls
}
```

#### Example 2: Dynamic Buffer with Occasional Updates

**Before:**
```cpp
class TerrainLOD
{
    DeviceBuffer *vertex_buffer;
    
    void UpdateLOD(const float *new_vertices, size_t size)
    {
        vertex_buffer->GetMemory()->Write(new_vertices, 0, size);
    }
};
```

**After:**
```cpp
class TerrainLOD
{
    StagedBuffer *vertex_buffer;
    
    void UpdateLOD(const float *new_vertices, size_t size)
    {
        // Write to staging buffer (will be copied to GPU next frame)
        vertex_buffer->Write(new_vertices, 0, size);
        // No manual flush needed - happens automatically in RenderFrame()
    }
    
    VkBuffer GetGPUBuffer() const
    {
        return vertex_buffer->GetDeviceBuffer();
    }
};
```

#### Example 3: Mapped Updates

**Before:**
```cpp
void UpdateBuffer(DeviceBuffer *buffer, const Data &data)
{
    void *mapped = buffer->GetMemory()->Map();
    memcpy(mapped, &data, sizeof(Data));
    buffer->GetMemory()->Unmap();
}
```

**After:**
```cpp
void UpdateBuffer(StagedBuffer *buffer, const Data &data)
{
    void *mapped = buffer->Map();
    memcpy(mapped, &data, sizeof(Data));
    buffer->Unmap();
    // Buffer is automatically marked dirty and queued for GPU sync
}
```

### Step 4: Update Rendering Code

If you stored buffer handles, update them to use device buffer:

**Before:**
```cpp
struct RenderObject
{
    DeviceBuffer *vertex_buffer;
    
    void Bind(RenderCmdBuffer *cmd)
    {
        VkBuffer buf = vertex_buffer->GetBuffer();
        // ...
    }
};
```

**After:**
```cpp
struct RenderObject
{
    StagedBuffer *vertex_buffer;
    
    void Bind(RenderCmdBuffer *cmd)
    {
        VkBuffer buf = vertex_buffer->GetDeviceBuffer();  // Use device buffer
        // ...
    }
};
```

### Step 5: Test and Verify

1. **Visual Correctness**: Ensure rendering looks identical
2. **Performance**: Measure FPS before/after
3. **Memory Usage**: Check GPU memory consumption

## Common Patterns

### Pattern 1: Immediate Upload (Static Data)

For data that's uploaded once and never changes:

```cpp
StagedBuffer *LoadStaticMesh(const char *filename)
{
    MeshData data = LoadFromFile(filename);
    
    StagedBuffer *buffer = device->CreateStagedBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        data.size,
        data.vertices
    );
    
    // Initial copy will happen on next RenderFrame()
    // After that, buffer is purely GPU-resident
    
    return buffer;
}
```

### Pattern 2: Deferred Upload (Async Loading)

For async resource loading:

```cpp
class AsyncLoader
{
    StagedBuffer *buffer;
    bool ready = false;
    
    void StartLoad()
    {
        // Create buffer without initial data
        buffer = device->CreateStagedBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            expected_size
        );
    }
    
    void OnDataLoaded(const void *data, size_t size)
    {
        buffer->Write(data, 0, size);
        ready = true;
        // Copy will happen automatically next frame
    }
    
    bool IsReady() const { return ready && !buffer->IsDirty(); }
};
```

### Pattern 3: Incremental Updates

For buffers updated in chunks:

```cpp
class StreamingBuffer
{
    StagedBuffer *buffer;
    
    void UpdateChunk(uint32_t chunk_id, const void *data, size_t size)
    {
        size_t offset = chunk_id * chunk_size;
        buffer->Write(data, offset, size);
        // Only the updated region will be copied
    }
};
```

## Performance Tuning

### Measuring Impact

Add performance metrics to quantify improvements:

```cpp
struct FrameStats
{
    double frame_time;
    uint32_t buffer_copies;
    size_t bytes_copied;
};

// In RenderFrame():
auto start = std::chrono::high_resolution_clock::now();

BufferUpdateQueue *queue = device->GetBufferUpdateQueue();
uint32_t copy_count = queue->GetPendingCount();  // Add this method if needed
queue->FlushAll(cmd);

auto end = std::chrono::high_resolution_clock::now();
stats.copy_time = std::chrono::duration<double>(end - start).count();
```

### Optimizing Copy Patterns

**Batch Updates**: Group multiple updates together:

```cpp
// Good: Single update
UpdateAllChunks();
RenderFrame();  // One batch copy

// Bad: Interleaved updates
for each chunk:
    UpdateChunk(chunk);
    RenderFrame();  // Many small copies
```

**Update Timing**: Update buffers early in the frame:

```cpp
void Tick(double delta)
{
    // Update all staged buffers here
    UpdateTerrainLOD();
    UpdateDynamicObjects();
    
    // Then render (automatic copy happens in RenderFrame)
    RenderFrame();
}
```

## Troubleshooting

### Issue: Rendering Artifacts

**Symptom**: Flickering, incorrect geometry, or visual glitches

**Possible Causes:**
1. Using staging buffer instead of device buffer for rendering
2. Missing synchronization barriers
3. Reading buffer before copy completes

**Solution:**
```cpp
// ❌ Wrong: Using staging buffer
vkCmdBindVertexBuffers(cmd, 0, 1, &staged->GetStagingBuffer(), ...);

// ✅ Correct: Using device buffer
vkCmdBindVertexBuffers(cmd, 0, 1, &staged->GetDeviceBuffer(), ...);
```

### Issue: Performance Regression

**Symptom**: FPS decreases after migration

**Possible Causes:**
1. Migrated per-frame buffers (too frequent updates)
2. Buffer too small (overhead exceeds benefit)
3. Too many small buffers (should batch into one)

**Solution:**
```cpp
// Revert per-frame buffers back to CPUOnly
DeviceBuffer *camera_ubo = device->CreateUBO(sizeof(CameraData));

// Keep staged buffers for static/infrequent data
StagedBuffer *mesh_vbo = device->CreateStagedBuffer(...);
```

### Issue: Memory Usage Increased

**Symptom**: Higher GPU memory consumption

**Expected**: Staged buffers use ~2x memory (staging + device)

**If problematic:**
1. Only migrate large, infrequently-updated buffers
2. Keep small, frequently-updated buffers as CPUOnly
3. Consider delayed destruction of staging buffers after initial upload (future optimization)

## Checklist

Before completing migration:

- [ ] Identified all buffer creation sites
- [ ] Assessed update frequency for each buffer
- [ ] Migrated appropriate buffers to StagedBuffer
- [ ] Updated rendering code to use GetDeviceBuffer()
- [ ] Verified visual correctness
- [ ] Measured performance improvement
- [ ] Documented changes in code comments
- [ ] Updated team about new patterns

## Additional Resources

- [Memory Management Architecture](./MEMORY_MANAGEMENT.md)
- [Vulkan Best Practices](https://developer.nvidia.com/vulkan-memory-management)
- [Performance Analysis Guide](./PERFORMANCE.md) (if exists)

## Getting Help

If you encounter issues:

1. Check validation layer output for synchronization warnings
2. Use RenderDoc to visualize buffer copies
3. Review [MEMORY_MANAGEMENT.md](./MEMORY_MANAGEMENT.md) for architecture details
4. Consult with the graphics team
