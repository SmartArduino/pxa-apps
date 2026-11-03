本目录为金庸 S60 原引擎提供独立 MIT SDL1 软件宿主、软件像素面、RWops 和真实 PNG 解码。没有复制 GPL SDLPoP 适配器，也没有替换原 GRP/RLE、Lua 或游戏逻辑。canonical SDL1 头保留 sdl12-compat 的 zlib 版权；stb_image 保留原 MIT/public-domain 声明。源码来源与 SHA 见 header-provenance.json、third_party/stb/head.json。

宿主包含 `jy_sdl.h` 后调用 `jy_sdl_bind(&ops)`。now_us 返回单调微秒；delay_us 必须实际等待/通过 C 栈桥合作返回（包括 duration=0 的让步），present 接收真实 SDL_Surface 和脏矩形列表；poll_event 可直接返回真实 SDL1 事件，也可由 jy_sdl_push_event 投递。错误回调结果会成为 SDL 错误；不能把尚未显示/等待的操作报告成功。SDL1 方向键保留 UP=273、DOWN=274、RIGHT=275、LEFT=276。事件过滤器、按键状态、modifier、按键重复使用原回调/宿主时钟；WaitEvent 通过真实 Delay 等待。

SetVideoMode 的 bpp=0 创建 32 位 RGBX 软件屏幕（Amask=0），UpdateRect/UpdateRects 把原脏矩形传给宿主。jy_sdl_copy_rgb24 提供有边界检查的 RGB24 转换。支持真实 8 位索引面、RGB555/RGB565、24 位和 32 位软件面，调色板、key、alpha、clip、像素格式转换、重叠 blit、外部 pitch；不支持的像素格式明确报错。每面最多 16MiB，pitch 不超过 Uint16 上限。未声称提供硬件加速、OpenGL 或线程。

RWops 使用 SDL1 canonical int 回调 ABI，文件通过 fs/jy_fs_fopen 流式读取；内存 RWops 保留 seek/read/write/count 边界。SDL_ttf 2.0.11 所需七个 SDL 函数和 RWops 宏可直接链接。IMG_Load/IMG_LoadPNG_RW 用固定官方 stb_image commit `003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5` 真正解码 PNG：ONLY_PNG/NO_STDIO/NO_HDR/NO_LINEAR/NO_SIMD，压缩输入上限 16MiB、4096 维/4M 像素上限。PNG 解码保留 palette/tRNS 透明度；原游戏 PNG 路径没有后续修改索引调色板的逻辑，原 GRP/RLE 索引面仍由原代码保留。音频设备初始化明确失败，原 InitSDL 正常禁用声音；Mix_* 由父工程 noaudio_device.c 实现，不假报可播放。

PNG 根据透明度解码为 RGB24 或 RGBA32，并将解码像素的所有权转交给软件 surface；大图不再复制一份完整的像素缓冲。PXA 图片宿主直接使用可由软件 blitter 处理的格式。`SDL_CreateRGBSurfaceFrom` 默认仍借用调用者内存；只有显式调用内部 `jy_sdl_own_pixels` 才转移所有权。

ARGB32 目标 alpha 的行为仍需按调用场景核对，不能将游戏的 RGBX32 屏幕兼容性推广为任意 SDL1 实现的逐像素一致性。字体的协程保护边界、Asyncify 和实际设备接入由父目录管理。

SDL1 文档和参考实现：

- https://www.libsdl.org/release/SDL-1.2.15/docs/html/sdlblitsurface.html
- https://www.libsdl.org/release/SDL-1.2.15/docs/html/sdlsetalpha.html
- https://www.libsdl.org/release/SDL-1.2.15/docs/html/sdlconvertsurface.html
- https://raw.githubusercontent.com/libsdl-org/sdl12-compat/release-1.2.68/src/SDL12_compat.c
