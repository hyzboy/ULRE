#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/type/ObjectManage.h>

VK_NAMESPACE_BEGIN

using BufferID = int;

GRAPH_MODULE_CLASS(BufferManager)
{
private:

    IDObjectManage<BufferID, DeviceBuffer> rm_buffers;                 ///<缓冲区合集

    void AddBuffer(const AnsiString &buf_name, DeviceBuffer *buf);

private:

    BufferManager(RenderFramework *);
    ~BufferManager() = default;

    friend class GraphModuleManager;

public: //Add/Get/Release

    BufferID        Add(DeviceBuffer *buf) { return rm_buffers.Add(buf); }
    DeviceBuffer *  Get(const BufferID &id) { return rm_buffers.Get(id); }
    void            Release(DeviceBuffer *buf) { rm_buffers.Release(buf); }

public: // VAB/VAO

    VAB *CreateVAB(VkFormat format, uint32_t count, const void *data, SharingMode sm = SharingMode::Exclusive);
    VAB *CreateVAB(VkFormat format, uint32_t count, SharingMode sm = SharingMode::Exclusive) { return CreateVAB(format, count, nullptr, sm); }

public: // Buffer creation methods

    #define BUFFER_MANAGER_CREATE_FUNC(name)  DeviceBuffer *Create##name(const AnsiString &buf_name, VkDeviceSize size, void *data, SharingMode sm = SharingMode::Exclusive);   \
                                              DeviceBuffer *Create##name(const AnsiString &buf_name, VkDeviceSize size, SharingMode sm = SharingMode::Exclusive) { return Create##name(buf_name, size, nullptr, sm); }

    BUFFER_MANAGER_CREATE_FUNC(UBO)
    BUFFER_MANAGER_CREATE_FUNC(SSBO)
    BUFFER_MANAGER_CREATE_FUNC(INBO)

    #undef BUFFER_MANAGER_CREATE_FUNC

public: // Index Buffer creation

    IndexBuffer *CreateIBO(IndexType index_type, uint32_t count, const void *data, SharingMode sm = SharingMode::Exclusive);
    IndexBuffer *CreateIBO8(uint32_t count, const uint8 *data, SharingMode sm = SharingMode::Exclusive) { return CreateIBO(IndexType::U8, count, (void *)data, sm); }
    IndexBuffer *CreateIBO16(uint32_t count, const uint16 *data, SharingMode sm = SharingMode::Exclusive) { return CreateIBO(IndexType::U16, count, (void *)data, sm); }
    IndexBuffer *CreateIBO32(uint32_t count, const uint32 *data, SharingMode sm = SharingMode::Exclusive) { return CreateIBO(IndexType::U32, count, (void *)data, sm); }

    IndexBuffer *CreateIBO(IndexType index_type, uint32_t count, SharingMode sm = SharingMode::Exclusive) { return CreateIBO(index_type, count, nullptr, sm); }
    IndexBuffer *CreateIBO8(uint32_t count, SharingMode sm = SharingMode::Exclusive) { return CreateIBO(IndexType::U8, count, nullptr, sm); }
    IndexBuffer *CreateIBO16(uint32_t count, SharingMode sm = SharingMode::Exclusive) { return CreateIBO(IndexType::U16, count, nullptr, sm); }
    IndexBuffer *CreateIBO32(uint32_t count, SharingMode sm = SharingMode::Exclusive) { return CreateIBO(IndexType::U32, count, nullptr, sm); }

};//class BufferManager

VK_NAMESPACE_END