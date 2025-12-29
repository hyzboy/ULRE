# Class Separation Refactoring Summary

## 修改说明 / Change Description

根据 @hyzboy 的要求，将所有新建的类分离到独立的 .h/.cpp 文件中。

Per @hyzboy's request, separated all newly created classes into individual .h/.cpp files.

## 文件结构变化 / File Structure Changes

### 之前 (Before) - 2 文件 / 2 files

**RenderItem.h** (包含 2 个类):
- `RenderItem` (基类)
- `PrimitiveRenderItem` (派生类)

**RenderCollector.h** (包含 3 个类):
- `MaterialPipelineKey` (结构体)
- `MaterialBatch` (类)
- `RenderCollector` (类)

### 之后 (After) - 5 文件 / 5 files

**分离的头文件 / Separated Header Files:**

1. **RenderItem.h** - 仅包含 RenderItem 基类
   ```cpp
   class RenderItem {
       // Abstract base class
       virtual methods...
   };
   ```

2. **PrimitiveRenderItem.h** (新建 / NEW)
   ```cpp
   #include<hgl/ecs/RenderItem.h>
   class PrimitiveRenderItem : public RenderItem {
       // Concrete implementation
   };
   ```

3. **MaterialPipelineKey.h** (新建 / NEW)
   ```cpp
   struct MaterialPipelineKey {
       Material* material;
       Pipeline* pipeline;
       // Comparison operators
   };
   ```

4. **MaterialBatch.h** (新建 / NEW)
   ```cpp
   #include<hgl/ecs/MaterialPipelineKey.h>
   class MaterialBatch {
       // Batch management
   };
   ```

5. **RenderCollector.h** - 更新包含
   ```cpp
   #include<hgl/ecs/RenderItem.h>
   #include<hgl/ecs/MaterialPipelineKey.h>
   #include<hgl/ecs/MaterialBatch.h>
   class RenderCollector : public System {
       // Collector implementation
   };
   ```

### 实现文件 / Implementation Files

**新增 / New:**
- `src/ecs/PrimitiveRenderItem.cpp` - PrimitiveRenderItem 实现
- `src/ecs/MaterialBatch.cpp` - MaterialBatch 实现

**更新 / Updated:**
- `src/ecs/RenderItem.cpp` - 移除 PrimitiveRenderItem 实现
- `src/ecs/RenderCollector.cpp` - 移除 MaterialBatch 实现，添加 PrimitiveRenderItem.h 引用

## 依赖关系图 / Dependency Graph

```
MaterialPipelineKey.h (独立 / standalone)
    ↓
MaterialBatch.h (依赖 MaterialPipelineKey)
    ↓
RenderItem.h (基类 / base class)
    ↓
PrimitiveRenderItem.h (依赖 RenderItem)
    ↓
RenderCollector.h (依赖以上所有 / depends on all above)
```

## CMakeLists.txt 更新

```cmake
SET(ECS_SOURCE  
    # Headers
    ${ECS_INCLUDE_PATH}/RenderItem.h
    ${ECS_INCLUDE_PATH}/PrimitiveRenderItem.h      # NEW
    ${ECS_INCLUDE_PATH}/MaterialPipelineKey.h      # NEW
    ${ECS_INCLUDE_PATH}/MaterialBatch.h            # NEW
    ${ECS_INCLUDE_PATH}/RenderCollector.h
    
    # Sources
    RenderItem.cpp
    PrimitiveRenderItem.cpp                        # NEW
    MaterialBatch.cpp                              # NEW
    RenderCollector.cpp
    ...
)
```

## 优势 / Benefits

1. **模块化** - 每个类有独立的文件，便于维护
2. **编译速度** - 修改单个类只需重新编译相关文件
3. **可读性** - 文件结构清晰，易于理解
4. **重用性** - 独立的类可以在其他地方单独使用
5. **符合规范** - 遵循"一个类一个文件"的最佳实践

1. **Modularity** - Each class in its own file for easier maintenance
2. **Compile Speed** - Changes to one class only recompile related files
3. **Readability** - Clear file structure, easier to understand
4. **Reusability** - Independent classes can be used separately elsewhere
5. **Best Practice** - Follows "one class per file" convention

## 提交信息 / Commit

- Commit: `933cf32`
- Message: "Separate each class into individual .h/.cpp files"
- Files Changed: 10 files
- Lines: +232 insertions, -170 deletions

## 验证 / Verification

所有类现在都在独立的文件中：

All classes are now in separate files:

✅ RenderItem.h/.cpp (基类 / base class)
✅ PrimitiveRenderItem.h/.cpp (派生类 / derived class)  
✅ MaterialPipelineKey.h (结构体 / struct, header-only)
✅ MaterialBatch.h/.cpp (批次管理 / batch management)
✅ RenderCollector.h/.cpp (收集器 / collector)
