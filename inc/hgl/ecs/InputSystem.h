#pragma once

#include<hgl/ecs/System.h>
#include<hgl/io/event/WindowEvent.h>
#include<hgl/math/Vector.h>

namespace hgl::ecs
{
    using namespace hgl::math;

    class InputSystem : public ecs::System,io::WindowEvent
    {
    protected:

        Vector2i mouse_coord;

        virtual io::EventProcResult OnEvent(const io::EventHeader &header, const uint64 data) override;

    public:

        io::EventDispatcher *GetEventDispatcher() { return this; }

        const Vector2i &GetMouseCoord() const { return mouse_coord; }

    public:

        InputSystem() = default;
        virtual ~InputSystem() = default;

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
