# FBX Loader for ULRE Engine

This implementation provides a FBX model loader for the ULRE Engine based on Autodesk FBX SDK.

## Architecture Overview

The FBX loader follows the same patterns as the existing AssimpLoader but is specifically designed for FBX files with the following key features:

- **Scene Node Mapping**: Each FBX node corresponds to a SceneNode in the engine
- **Mesh Processing**: Each independent model creates its own Primitive/Mesh object
- **Vertex Data**: Currently processes vertex positions and normals only
- **Future-Ready Structure**: Placeholder structures for materials, textures, lights, and animations

## File Structure

- `FBXLoader.h` - Main FBX loader class definition
- `FBXLoader.cpp` - FBX loader implementation
- `FBXLoaderDemo.cpp` - Standalone demonstration (works without FBX SDK)
- `Makefile` - Simple build script for the demo

## Current Implementation Status

### ✅ Implemented
- Basic FBX loader class structure
- Scene node traversal architecture
- Mesh data processing framework
- Placeholder structures for future features
- Integration with existing engine patterns

### ⚠️ Currently Limited
- Only vertex positions and normals are processed
- No texture coordinate extraction
- No material properties extraction
- No skeletal animation support

### 📋 Placeholder Structures (Ready for Implementation)
- Materials (colors, properties, shader parameters)
- Textures (diffuse, normal, specular, etc.)
- Lights (position, color, type, intensity)
- Animations (keyframes, timing, interpolation)

## FBX SDK Integration

### Prerequisites
1. Download and install Autodesk FBX SDK from:
   https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2020-3-4

2. Extract the SDK to a suitable location (e.g., `/opt/fbx-sdk/` or `C:\Program Files\Autodesk\FBX\FBX SDK\`)

### Build Configuration

#### Method 1: Using CMake (Recommended)
```cmake
# In CMakeLists.txt, uncomment and adjust the FBX SDK lines:
find_package(FBX QUIET)
if(FBX_FOUND)
    target_compile_definitions(ModelConvert PRIVATE HAVE_FBX_SDK)
    target_include_directories(ModelConvert PRIVATE ${FBX_INCLUDE_DIR})
    target_link_libraries(ModelConvert ${FBX_LIBRARIES})
endif()
```

#### Method 2: Using Makefile
```makefile
# In Makefile, uncomment and adjust these lines:
FBX_SDK_ROOT = /path/to/fbx/sdk
CXXFLAGS += -DHAVE_FBX_SDK -I$(FBX_SDK_ROOT)/include
LDFLAGS += -L$(FBX_SDK_ROOT)/lib -lfbxsdk
```

### Example Linux Setup
```bash
# Download FBX SDK
wget https://www.autodesk.com/content/dam/autodesk/www/adn/fbx/2020-3-4/fbx202034_fbxsdk_linux.tar.gz
tar -xzf fbx202034_fbxsdk_linux.tar.gz
sudo ./fbx202034_fbxsdk_linux /usr/local

# Update Makefile
FBX_SDK_ROOT = /usr/local
CXXFLAGS += -DHAVE_FBX_SDK -I$(FBX_SDK_ROOT)/include
LDFLAGS += -L$(FBX_SDK_ROOT)/lib -lfbxsdk
```

## Usage

### Testing the Architecture (Without FBX SDK)
```bash
cd src/Tools/ModelConvert
make
./FBXLoaderDemo sample_model.fbx
```

### Using with Actual FBX Files (With FBX SDK)
```bash
# Compile with FBX SDK support
make clean
make  # with FBX SDK configured

# Convert FBX files
./ModelConvert model.fbx
./FBXLoaderDemo actual_model.fbx
```

### Integration with Main Application
```cpp
#include "FBXLoader.h"

FBXLoader loader;
if(loader.LoadFile("model.fbx")) {
    loader.PrintSceneInfo();
    // Process loaded data...
}
```

## Data Flow

1. **FBX File Loading**: FBX SDK reads and parses the file
2. **Scene Traversal**: Each FBX node is processed recursively
3. **Mesh Extraction**: Vertex positions and normals are extracted
4. **Coordinate Conversion**: FBX coordinates converted to engine space
5. **Engine Integration**: SceneNode and Primitive objects are created

## Coordinate System Notes

- FBX uses right-handed coordinate system with Y-up
- Conversion may be needed depending on engine requirements
- Current implementation preserves FBX coordinates

## Future Development Roadmap

### Phase 1 (Current): Basic Mesh Loading
- ✅ Vertex positions and normals
- ⚠️ Basic scene hierarchy

### Phase 2: Complete Mesh Support
- [ ] Texture coordinates (UV mapping)
- [ ] Vertex colors
- [ ] Tangent and bitangent vectors
- [ ] Multiple UV channels

### Phase 3: Material and Texture Support
- [ ] Material property extraction
- [ ] Texture file references
- [ ] Multi-material meshes
- [ ] PBR material support

### Phase 4: Lighting and Environment
- [ ] Light extraction (directional, point, spot)
- [ ] Environment settings
- [ ] Camera information

### Phase 5: Animation Support
- [ ] Keyframe animation
- [ ] Skeletal animation
- [ ] Morph targets
- [ ] Animation curves

## Troubleshooting

### Common Issues

1. **FBX SDK Not Found**
   - Ensure FBX SDK is properly installed
   - Check include and library paths
   - Verify HAVE_FBX_SDK is defined

2. **Linking Errors**
   - Ensure correct FBX SDK version (2020.3.4 recommended)
   - Check library architecture (x64 vs x86)
   - Verify all required libraries are linked

3. **Runtime Errors**
   - Ensure FBX runtime libraries are in PATH
   - Check file permissions
   - Verify FBX file is not corrupted

### Debug Mode
Enable verbose logging by setting debug flags in the loader configuration.

## Contributing

When extending the FBX loader:

1. Follow existing code patterns from AssimpLoader
2. Maintain compatibility with engine SceneNode structure
3. Add appropriate error handling and logging
4. Update placeholder structures gradually
5. Test with various FBX file types

## License

This code follows the same license as the ULRE Engine project.