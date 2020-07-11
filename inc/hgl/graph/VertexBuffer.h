#ifndef HGL_GRAPH_VERTEX_BUFFER_INCLUDE
#define HGL_GRAPH_VERTEX_BUFFER_INCLUDE

#include<hgl/type/Color3f.h>
#include<hgl/type/Color4f.h>
#include<hgl/type/RectScope.h>
#include<hgl/type/BaseString.h>
#include<hgl/graph/VertexBufferCreater.h>
#include<hgl/log/LogInfo.h>
namespace hgl
{
    namespace graph
    {
        /**
        * 顶点属性数据实际模板
        */
        template<typename T,int C> class VertexBufferBase:public VertexBufferCreater
        {
        protected:

            T *mem_type;                                                                                ///<符合当前类型的地址
            T *access;                                                                                  ///<当前访问地址

            T *start;                                                                                   ///<访问起始地址

        public:

            VertexBufferBase(uint32_t _size,const T *_data=nullptr):VertexBufferCreater(_size,C,sizeof(T))
            {
                mem_type=(T *)GetData();
                access=0;
                start=0;

                if(_data)
                    memcpy(mem_type,_data,total_bytes);
            }

            virtual ~VertexBufferBase()=default;

            void BufferData(const T *ptr)
            {
                if(!ptr)return;

                memcpy(mem_type,ptr,total_bytes);
            }

            /**
            * 取得数据区地址
            * @param offset 从第几个数据开始访问
            * @return 访问地址
            */
            T *Get(uint32_t offset=0)
            {
                if(!mem_type||offset>=count)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer::Get() out,offset:")+OSString::valueOf(offset));
                    return(nullptr);
                }

                return mem_type+offset*C;
            }

            /**
            * 开始访问数据区
            * @param offset 从第几个数据开始访问
            * @return 访问地址
            */
            void *Begin(uint32_t offset=0)
            {
                if(access)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer::Begin() access!=0,offset:")+OSString::valueOf(offset));
                    return(nullptr);
                }

                access=Get(offset);

                if(access)
                    start=access;

                return access;
            }

            /**
            * 结束访问
            */
            void End()
            {
                access=nullptr;
                start=nullptr;
            }

