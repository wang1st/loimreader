# LoimReader 第三方软件声明

LoimReader 3.0 的新发行链路使用下列宽松许可组件。Qt、Poppler-Qt
及 GPL-only 组件不进入 3.0 的链接或安装清单。

## SDL 3.4.10

- 来源：https://github.com/libsdl-org/SDL
- 固定提交：`8e37db5e797b6167f3a00d697d816a684bd259c7`
- 许可：zlib License
- 完整许可文本：安装包中的 `SDL3-LICENSE.txt`

## SDL_image 3.4.4

- 来源：https://github.com/libsdl-org/SDL_image
- 固定提交：`bec9134a26c7d0f31b36d6083c25296e04cabff5`
- 许可：zlib License
- 完整许可文本：安装包中的 `SDL3_image-LICENSE.txt`

## SDL_ttf 3.2.2

- 来源：https://github.com/libsdl-org/SDL_ttf
- 固定提交：`a1ce3670aec736ecbf0936c43f2f0cc53aa61e5b`
- 许可：zlib License
- 完整许可文本：安装包中的 `SDL3_ttf-LICENSE.txt`

## FreeType 2.13.2

SDL_ttf 在本项目中静态使用其随附的 FreeType 字体渲染后端。

- 来源：https://gitlab.freedesktop.org/freetype/freetype
- 固定提交：`9973564cfa63763a3e4ac67c09147899539b1e07`
- 许可：FreeType Project License
- 完整许可文本：安装包中的 `FreeType-LICENSE.txt`

## stb_image 2.30

SDL_image 在本项目的 PNG/JPEG 解码后端中使用 `stb_image`。

- 作者：Sean Barrett 及贡献者
- 来源：https://github.com/nothings/stb
- 选择许可：MIT License

Copyright (c) 2017 Sean Barrett

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## libwebp 1.3.2

LoimReader 静态使用 libwebp 解码 WebP 图片，包括扩展名被错误写成
`.jpg` 的 WebP 内容。

- 来源：https://github.com/libsdl-org/libwebp
- 固定提交：`42611dee80e9dd744e6fca13c11c682d36463b9d`
- 许可：BSD 3-Clause License
- 完整许可文本：安装包中的 `libwebp-LICENSE.txt`

## Monocypher 4.0.3

Ed25519 verification uses unmodified Monocypher sources, dual licensed under
BSD-2-Clause OR CC0-1.0. See vendor/monocypher/LICENCE.md and the installed
Monocypher-LICENSE.txt. Upstream: https://monocypher.org/.
