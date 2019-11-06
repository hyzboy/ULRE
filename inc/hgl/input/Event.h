#pragma once
#include<hgl/type/DataType.h>
namespace hgl
{
    namespace device_input
    {
        #pragma pack(push,1)
        union InputEvent
        {
            struct
            {
                uint8 key;
            }key_push;

            struct
            {
                uint8 key;
            }key_pop;

            struct
            {
                uint16 x, y;
            }mouse_move;

            struct
            {
                uint8 key;
            }mouse_key_push;

            struct
            {
                uint8 key;
            }mouse_key_pop;

            struct
            {
                int16 x, y;
            }mouse_wheel;

            struct
            {
                int8 x, y;
            }joystick_axis;

            struct
            {
                uint8 key;
            }joystick_key_push;

            struct
            {
                uint8 key;
            }joystick_key_pop;

            struct
            {
                uint16 x, y;
                uint16 power;
            }wacom;

            struct
            {
                u16char ch;
            }char_input;
        };//union InputEvent
        #pragma pack(pop)
    }//namespace device_input
}//namespace hgl
