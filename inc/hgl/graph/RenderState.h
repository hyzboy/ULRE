#ifndef HGL_RENDER_STATE_INCLUDE
#define HGL_RENDER_STATE_INCLUDE

#include<hgl/type/Color4f.h>
//#include<hgl/CompOperator.h>
#include<hgl/type/Set.h>
#include<GLEWCore/glew.h>
namespace hgl
{
    namespace graph
    {
        struct RenderStateBlock
        {
            /**
             * 应用当前状态
             */
            virtual void Apply()=0;

            /**
             * 创建一个自身状态的复制品
             */
            virtual RenderStateBlock *CreateCopy()const=0;
        };//struct RenderStateBlock

        #define RSB_OBJECT_BEGIN(rs_name)   struct rs_name:public RenderStateBlock   \
                                            {   \
                                                rs_name()=default;  \
                                                rs_name(const rs_name *obj){if(!obj)memcpy(this,obj,sizeof(rs_name));}  \
                                                void Apply() override;  \
                                                RenderStateBlock *CreateCopy()const override{return(new rs_name(this));}  \
                                                \
                                            public:

        #define RSB_OBJECT_END              };

        RSB_OBJECT_BEGIN(ColorState)
            bool red    =true;
            bool green  =true;
            bool blue   =true;
            bool alpha  =true;

            bool clear_color=false;
            GLfloat clear_color_value[4]={0,0,0,0};
        RSB_OBJECT_END

        enum class DEPTH_TEST
        {
            NEVER        =GL_NEVER,
            LESS         =GL_LESS,
            EQUAL        =GL_EQUAL,
            LEQUAL       =GL_LEQUAL,
            GREATER      =GL_GREATER,
            NOTEQUAL     =GL_NOTEQUAL,
            GEQUAL       =GL_GEQUAL,
            ALWAYS       =GL_ALWAYS,
        };//enum class DEPTH_TEST_FUNC

        RSB_OBJECT_BEGIN(DepthState)
            GLfloat     near_depth=0,
                        far_depth=1;

            bool        depth_mask=true;
            GLfloat     clear_depth_value=far_depth;    //清空深度时所用的值
            bool        clear_depth;                    //是否要清空深度

            DEPTH_TEST  depth_func=DEPTH_TEST::LESS;
            bool        depth_test;
        RSB_OBJECT_END

        enum class FACE
        {
            FRONT             =GL_FRONT,
            BACK              =GL_BACK,
            FRONT_AND_BACK    =GL_FRONT_AND_BACK,
        };//enum class CULL_FACE_MODE

        RSB_OBJECT_BEGIN(CullFaceState)
            bool    enabled =true;
            FACE    mode    =FACE::BACK;
        RSB_OBJECT_END

        enum class FILL_MODE
        {
            POINT  =GL_POINT,
            LINE   =GL_LINE,
            FACE   =GL_FILL,
        };//enum class FILL_MODE

        RSB_OBJECT_BEGIN(PolygonModeState)
            FACE        face=FACE::FRONT_AND_BACK;
            FILL_MODE   mode=FILL_MODE::FACE;
        RSB_OBJECT_END

        /**
         * 具体渲染状态数据
         */
        class RenderStateData:public RenderStateBlock
        {
            Set<RenderStateBlock *> state_list;

        public:

            RenderStateData()=default;
            RenderStateData(const RenderStateData *obj)
            {
                operator = (obj);
            }

            RenderStateData(const RenderStateData &obj)
            {
                operator = (&obj);
            }

            void Add(RenderStateBlock *rsb)
            {
                if(!rsb)
                    return;

                state_list.Add(rsb);
            }

            void Apply() override
            {
                const int count=state_list.GetCount();

                if(count<=0)
                    return;

                RenderStateBlock **rsb=state_list.GetData();

                for(int i=0;i<count;i++)
                {
                    (*rsb)->Apply();
                    ++rsb;
                }
            }

            RenderStateBlock *CreateCopy() const override
            {
                return(new RenderStateData(this));
            }

            void operator = (const RenderStateData *obj)
            {
                const int count=obj->state_list.GetCount();

                if(count<=0)return;

                RenderStateBlock **rsb=obj->state_list.GetData();

                for(int i=0;i<count;i++)
                {
                    state_list.Add((*rsb)->CreateCopy());
                    ++rsb;
                }
            }
        };//class RenderStateData:public RenderStateBlock

        /**
         * 渲染状态
         */
        class RenderState
        {
            RenderStateData state_data;

        public:

            RenderState()=default;
            RenderState(const RenderStateData &rsd):state_data(rsd){}
            virtual ~RenderState()=default;

            virtual void Add(RenderStateBlock *rsb) { state_data.Add(rsb); }

            virtual void Apply()
            {
                state_data.Apply();
            }
        };//class RenderState
    }//namespace graph
}//namespace hgl
#endif//HGL_RENDER_STATE_INCLUDE
