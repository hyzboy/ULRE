#pragma once
#define NTB_BIT_NORMAL                  (1<<0)
#define NTB_BIT_TANGENT                 (1<<1)
#define NTB_BIT_BINORMAL                (1<<2)

#define NTB_BIT_ALL                     (NTB_BIT_NORMAL|NTB_BIT_TANGENT|NTB_BIT_BINORMAL)

#define NTB_BIT_COMPRESS_NORMAL         (1<<3)
#define NTB_BIT_COMPRESS_TANGENT        (1<<4)
#define NTB_BIT_COMPRESS_BINORMAL       (1<<5)

#define NTB_BIT_COMPRESS_ALL            (NTB_BIT_COMPRESS_NORMAL|NTB_BIT_COMPRESS_TANGENT|NTB_BIT_COMPRESS_BINORMAL)

#define NTB_BIT_COMPRESS_NORMAL_TANGENT (1<<6)
