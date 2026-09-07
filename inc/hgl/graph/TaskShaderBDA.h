#pragma once

#include <cstddef>
#include <cstdint>

namespace hgl::graph
{
    enum class TaskMeshFamily : uint32_t
    {
        Triangle = 0,
        Line     = 1,
        Text     = 2,
        Count
    };

    constexpr uint32_t TaskShaderBDAAbiRevision = 1;

    // The field list is the ABI source for the CPU root and future fixed GLSL
    // Task/Mesh declarations. Keep the order stable once SPV artifacts exist.
#define HGL_TASK_MESH_DISPATCH_ROOT_FIELD_LIST(M) \
    M(mesh_draw_params_address, uint64_t, "uint64_t") \
    M(l2w_address,              uint64_t, "uint64_t") \
    M(l2w_index_address,        uint64_t, "uint64_t") \
    M(material_data_addresses_address, uint64_t, "uint64_t") \
    M(draw_count,               uint32_t, "uint") \
    M(dispatch_base,            uint32_t, "uint") \
    M(family,                   uint32_t, "uint") \
    M(abi_revision,             uint32_t, "uint")

    struct alignas(16) MeshDispatchRoot
    {
#define HGL_TASK_MESH_DISPATCH_ROOT_CPU_FIELD(name, cpu_type, glsl_type) cpu_type name;
        HGL_TASK_MESH_DISPATCH_ROOT_FIELD_LIST(HGL_TASK_MESH_DISPATCH_ROOT_CPU_FIELD)
#undef HGL_TASK_MESH_DISPATCH_ROOT_CPU_FIELD
    };

    constexpr const char *const kTaskMeshDispatchRootFieldNames[] =
    {
#define HGL_TASK_MESH_DISPATCH_ROOT_NAME_FIELD(name, cpu_type, glsl_type) #name,
        HGL_TASK_MESH_DISPATCH_ROOT_FIELD_LIST(HGL_TASK_MESH_DISPATCH_ROOT_NAME_FIELD)
#undef HGL_TASK_MESH_DISPATCH_ROOT_NAME_FIELD
    };

    constexpr const char *const kTaskMeshDispatchRootFieldGLSLTypes[] =
    {
#define HGL_TASK_MESH_DISPATCH_ROOT_GLSL_FIELD(name, cpu_type, glsl_type) glsl_type,
        HGL_TASK_MESH_DISPATCH_ROOT_FIELD_LIST(HGL_TASK_MESH_DISPATCH_ROOT_GLSL_FIELD)
#undef HGL_TASK_MESH_DISPATCH_ROOT_GLSL_FIELD
    };

    constexpr uint32_t kTaskMeshDispatchRootFieldCount =
        static_cast<uint32_t>(
            sizeof(kTaskMeshDispatchRootFieldNames)
            / sizeof(kTaskMeshDispatchRootFieldNames[0]));

#undef HGL_TASK_MESH_DISPATCH_ROOT_FIELD_LIST

    static_assert(alignof(MeshDispatchRoot) == 16);
    static_assert(offsetof(MeshDispatchRoot, mesh_draw_params_address) == 0);
    static_assert(offsetof(MeshDispatchRoot, l2w_address) == 8);
    static_assert(offsetof(MeshDispatchRoot, l2w_index_address) == 16);
    static_assert(offsetof(MeshDispatchRoot, material_data_addresses_address) == 24);
    static_assert(offsetof(MeshDispatchRoot, draw_count) == 32);
    static_assert(offsetof(MeshDispatchRoot, dispatch_base) == 36);
    static_assert(offsetof(MeshDispatchRoot, family) == 40);
    static_assert(offsetof(MeshDispatchRoot, abi_revision) == 44);
    static_assert(sizeof(MeshDispatchRoot) == 48);

    // Task writes one payload for the Mesh workgroup(s) it emits. Do not copy
    // the full MeshDrawParams row into this payload.
#define HGL_TASK_MESH_PAYLOAD_FIELD_LIST(M) \
    M(mesh_draw_params_address, uint64_t, "uint64_t") \
    M(record_index,            uint32_t, "uint") \
    M(mesh_chunk_index,        uint32_t, "uint") \
    M(instance_index,          uint32_t, "uint") \
    M(mesh_chunk_count,        uint32_t, "uint") \
    M(instance_count,          uint32_t, "uint") \
    M(reserved,                uint32_t, "uint")

