# TileMap渲染示例

这两个示例演示了如何使用TileData系统渲染2D tilemap。

## 文件说明

### TileMap2D.cpp
- 2D平面的tilemap渲染示例
- 使用二维数组（uint16）表示tilemap数据
- 每个数组元素代表一个tile的索引
- 在2D屏幕空间中渲染tilemap

### TileMap3D.cpp  
- 3D空间的tilemap渲染示例
- 使用相同的tilemap数据结构
- 在3D世界空间中渲染tilemap（XZ平面）
- 包含3D摄像机控制

## 主要特性

1. **TileMapData结构**：管理二维tile索引数组
2. **动态tile创建**：程序运行时生成示例tile图片
3. **TileData集成**：使用现有的TileData系统管理tile纹理
4. **批量渲染**：所有tile在一次draw call中渲染
5. **UV映射**：自动计算每个tile的纹理坐标

## TileMapData API

```cpp
struct TileMapData
{
    uint32_t width, height;     // tilemap尺寸
    uint16_t *data;            // tile索引数组
    
    uint16_t GetTile(uint32_t x, uint32_t y);        // 获取tile索引
    void SetTile(uint32_t x, uint32_t y, uint16_t tile_id); // 设置tile索引
};
```

## 使用方法

1. 创建TileMapData实例
2. 使用SetTile()设置每个位置的tile索引
3. TileData系统自动管理tile纹理
4. 渲染系统根据tile索引生成相应的几何体和UV坐标

## 扩展说明

这些示例可以轻松扩展以支持：
- 加载外部tilemap数据文件
- 支持更大的tilemap
- 多层tilemap渲染
- 动画tile支持
- 碰撞检测集成