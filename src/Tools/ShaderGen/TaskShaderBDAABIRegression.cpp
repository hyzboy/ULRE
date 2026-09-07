#include <hgl/graph/TaskShaderBDA.h>
#include <hgl/mtl/contract/ShaderGenContract.h>

using namespace hgl::graph;
using namespace hgl::graph::mtl::contract;

namespace
{
    PhysicalDeviceProfileLite MakeValidProfile()
    {
        PhysicalDeviceProfileLite profile{};

        profile.features.task_shader = true;
        profile.features.mesh_shader = true;
        profile.features.buffer_device_address = true;
        profile.features.scalar_block_layout = true;
        profile.features.shader_int64 = true;

        profile.limits.max_push_constants_size = 128;
        profile.limits.max_task_payload_size = TaskShaderBDARequiredPayloadBytes;
        profile.limits.max_task_payload_and_shared_memory_size =
            TaskShaderBDARequiredPayloadBytes;
        profile.limits.max_task_shared_memory_size = 1;
        profile.limits.max_task_work_group_total_count = 1;
        profile.limits.max_task_work_group_count_x = 1;
        profile.limits.max_task_work_group_count_y = 1;
        profile.limits.max_task_work_group_count_z = 1;
        profile.limits.max_task_work_group_invocations = 1;
        profile.limits.max_task_work_group_size_x = 1;
        profile.limits.max_task_work_group_size_y = 1;
        profile.limits.max_task_work_group_size_z = 1;

        profile.limits.max_mesh_work_group_total_count = 1;
        profile.limits.max_mesh_work_group_count_x = 1;
        profile.limits.max_mesh_work_group_count_y = 1;
        profile.limits.max_mesh_work_group_count_z = 1;
        profile.limits.max_mesh_work_group_invocations = 1;
        profile.limits.max_mesh_work_group_size_x = 1;
        profile.limits.max_mesh_work_group_size_y = 1;
        profile.limits.max_mesh_work_group_size_z = 1;
        profile.limits.max_mesh_output_vertices = 3;
        profile.limits.max_mesh_output_primitives = 1;

        return profile;
    }
}

int main()
{
    if (kTaskMeshDispatchRootFieldCount != 8
     || kTaskMeshPayloadFieldCount != 7
     || sizeof(MeshDispatchRoot) != 48
     || sizeof(TaskMeshPayload) != 32
     || sizeof(TextDispatchRoot) != 96)
        return 1;

    PhysicalDeviceProfileLite profile = MakeValidProfile();
    const char *reason = nullptr;
    if (!ValidateTaskShaderBDAProfile(profile, reason))
        return 2;

    profile.limits.max_task_payload_size = TaskShaderBDARequiredPayloadBytes - 1;
    if (ValidateTaskShaderBDAProfile(profile, reason))
        return 3;

    profile = MakeValidProfile();
    profile.features.task_shader = false;
    if (ValidateTaskShaderBDAProfile(profile, reason))
        return 4;

    return 0;
}
