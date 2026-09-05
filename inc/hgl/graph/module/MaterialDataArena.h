#pragma once

#include<hgl/type/BlockPool.h>
#include<hgl/type/TypedBlockAllocator.h>
#include<hgl/type/MemoryAllocator.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/type/String.h>
#include<vector>
#include<memory>

namespace hgl::graph
{
class VulkanDevice;
class MaterialDataArena;

/**
 * 材质数据 Arena 容量（字节）：默认 64MB，环境变量 ULRE_MATERIAL_ARENA_MB
 * 可配置（16..512，越界值回退默认）。进程内只计算一次。
 */
uint64 GetMaterialDataArenaCapacity();

/**
 * 进程级 arena 单例（惰性创建）。首调用以传入设备初始化（容量取
 * GetMaterialDataArenaCapacity()）；后续调用忽略 device 返回同一实例。
 * device 为 null 且尚未创建时返回 null。
 */
MaterialDataArena *AcquireMaterialDataArena(VulkanDevice *device);

/**
 * 材质数据 Arena（全局唯一材质实例数据区）
 *
 * 一块预分配的 HOST_VISIBLE|SHADER_DEVICE_ADDRESS VkBuffer，以 16 字节块粒度
 * 由 CMCore BlockPool/TypedBlockAllocator 管理所有材质实例数据行（不分类型共用）。
 *
 * 关键不变量（见 doc/material-ssbo-arena-bda-refactor-plan-2026-09.md §2）：
 *   I1 永不 realloc、永不搬移——device address 会话内恒定，已下发 shader 的地址永不失效；
 *   I2 CPU 寻址契约 GetBlockPtr(b)==base+b*block_size（与 TypedBlockAllocatorTest 一致）；
 *   I3 block index 进入 GPU 的唯一通道是每帧重写的地址行表——块号不得存入任何跨帧结构；
 *   I4 0 号块为 BlockPool 保留哨兵，Init 时零填充，其地址是永远可安全解引用的"默认行"。
 *
 * CPU 写路径：HOST_COHERENT 持久映射直写，无需 flush；写必须在 vkQueueSubmit 之前完成。
 */
class MaterialDataArena
{
    /**
     * 映射显存适配器：把 Arena buffer 的持久映射指针注入 BlockPool。
     * 只暴露窗口、绝不分配——CanRealloc 恒 false（I1 红线）。
     */
    class MappedArenaAllocator:public AbstractMemoryAllocator
    {
        uint64 capacity=0;

    protected:

        bool AllocMemory() override
        {
            if(!memory_block||alloc_size>capacity)
                return(false);

            return(true);           // memory_block 已在构造时指向映射窗口
        }

    public:

        virtual const bool CanRealloc()const override{return(false);}

        MappedArenaAllocator(uint8 *base,const uint64 bytes)
        {
            memory_block=base;
            capacity=bytes;
        }

        void Free() override
        {
            // 映射窗口归 buffer 所有，这里仅复位状态
            memory_block=nullptr;
            data_size=0;
            alloc_size=0;
        }

        bool Write(const void *source,const uint64 offset,const uint64 size) override
        {
            if(!source||size==0)return(false);
            if(!memory_block||offset+size>capacity)return(false);

            memcpy((uint8 *)memory_block+offset,source,size);
            return(true);
        }
    };//class MappedArenaAllocator

    struct AllocatorHolderBase
    {
        uint32 type_id=0;
        virtual ~AllocatorHolderBase()=default;
    };

    template<typename T>
    struct AllocatorHolder:public AllocatorHolderBase
    {
        TypedBlockAllocator<T> alloc;
    };

    static uint32 AcquireTypeSeed()
    {
        static uint32 seed=0;
        return seed++;
    }

    template<typename T>
    static uint32 TypeIdOf()
    {
        static const uint32 id=AcquireTypeSeed();
        return id;
    }

    AllocatorHolderBase *FindAllocator(const uint32 type_id)
    {
        for(auto &holder:typed_allocators)
            if(holder->type_id==type_id)
                return holder.get();

        return nullptr;
    }

