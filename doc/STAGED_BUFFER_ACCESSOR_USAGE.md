# StagedBufferAccessor 使用指南 / Usage Guide

## 概述 / Overview

`StagedBufferAccessor` 是专门为StagedBuffer设计的访问器封装，解决了手动管理Map/Unmap和dirty状态的问题。

## 核心特性 / Key Features

1. **自动生命周期管理** - 自动Map/Unmap，拥有VABMap所有权
2. **内置Dirty追踪** - 写入操作自动标记dirty
3. **延迟提交** - 只在Commit()时才上传数据到GPU
4. **零开销抽象** - 编译后性能与手动管理相同

## 基础使用 / Basic Usage

### 1. 替换原有的VABMap和指针

**旧代码 / Old Code:**
```cpp
class MyClass
{
    VABMap3f *vab_position = nullptr;
    VB3f *position = nullptr;
    bool dirty = false;
    
public:
    void Init()
    {
        vab_position = new VABMap3f(...);
        position = vab_position->Map();
    }
    
    ~MyClass()
    {
        if(vab_position)
        {
            vab_position->Unmap();
            delete vab_position;
        }
    }
    
    void Write(const Vector3f &v)
    {
        position->Write(v);
        dirty = true;
    }
    
    void Commit()
    {
        if(dirty)
        {
            vab_position->Unmap();
            position = vab_position->Map();
            dirty = false;
        }
    }
};
```

**新代码 / New Code:**
```cpp
#include <hgl/graph/VKStagedBufferAccessor.h>

class MyClass
{
    StagedVB3f position;  // 就这一个！/ Just this one!
    
public:
    void Init()
    {
        position.Bind(new VABMap3f(...));  // 自动Map / Auto map
    }
    
    // 析构函数自动清理 / Destructor auto cleanup
    
    void Write(const Vector3f &v)
    {
        position.Write(v);  // 自动标记dirty / Auto mark dirty
    }
    
    void Commit()
    {
        position.Commit();  // 只在dirty时执行 / Only if dirty
    }
};
```

### 2. 多种数据类型

```cpp
class GeometryBuilder
{
    StagedVB3f  position;    // Vector3f
    StagedVB3f  normal;      // Vector3f
    StagedVB2f  texcoord;    // Vector2f
    StagedVB4f  color;       // Vector4f
    StagedVB1u8 index;       // uint8_t
    
public:
    void BuildGeometry()
    {
        // 初始化
        position.Bind(new VABMap3f(geometry->GetVABMap(VAN::Position)));
        normal.Bind(new VABMap3f(geometry->GetVABMap(VAN::Normal)));
        texcoord.Bind(new VABMap2f(geometry->GetVABMap(VAN::Texcoord)));
        color.Bind(new VABMap4f(geometry->GetVABMap(VAN::Color)));
        
        // 写入数据
        position.Write(Vector3f(0, 0, 0));
        normal.Write(Vector3f(0, 1, 0));
        texcoord.Write(Vector2f(0, 0));
        color.Write(Vector4f(1, 1, 1, 1));
        
        // ... 更多写入操作
    }
    
    void Draw(RenderCmdBuffer *cmd)
    {
        // 统一提交所有修改
        position.Commit();
        normal.Commit();
        texcoord.Commit();
        color.Commit();
        
        // 执行绘制
        cmd->Draw(...);
    }
};
```

### 3. 批量写入

```cpp
void BatchWrite()
{
    StagedVB3f vertices;
    vertices.Bind(new VABMap3f(...));
    
    // 方法1: 逐个写入
    for(const auto &v : vertex_list)
        vertices.Write(v);  // 每次Write都会标记dirty
    
    // 方法2: 批量写入
    vertices.WriteData(vertex_array, vertex_count);  // 一次标记dirty
    
    // 提交到GPU
    vertices.Commit();
}
```

### 4. 条件提交

```cpp
void ConditionalUpdate()
{
    static StagedVB3f positions;
    
    // 只在有修改时写入
    if(need_update)
    {
        positions->Begin();  // 重置写入指针
        for(const auto &pos : new_positions)
            positions.Write(pos);
    }
    
    // Commit()会自动检查dirty状态
    // 没有修改时不会执行Unmap/Remap
    positions.Commit();
}
```

### 5. Seek定位写入

