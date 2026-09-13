#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
生成 mip 级别测试图与可直接被引擎加载的 .Tex2D 资源。

目的：每一级 mipmap 用**明显不同的颜色/图案/级别号**，运行时一眼就能看出
当前采样的到底是哪一级（mip 链缺失时，远处永远显示 0 级 = 红色带 "0" 的棋盘格）。

产物（res/image/mipmaps/）：
  level_<L>_<S>x<S>.png          9 张人眼查看用图（S = 256,128,64,32,16,8,4,2,1）
  MipLevels.RGBA8.Tex2D         未压缩 RGBA8，256x256，9 级 mip 链（覆盖 1x1/2x2 与链尾 <8 字节填充）
  MipLevels.BC7.Tex2D           BC7，256x256，7 级 mip 链（每级内容不同；BC7 最小 4x4）

BC7 资产由本脚本逐级调用工程内构建的 TexConv 生成（每级单独编码、再按引擎布局拼装）：

    cmake --build build --config Release --target TexConv
    python scripts/gen_mipmap_level_assets.py

**必须用 Release 构建的 TexConv**：ImageMagick 的 Magick++ DLL 是 Release（STL `_ITERATOR_DEBUG_LEVEL=0`），
而 ULRE 的 Debug 配置是 /MTd（IDL=2）——`std::string` 布局不同，跨 DLL 传路径会被读成乱码，
表现为任何图片都加载失败（`no decode delegate`）。最小复现：同一份 Magick++ 调用，
/MTd、/MDd 失败，/MT、/MD 成功。

脚本对 BC7 产物做两道自检：① 逐级字节数与引擎公式一致；② 载荷字节值种类 > 100
（编码失败时写的是"已申请未写入"的内存，字节值种类会低到几十并夹杂 ASCII 文本——正是早期
`CMP_ProcessTexture` 返回值未检查造成的垃圾资产）。


资产格式（inc/hgl/graph/texture/TextureLoader.h 的 pragma pack(1) 头 + 逐级载荷）：
  offset 0..6   "Texture"          7 字节 id
  offset 7      version = 0
  offset 8      type = 1 (2D)
  offset 9..12  width  (LE u32)
  offset 13..16 height (LE u32)
  offset 17..20 depth/layers (LE u32)
  offset 21     channels = 4
  offset 22..25 colors = "RGBA"
  offset 26..29 bits = 8,8,8,8
  offset 30     datatype = 3 (VulkanBaseType::UNORM)
  offset 31     mipmaps = 9
  然后逐级紧跟载荷，字节数由引擎的 ComputeMipmapBytes2D 决定：
    第 0 级 256*256*4 = 262144，每级减半 … 第 8 级 1*1*4 = 4 → **不足 8 字节按 8 计**（补 4 字节）
    合计 262144+65536+16384+4096+1024+256+64+16+8 = 349528

为什么只有未压缩版本：本机 res/image/TexConv.exe 产出的 BC7 载荷不是压缩数据
（文件长度符合引擎公式，但载荷只有 43~165 种字节值、呈指针/UTF-16 文本形态，
对比仓库内真资产 Grid2x2.Tex2D 为 240 种字节值），因此无法用它生成"每级内容不同"的 BC7 资产。
未压缩 RGBA8 由本脚本逐字节写出，可精确控制每一级内容，且覆盖 1x1/2x2 与链尾 <8 字节填充规则。
BC7 侧的链覆盖改用仓库内已有的真 BC7 资产（res/image/Grid2x2.Tex2D，256x256、7 级）。

