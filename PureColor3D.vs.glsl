#version 450 core
#extension GL_ARB_gpu_shader_int64 : enable

#define AMBIENT_MODEL 0
#define LIGHT_MODEL 2
#define SPECULAR_SPLIT 0
#define SHADOW_MODE 0

layout(set=0, binding=0) uniform ViewportInfo {
    // 数据将在运行时填充
    vec4 _placeholder;
} viewport;

layout(set=0, binding=1) uniform CameraInfo {
    // 数据将在运行时填充
    vec4 _placeholder;
} camera;

layout(set=0, binding=2, std430) buffer LocalToWorldData {
    // 数据将在运行时填充
    vec4 _data[];
} l2w;

layout(set=0, binding=3, std430) buffer MaterialInstanceData {
    // 数据将在运行时填充
    vec4 _data[];
} mtl;

struct VertexInput {
    vec3 Position;
    uint TransformID;
    uint MaterialInstanceID;
};

struct VS_Output {
    vec4 ClipPos;           // 隐式，写入 gl_Position
    vec3 WorldPosition;     // 世界坐标
    uint MaterialInstanceID; // 材质实例索引
};

struct LightingOutput {
    vec3 diffuse;           // 漫反射颜色
    vec3 specular;          // 高光颜色
    vec3 reflection;        // 反射色（IBL 用）
};

vec4 Color;

// 坐标变换辅助函数
vec4 GetLocalToWorldPos(vec4 local_pos) {
    return GetLocalToWorld() * local_pos;
}

vec4 GetClipSpacePos(vec4 world_pos) {
    return ViewProj * world_pos;
}

vec4 GetScreenSpacePos(vec4 clip_pos) {
    return clip_pos;  // 隐式除以 w
}


mat4 GetLocalToWorld() {
    return LocalToWorld;  // 从 LocalToWorld UBO 读取
}


mat3 GetNormalMatrix() {
    // = transpose(inverse(mat3(ViewMatrix * LocalToWorld)))
    // 框架简化为直接计算
    return mat3(GetLocalToWorld());
}


vec3 GetNormal(vec3 local_normal) {
    return normalize(GetNormalMatrix() * local_normal);
}


vec4 GetPosition3D() {
    return GetLocalToWorld() * vec4(Position, 1.0);
}

vec4 GetClipPosition() {
    return ViewProj * GetPosition3D();
}


MaterialInstance GetMaterialInstance() {
    return mi.mi[MaterialInstanceID];
}

MaterialInstance GetMI() {
    return GetMaterialInstance();
}


vec4 VertexShaderBusiness(const VertexInput vi)
{
    return vec4(vi.Position, 1.0);
}



void main() {
    VertexInput vi;
    vi.Position = Position;

    vec4 local_pos = VertexShaderBusiness(vi);
    gl_Position = GetClipPosition();
    
    VS_Output vso;
    vso.ClipPos = gl_Position;
    vso.WorldPosition = GetPosition3D().xyz;
    vso.MaterialInstanceID = MaterialInstanceID;
}
