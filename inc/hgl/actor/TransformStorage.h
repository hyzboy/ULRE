#pragma once

#include<hgl/actor/TransformComponent.h>
#include<vector>

#define TRANSFORM_STORAGE_NAMESPACE         hgl::actor
#define TRANSFORM_STORAGE_NAMESPACE_BEGIN   namespace TRANSFORM_STORAGE_NAMESPACE {
#define TRANSFORM_STORAGE_NAMESPACE_END     }

TRANSFORM_STORAGE_NAMESPACE_BEGIN

/**
 * TransformStorage
 * 使用SOA(Structure of Arrays)方式存储Transform数据
 * 
 * 在Frostbite引擎风格的ECS中，所有Transform数据以SOA方式排列，
 * 这样可以提高缓存命中率和并行处理效率
 */
class TransformStorage
{
private:
    // SOA布局: 每个属性独立存储在连续数组中
    std::vector<Vector3f>  positions;      ///<位置数组
    std::vector<Vector4f>  rotations;      ///<旋转数组(四元数: x, y, z, w)
    std::vector<Vector3f>  scales;         ///<缩放数组
    
    std::vector<int>       free_indices;   ///<空闲索引列表，用于复用已删除的槽位

public:
    TransformStorage() = default;
    virtual ~TransformStorage() = default;

public:
    /**
     * 添加一个Transform组件
     * @param position 位置
     * @param rotation 旋转(四元数)
     * @param scale 缩放
     * @return 返回在Storage中的索引
     */
    int AddTransform(const Vector3f& position, const Vector4f& rotation, const Vector3f& scale)
    {
        int index;
        
        // 如果有空闲槽位，复用它
        if (!free_indices.empty())
        {
            index = free_indices.back();
            free_indices.pop_back();
            
            positions[index] = position;
            rotations[index] = rotation;
            scales[index] = scale;
        }
        else
        {
            // 否则在末尾添加新元素
            index = static_cast<int>(positions.size());
            positions.push_back(position);
            rotations.push_back(rotation);
            scales.push_back(scale);
        }
        
        return index;
    }
    
    /**
     * 删除指定索引的Transform
     * @param index 要删除的索引
     */
    void RemoveTransform(int index)
    {
        if (index < 0 || index >= static_cast<int>(positions.size()))
            return;
            
        // 将索引加入空闲列表以供复用
        free_indices.push_back(index);
    }
    
    /**
     * 获取Transform数量(包括已删除的槽位)
     */
    int GetCapacity() const
    {
        return static_cast<int>(positions.size());
    }
    
    /**
     * 获取实际使用的Transform数量
     */
    int GetActiveCount() const
    {
        return static_cast<int>(positions.size() - free_indices.size());
    }

public:
    // 获取SOA数组的直接访问(用于批量处理)
    
    /**
     * 获取位置数组
     */
    Vector3f* GetPositions()
    {
        if (positions.empty())
            return nullptr;
        return positions.data();
    }
    
    const Vector3f* GetPositions() const
    {
        if (positions.empty())
            return nullptr;
        return positions.data();
    }
    
    /**
     * 获取旋转数组
     */
    Vector4f* GetRotations()
    {
        if (rotations.empty())
            return nullptr;
        return rotations.data();
    }
    
    const Vector4f* GetRotations() const
    {
        if (rotations.empty())
            return nullptr;
        return rotations.data();
    }
    
    /**
     * 获取缩放数组
     */
    Vector3f* GetScales()
    {
        if (scales.empty())
            return nullptr;
        return scales.data();
    }
    
    const Vector3f* GetScales() const
    {
        if (scales.empty())
            return nullptr;
        return scales.data();
    }

public:
    // 单个元素访问
    
    /**
     * 获取指定索引的位置
     */
    Vector3f& GetPosition(int index)
    {
        return positions[index];
    }
    
    const Vector3f& GetPosition(int index) const
    {
        return positions[index];
    }
    
    /**
     * 设置指定索引的位置
     */
    void SetPosition(int index, const Vector3f& position)
    {
        if (index >= 0 && index < static_cast<int>(positions.size()))
            positions[index] = position;
    }
    
    /**
     * 获取指定索引的旋转
     */
    Vector4f& GetRotation(int index)
    {
        return rotations[index];
    }
    
    const Vector4f& GetRotation(int index) const
    {
        return rotations[index];
    }
    
    /**
     * 设置指定索引的旋转
     */
    void SetRotation(int index, const Vector4f& rotation)
    {
        if (index >= 0 && index < static_cast<int>(rotations.size()))
            rotations[index] = rotation;
    }
    
    /**
     * 获取指定索引的缩放
     */
    Vector3f& GetScale(int index)
    {
        return scales[index];
    }
    
    const Vector3f& GetScale(int index) const
    {
        return scales[index];
    }
    
    /**
     * 设置指定索引的缩放
     */
    void SetScale(int index, const Vector3f& scale)
    {
        if (index >= 0 && index < static_cast<int>(scales.size()))
            scales[index] = scale;
    }
    
    /**
     * 检查索引是否有效且未被删除
     */
    bool IsValidIndex(int index) const
    {
        if (index < 0 || index >= static_cast<int>(positions.size()))
            return false;
            
        // 检查是否在空闲列表中
        for (size_t i = 0; i < free_indices.size(); ++i)
        {
            if (free_indices[i] == index)
                return false;
        }
        
        return true;
    }
    
    /**
     * 清空所有Transform数据
     */
    void Clear()
    {
        positions.clear();
        rotations.clear();
        scales.clear();
        free_indices.clear();
    }
};

TRANSFORM_STORAGE_NAMESPACE_END
