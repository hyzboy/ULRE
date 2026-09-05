#include<hgl/graph/module/MaterialDataArena.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/log/Log.h>
#include<cstdlib>

namespace hgl::graph
{
/**
 * 材质数据 Arena 容量（字节）：默认 64MB，环境变量 ULRE_MATERIAL_ARENA_MB
 * 可配置（16..512，越界值回退默认）。
 */
uint64 GetMaterialDataArenaCapacity()
{
    constexpr uint64 MB=1024ull*1024ull;

    static const uint64 capacity=[]()
    {
        uint64 mb=64;

        if(const wchar_t *env=_wgetenv(L"ULRE_MATERIAL_ARENA_MB"))
        {
            const long value=wcstol(env,nullptr,10);

            if(value>=16&&value<=512)
                mb=(uint64)value;
        }

        return mb*MB;
    }();

    return capacity;
}

bool MaterialDataArena::Init(VulkanDevice *dev,const AnsiString &name,const uint64 arena_bytes,const uint32 in_block_size)
{
    if(!dev||arena_bytes==0)
        return false;

    if(arena_buffer)
        return false;               // 已初始化；如需重建先 Close()

    Close();

    block_size=(in_block_size>=16)?in_block_size:16;

    // 容量须为块大小整倍数，向下对齐
    capacity=arena_bytes-arena_bytes%block_size;

    arena_buffer=dev->CreateArenaBuffer(name,(VkDeviceSize)capacity);

    if(!arena_buffer)
    {
        GLogError(u8"[MaterialDataArena] CreateArenaBuffer failed: name=%s bytes=%llu",
                  name.c_str(),(unsigned long long)capacity);
        return false;
    }

    // 持久映射整块（ReBarBuffer::Map 转发 DeviceMemory::Map，后者缓存映射指针）
    mapped=(uint8 *)arena_buffer->GetGPUBuffer()->Map(0,(VkDeviceSize)capacity);

    if(!mapped)
    {
        GLogError(u8"[MaterialDataArena] persist map failed: name=%s",name.c_str());
        Close();
        return false;
    }

    device_address=dev->GetBufferDeviceAddress(arena_buffer->GetBuffer());

    if(device_address==0)
    {
        GLogError(u8"[MaterialDataArena] query device address failed: name=%s",name.c_str());
        Close();
        return false;
    }

    block_count=(uint32)(capacity/block_size);

    memory_allocator=new MappedArenaAllocator(mapped,capacity);

    pool=new BlockPool;

    if(!pool->Init(memory_allocator,block_count,block_size))
    {
        GLogError(u8"[MaterialDataArena] BlockPool init failed: blocks=%u",block_count);
        Close();
        return false;
    }

    // I4：0 号块零填充——"默认行"，任何 block index 的地址都安全可解引用
    memset(mapped,0,block_size);

    GLogInfo(u8"[MaterialDataArena] init ok: name=%s capacity=%lluMB blocks=%ux%uB base=0x%llx",
             name.c_str(),
             (unsigned long long)(capacity/(1024ull*1024ull)),
             block_count,block_size,
             (unsigned long long)device_address);
    return true;
}

void MaterialDataArena::Close()
{
    // 先清 typed allocator：其析构会 ReleaseAll 把批量段归还仍存活的 block_pool，
    // 必须发生在 block_pool 复位之前
    typed_allocators.clear();

    if(pool)
    {
        delete pool;
        pool=nullptr;
    }

    if(memory_allocator)
    {
        delete memory_allocator;
        memory_allocator=nullptr;
    }

    device_address=0;
    block_count=0;
    capacity=0;
    mapped=nullptr;

    if(arena_buffer)
    {
        delete arena_buffer;        // 析构链: DeviceBuffer → ReBarBuffer → DeviceMemory/VkBuffer
        arena_buffer=nullptr;
    }
}
}//namespace hgl::graph
