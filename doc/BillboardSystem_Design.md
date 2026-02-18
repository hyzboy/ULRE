# Billboard System Design & Architecture

## Overview

This document outlines the **current implementation** of the Billboard rendering system and **planned enhancements** toward a high-performance, batched, array-texture-based approach.

### Current State (Phase 1)
- Single shared `Primitive` (geometry + pipeline) for all billboards
- Per-billboard optional material override with custom textures
- System-driven resource creation (primitive, material, texture binding)
- Billboard rotation computed per-frame to face camera

### Target Vision (Phase 2+)
- Unified `Texture2DArray` with **LRU residence management**
- Single `vkCmdDrawIndirect` for all billboards
- Per-instance texture layer index
- Zero per-frame material/texture binding overhead

---

## Phase 1: Current Architecture

### Components

#### `BillboardComponent`
**Location:** `inc/hgl/ecs/components/BillboardComponent.h`

```cpp
class BillboardComponent : public PrimitiveComponent
{
private:
    bool fixed_size;                          // Pixel-size vs world-space mode
    hgl::math::Vector2u pixel_size;           // Size in pixels (fixed mode)
    glm::vec2 world_size;                     // Size in world units (dynamic mode)
    VkFrontFace front_face;                   // Face winding order
    
    hgl::OSString texture_path;               // Optional texture path (system loads)
    hgl::OSString applied_texture;            // Last applied texture path
    bool texture_dirty;                       // Dirty flag for texture change
    hgl::graph::Texture2D* texture;           // Cached texture
    hgl::graph::Sampler* sampler;             // Cached sampler
};
```

**Usage:**
```cpp
auto billboard = entity->AddComponent<BillboardComponent>();
billboard->SetPixelSize(256, 256);
billboard->SetTexturePath(OS_TEXT("res/image/lena.Tex2D"));
billboard->SetVisible(true);
```

#### `BillboardRenderSystem`
**Location:** `inc/hgl/ecs/systems/render/BillboardRenderSystem.h`

**Responsibilities:**
1. Create & cache shared resources (primitive, material, pipeline)
2. Load textures and bind per-billboard material instances
3. Update billboard rotations each frame (face-to-camera)

**Key Methods:**
- `EnsureSharedResources()` — creates pipeline, geometry, primitive (once per render pass)
- `EnsureBillboardMaterial(billboard)` — loads texture, creates override material instance
- `UpdateBillboardRotation(billboard, transform, dt)` — computes facing rotation

**Shared Resources:**
```cpp
static graph::Primitive* shared_primitive;              // All billboards share this
static graph::MaterialInstance* shared_material_instance;
static graph::Pipeline* shared_pipeline;
static graph::RenderPass* shared_render_pass;
static graph::Sampler* shared_sampler;
```

### Rendering Flow

```
1. ECS Update Phase (Tick)
   └─ BillboardRenderSystem::Update()
      ├─ EnsureSharedResources() → create primitive/pipeline once
      ├─ For each Billboard:
      │  ├─ Assign shared_primitive (if missing)
      │  ├─ EnsureBillboardMaterial() → load texture, create material override
      │  └─ UpdateBillboardRotation() → face camera
      └─ Done

2. ECS Render Phase
   └─ RenderPrimitiveCollectSystem → collects all RenderableComponent (including Billboard)
   └─ RenderPrimitiveBatchSystem → groups by (material, pipeline)
   └─ RenderPrimitiveSubmitSystem → issues vkCmdDraw per batch
```

---

## Phase 2: Texture Array & LRU Enhancement

### Goals
- Eliminate per-billboard material instance creation
- Use single `Texture2DArray` for all billboard textures
- Implement LRU cache for texture residence
- All billboards rendered in **one `vkCmdDrawIndirect`**

### Design

#### 1. Sampler Architecture

**Global Sampler Pool** (small, reusable)
```cpp
struct SamplerKey {
    VkFilter magFilter;
    VkFilter minFilter;
    VkSamplerMipmapMode mipmapMode;
    VkSamplerAddressMode addressMode;
    // ... other sampler params
};

class SamplerCache {
public:
    hgl::graph::Sampler* Get(const SamplerKey& key);
    void Release(hgl::graph::Sampler* sampler);
};
```

