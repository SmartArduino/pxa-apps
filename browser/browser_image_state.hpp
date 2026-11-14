#pragma once
#include <pxa/core.hpp>
#include <cstdint>
#include <string_view>

namespace browser::image_load {
// Store the failure category in the existing byte per image, without keeping
// error strings or another URL/error table alive for the document.
enum class State : std::uint8_t {
    unseen, loading, ready, failed, unsupported, limited, denied,
    resources, timed_out, disabled, cancelled
};
static_assert(sizeof(State) == 1);
constexpr bool is_failure(State state) { return state >= State::failed; }
constexpr State failure(pxa::Error error) {
    switch (error) {
    case pxa::Error::unsupported: return State::unsupported;
    case pxa::Error::limit_exceeded: return State::limited;
    case pxa::Error::denied: return State::denied;
    case pxa::Error::resource_limit:
    case pxa::Error::quota_exceeded: return State::resources;
    case pxa::Error::timed_out: return State::timed_out;
    case pxa::Error::cancelled: return State::cancelled;
    default: return State::failed;
    }
}
constexpr bool retry_on_open(State state, bool explicit_retry = false) {
    return explicit_retry || (state != State::unsupported && state != State::limited &&
        state != State::denied && state != State::disabled);
}
constexpr std::string_view title(State state) {
    switch (state) {
    case State::unsupported: return "图片编码或响应暂不支持";
    case State::limited: return "图片超出加载限制";
    case State::denied: return "图片访问被拒绝";
    case State::resources: return "图片解码资源不足";
    case State::timed_out: return "图片加载超时";
    case State::disabled: return "GIF 加载已关闭";
    case State::cancelled: return "图片加载已停止";
    default: return "图片未能加载";
    }
}
constexpr std::string_view hint(State state, bool detailed) {
    if (state == State::resources && detailed) return "可切回省存模式再试";
    if (state == State::resources) return "可稍后重试";
    if (state == State::disabled) return "可在菜单中开启 GIF";
    if (state == State::cancelled) return "可继续加载当前图片";
    if (state == State::limited || state == State::unsupported || state == State::denied)
        return "可切换图片或返回网页";
    return "可检查网络后重试";
}
} // namespace browser::image_load
