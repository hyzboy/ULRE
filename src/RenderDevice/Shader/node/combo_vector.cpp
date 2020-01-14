#include<hgl/graph/shader/node/combo_vector.h>

SHADER_NODE_NAMESPACE_BEGIN
bool ComboVector1to2::GenOutputParamCode(UTF8StringList &sl)
{
    UTF8String name_x,name_y;
    UTF8String name_output;

    if(!GetInputParamName(name_x,ip_x))return(false);
    if(!GetInputParamName(name_y,ip_y))return(false);

    if(!GetOutputParamName(name_output,op_xy))return(false);

    sl.Add(name_output+"=vec2("+name_x+","+name_y+");");

    return(true);
}

bool ComboVector1to3::GenOutputParamCode(UTF8StringList &sl)
{
    UTF8String name_x,name_y,name_z;
    UTF8String name_output;

    if(!GetInputParamName(name_x,ip_x))return(false);
    if(!GetInputParamName(name_y,ip_y))return(false);
    if(!GetInputParamName(name_z,ip_z))return(false);

    if(!GetOutputParamName(name_output,op_xyz))return(false);

    sl.Add(name_output+"=vec3("+name_x+","+name_y+","+name_z+");");

    return(true);
}
SHADER_NODE_NAMESPACE_END