**Billboard Component Change:**
```cpp
class BillboardComponent {
    hgl::OSString texture_path;               // Texture source (path or asset id)
    SamplerKey sampler_key;                   // Sampler configuration (optional, defaults to standard)
    uint32_t texture_layer_index;             // Layer in Texture2DArray (assigned by system)
};
```

#### 2. Texture2DArray Manager

**Purpose:** Unified residence for all billboard textures

```cpp
struct Texture2DArrayConfig {
    uint32_t layer_count;                     // Max layers (e.g., 256, 512)
    uint32_t width;                           // All layers must have same size
    uint32_t height;
    VkFormat format;                          // All layers must have same format
    bool generate_mipmaps;
};

class BillboardTextureArrayManager {
public:
    // Acquire layer for texture source; loads & resizes if needed
    uint32_t AcquireLayer(const hgl::OSString& texture_path);
    
    // Release layer; triggers LRU cleanup if needed
    void ReleaseLayer(uint32_t layer_index);
    
    // Update layer data after resize
    void UpdateLayer(const void* data, uint32_t data_size, uint32_t layer_index);
    
    // Get current array texture
    hgl::graph::Texture2DArray* GetArray() const;
    
    // Statistics
    struct Stats {
        uint32_t used_layers;
        uint32_t max_layers;
        uint32_t evictions_total;
    };
    Stats GetStats() const;
private:
    struct LayerEntry {
        hgl::OSString source_path;
        uint64_t last_used_frame;               // For LRU
        uint32_t ref_count;
    };
    std::vector<LayerEntry> layers;
    hgl::graph::Texture2DArray* array;
};
```

#### 3. Instance Data & Indirect Buffer

**Billboard Instance Data** (per billboard in render list)
```cpp
struct BillboardInstanceData {
    uint32_t transform_index;                 // Index into transform buffer
    uint32_t texture_layer;                   // Layer in Texture2DArray
    uint32_t color;                           // Packed RGBA (optional)
    float uvScale;                            // Texture coordinate scale (reserved)
};
```

**Unified Indirect Buffer:**
```cpp
// All billboards rendered as:
vkCmdDrawIndirect(..., 1 pipeline, 1 pass, 1 material, 1 texture_array, ALL billboards)

// Shader reads:
- transform from transform_buffer[instanceData.transform_index]
- texture from sampler2DArray[instanceData.texture_layer]
```

#### 4. LRU Residence Strategy

**Algorithm:**
- Track `last_used_frame` per layer
- When acquiring new texture:
  - If free layer exists → use it immediately
  - Else if `used_layers < max_layers` → allocate new
  - Else → evict least-recently-used layer
- Defer actual GPU upload to next render pass (batch updates)

**Eviction:**
```
foreach (layer in layers) {
    if (layer.last_used_frame + EVICTION_GRACE_FRAMES < current_frame) {
        UnloadLayer(layer);
        FreeLayer(layer); // mark for reuse
    }
}
```

---

## Phase 3: Multi-Array & Advanced Features

### Scope
- Multiple `Texture2DArray` buckets (by format, size, mip-level)
- Per-format pipeline variants
- Texture filtering per-layer (e.g., point vs linear)
- Async texture loading & streaming
- Budget-aware LRU (e.g., max VRAM per array)

---

## Implementation Roadmap

| Phase | Feature | Effort | Impact | Status |
|-------|---------|--------|--------|--------|
| **1** | Shared primitive + system material mgmt | Low | ~50% perf uplift (fewer draw calls) | ✅ Done |
| **2.1** | Global sampler pool | Low | ~5% overhead reduction | Planned |
| **2.2** | Single Texture2DArray (fixed size) | Medium | ~45% perf uplift (batch textures) | Planned |
| **2.3** | LRU cache & dynamic replacement | Medium | Better memory usage | Planned |
| **3** | Multi-array & advanced buckets | High | Flexibility + robustness | Future |

---

## Technical Considerations

### GPU Synchronization
- **Texture replacement:** Must not target layer being rendered this frame
- **Solution:** Double-buffer array or defer replacement to frame boundary
- **Instance buffer update:** Can flush async (CPU-GPU pipeline overlap)

