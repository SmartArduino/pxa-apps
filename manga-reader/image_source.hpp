#pragma once
#include <pxa/fs.hpp>
#include <optional>
#include <cstring>
namespace manga {
struct ImageSource {
    std::optional<pxa::File> file;std::span<const std::byte> memory{};std::size_t cursor=0;
    pxa::Result<std::uint32_t> read(std::span<std::byte> out){if(file)return file->read(out);auto n=std::min(out.size(),memory.size()-cursor);std::memcpy(out.data(),memory.data()+cursor,n);cursor+=n;return std::uint32_t(n);}
    pxa::Task<std::uint64_t> seek(std::int64_t offset){if(file)co_return co_await file->seek(offset);if(offset<0||std::uint64_t(offset)>memory.size())co_return std::unexpected(pxa::Error::invalid_argument);cursor=offset;co_return std::uint64_t(cursor);}
    pxa::Result<void> close(){if(file)return file->close();return {};}
};
}
