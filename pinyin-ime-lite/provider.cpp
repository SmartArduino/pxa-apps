#include <pxa/app.hpp>
#include "../pinyin-ime/session.hpp"
#include "engine.hpp"

struct LiteProvider {
    PinyinSession<pinyin_lite::CharacterEngine> session;
    pxa::Result<void> on_start(pxa::Context&, std::span<const std::byte>) {
        ime_t9_init(); return {};
    }
    pxa::Result<bool> on_event(pxa::Context& ctx, const pxa::Event& event) {
        auto input = pxa::ime::decode_input(event);
        if (!input) return input.error() == pxa::Error::not_found ? pxa::Result<bool>(false) : std::unexpected(input.error());
        auto result = session.process(*input, ctx.transport());
        if (!result && result.error() != pxa::Error::bad_state && result.error() != pxa::Error::denied)
            return std::unexpected(result.error());
        return true;
    }
    void on_stop(pxa::StopReason) { session.reset(); ime_t9_deinit(); }
};
PXA_SERVICE(LiteProvider)
