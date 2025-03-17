#pragma once

#include<hgl/component/Component.h>

namespace hgl::graph
{
    /**
    * 可渲染组件
    */
    class RenderComponent: public BaseComponent
    {
    public:

        RenderComponent()=default;
        virtual ~RenderComponent()=default;

        virtual void Render()=0;
    };//class RenderComponent
}//namespace hgl::graph
