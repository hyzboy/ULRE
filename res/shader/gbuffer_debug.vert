#version 450 core

layout(location = 0) in vec2 Vertex;

layout(location = 0) out vec2 FragmentPosition;

layout(location = 1) out uint FragmentTexID;
layout(location = 2) out vec2 FragmentTexCoord;

void main()
{
    gl_Position=vec4(Vertex,0.0,1.0);

    FragmentPosition=Vertex;
    FragmentTexCoord=(Vertex+1.0)/2.0;
    
    if(Vertex.x<0)
    {
        if(Vertex.y<0)
            FragmentTexID=0;
        else
            FragmentTexID=1;
    }
    else
    {
        if(Vertex.y<0)
            FragmentTexID=2;
        else
            FragmentTexID=3;
    }
}