    DeviceBuffer *arena_buffer=nullptr;
    uint8 *mapped=nullptr;                  ///< 持久映射基址
    uint64 capacity=0;                      ///< 总字节数
    uint32 block_size=16;                   ///< 块粒度（字节）
    uint32 block_count=0;                   ///< 总块数
    uint64 device_address=0;                ///< arena 基址（会话恒定）

    MappedArenaAllocator *memory_allocator=nullptr;
    BlockPool *pool=nullptr;                ///< 块号池（堆持有：BlockPool 禁拷贝，且需支持复位重建）

    std::vector<std::unique_ptr<AllocatorHolderBase>> typed_allocators;

public:

    MaterialDataArena()=default;
    ~MaterialDataArena(){Close();}

    MaterialDataArena(const MaterialDataArena &)=delete;
    MaterialDataArena &operator=(const MaterialDataArena &)=delete;

    bool Init(VulkanDevice *dev,const AnsiString &name,const uint64 arena_bytes,const uint32 in_block_size=16);
    void Close();

    uint64 GetDeviceAddress()const{return device_address;}                          ///< arena 基址
    /**
     * 块号 → GPU 地址。越界块号钳到 0 号默认行（零填充）——
     * 防 BDA 解引用非法地址导致 GPU page fault / 驱动 TDR。
     */
    uint64 AddressOf(const uint32 block)const
    {
        const uint32 safe_block=(block<block_count)?block:0u;
        return device_address+(uint64)safe_block*block_size;
    }

    void *GetBase(){return mapped;}                                                 ///< 映射基址
    void *GetBlockPtr(const uint32 block){return mapped+(size_t)block*block_size;}  ///< 块号 → CPU 指针

    template<typename T>
    T *GetBlockPtr(const uint32 block){return (T *)GetBlockPtr(block);}

    uint32 AcquireBlocks(const uint32 count){return pool?pool->Acquire(count):0;}               ///< 申请连续块，返回起始块号(0=失败)
    bool ReleaseBlocks(const uint32 start){return pool&&pool->Release(start);}                  ///< 释放连续块

    /**
     * 连续段分配：一次性申请可容纳 count 个 T 行的连续块区间。
     * 供 AllocateArrayAccessor 的"连续行"语义使用；返回起始块号，0=失败。
     */
    template<typename T>
    uint32 AcquireRange(const uint32 count)
    {
        if(count==0||!pool)return(0);
        return pool->Acquire(count*SlotBlocks<T>());
    }

    template<typename T>
    uint32 SlotBlocks()const
    {
        const uint32 sb=((uint32)sizeof(T)+block_size-1)/block_size;
        return sb?sb:1;
    }

    /**
     * 按类型懒建共享分配器（同类型对象聚簇，对象级释放不归还上级池）。
     * 返回的指针生命周期与 arena 相同，调用方不得 delete。
     */
    template<typename T>
    TypedBlockAllocator<T> *GetAllocator(const uint32 batch_slots=16)
    {
        if(!pool)
            return nullptr;

        const uint32 type_id=TypeIdOf<T>();

        if(auto *holder=FindAllocator(type_id))
            return &(static_cast<AllocatorHolder<T> *>(holder)->alloc);

        auto holder=std::make_unique<AllocatorHolder<T>>();

        holder->type_id=type_id;

        if(!holder->alloc.Init(pool,batch_slots))
            return nullptr;

        TypedBlockAllocator<T> *result=&(holder->alloc);

        typed_allocators.emplace_back(std::move(holder));
        return result;
    }

    BlockPool *GetBlockPool(){return pool;}

    uint32 GetBlockSize()const{return block_size;}
    uint32 GetBlockCount()const{return block_count;}
    uint32 GetFreeBlocks()const{return pool?pool->GetFreeCount():0;}
    uint64 GetCapacity()const{return capacity;}
    DeviceBuffer *GetBuffer(){return arena_buffer;}
};//class MaterialDataArena
}//namespace hgl::graph