### Memory Layout
- **Texture2DArray constraints:**
  - All layers same size (must pre-compute or force resize)
  - All layers same format
  - Mipmap generation impacts layer count (mip chain consumes extra)
- **CPU constraints:**
  - Texture loading/resizing blocks; consider worker threads

### Shader Integration
```glsl
// In fragment shader
vec2 uv = compute_uv_for_billboard(...);
int layer = push_constants.texture_layer;  // or per-instance data
vec4 color = texture(sampler2DArray, vec3(uv, layer));
```

---

## Alternative Approaches Considered

### 1. Sprite Sheet Atlas
- ❌ **Pros:** No LRU complexity
- ❌ **Cons:** Manual atlas layout; wastes space on mismatched sizes
- 🔄 **Decision:** Array texture is more flexible

### 2. Separate Texture per Billboard
- ❌ **Pros:** No size/format constraints
- ❌ **Cons:** Descriptor pressure; draw call multiplicity
- 🔄 **Decision:** Defeats the purpose of this design

### 3. Dynamic Descriptor Set Update
- ❌ **Pros:** No array texture limitation
- ❌ **Cons:** Higher CPU cost per frame; synchronization complexity
- 🔄 **Decision:** Array texture scales better

---

## File Structure (Post-Phase 2)

```
inc/hgl/ecs/
  ├── components/
  │   └── BillboardComponent.h              (enhanced: layer_index, sampler_key)
  └── systems/
      └── render/
          ├── BillboardRenderSystem.h       (unchanged interface)
          ├── BillboardTextureArrayManager.h (new)
          └── BillboardInstanceBuffer.h     (new)

src/ecs/
  ├── components/
  │   └── BillboardComponent.cpp
  └── systems/
      └── render/
          ├── BillboardRenderSystem.cpp
          ├── BillboardTextureArrayManager.cpp (new)
          └── BillboardInstanceBuffer.cpp (new)

doc/
  └── BillboardSystem_Design.md (this file)
```

---

## Testing Strategy

### Phase 1 Tests
- ✅ Billboard visibility & transformation
- ✅ Per-billboard material override
- ❌ (Not applicable; no array)

### Phase 2 Tests
- [ ] Texture2DArray resize on mismatch
- [ ] LRU eviction & re-acquisition
- [ ] Frame-boundary texture replacement
- [ ] Indirect draw count correctness
- [ ] Layer index stability (no flicker)

### Benchmarks
- FPS with 1k, 10k, 100k billboards
- Memory footprint (texture array vs per-billboard)
- GPU time (single indirect vs per-batch draws)

---

## Glossary

| Term | Definition |
|------|-----------|
| **Layer** | A single 2D texture slice within Texture2DArray |
| **Array** | Vulkan `Texture2DArray` resource (N layers, same dimensions) |
| **LRU** | Least-Recently-Used eviction policy for cache |
| **Grace period** | Frames to keep least-used layer before eviction |
| **Dirty flag** | Indicates texture path changed; needs reload |
| **Instance data** | Per-billboard metadata (layer, color, etc.) |
| **Indirect buffer** | GPU buffer holding draw parameters (instance count, etc.) |

---

## References

- Vulkan Spec: `VkTextureSampler`, `VK_IMAGE_VIEW_TYPE_2D_ARRAY`
- HGL: `Texture2DArray`, `Sampler`, `Texture2D`
- ECS: `BillboardComponent`, `TransformComponent`, `RenderableComponent`

---

## Questions & Future Work

1. **What's the max Texture2DArray layer count?**  
   Typically 2048; depends on GPU. Proposal: configurable per app.

2. **Should we support partial layer updates (subrect)?**  
   Yes, Phase 3. For now: full layer replacement only.

3. **How to handle non-power-of-2 textures?**  
   Phase 2.3: Auto-pad to next POT or configurable "virtual size" in array.

4. **Can we use bindless textures instead?**  
   Future: Yes, if targeting modern GPUs (VK 1.2+). For now: array + index.

---

**Document Version:** 1.0  
**Last Updated:** 2026-02-18  
**Author:** ECS Architecture Group
