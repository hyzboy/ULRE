#include<hgl/ecs/InputSystem.h>
#include<iostream>

namespace hgl::ecs
{
    io::EventProcResult InputSystem::OnEvent(const io::EventHeader &header, const uint64 data)
    {
        std::cout<<"InputSystem::OnEvent: type="<<static_cast<int>(header.type)<<" id="<<header.id<<std::endl;

        return io::WindowEvent::OnEvent(header, data);
    }

    void InputSystem::Update(float deltaTime)
    {
        // 这里可以处理持续按键等输入状态
    }
}//namespace hgl::ecs