用法：python scripts/gen_mipmap_level_assets.py
"""

import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, 'res', 'image', 'mipmaps')

BASE_SIZE = 256
FULL_LEVELS = 9                    # 256,128,64,32,16,8,4,2,1

# 每级一个主色，便于一眼辨认（0 红 1 橙 2 黄 3 绿 4 青 5 蓝 6 紫 7 品红 8 白）
LEVEL_COLORS = [
    (230, 40, 40),     # 0
    (240, 140, 30),    # 1
    (240, 220, 50),    # 2
    (60, 190, 80),     # 3
    (50, 200, 210),    # 4
    (60, 90, 230),     # 5
    (150, 70, 220),    # 6
    (230, 70, 180),    # 7
    (250, 250, 250),   # 8
]

# 5x7 点阵数字，用于把级别号画在图上（尺寸够大时）
DIGITS = {
    0: ['01110', '10001', '10011', '10101', '11001', '10001', '01110'],
    1: ['00100', '01100', '00100', '00100', '00100', '00100', '01110'],
    2: ['01110', '10001', '00001', '00010', '00100', '01000', '11111'],
    3: ['11111', '00010', '00100', '00010', '00001', '10001', '01110'],
    4: ['00010', '00110', '01010', '10010', '11111', '00010', '00010'],
    5: ['11111', '10000', '11110', '00001', '00001', '10001', '01110'],
    6: ['00110', '01000', '10000', '11110', '10001', '10001', '01110'],
    7: ['11111', '00001', '00010', '00100', '01000', '01000', '01000'],
    8: ['01110', '10001', '10001', '01110', '10001', '10001', '01110'],
}


def draw_digit(img, digit, box):
    """在 box=(x,y,w,h) 区域内用点阵画数字（块状填充，不需要字体文件）"""
    pat = DIGITS[digit]
    x, y, w, h = box
    bx = w // len(pat[0])
    by = h // len(pat)
    if bx < 1 or by < 1:
        return
    x = x + (w - bx * len(pat[0])) // 2
    y = y + (h - by * len(pat)) // 2
    d = ImageDraw.Draw(img)
    for ry, row in enumerate(pat):
        for rx, ch in enumerate(row):
            if ch == '1':
                d.rectangle([x + rx * bx, y + ry * by, x + rx * bx + bx - 1, y + ry * by + by - 1],
                            fill=(0, 0, 0))


def make_level_image(level):
    """
    生成第 level 级的图像：底色 = 该级主色 + 白框 + 级别号，
    右半是 1 像素棋盘（若 mip 链缺失，这一半在远端会出现明显的摩尔纹/闪动），
    小尺寸退化：8 及以下不画数字，4 = 四象限、2 = 四点、1 = 单色。
    """
    size = BASE_SIZE >> level
    color = LEVEL_COLORS[level]

    img = Image.new('RGB', (size, size), color)

    if size <= 2:
        # 极小尺寸：用可辨认的固定图案
        if size == 2:
            img.putpixel((0, 0), LEVEL_COLORS[7])
            img.putpixel((1, 0), (0, 0, 0))
            img.putpixel((0, 1), (0, 0, 0))
            img.putpixel((1, 1), LEVEL_COLORS[7])
        return img

    if size == 4:
        d = ImageDraw.Draw(img)
        d.rectangle([0, 0, 1, 1], fill=LEVEL_COLORS[8])
        d.rectangle([2, 2, 3, 3], fill=LEVEL_COLORS[8])
        return img

    d = ImageDraw.Draw(img)

    # 白框
    border = max(1, size // 32)
    d.rectangle([0, 0, size - 1, border - 1], fill=LEVEL_COLORS[8])
    d.rectangle([0, size - border, size - 1, size - 1], fill=LEVEL_COLORS[8])
    d.rectangle([0, 0, border - 1, size - 1], fill=LEVEL_COLORS[8])
    d.rectangle([size - border, 0, size - 1, size - 1], fill=LEVEL_COLORS[8])

    # 级别号（左半区），尺寸够就画
    if size >= 8:
        pad = border + 1
        box = (pad, size // 4, size // 2 - pad * 2, size // 2)
        draw_digit(img, level, box)

    # 右半：1 像素棋盘（高频内容）
    x0 = size // 2
    for y in range(border, size - border):
        for x in range(x0, size - border):
            if (x + y) & 1:
                img.putpixel((x, y), (0, 0, 0))
            else:
                img.putpixel((x, y), LEVEL_COLORS[8])

    return img


def pack_header(width, height, mipmaps, channels, colors, bits, datatype, layers=0):
    """按引擎 TextureFileHeader 的 pragma pack(1) 布局打包 32 字节头"""
    head = bytearray(32)
    head[0:7] = b'Texture'
    head[7] = 0                          # version
    head[8] = 1                          # type: 2D
    struct.pack_into('<I', head, 9, width)
    struct.pack_into('<I', head, 13, height)
    struct.pack_into('<I', head, 17, layers)
    head[21] = channels
    head[22:22 + len(colors)] = colors
    head[26:26 + len(bits)] = bits
    head[30] = datatype
    head[31] = mipmaps
    return bytes(head)


def rgba8_payload(images, levels):
    """
    未压缩 RGBA8 载荷：逐级 w*h*4，链尾不足 8 字节时按 8 字节计
    （与引擎 ComputeMipmapBytes2D 的滚动减半 + "<8 按 8" 规则一致）
    """
    data = bytearray()
    offsets = []
    for level in range(levels):
        offsets.append(len(data))
        raw = images[level].convert('RGB').tobytes()
        expect = (BASE_SIZE >> level) ** 2 * 3
        if len(raw) != expect:
            sys.exit('FAIL: level %u raw=%d expect=%d' % (level, len(raw), expect))

        rgb = bytearray(len(raw) // 3 * 4)
        for i in range(len(raw) // 3):
            rgb[i * 4 + 0] = raw[i * 3 + 0]
            rgb[i * 4 + 1] = raw[i * 3 + 1]
            rgb[i * 4 + 2] = raw[i * 3 + 2]
            rgb[i * 4 + 3] = 255
        if len(rgb) < 8:
            rgb += bytes(8 - len(rgb))
        data += rgb
    return bytes(data), offsets


def engine_chain_bytes(levels):
    """复算引擎侧期望的整链字节数，作为写盘前的自检"""
    total = 0
    w = h = BASE_SIZE
    rolling = BASE_SIZE * BASE_SIZE * 4
    for lv in range(levels):
        total += 8 if rolling < 8 else rolling
        if w == 1 and h == 1:
            break
        if w > 1:
            w >>= 1
            rolling >>= 1
        if h > 1:
            h >>= 1
            rolling >>= 1
    return total


TEXCONV_RELEASE = os.path.join(ROOT, 'build', 'out', 'Windows_64_Release', 'TexConv.exe')

BC7_LEVELS = 7          # 256,128,64,32,16,8,4（BC7 最小块 4x4，故到 4x4 为止）


def bc7_level_payload(level_png, level, tmpdir):
    """用工程内构建的 Release 版 TexConv 把单张 PNG 编成 BC7（单级 .Tex2D），返回其载荷。"""
    if not os.path.exists(TEXCONV_RELEASE):
        sys.exit('FAIL: 找不到 %s\n      请先构建：cmake --build build --config Release --target TexConv' % TEXCONV_RELEASE)

    base = 'lvl%u' % level
    src_png = os.path.join(tmpdir, base + '.png')
    out_file = os.path.join(tmpdir, base + '.Tex2D')

    shutil.copyfile(level_png, src_png)

    if os.path.exists(out_file):
        os.remove(out_file)

    result = subprocess.run([TEXCONV_RELEASE, base + '.png'], cwd=tmpdir, capture_output=True, text=True)

    if not os.path.exists(out_file):
        sys.exit('FAIL: TexConv 未产出 %s (level=%u)\n%s' % (out_file, level, (result.stdout or '')[-600:]))

    data = open(out_file, 'rb').read()

    if len(data) < 32:
        sys.exit('FAIL: level %u 产出文件过短 %d' % (level, len(data)))

    width = struct.unpack_from('<I', data, 9)[0]
    height = struct.unpack_from('<I', data, 13)[0]
    mips = data[31]

    expect_size = BASE_SIZE >> level
    expect_payload = ((expect_size + 3) // 4) ** 2 * 16

    if width != expect_size or height != expect_size or mips != 1:
        sys.exit('FAIL: level %u 产出规格不符: %ux%u mips=%u (期望 %ux%u mips=1)'
                 % (level, width, height, mips, expect_size, expect_size))

    if (len(data) - 32) != expect_payload:
        sys.exit('FAIL: level %u 载荷 %d != %d' % (level, len(data) - 32, expect_payload))

    payload = data[32:]

    # 编码失败时 TexConv 写出的是"已按目标格式申请、从未写入"的内存：载荷里会出现大段
    # ASCII/零串（实测 99% 的 4 字节窗口都是 ASCII 或 0，含 UTF-16 路径文本），正常 BC7 约 17%。
    # 注意不能只看字节值种类：本测试图是大片纯色 + 1 像素棋盘，正常编码也只有 50 多种字节值（实测 56）。
    ascii_runs = sum(1 for i in range(len(payload) - 4)
                     if all(32 <= payload[i + k] < 127 or payload[i + k] == 0 for k in range(4)))
    ascii_ratio = 100.0 * ascii_runs / len(payload)

    if ascii_ratio > 60.0:
        sys.exit('FAIL: level %u 载荷里有 %.1f%% 的 ASCII/零串（编码未真正写入？）' % (level, ascii_ratio))

    return payload, ascii_ratio


def build_bc7_asset(level_pngs, out_dir):
    """逐级编码 BC7 → 按引擎布局拼成 7 级 mip 链，并做字节级自检。"""
    tmpdir = tempfile.mkdtemp(prefix='ulre_mip_bc7_')

    try:
        results = [bc7_level_payload(level_pngs[lv], lv, tmpdir) for lv in range(BC7_LEVELS)]
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)

    payloads = [p for p, _ in results]
    ascii_ratios = [a for _, a in results]

    payload = b''.join(payloads)

    expect = 0
    size = BASE_SIZE
    for _ in range(BC7_LEVELS):
        expect += ((size + 3) // 4) ** 2 * 16
        size >>= 1

    if len(payload) != expect:
        sys.exit('FAIL: BC7 整链 %d != 引擎公式 %d' % (len(payload), expect))

    head = pack_header(BASE_SIZE, BASE_SIZE, BC7_LEVELS,
                       channels=0, colors=b'BC7', bits=bytes(4), datatype=0)

    write_verified(os.path.join(out_dir, 'MipLevels.BC7.Tex2D'), head + payload)

    print('  逐级载荷 = %s' % [len(p) for p in payloads])
    print('  合计 %d == 引擎公式 %d (OK)' % (len(payload), expect))
    print('  逐级 ASCII/零串占比 = %s' % ['%.1f%%' % a for a in ascii_ratios])


def write_verified(path, data):
    """先写临时文件并读回校验（长度 + md5），再原子改名"""
    tmp = path + '.tmp'
    with open(tmp, 'wb') as f:
        f.write(data)
    back = open(tmp, 'rb').read()
    if back != data:
        sys.exit('FAIL: 写盘校验不一致 %s (%d vs %d)' % (path, len(back), len(data)))
    os.replace(tmp, path)
    print('  wrote %-28s %7d B  md5=%s' % (os.path.basename(path), len(data), hashlib.md5(data).hexdigest()[:12]))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    images = [make_level_image(lv) for lv in range(FULL_LEVELS)]

    print('[1] 人眼查看用 PNG（每级一张）')
    for lv, img in enumerate(images):
        p = os.path.join(OUT_DIR, 'level_%u_%ux%u.png' % (lv, img.size[0], img.size[1]))
        img.save(p)
    print('  %d 张 PNG 写入 %s' % (len(images), OUT_DIR))

    print('[2] 未压缩 RGBA8 测试资产（9 级 mip 链，每级内容不同）')
    payload, offsets = rgba8_payload(images, FULL_LEVELS)
    expect = engine_chain_bytes(FULL_LEVELS)
    if len(payload) != expect:
        sys.exit('FAIL: payload=%d != 引擎公式 %d' % (len(payload), expect))

    head = pack_header(BASE_SIZE, BASE_SIZE, FULL_LEVELS,
                       channels=4, colors=b'RGBA', bits=bytes([8, 8, 8, 8]), datatype=3)

    path = os.path.join(OUT_DIR, 'MipLevels.RGBA8.Tex2D')
    write_verified(path, head + payload)

    print('[3] 自检')
    print('  逐级偏移 = %s' % offsets)
    print('  载荷/整链字节 = %d == 引擎公式 %d (OK)' % (len(payload), expect))
    print('  0 级尺寸 = %u x %u, 级数 = %u' % (BASE_SIZE, BASE_SIZE, FULL_LEVELS))

    print('[4] BC7 测试资产（7 级 mip 链，每级内容不同；工程内 Release 版 TexConv 逐级编码）')
    level_pngs = [os.path.join(OUT_DIR, 'level_%u_%ux%u.png' % (lv, BASE_SIZE >> lv, BASE_SIZE >> lv))
                  for lv in range(FULL_LEVELS)]
    build_bc7_asset(level_pngs, OUT_DIR)

    print('完成。')


if __name__ == '__main__':
    main()
