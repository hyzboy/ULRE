// ─────────────────────────────────────────────────────────────────────────────
// @ulre-codegen begin
//   由 src/Tools/BufferGen/buffer_gen.py 生成，请勿手工修改。
//   源：inc/hgl/graph/ssbo/MaterialDataRows.h → MaterialDataRow
//   布局：kind=ssbo bda=1 access=readwrite size=16 align=4 ref_align=16
//   说明：材质数据组件行（GlobalSSBOType::MaterialData）：行内只说「引用哪一行内容」+ 授权位/revision；行 0 = 无材质数据哨兵。当前 shader 侧只经组件行三元组定位内容行，不直接读本行 ⇒ 本文件暂无人包含。
// @ulre-codegen end
// ─────────────────────────────────────────────────────────────────────────────
#ifndef HGL_SHADERLIB_SSBO_MATERIAL_DATA_ROW_GLSL
#define HGL_SHADERLIB_SSBO_MATERIAL_DATA_ROW_GLSL

struct MaterialDataRowData
{
    uint surface_type;
    uint surface_ssbo_id;
    uint surface_data_index;
    uint authored_flags;
};

layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialDataRowRef
{
    uint surface_type;
    uint surface_ssbo_id;
    uint surface_data_index;
    uint authored_flags;
};

#endif // HGL_SHADERLIB_SSBO_MATERIAL_DATA_ROW_GLSL
