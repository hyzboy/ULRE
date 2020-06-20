#include<hgl/graph/TileData.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            void AnalyseSize(int &fw,int &fh,const int w,const int h,const int count,const int max_texture_size)
			{
				int total,tw,th,t;

				fw=fh=0;

				tw=max_texture_size;
				while(tw>=w)
				{
					th=max_texture_size;
					while(th>=h)
					{
						t=(tw/w)*(th/h);

						if(!fw)
						{
							fw=tw;
							fh=th;

							total=t;
						}
						else
						{
							if(t==count)
							{
								//正好，就要这么大的

								fw=tw;
								fh=th;

								return;
							}
							else
							if(t>count)                //要比要求的最大值大
							{
								if(t<total)            //找到最接近最大值的
								{
									//比现在选中的更节省
									fw=tw;
									fh=th;

									total=t;
								}
							}
						}

						th>>=1;
					}

					tw>>=1;
				}
			}//void AnalyseSize
        }//namespace
    }//namespace graph
}//namespace hgl
