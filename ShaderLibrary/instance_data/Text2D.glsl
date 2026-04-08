// instance_data/Text2D.glsl
// InstanceDataLayout::Text2D — stride = 4 bytes
// Used by: Text2D shaders
struct MaterialInstance
{
    uint TextColor;     // 4 bytes (packed RGBA via unpackUnorm4x8)
};
