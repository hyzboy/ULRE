#include<hgl/graph/Texture1D.h>
#include<hgl/LogInfo.h>

namespace hgl
{
    namespace graph
    {
        class Texture1DDSA:public Texture1D
        {
        public:

            using Texture1D::Texture1D;

            bool _SetImage(Texture1DData *tex) override
            {
                glTextureStorage1D(texture_id, 1, tex->video_format, tex->length);

                if(!tex->bitmap)
                    return(true);

                if(tex->source_format->compress)      //原本就是压缩格式
                    glCompressedTextureSubImage1D(texture_id, 0, 0, tex->length, tex->video_format, tex->bitmap_bytes, tex->bitmap);
                else                    //正常非压缩格式
                    glTextureSubImage1D(texture_id, 0, 0, tex->length, tex->source_format->pixel_format, tex->source_format->data_type, tex->bitmap);

                if(tex->gen_mipmaps)
                {
                    glGenerateTextureMipmap(texture_id);

                    //                  glTexEnvf(GL_TEXTURE_FILTER_CONTROL,GL_TEXTURE_LOD_BIAS,-1.5f);     //设置LOD偏向,负是更精细，正是更模糊
                }

                return(true);
            }

            int _GetImage(void *data_pointer, TSF fmt, int level) override
            {
                int compress;
                int bytes;

                const TextureFormat *tsf = TextureFormatInfoList + fmt;

                glGetTextureLevelParameteriv(texture_id, level, GL_TEXTURE_COMPRESSED, &compress);

                if (compress)
                {
                    glGetTextureLevelParameteriv(texture_id, level, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &bytes);

                    if (data_pointer)
                        glGetCompressedTextureImage(texture_id, level, bytes, data_pointer);
                }
                else
                {
                    if (tsf->video_bytes == 0)return(-1);

                    bytes = length*tsf->video_bytes;

                    if (data_pointer)
                        glGetTextureImage(texture_id, level, tsf->pixel_format, tsf->data_type, bytes, data_pointer);
                }

                return(bytes);
            }

            bool _ChangeImage(uint s, uint l, void *data, uint bytes, TSF sf) override
            {
                const TextureFormat *sfmt = TextureFormatInfoList + sf;       //原始数据格式

                if (sfmt->compress)
                    glCompressedTextureSubImage1D(texture_id, 0, s, l, sfmt->video_format, bytes, data);
                else
                    glTextureSubImage1D(texture_id, 0, s, l, sfmt->pixel_format, sfmt->data_type, data);

                return(true);
            }

            void GenMipmaps() override
            {
                glGenerateTextureMipmap(texture_id);
            }

            void GetMipmapLevel(int &base_level,int &max_level) override
            {
                glGetTextureParameteriv(texture_id,GL_TEXTURE_BASE_LEVEL,&base_level);
                glGetTextureParameteriv(texture_id,GL_TEXTURE_MAX_LEVEL,&max_level);
            }
        };//class Texture1DDSA:public Texture1D

        Texture1D *CreateTexture1DDSA()
        {
            uint id;

            glCreateTextures(GL_TEXTURE_1D,1,&id);

            return(new Texture1DDSA(id));
        }
    }//namespace graph
}//namespace hgl
