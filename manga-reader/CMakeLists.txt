cmake_minimum_required(VERSION 3.20)
project(manga_reader LANGUAGES C CXX)
add_compile_definitions(PXA_COROUTINE_SLOT_COUNT=16 PXA_COROUTINE_SLOT_BYTES=2048)
find_package(Pxa CONFIG REQUIRED)
pxa_add_app(main SOURCES main.cpp progressive_jpeg.cpp jpeg_rgb.cpp vendor/picojpeg/picojpeg.c vendor/tiny-aes/aes.c)
target_compile_definitions(main PRIVATE CBC=1 ECB=0 CTR=0)
target_link_options(main PRIVATE "-Wl,-z,stack-size=65536")

# Decoder-only libwebp; scalar C, no threads, no encoder/mux or system linkage.
file(GLOB WEBP_DECODER_SOURCES CONFIGURE_DEPENDS vendor/libwebp/src/dec/*.c vendor/libwebp/src/dsp/*.c vendor/libwebp/src/utils/*.c)
add_library(manga_webp STATIC ${WEBP_DECODER_SOURCES} webp_alloc.c)
target_include_directories(manga_webp PRIVATE webp_config vendor/libwebp)
target_compile_definitions(manga_webp PRIVATE HAVE_CONFIG_H=1)
set_source_files_properties(vendor/libwebp/src/utils/utils.c PROPERTIES COMPILE_DEFINITIONS "malloc=manga_webp_malloc;calloc=manga_webp_calloc;free=manga_webp_free")
target_link_libraries(main PRIVATE manga_webp)

# zlib's permissive encoder only. All use stays inside this application.
add_library(manga_deflate STATIC vendor/zlib/deflate.c vendor/zlib/trees.c vendor/zlib/adler32.c vendor/zlib/zutil.c)
target_compile_definitions(manga_deflate PRIVATE Z_SOLO NO_GZIP)
target_link_libraries(main PRIVATE manga_deflate)