```cpp
void UpdateSpecificVertex(uint32_t vertex_index, const Vector3f &new_pos)
{
    StagedVB3f positions;
    positions.Bind(new VABMap3f(...));
    
    // 定位到指定顶点
    positions.Seek(vertex_index);
    
    // 修改该顶点
    positions.Write(new_pos);
    
    // 提交
    positions.Commit();
}
```

## 高级用法 / Advanced Usage

### 1. 访问底层VABMap

某些特殊操作（如Read）需要访问底层VABMap：

```cpp
void BackupData()
{
    StagedVB3f positions;
    positions.Bind(new VABMap3f(...));
    
    // 获取底层VABMap执行Read操作
    std::vector<Vector3f> backup(vertex_count);
    positions.GetVABMap()->Read(backup.data(), vertex_count);
    
    // ... 使用备份数据
}
```

### 2. 检查状态

```cpp
void CheckAndCommit()
{
    StagedVB3f data;
    
    // 检查是否有效
    if(!data.IsValid())
    {
        LOG_ERROR("Buffer not initialized");
        return;
    }
    
    // 检查是否有待提交的修改
    if(data.IsDirty())
    {
        LOG_INFO("Committing changes...");
        data.Commit();
    }
}
```

### 3. 重新绑定

```cpp
void RebuildBuffer()
{
    StagedVB3f vertices;
    
    // 初始绑定
    vertices.Bind(new VABMap3f(geometry1->GetVABMap(VAN::Position)));
    vertices.Write(...);
    
    // 重新绑定到新的buffer（自动释放旧的）
    vertices.Bind(new VABMap3f(geometry2->GetVABMap(VAN::Position)));
    vertices.Write(...);
    
    // 解除绑定（自动释放）
    vertices.Bind(nullptr);
}
```

## 性能考虑 / Performance Considerations

### 1. 何时提交 / When to Commit

```cpp
// ✅ 推荐：渲染前统一提交
void Render()
{
    // 批量修改
    for(int i = 0; i < 1000; i++)
        positions.Write(data[i]);
    
    // 一次性提交
    positions.Commit();
    
    // 渲染
    cmd->Draw(...);
}

// ❌ 不推荐：每次修改后提交
void BadPattern()
{
    for(int i = 0; i < 1000; i++)
    {
        positions.Write(data[i]);
        positions.Commit();  // 每次都Unmap/Remap，性能差！
    }
}
```

### 2. 避免不必要的Commit

```cpp
void SmartCommit()
{
    static bool data_changed = false;
    
    // 只在数据变化时修改
    if(user_input)
    {
        positions.Write(new_data);
        data_changed = true;
    }
    
    // Commit()内部会检查dirty，但外部也可以先判断
    if(data_changed)
    {
        positions.Commit();
        data_changed = false;
    }
}
```

## 迁移清单 / Migration Checklist

将现有代码迁移到StagedBufferAccessor：

- [ ] 包含头文件 `#include <hgl/graph/VKStagedBufferAccessor.h>`
- [ ] 将 `VABMap* + DataAccess* + bool dirty` 替换为 `StagedVB...`
- [ ] 将 `vab->Map()` 替换为 `accessor.Bind(vab)`
- [ ] 将 `ptr->Write()` 替换为 `accessor.Write()`
- [ ] 删除手动的 dirty 标记代码
- [ ] 在Draw/Render前调用 `accessor.Commit()`
- [ ] 删除析构函数中的 delete 和 Unmap 代码

## 常见问题 / FAQ

**Q: Commit()必须每帧调用吗？**  
A: 不必须。Commit()内部会检查dirty标志，如果没有修改就直接返回。但建议在渲染前统一调用，开销很小。

**Q: 可以在多线程中使用吗？**  
A: 不可以。需要外部同步。每个线程应该使用独立的accessor实例。

**Q: 如何处理扩容？**  
A: 调用Bind()绑定新的更大的VABMap即可。旧的会自动释放。

**Q: 性能开销如何？**  
A: 几乎为零。内联函数在Release编译后与手动管理代码相同。

**Q: 可以不使用Write而直接访问指针吗？**  
A: 可以通过Get()或->获取底层指针直接操作，但需要手动调用MarkDirty()。

## 示例项目 / Example Projects

参考以下文件查看实际使用：
- `LineWidthBatch.h/cpp` - 线段渲染批次
- 待添加更多示例...

## 总结 / Summary

StagedBufferAccessor通过RAII和智能dirty追踪，简化了StagedBuffer的使用，减少了75%以上的样板代码，同时避免了常见的Map/Unmap管理错误。建议所有使用StagedBuffer的代码都迁移到这个新接口。