            /**
            * 写入指定数量的数据
            * @param vp 数据指针
            * @param number 数据数量
            */
            bool WriteData(const T *vp,const uint32_t number)
            {
                if(!this->access||this->access+C*number>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer::Write(const T *,number) out,number:")+OSString::valueOf(number));
                    return(false);
                }

                memcpy(access,vp,C*number*sizeof(T));

                access+=C*number;

                return(true);
            }
        };//class VertexBuffer

        /**
        * 一元数据缓冲区
        */
        template<typename T> class VertexBuffer1:public VertexBufferBase<T,1>
        {
        public:

            using VertexBufferBase<T,1>::VertexBufferBase;
            virtual ~VertexBuffer1()=default;

            VkFormat    GetDataType()const override;

            /**
            * 计算绑定盒
            * @param min_vertex 最小值坐标
            * @param max_vertex 最大值坐标
            */
            template<typename V>
            void GetBoundingBox(V &min_vertex,V &max_vertex) const
            {
                T *p=this->mem_type;

                //先以corner为最小值,length为最大值，求取最小最大值
                min_vertex.x=*p++;

                max_vertex=min_vertex;

                for(uint32_t i=1;i<this->count;i++)
                {
                    if(*p<min_vertex.x)min_vertex.x=*p;
                    if(*p>max_vertex.x)max_vertex.x=*p;
                    ++p;
                }
            }

            AABB GetAABB()const
            {
                vec min_point,max_point;

                GetBoundingBox(min_point,max_point);

                min_point.y=0;
                min_point.z=0;
                min_point.w=0;

                max_point.y=0;
                max_point.z=0;
                max_point.w=0;

                return AABB(min_point,max_point);
            }

            bool Write(const T v1)
            {
                if(!this->access||this->access+1>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer1::Write(const T) out"));
                    return(false);
                }

                *this->access++=v1;
                return(true);
            }

            /**
             * 将一个值重复多次写入缓冲区
             * @param v 值
             * @param count 写入数量
             */
            bool Write(const T v,const uint32_t count)
            {
                if(!this->access||this->access+count>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer1::Write(const T,")+OSString::valueOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                hgl_set(this->access,v,count);
                this->access+=count;
                return(true);
            }
        };//class VertexBuffer1

        /**
        * 二元数据缓冲区
        */
        template<typename T> class VertexBuffer2:public VertexBufferBase<T,2>
        {
        public:

            using VertexBufferBase<T,2>::VertexBufferBase;
            virtual ~VertexBuffer2()=default;

            VkFormat    GetDataType()const override;

            /**
            * 计算绑定盒
            * @param min_vertex 最小值坐标
            * @param max_vertex 最大值坐标
            */
            template<typename V>
            void GetBoundingBox(V &min_vertex,V &max_vertex) const
            {
                T *p=this->mem_type;

                //先以corner为最小值,length为最大值，求取最小最大值
                min_vertex.x=*p++;
                min_vertex.y=*p++;

                max_vertex=min_vertex;

                for(uint32_t i=1;i<this->count;i++)
                {
                    if(*p<min_vertex.x)min_vertex.x=*p;
                    if(*p>max_vertex.x)max_vertex.x=*p;
                    ++p;

                    if(*p<min_vertex.y)min_vertex.y=*p;
                    if(*p>max_vertex.y)max_vertex.y=*p;
                    ++p;
                }
            }

            AABB GetAABB()const
            {
                vec min_point,max_point;

                GetBoundingBox(min_point,max_point);

                min_point.z=0;
                min_point.w=0;
                max_point.z=0;
                max_point.w=0;

                return AABB(min_point,max_point);
            }

            bool Write(const T v1,const T v2)
            {
                if(!this->access||this->access+2>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer2::Write(const T ,const T) out"));
                    return(false);
                }

                *this->access++=v1;
                *this->access++=v2;

                return(true);
            }

            bool Write(const T *v)
            {
                if(!this->access||this->access+2>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer2::Write(T *) out"));
                    return(false);
                }

                *this->access++=*v++;
                *this->access++=*v;

                return(true);
            }

            template<typename V2>
            bool Write(const V2 &v)
            {
                if(!this->access||this->access+2>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer2::Write(vec2 &) out"));
                    return(false);
                }

                *this->access++=v.x;
                *this->access++=v.y;

                return(true);
            }

            /**
             * 将一个值重复多次写入缓冲区
             * @param v 值
             * @param count 写入数量
             */
            template<typename V2>
            bool Fill(const V2 &v,const uint32_t count)
            {
                if(!this->access||this->access+(count<<1)>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer1::Write(const Vector2f &,")+OSString::valueOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v.x;
                    *this->access++=v.y;
                }

                return(true);
            }

            bool WriteLine(const T start_x,const T start_y,const T end_x,const T end_y)
            {
                if(!this->access||this->access+4>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer2::WriteLine(T,T,T,T) out"));
                    return(false);
                }

                *this->access++=start_x;
                *this->access++=start_y;
                *this->access++=end_x;
                *this->access++=end_y;

                return(true);
            }

            template<typename V2>
            bool WriteLine(const V2 &start,const V2 &end)
            {
                if(!this->access||this->access+4>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer2::WriteLine(vec2,vec2) out"));
                    return(false);
                }

                *this->access++=start.x;
                *this->access++=start.y;
                *this->access++=end.x;
                *this->access++=end.y;

                return(true);
            }

            /**
            * 写入2D三角形
            */
            template<typename V2>
            bool WriteTriangle(const V2 &v1,const V2 &v2,const V2 &v3)
            {
                if(!this->access||this->access+6>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer2::WriteTriangle(vec2,vec2,vec2) out"));
                    return(false);
                }

                *this->access++=v1.x;
                *this->access++=v1.y;

                *this->access++=v2.x;
                *this->access++=v2.y;

                *this->access++=v3.x;
                *this->access++=v3.y;

                return(true);
            }

            /**
            * 写入2D三角形
            */
            template<typename V2>
            bool WriteTriangle(const V2 *v)
            {
                if(!this->access||this->access+6>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer2::WriteTriangle(vec2 *) out"));
                    return(false);
                }

                *this->access++=v->x;
                *this->access++=v->y;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;

                return(true);
            }

            /**
            * 写入2D四边形坐标数据
            */
            template<typename V2>
            bool WriteQuad(const V2 &lt,const V2 &rt,const V2 &rb,const V2 &lb)
            {
                if(WriteTriangle(lt,lb,rb))
                if(WriteTriangle(lt,rb,rt))
                    return(true);

                LOG_HINT(OS_TEXT("VertexBuffer2::WriteQuad(vec2 &,vec2 &,vec2 &,vec2 &) error"));
                return(false);
            }

            /**
            * 写入2D矩形（两个三角形）坐标数据
            */
            template<typename V>
            bool WriteRect(const T left,const T top,const T width,const T height)
            {
                const vec2<V> lt(left      ,top);
                const vec2<V> rt(left+width,top);
                const vec2<V> rb(left+width,top+height);
                const vec2<V> lb(left      ,top+height);

                return WriteQuad(lt,rt,rb,lb);
            }

            template<typename V>
            bool WriteRect(const RectScope2<V> &scope)
            {
                return WriteQuad(   scope.GetLeftTop(),
                                    scope.GetRightTop(),
                                    scope.GetRightBottom(),
                                    scope.GetLeftBottom());
            }

            template<typename V>
            bool WriteRectFan(const RectScope2<V> &scope)
            {
                if(!this->access||this->access+8>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer2::WriteRectFan(RectScope2 *) out"));
                    return(false);
                }
                
                *this->access++=scope.GetLeft();
                *this->access++=scope.GetBottom();
                
                *this->access++=scope.GetRight();
                *this->access++=scope.GetBottom();

                *this->access++=scope.GetRight();
                *this->access++=scope.GetTop();

                *this->access++=scope.GetLeft();
                *this->access++=scope.GetTop();

                return(true);
            }

            template<typename V>
            bool WriteRectTriangleStrip(const RectScope2<V> &scope)
            {
                if(!this->access||this->access+8>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer2::WriteRectTriangleStrip(RectScope2 *) out"));
                    return(false);
                }

                *this->access++=scope.GetLeft();
                *this->access++=scope.GetTop();

                *this->access++=scope.GetLeft();
                *this->access++=scope.GetBottom();

                *this->access++=scope.GetRight();
                *this->access++=scope.GetTop();

                *this->access++=scope.GetRight();
                *this->access++=scope.GetBottom();

                return(true);
            }
        };//class VertexBuffer2

        /**
        * 三元数据缓冲区
        */
        template<typename T> class VertexBuffer3:public VertexBufferBase<T,3>
        {
        public:

            using VertexBufferBase<T,3>::VertexBufferBase;
            virtual ~VertexBuffer3()=default;

            VkFormat    GetDataType()const override;

            /**
            * 计算绑定盒
            * @param min_vertex 最小值坐标
            * @param max_vertex 最大值坐标
            */
            template<typename V>
            void GetBoundingBox(V &min_vertex,V &max_vertex) const
            {
                T *p=this->mem_type;

                //先以corner为最小值,length为最大值，求取最小最大值
                min_vertex.x=*p++;
                min_vertex.y=*p++;
                min_vertex.z=*p++;

                max_vertex=min_vertex;

                for(uint32_t i=1;i<this->count;i++)
                {
                    if(*p<min_vertex.x)min_vertex.x=*p;
                    if(*p>max_vertex.x)max_vertex.x=*p;
                    ++p;

                    if(*p<min_vertex.y)min_vertex.y=*p;
                    if(*p>max_vertex.y)max_vertex.y=*p;
                    ++p;

                    if(*p<min_vertex.z)min_vertex.z=*p;
                    if(*p>max_vertex.z)max_vertex.z=*p;
                    ++p;
                }
            }

            AABB GetAABB()const
            {
                vec min_point,max_point;

                GetBoundingBox(min_point,max_point);

                min_point.w=0;
                max_point.w=0;

                return AABB(min_point,max_point);
            }

            bool Write(const T v1,const T v2,const T v3)
            {
                if(!this->access||this->access+3>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer3::Write(T,T,T) out"));
                    return(false);
                }

                *this->access++=v1;
                *this->access++=v2;
                *this->access++=v3;

                return(true);
            }

            bool Write3(const T *v)
            {
                if(!this->access||this->access+3>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer3::Write(T *) out"));
                    return(false);
                }

                *this->access++=*v++;
                *this->access++=*v++;
                *this->access++=*v;

                return(true);
            }

            template<typename V3>
            bool Write(const V3 &v)
            {
                if(!this->access||this->access+3>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer3::Write(vec3 &) out"));
                    return(false);
                }

                *this->access++=v.x;
                *this->access++=v.y;
                *this->access++=v.z;

                return(true);
            }

            /**
             * 将一个值重复多次写入缓冲区
             * @param v 值
             * @param count 写入数量
             */
            template<typename V3>
            bool Fill(const V3 &v,const uint32_t count)
            {
                if(!this->access||this->access+(count*3)>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer3::Write(const Vector3f,")+OSString::valueOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v.x;
                    *this->access++=v.y;
                    *this->access++=v.z;
                }

                return(true);
            }

            /**
            * 写入多个值到缓冲区
            * @param v 值
            * @param count 写入数量
            */
            template<typename V3>
            bool Write(const V3 *v,const uint32_t count)
            {
                if(!this->access||this->access+(count*3)>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer3::Write(const Vector3f,")+OSString::valueOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v->x;
                    *this->access++=v->y;
                    *this->access++=v->z;

                    ++v;
                }

                return(true);
            }

            bool Write(const Color3f &v)
            {
                if(!this->access||this->access+3>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer3::Write(color3f &) out"));
                    return(false);
                }

                *this->access++=v.r;
                *this->access++=v.g;
                *this->access++=v.b;

                return(true);
            }

            bool WriteLine(const T start_x,const T start_y,const T start_z,const T end_x,const T end_y,const T end_z)
            {
                if(!this->access||this->access+6>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer3::WriteLine(T,T,T,T,T,T) out"));
                    return(false);
                }

                *this->access++=start_x;
                *this->access++=start_y;
                *this->access++=start_z;
                *this->access++=end_x;
                *this->access++=end_y;
                *this->access++=end_z;

                return(true);
            }

            template<typename V3>
            bool WriteLine(const V3 &start,const V3 &end)
            {
                if(!this->access||this->access+6>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer3::WriteLine(vec3,vec3) out"));
                    return(false);
                }

                *this->access++=start.x;
                *this->access++=start.y;
                *this->access++=start.z;
                *this->access++=end.x;
                *this->access++=end.y;
                *this->access++=end.z;

                return(true);
            }

            /**
            * 写入3D三角形
            */
            template<typename V3>
            bool WriteTriangle(const V3 &v1,const V3 &v2,const V3 &v3)
            {
                if(!this->access||this->access+9>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer3::WriteTriangle(vec3,vec3,vec3) out"));
                    return(false);
                }

                *this->access++=v1.x;
                *this->access++=v1.y;
                *this->access++=v1.z;

                *this->access++=v2.x;
                *this->access++=v2.y;
                *this->access++=v2.z;

                *this->access++=v3.x;
                *this->access++=v3.y;
                *this->access++=v3.z;

                return(true);
            }

            /**
            * 写入3D三角形
            */
            template<typename V3>
            bool WriteTriangle(const V3 *v)
            {
                if(!this->access||this->access+9>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer3::WriteTriangle(vec3 *) out"));
                    return(false);
                }

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;

                return(true);
            }
        };//class VertexBuffer3

        /**
        * 四元数据缓冲区
        */
        template<typename T> class VertexBuffer4:public VertexBufferBase<T,4>
        {
        public:

            using VertexBufferBase<T,4>::VertexBufferBase;
            virtual ~VertexBuffer4()=default;

            VkFormat    GetDataType()const override;

            /**
            * 计算绑定盒
            * @param min_vertex 最小值坐标
            * @param max_vertex 最大值坐标
            */
            template<typename V>
            void GetBoundingBox(V &min_vertex,V &max_vertex) const
            {
                T *p=this->mem_type;

                //先以corner为最小值,length为最大值，求取最小最大值
                min_vertex.x=*p++;
                min_vertex.y=*p++;
                min_vertex.z=*p++;
                ++p;

                max_vertex=min_vertex;

                for(uint32_t i=1;i<this->count;i++)
                {
                    if(*p<min_vertex.x)min_vertex.x=*p;
                    if(*p>max_vertex.x)max_vertex.x=*p;
                    ++p;

                    if(*p<min_vertex.y)min_vertex.y=*p;
                    if(*p>max_vertex.y)max_vertex.y=*p;
                    ++p;

                    if(*p<min_vertex.z)min_vertex.z=*p;
                    if(*p>max_vertex.z)max_vertex.z=*p;
                    ++p;

                    ++p;
                }
            }

            AABB GetAABB()const
            {
                vec min_point,max_point;

                GetBoundingBox(min_point,max_point);

                min_point.w=0;
                max_point.w=0;

                return AABB(min_point,max_point);
            }

            bool Write(const T v1,const T v2,const T v3,const T v4)
            {
                if(!this->access||this->access+4>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::Write(T,T,T,T) out"));
                    return(false);
                }

                *this->access++=v1;
                *this->access++=v2;
                *this->access++=v3;
                *this->access++=v4;

                return(true);
            }

            bool Write(const T *v)
            {
                if(!this->access||this->access+4>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::Write(T *) out"));
                    return(false);
                }

                *this->access++=*v++;
                *this->access++=*v++;
                *this->access++=*v++;
                *this->access++=*v;

                return(true);
            }

            template<typename V4>
            bool Write(const V4 &v)
            {
                if(!this->access||this->access+4>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::Write(color4 &) out"));
                    return(false);
                }

                *this->access++=v.x;
                *this->access++=v.y;
                *this->access++=v.z;
                *this->access++=v.w;

                return(true);
            }

            bool Write(const Color4f &v)
            {
                if(!this->access||this->access+4>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::Write(color4 &) out"));
                    return(false);
                }

                *this->access++=v.r;
                *this->access++=v.g;
                *this->access++=v.b;
                *this->access++=v.a;

                return(true);
            }

            bool Fill(const Color4f &v,const uint32_t count)
            {
                if(count<=0)return(false);

                if(!this->access||this->access+(4*count)>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::Write(color4 &,count) out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v.r;
                    *this->access++=v.g;
                    *this->access++=v.b;
                    *this->access++=v.a;
                }

                return(true);
            }

            /**
             * 将一个值重复多次写入缓冲区
             * @param v 值
             * @param count 写入数量
             */
            template<typename V4>
            bool Fill(const V4 &v,const uint32_t count)
            {
                if(!this->access||this->access+(count<<2)>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::Write(const Vector4f,")+OSString::valueOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v.x;
                    *this->access++=v.y;
                    *this->access++=v.z;
                    *this->access++=v.w;
                }

                return(true);
            }

            /**
            * 将多个值写入缓冲区
            * @param v 值
            * @param count 写入数量
            */
            template<typename V4>
            bool Write(const V4 *v,const uint32_t count)
            {
                if(!this->access||this->access+(count<<2)>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::Write(const Vector4f,")+OSString::valueOf(count)+OS_TEXT(") out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=v->x;
                    *this->access++=v->y;
                    *this->access++=v->z;
                    *this->access++=v->w;

                    ++v;
                }

                return(true);
            }

            bool WriteLine(const T start_x,const T start_y,const T start_z,const T end_x,const T end_y,const T end_z)
            {
                if(!this->access||this->access+8>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::WriteLine(T,T,T,T,T,T) out"));
                    return(false);
                }

                *this->access++=start_x;
                *this->access++=start_y;
                *this->access++=start_z;
                *this->access++=1.0f;
                *this->access++=end_x;
                *this->access++=end_y;
                *this->access++=end_z;
                *this->access++=1.0f;

                return(true);
            }

            template<typename V4>
            bool WriteLine(const V4 &start,const V4 &end)
            {
                if(!this->access||this->access+8>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::WriteLine(vec3,vec3) out"));
                    return(false);
                }

                *this->access++=start.x;
                *this->access++=start.y;
                *this->access++=start.z;
                *this->access++=1.0f;
                *this->access++=end.x;
                *this->access++=end.y;
                *this->access++=end.z;
                *this->access++=1.0f;

                return(true);
            }

            /**
            * 写入3D三角形
            */
            template<typename V4>
            bool WriteTriangle(const V4 &v1,const V4 &v2,const V4 &v3)
            {
                if(!this->access||this->access+12>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::WriteTriangle(vec3,vec3,vec3) out"));
                    return(false);
                }

                *this->access++=v1.x;
                *this->access++=v1.y;
                *this->access++=v1.z;
                *this->access++=1.0f;

                *this->access++=v2.x;
                *this->access++=v2.y;
                *this->access++=v2.z;
                *this->access++=1.0f;

                *this->access++=v3.x;
                *this->access++=v3.y;
                *this->access++=v3.z;
                *this->access++=1.0f;

                return(true);
            }

            /**
            * 写入3D三角形
            */
            template<typename V4>
            bool WriteTriangle(const V4 *v)
            {
                if(!this->access||this->access+12>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::WriteTriangle(vec3 *) out"));
                    return(false);
                }

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;
                *this->access++=1.0f;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;
                *this->access++=1.0f;
                ++v;

                *this->access++=v->x;
                *this->access++=v->y;
                *this->access++=v->z;
                *this->access++=1.0f;

                return(true);
            }

            /**
            * 写入2D矩形,注:这个函数会依次写入Left,Top,Width,Height四个值
            */
            template<typename V>
            bool WriteRectangle2D(const RectScope2<V> &rect)
            {
                if(!this->access||this->access+4>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::WriteRectangle2D(RectScope2 ) out"));
                    return(false);
                }

                *this->access++=rect.Left;
                *this->access++=rect.Top;
                *this->access++=rect.Width;
                *this->access++=rect.Height;

                return(true);
            }

            /**
            * 写入2D矩形,注:这个函数会依次写入Left,Top,Width,Height四个值
            */
            template<typename V>
            bool WriteRectangle2D(const RectScope2<V> *rect,const uint32_t count)
            {
                if(!this->access||this->access+(4*count)>this->mem_end)
                {
                    LOG_HINT(OS_TEXT("VertexBuffer4::WriteRectangle2D(RectScope2 *,count) out"));
                    return(false);
                }

                for(uint32_t i=0;i<count;i++)
                {
                    *this->access++=rect->Left;
                    *this->access++=rect->Top;
                    *this->access++=rect->Width;
                    *this->access++=rect->Height;

                    ++rect;
                }

                return(true);
            }
        };//class VertexBuffer4

        //缓冲区具体数据类型定义
        typedef VertexBuffer1<int8  >   VB1i8   ,VB1b;  template<> inline VkFormat VertexBuffer1<int8   >::GetDataType()const{return FMT_R8I;   }
        typedef VertexBuffer1<int16 >   VB1i16  ,VB1s;  template<> inline VkFormat VertexBuffer1<int16  >::GetDataType()const{return FMT_R16I;  }
        typedef VertexBuffer1<int32 >   VB1i32  ,VB1i;  template<> inline VkFormat VertexBuffer1<int32  >::GetDataType()const{return FMT_R32I;  }
        typedef VertexBuffer1<uint8 >   VB1u8   ,VB1ub; template<> inline VkFormat VertexBuffer1<uint8  >::GetDataType()const{return FMT_R8U;   }
        typedef VertexBuffer1<uint16>   VB1u16  ,VB1us; template<> inline VkFormat VertexBuffer1<uint16 >::GetDataType()const{return FMT_R16U;  }
        typedef VertexBuffer1<uint32>   VB1u32  ,VB1ui; template<> inline VkFormat VertexBuffer1<uint32 >::GetDataType()const{return FMT_R32U;  }
        typedef VertexBuffer1<float >   VB1f;           template<> inline VkFormat VertexBuffer1<float  >::GetDataType()const{return FMT_R32F;  }
        typedef VertexBuffer1<double>   VB1d;           template<> inline VkFormat VertexBuffer1<double >::GetDataType()const{return FMT_R64F;  }

        typedef VertexBuffer2<int8  >   VB2i8   ,VB2b;  template<> inline VkFormat VertexBuffer2<int8   >::GetDataType()const{return FMT_RG8I;  }
        typedef VertexBuffer2<int16 >   VB2i16  ,VB2s;  template<> inline VkFormat VertexBuffer2<int16  >::GetDataType()const{return FMT_RG16I; }
        typedef VertexBuffer2<int32 >   VB2i32  ,VB2i;  template<> inline VkFormat VertexBuffer2<int32  >::GetDataType()const{return FMT_RG32I; }
        typedef VertexBuffer2<uint8 >   VB2u8   ,VB2ub; template<> inline VkFormat VertexBuffer2<uint8  >::GetDataType()const{return FMT_RG8U;  }
        typedef VertexBuffer2<uint16>   VB2u16  ,VB2us; template<> inline VkFormat VertexBuffer2<uint16 >::GetDataType()const{return FMT_RG16U; }
        typedef VertexBuffer2<uint32>   VB2u32  ,VB2ui; template<> inline VkFormat VertexBuffer2<uint32 >::GetDataType()const{return FMT_RG32U; }
        typedef VertexBuffer2<float >   VB2f;           template<> inline VkFormat VertexBuffer2<float  >::GetDataType()const{return FMT_RG32F; }
        typedef VertexBuffer2<double>   VB2d;           template<> inline VkFormat VertexBuffer2<double >::GetDataType()const{return FMT_RG64F; }

//        typedef VertexBuffer3<int8  >   VB3i8   ,VB3b;  template<> inline VkFormat VertexBuffer3<int8   >::GetDataType()const{return FMT_RGB8I;  }
//        typedef VertexBuffer3<int16 >   VB3i16  ,VB3s;  template<> inline VkFormat VertexBuffer3<int16  >::GetDataType()const{return FMT_RGB16I; }
        typedef VertexBuffer3<int32 >   VB3i32  ,VB3i;  template<> inline VkFormat VertexBuffer3<int32  >::GetDataType()const{return FMT_RGB32I; }
//        typedef VertexBuffer3<uint8 >   VB3u8   ,VB3ub; template<> inline VkFormat VertexBuffer3<uint8  >::GetDataType()const{return FMT_RGB8U;  }
//        typedef VertexBuffer3<uint16>   VB3u16  ,VB3us; template<> inline VkFormat VertexBuffer3<uint16 >::GetDataType()const{return FMT_RGB16U; }
        typedef VertexBuffer3<uint32>   VB3u32  ,VB3ui; template<> inline VkFormat VertexBuffer3<uint32 >::GetDataType()const{return FMT_RGB32U; }
        typedef VertexBuffer3<float >   VB3f;           template<> inline VkFormat VertexBuffer3<float  >::GetDataType()const{return FMT_RGB32F; }
        typedef VertexBuffer3<double>   VB3d;           template<> inline VkFormat VertexBuffer3<double >::GetDataType()const{return FMT_RGB64F; }

        typedef VertexBuffer4<int8  >   VB4i8   ,VB4b;  template<> inline VkFormat VertexBuffer4<int8   >::GetDataType()const{return FMT_RGBA8I;  }
        typedef VertexBuffer4<int16 >   VB4i16  ,VB4s;  template<> inline VkFormat VertexBuffer4<int16  >::GetDataType()const{return FMT_RGBA16I; }
        typedef VertexBuffer4<int32 >   VB4i32  ,VB4i;  template<> inline VkFormat VertexBuffer4<int32  >::GetDataType()const{return FMT_RGBA32I; }
        typedef VertexBuffer4<uint8 >   VB4u8   ,VB4ub; template<> inline VkFormat VertexBuffer4<uint8  >::GetDataType()const{return FMT_RGBA8U;  }
        typedef VertexBuffer4<uint16>   VB4u16  ,VB4us; template<> inline VkFormat VertexBuffer4<uint16 >::GetDataType()const{return FMT_RGBA16U; }
        typedef VertexBuffer4<uint32>   VB4u32  ,VB4ui; template<> inline VkFormat VertexBuffer4<uint32 >::GetDataType()const{return FMT_RGBA32U; }
        typedef VertexBuffer4<float >   VB4f;           template<> inline VkFormat VertexBuffer4<float  >::GetDataType()const{return FMT_RGBA32F; }
        typedef VertexBuffer4<double>   VB4d;           template<> inline VkFormat VertexBuffer4<double >::GetDataType()const{return FMT_RGBA64F; }

        /**
         * 根据格式要求，创建对应的VBC
         * @param base_type 基础格式,参见spirv_cross/spirv_common.hpp中的spirv_cross::SPIRType
         * @param vecsize vec数量
         * @param vertex_count 顶点数量
         */
        VertexBufferCreater *CreateVB(const uint32_t base_type,const uint32_t vecsize,const uint32_t vertex_count);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_BUFFER_INCLUDE
