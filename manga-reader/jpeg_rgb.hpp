#pragma once
#include <cstdint>
#include <memory>
#include <vector>
namespace manga {
// Bounded, cooperative 4:4:4 JPEG writer. No decode, file access or Host linkage.
class JpegRgb {
    struct Impl;std::unique_ptr<Impl> impl;
public:
    JpegRgb();~JpegRgb();
    bool begin(const std::uint8_t* rgb,unsigned width,unsigned height,unsigned quality=95);
    bool step();bool done()const;bool limited()const;
    std::vector<std::uint8_t> take();
};
}
