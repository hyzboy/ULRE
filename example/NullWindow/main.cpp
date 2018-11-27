#include<hgl/render/RenderDevice.h>
#include<iostream>
#include<math.h>

using namespace hgl;

void put(const struct VideoMode *vm)
{
    std::cout<<"\t"<<vm->width<<"x"<<vm->height<<","<<vm->bit<<"bits,"<<vm->freq<<"hz";
}

void put(const struct Display *disp)
{
    const int inch=sqrt((disp->width*disp->width)+(disp->height*disp->height))*0.03937008;

    std::cout<<"["<<disp->name.c_str()<<"]["<<disp->width<<"x"<<disp->height<<" mm,"<<inch<<" inch][offset "<<disp->x<<","<<disp->y<<"]"<<std::endl;

    std::cout<<"\tcurrent video mode: ";
    put(disp->GetCurVideoMode());
    std::cout<<std::endl;

    const ObjectList<VideoMode> &vml=disp->GetVideoModeList();

    for(int i=0;i<vml.GetCount();i++)
    {
        std::cout<<"\t"<<i<<" : ";
        put(vml[i]);
        std::cout<<std::endl;
    }
}

int main(void)
{
    RenderDevice *device=CreateRenderDeviceGLFW();

    if(!device)
    {
        std::cerr<<"Create RenderDevice(GLFW) failed."<<std::endl;
        return -1;
    }

    if(!device->Init())
    {
        std::cerr<<"Init RenderDevice(GLFW) failed."<<std::endl;
        return -2;
    }

    {
        const UTF8String device_name=device->GetName();

        std::cout<<"RenderDevice: "<<device_name.c_str()<<std::endl;
    }

    List<Display *> disp_list;

    device->GetDisplayList(disp_list);

    const int count=disp_list.GetCount();

    std::cout<<"Device have "<<count<<" display."<<std::endl;

    for(int i=0;i<count;i++)
    {
        std::cout<<"Display "<<i<<" ";
        put(GetObject(disp_list,i));

        std::cout<<std::endl;
    }

    return 0;
}
