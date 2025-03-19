#pragma once

#include<hgl/component/Component.h>

namespace hgl::graph
{
    /**
    * 可渲染组件
    */
    class RenderComponent: public Component
    {
    public:

        RenderComponent()=default;
        virtual ~RenderComponent()=default;

        virtual void Render()=0;
    };//class RenderComponent
}//namespace hgl::graph
