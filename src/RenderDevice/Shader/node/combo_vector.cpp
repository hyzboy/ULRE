#include<hgl/graph/shader/node/combo_vector.h>

BEGIN_SHADER_NODE_NAMESPACE
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
END_SHADER_NODE_NAMESPACE
