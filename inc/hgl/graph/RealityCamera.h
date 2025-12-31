#pragma once

#include<math>
#include<hgl/CoreType.h>

namespace hgl::graph
{
    namespace RealityCamera
    {
        struct SensorSize
        {
            float width;                ///<传感器宽度(毫米)
            float height;               ///<传感器高度(毫米)
        };

        enum class SensorSizeType
        {
            FullFrame,                  ///<全画幅(35mm)
            APS_C,                      ///<APS-C
            APS_H,                      ///<APS-H
            MicroFourThirds,            ///<Micro Four Thirds
            OneInch,                    ///<1英寸
            FourThirds,                 ///<4/3英寸
            Super35,                    ///<Super 35mm
            MediumFormat                ///<中画幅
        };

        constexpr const SensorSize SensorSizeList[]=
        {
            {36.0f,24.0f},              ///<FullFrame
            {22.5f,15.0f},              ///<APS-C
            {28.7f,19.1f},              ///<APS-H
            {17.3f,13.0f},              ///<MicroFourThirds
            {13.2f,8.8f},               ///<OneInch
            {18.0f,13.5f},              ///<FourThirds
            {24.89f,18.66f},            ///<Super35
            {53.4f,40.0f}               ///<MediumFormat
        };

        struct Lens
        {
            struct Aperture
            {
                float min;              ///<最小光圈
                float max;              ///<最大光圈
            }aperture;

            struct FocalLength
            {
                float min;              ///<最小焦距(毫米)
                float max;              ///<最大焦距(毫米)
            }focal_length;              ///<焦距(毫米)

            struct Focus
            {
                float min;              ///<最小对焦距离(毫米)
                float max;              ///<最大对焦距离(毫米)
            }focus;
        };

        /**
        * 相机设置
        */
        struct Settings
        {
            SensorSize  sensor_size;                    ///<传感器尺寸(毫米)

            float       focal_length    =50.0f;         ///<焦距(毫米)
            float       focus_distance  =1000.0f;       ///<对焦距离(毫米)

            float       aperture        =1.0f;          ///<光圈(Fnumber)
            float       shutter         =1.0f/8.0f;     ///<快门时间(秒)
            float       iso             =800.0f;        ///<ISO感光度
            float       exposure        =0.0f;          ///<曝光补偿(单位：EV)

            uint        rotate_angle    =0;             ///<旋转角度(单位：度，仅可以为0,90,180,270)

        public:

            const bool isVertical   ()const{return rotate_angle==90||rotate_angle==270;}
            const bool isHorizontal ()const{return rotate_angle== 0||rotate_angle==180;}

            const float GetFOV()const
            {
                const float size=(isVertical?sensor_size.height:sensor_size.width);

                return 2.0f*std::atan(size/(2.0f*focal_length));
            }

            const void GetDOF(float &dof_near,float &dof_far)const
            {
                dof_near=focus_distance/(1.0f+(focus_distance*aperture)/(focal_length*focus_distance));
                dof_far =focus_distance/(1.0f-(focus_distance*aperture)/(focal_length*focus_distance));
            }
        };//
    }//namespace RealityCamera
}//namespace hgl::graph
