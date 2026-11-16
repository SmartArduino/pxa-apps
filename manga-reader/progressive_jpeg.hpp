#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
namespace manga {
using JpegRead=std::size_t(*)(void*,std::span<std::byte>);
struct JpegHeader {unsigned width=0,height=0;};
bool jpeg_header(JpegRead,void*,JpegHeader&);
struct ProgressiveJpeg {
    std::uint8_t* pixels=nullptr;
    unsigned width=0,height=0,channels=3;
    bool limited=false;
    std::size_t peak_bytes=0;
    ProgressiveJpeg()=default;
    ProgressiveJpeg(const ProgressiveJpeg&)=delete;
    ProgressiveJpeg& operator=(const ProgressiveJpeg&)=delete;
    ~ProgressiveJpeg();
    bool load(JpegRead,void*);
};
}