    struct alignas(16) TaskMeshPayload
    {
#define HGL_TASK_MESH_PAYLOAD_CPU_FIELD(name, cpu_type, glsl_type) cpu_type name;
        HGL_TASK_MESH_PAYLOAD_FIELD_LIST(HGL_TASK_MESH_PAYLOAD_CPU_FIELD)
#undef HGL_TASK_MESH_PAYLOAD_CPU_FIELD
    };

    constexpr const char *const kTaskMeshPayloadFieldNames[] =
    {
#define HGL_TASK_MESH_PAYLOAD_NAME_FIELD(name, cpu_type, glsl_type) #name,
        HGL_TASK_MESH_PAYLOAD_FIELD_LIST(HGL_TASK_MESH_PAYLOAD_NAME_FIELD)
#undef HGL_TASK_MESH_PAYLOAD_NAME_FIELD
    };

    constexpr const char *const kTaskMeshPayloadFieldGLSLTypes[] =
    {
#define HGL_TASK_MESH_PAYLOAD_GLSL_FIELD(name, cpu_type, glsl_type) glsl_type,
        HGL_TASK_MESH_PAYLOAD_FIELD_LIST(HGL_TASK_MESH_PAYLOAD_GLSL_FIELD)
#undef HGL_TASK_MESH_PAYLOAD_GLSL_FIELD
    };

    constexpr uint32_t kTaskMeshPayloadFieldCount =
        static_cast<uint32_t>(
            sizeof(kTaskMeshPayloadFieldNames)
            / sizeof(kTaskMeshPayloadFieldNames[0]));

#undef HGL_TASK_MESH_PAYLOAD_FIELD_LIST

    static_assert(alignof(TaskMeshPayload) == 16);
    static_assert(offsetof(TaskMeshPayload, mesh_draw_params_address) == 0);
    static_assert(offsetof(TaskMeshPayload, record_index) == 8);
    static_assert(offsetof(TaskMeshPayload, mesh_chunk_index) == 12);
    static_assert(offsetof(TaskMeshPayload, instance_index) == 16);
    static_assert(offsetof(TaskMeshPayload, mesh_chunk_count) == 20);
    static_assert(offsetof(TaskMeshPayload, instance_count) == 24);
    static_assert(offsetof(TaskMeshPayload, reserved) == 28);
    static_assert(sizeof(TaskMeshPayload) == 32);

    // Text keeps the shared root prefix and appends the three text-table
    // addresses plus the character range used by the fixed Text Task shader.
    struct alignas(16) TextDispatchRoot
    {
        MeshDispatchRoot common;
        uint64_t char_info_address;
        uint64_t char_style_address;
        uint64_t char_instance_address;
        uint32_t char_count;
        uint32_t char_dispatch_base;
        uint32_t reserved0;
        uint32_t reserved1;
    };

    static_assert(alignof(TextDispatchRoot) == 16);
    static_assert(offsetof(TextDispatchRoot, common) == 0);
    static_assert(offsetof(TextDispatchRoot, char_info_address) == 48);
    static_assert(offsetof(TextDispatchRoot, char_style_address) == 56);
    static_assert(offsetof(TextDispatchRoot, char_instance_address) == 64);
    static_assert(offsetof(TextDispatchRoot, char_count) == 72);
    static_assert(offsetof(TextDispatchRoot, char_dispatch_base) == 76);
    static_assert(offsetof(TextDispatchRoot, reserved0) == 80);
    static_assert(offsetof(TextDispatchRoot, reserved1) == 84);
    static_assert(sizeof(TextDispatchRoot) == 96);

    constexpr uint32_t TaskShaderBDARequiredPayloadBytes =
        static_cast<uint32_t>(sizeof(TaskMeshPayload));
    constexpr uint32_t TaskShaderBDARequiredPushConstantBytes =
        static_cast<uint32_t>(sizeof(TextDispatchRoot));
}
